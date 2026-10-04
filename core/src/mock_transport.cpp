// File/layer: core/src/mock_transport.cpp | C++17, Qt-free
// Testing: MockTransportDeliversFrames / MockTransportCloseJoinsWorker

#include "qttune/core.h"
#include "qttune/internal.h"
#include "session.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

constexpr uint32_t DEFAULT_FRAME_INTERVAL_MS = 50;

} // namespace

struct qttune_mock_transport {
    std::atomic<bool> stop_requested{false};
    std::thread worker;
    qttune_session_t* session = nullptr;
    uint32_t interval_ms = DEFAULT_FRAME_INTERVAL_MS;
    uint64_t frame_counter = 0;
};

namespace {

void mock_worker(qttune_mock_transport* t)
{
    const auto interval = std::chrono::milliseconds(t->interval_ms);

    while (!t->stop_requested.load()) {
        qttune_frame_t frame{};
        frame.timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
        frame.is_extended =
            static_cast<uint8_t>((t->frame_counter % 16) == 0);
        frame.data_length = 8;
        frame.data[0] = static_cast<uint8_t>(t->frame_counter & 0xFF);
        frame.data[1] =
            static_cast<uint8_t>((t->frame_counter >> 8) & 0xFF);
        for (size_t i = 2; i < 8; ++i) {
            frame.data[i] =
                static_cast<uint8_t>((t->frame_counter + i) & 0xFF);
        }
        t->frame_counter++;

        /* Internal dispatch: snapshot under lock, invoke unlocked,
         * and it checks session->closed before firing. */
        qttune_session_dispatch_callbacks(t->session, &frame);

        std::this_thread::sleep_for(interval);
    }
}

} // namespace

extern "C" {

qttune_status_t qttune_mock_transport_attach(qttune_session_t* session,
                                             const char* /*uri*/)
{
    /* uri params (?interval=...) are a later addition */
    if (session->transport != nullptr) {
        return QT_OK; /* idempotent */
    }
    try {
        session->transport = new qttune_mock_transport();
    } catch (...) {
        return QT_ERR_OUT_OF_MEMORY;
    }
    session->transport->session = session;
    /* NOTE: worker thread NOT started here — explicit start required.
     * Keeps registration-only sessions single-threaded for tests. */
    return QT_OK;
}

qttune_status_t qttune_mock_transport_start(qttune_session_t* session)
{
    qttune_mock_transport* t = session->transport;
    if (t == nullptr) {
        return QT_ERR_NO_TRANSPORT;
    }
    if (t->worker.joinable()) {
        return QT_OK; /* already running — idempotent */
    }
    try {
        t->worker = std::thread(mock_worker, t);
        return QT_OK;
    } catch (...) {
        return QT_ERR_OUT_OF_MEMORY;
    }
}

void qttune_mock_transport_detach(qttune_session_t* session)
{
    qttune_mock_transport* t = session->transport;
    if (t == nullptr) {
        return;
    }
    session->transport = nullptr;

    t->stop_requested.store(true);
    if (t->worker.joinable()) {
        t->worker.join();
    }
    delete t;
}

} // extern "C"
