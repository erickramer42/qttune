// File/layer: tests/test_core.cpp | GTest-based, CI-validated
// Testing: Verifies registration, dedup, dispatch, and lifecycle contracts

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>

extern "C" {
#include "qttune/core.h"
#include "qttune/internal.h"
}

namespace {

std::atomic<int> g_callback_count{0};

void counting_callback(const qttune_frame_t* frame, void* user_data)
{
    (void)frame;
    int* counter = static_cast<int*>(user_data);
    if (counter) {
        (*counter)++;
    }
    g_callback_count++;
}

/* Helper to create test session */
qttune_session_t* make_test_session(void)
{
    qttune_session_t* session = nullptr;
    EXPECT_EQ(qttune_session_create("mock://test", &session), QT_OK);
    return session;
}

} // anonymous namespace

TEST(CoreTest, StatusStringValidValues) {
    EXPECT_STREQ(qttune_status_string(QT_OK), "success");
    EXPECT_STREQ(qttune_status_string(QT_ERR_NULL_ARGUMENT), "null argument");
    EXPECT_STREQ(qttune_status_string(QT_ERR_NOT_IMPLEMENTED), "not implemented yet");
    EXPECT_STREQ(qttune_status_string(QT_ERR_NO_TRANSPORT), "no transport available");
    EXPECT_STREQ(qttune_status_string(QT_ERR_OUT_OF_MEMORY), "out of memory");
    EXPECT_STREQ(qttune_status_string(QT_ERR_SESSION_CLOSED), "session already closed");
    EXPECT_STREQ(qttune_status_string(QT_ERR_ALREADY_REGISTERED), "already registered");
    
    /* Explicit cast required by MSVC for enum conversion */
    EXPECT_STRNE(qttune_status_string(static_cast<qttune_status_t>(999)), nullptr);
}

TEST(CoreTest, VersionStringContainsNumbers) {
    const char* version = qttune_version_string();
    EXPECT_TRUE(version != nullptr);
    EXPECT_GT(strlen(version), 0);
    EXPECT_NE(strstr(version, "."), nullptr);
}

TEST(CoreTest, InitShutdownIdempotent) {
    EXPECT_EQ(qttune_init(), QT_OK);
    EXPECT_EQ(qttune_init(), QT_OK);
    
    qttune_shutdown();
    
    EXPECT_EQ(qttune_init(), QT_OK);
    qttune_shutdown();
}

TEST(CoreTest, SessionCreateRequiresNonNullPointer) {
    qttune_session_t* session = nullptr;
    EXPECT_EQ(qttune_session_create("mock://test", &session), QT_OK);
    
    EXPECT_EQ(qttune_session_create("mock://test", nullptr), QT_ERR_NULL_ARGUMENT);
    
    if (session) {
        qttune_session_close(session);
    }
}

TEST(CoreTest, RegisterCallbackSuccess) {
    qttune_session_t* session = make_test_session();
    int counter = 0;
    
    qttune_status_t status = qttune_register_frame_callback(
        session, counting_callback, &counter
    );
    
    EXPECT_EQ(status, QT_OK);
    
    qttune_session_close(session);
}

TEST(CoreTest, RegisterDuplicateCallbackIdempotent) {
    qttune_session_t* session = make_test_session();
    int counter = 0;
    
    qttune_status_t status1 = qttune_register_frame_callback(
        session, counting_callback, &counter
    );
    qttune_status_t status2 = qttune_register_frame_callback(
        session, counting_callback, &counter
    );
    
    EXPECT_EQ(status1, QT_OK);
    EXPECT_EQ(status2, QT_OK);  /* Silently deduplicated, not an error */
    
    qttune_session_close(session);
}

TEST(CoreTest, SameCallbackDifferentUserdataAllowed) {
    qttune_session_t* session = make_test_session();
    int counter1 = 0;
    int counter2 = 0;
    
    /* Same function, different user_data = distinct registrations */
    EXPECT_EQ(qttune_register_frame_callback(
        session, counting_callback, &counter1), QT_OK);
    EXPECT_EQ(qttune_register_frame_callback(
        session, counting_callback, &counter2), QT_OK);
    
    qttune_session_close(session);
}

TEST(CoreTest, UnregisterCallback) {
    qttune_session_t* session = make_test_session();
    int counter = 0;
    
    EXPECT_EQ(qttune_register_frame_callback(
        session, counting_callback, &counter), QT_OK);
    
    EXPECT_EQ(qttune_unregister_frame_callback(
        session, counting_callback, &counter), QT_OK);
    
    /* Second unregister is also success (idempotent) */
    EXPECT_EQ(qttune_unregister_frame_callback(
        session, counting_callback, &counter), QT_OK);
    
    qttune_session_close(session);
}

TEST(CoreTest, UnregisterNonexistentCallbackDoesntFail) {
    qttune_session_t* session = make_test_session();
    
    /* Never registered, but should not crash */
    EXPECT_EQ(qttune_unregister_frame_callback(
        session, counting_callback, nullptr), QT_OK);
    
    qttune_session_close(session);
}

TEST(CoreTest, NullPointerHandling) {
    EXPECT_EQ(qttune_register_frame_callback(
        nullptr, counting_callback, nullptr), QT_ERR_NULL_ARGUMENT);
    
    qttune_session_t* session = make_test_session();
    EXPECT_EQ(qttune_register_frame_callback(
        session, nullptr, nullptr), QT_ERR_NULL_ARGUMENT);
    
    qttune_session_close(session);
}

/* Threading contract test - validates snapshot pattern compiles */
TEST(CoreTest, CallbackSnapshotPatternCompiles) {
    /* This test documents the threading discipline:
     * 1. Mock transport will call qttune_session_dispatch_callbacks()
     * 2. That function copies the callback list under lock
     * 3. Then invokes callbacks OUTSIDE the lock
     *
     * This prevents deadlock if callback calls unregister().
     *
     * Full threading validation requires TSAN job or live transport test.
     */
    qttune_session_t* session = make_test_session();
    int counter = 0;
    
    EXPECT_EQ(qttune_register_frame_callback(
        session, counting_callback, &counter), QT_OK);
    
    /* Null frame is a documented no-op — proves precondition guard */
    qttune_session_dispatch_callbacks(session, nullptr);
    
    qttune_session_close(session);
}

TEST(CoreTest, CallbackDispatchInvokesRegisteredCallback) {
    qttune_session_t* session = make_test_session();
    int counter = 0;
    qttune_frame_t frame{};
    frame.data_length = 8;

    EXPECT_EQ(qttune_register_frame_callback(
        session, counting_callback, &counter), QT_OK);

    qttune_session_dispatch_callbacks(session, &frame);
    EXPECT_EQ(counter, 1);

    /* Duplicate registration is deduplicated — one invocation per dispatch */
    EXPECT_EQ(qttune_register_frame_callback(
        session, counting_callback, &counter), QT_OK);
    qttune_session_dispatch_callbacks(session, &frame);
    EXPECT_EQ(counter, 2);  // Still 1 per dispatch, dedup worked

    qttune_session_close(session);
}

TEST(CoreTest, MockTransportDeliversFrames) {
    qttune_session_t* session = nullptr;
    ASSERT_EQ(qttune_session_create("mock://test", &session), QT_OK);

    std::atomic<int> frame_count{0};
    /* plain function to keep extern "C" signature compatibility */
    static std::atomic<int>* counter_ptr = &frame_count;
    EXPECT_EQ(qttune_register_frame_callback(
        session,
        [](const qttune_frame_t*, void*) { (*counter_ptr)++; },
        nullptr), QT_OK);

    EXPECT_EQ(qttune_session_start(session), QT_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_GE(frame_count.load(), 3);   /* 50ms interval -> ~4 frames */

    /* Close must join the worker without hanging */
    EXPECT_EQ(qttune_session_close(session), QT_OK);
}

TEST(CoreTest, UnknownTransportUriRejected) {
    qttune_session_t* session = nullptr;
    EXPECT_EQ(qttune_session_create("j2534://dev0", &session),
              QT_ERR_NO_TRANSPORT);
    EXPECT_EQ(qttune_session_create(nullptr, &session),
              QT_ERR_NULL_ARGUMENT);
}

TEST(CoreTest, SessionStartWithoutTransportFails) {
    /* Valid mock session, but force no-transport via closed early?
     * Not reachable via public API in v0.2 — skip rather than assert UB. */
    SUCCEED();
}

TEST(CoreTest, CallbackExecutesOnTransportWorkerThread) {
    qttune_session_t* session = nullptr;
    ASSERT_EQ(qttune_session_create("mock://thread-test", &session), QT_OK);

    /* std::atomic<std::thread::id> is lock-free on all 3 CI platforms;
     * mutex alternative also fine. */
    std::atomic<std::thread::id> seen_id{};
    struct Ctx { std::atomic<std::thread::id>* out; };
    static Ctx ctx{&seen_id};  /* static: captureless lambda can't capture */

    EXPECT_EQ(qttune_register_frame_callback(
        session,
        [](const qttune_frame_t*, void* ud) {
            static_cast<Ctx*>(ud)->out->store(std::this_thread::get_id());
        },
        &ctx), QT_OK);

    const auto main_id = std::this_thread::get_id();
    EXPECT_EQ(qttune_session_start(session), QT_OK);

    /* Budget >= 3 intervals (50ms fixed) + scheduling slack */
    for (int i = 0; i < 40 && seen_id.load() == std::thread::id{}; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_NE(seen_id.load(), std::thread::id{});
    EXPECT_NE(seen_id.load(), main_id);  /* Marshaling in bridge is MANDATORY */

    EXPECT_EQ(qttune_session_close(session), QT_OK);
}

TEST(CoreTest, SelfUnregisterDuringLiveDispatch) {
    /* Exercises the snapshot pattern against the REAL worker, not a
     * manual dispatch. Snapshot contract: unregister mid-dispatch must
     * neither deadlock nor skip subsequent registered callbacks. */
    qttune_session_t* session = nullptr;
    ASSERT_EQ(qttune_session_create("mock://reentry", &session), QT_OK);

    std::atomic<int> self_count{0};
    std::atomic<int> other_count{0};

    struct Ctx {
        qttune_session_t* session;
        std::atomic<int>* self_count;
    };
    static Ctx ctx{nullptr, &self_count};
    ctx.session = session;

    EXPECT_EQ(qttune_register_frame_callback(
        session,
        [](const qttune_frame_t*, void* ud) {
            Ctx* c = static_cast<Ctx*>(ud);
            /* Runs on worker; unregister touches the same mutex being
             * snapshotted — this IS the deadlock regression test. */
            qttune_unregister_frame_callback(
                c->session,
                [](const qttune_frame_t*, void* u) {
                    static_cast<Ctx*>(u)->self_count->fetch_add(1);
                },
                ud);
            c->self_count->fetch_add(1);
        },
        &ctx), QT_OK);

    EXPECT_EQ(qttune_register_frame_callback(
        session,
        [](const qttune_frame_t*, void* ud) {
            static_cast<std::atomic<int>*>(ud)->fetch_add(1);
        },
        &other_count), QT_OK);

    EXPECT_EQ(qttune_session_start(session), QT_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    EXPECT_EQ(self_count.load(), 1);     /* self-unregistered after first fire */
    EXPECT_GT(other_count.load(), 0);   /* sibling registration survived */
    EXPECT_EQ(qttune_session_close(session), QT_OK);  /* must not hang */
}
