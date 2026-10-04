// File/layer: tests/test_core.cpp | GTest-based, CI-validated
// Testing: Verifies registration, dedup, dispatch, and lifecycle contracts

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

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
