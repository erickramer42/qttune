// File/layer: core/src/core.cpp | C++17, Qt-free, CI-validated headless build
// Testing: Unit test required for registration/unregister/idempotence

#include "qttune/version.h"
#include "qttune/core.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>
#include <memory>
#include <utility>


struct qttune_session {
    std::atomic<bool> closed{false};
    
    // Callback registry - protected by mutex during mutation
    std::mutex callback_mutex;
    std::vector<std::pair<qttune_frame_callback_t, void*>> callbacks;
    
    // For v0.3+: transport handle will live here
};


namespace {
    std::atomic<bool> g_initialized{false};
} // namespace


extern "C" {
#include "qttune/core.h"
#include "qttune/internal.h"

qttune_status_t qttune_init(void)
{
    bool expected = false;
    g_initialized.compare_exchange_strong(expected, true);
    return QT_OK;  // Idempotent by design
}


void qttune_shutdown(void)
{
    g_initialized.store(false);
}


const char* qttune_version_string(void)
{
    return QTTUNE_VERSION_STRING;
}


const char* qttune_status_string(qttune_status_t status)
{
    switch (status) {
        case QT_OK:                  return "success";
        case QT_ERR_NULL_ARGUMENT:   return "null argument";
        case QT_ERR_NOT_IMPLEMENTED: return "not implemented yet";
        case QT_ERR_NO_TRANSPORT:    return "no transport available";
        case QT_ERR_OUT_OF_MEMORY:   return "out of memory";
        case QT_ERR_SESSION_CLOSED:  return "session already closed";
        case QT_ERR_ALREADY_REGISTERED: return "already registered";
        default:                     return "unknown status";
    }
}


qttune_status_t qttune_session_create(
    const char* transport_uri,
    qttune_session_t** out_session)
{
    if (out_session == nullptr) {
        return QT_ERR_NULL_ARGUMENT;
    }
    (void)transport_uri;  // Phase 2: parse URI

    try {
        *out_session = new qttune_session();
        return QT_OK;
    } catch (...) {
        return QT_ERR_OUT_OF_MEMORY;
    }
}


qttune_status_t qttune_session_close(qttune_session_t* session)
{
    if (session == nullptr) {
        return QT_ERR_NULL_ARGUMENT;
    }

    /* 
     * THREADING GUARANTEE: Mark closed BEFORE destroying callbacks.
     * Transports checking closed==true will stop dispatching.
     * Caller MUST ensure transport thread has quiesced before this returns.
     */
    bool expected = false;
    if (!session->closed.compare_exchange_strong(expected, true)) {
        return QT_ERR_SESSION_CLOSED;
    }

    /* Clear callback registry */
    {
        std::lock_guard<std::mutex> lock(session->callback_mutex);
        session->callbacks.clear();
    }

    delete session;
    return QT_OK;
}


qttune_status_t qttune_session_send(
    qttune_session_t* session,
    const qttune_frame_t* frame)
{
    if (session == nullptr || frame == nullptr) {
        return QT_ERR_NULL_ARGUMENT;
    }
    if (frame->data_length > sizeof(frame->data)) {
        return QT_ERR_NULL_ARGUMENT;
    }
    return QT_ERR_NOT_IMPLEMENTED;
}


qttune_status_t qttune_session_receive(
    qttune_session_t* session,
    qttune_frame_t* out_frame,
    uint32_t timeout_ms)
{
    (void)session;
    (void)out_frame;
    (void)timeout_ms;
    return QT_ERR_NOT_IMPLEMENTED;
}


qttune_status_t qttune_register_frame_callback(
    qttune_session_t* session,
    qttune_frame_callback_t callback,
    void* user_data)
{
    if (session == nullptr || callback == nullptr) {
        return QT_ERR_NULL_ARGUMENT;
    }
    if (session->closed.load()) {
        return QT_ERR_SESSION_CLOSED;
    }

    std::lock_guard<std::mutex> lock(session->callback_mutex);

    /* Silent dedup: check if identical pair exists */
    for (const auto& entry : session->callbacks) {
        if (entry.first == callback && entry.second == user_data) {
            return QT_OK;  /* Already registered - idempotent */
        }
    }

    try {
        session->callbacks.emplace_back(callback, user_data);
        return QT_OK;
    } catch (...) {
        return QT_ERR_OUT_OF_MEMORY;
    }
}


qttune_status_t qttune_unregister_frame_callback(
    qttune_session_t* session,
    qttune_frame_callback_t callback,
    void* user_data)
{
    if (session == nullptr) {
        return QT_ERR_NULL_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(session->callback_mutex);

    /* Find and remove by identity */
    auto it = session->callbacks.begin();
    while (it != session->callbacks.end()) {
        if (it->first == callback && it->second == user_data) {
            it = session->callbacks.erase(it);
            break;  /* Remove only first match */
        } else {
            ++it;
        }
    }

    /* Idempotent: not found is still success */
    return QT_OK;
}


/*
 * INTERNAL DISPATCH FUNCTION - for mock transport to invoke callbacks
 *
 * THREADING: Must be called from transport worker thread.
 * Creates a snapshot copy to avoid holding lock during invocation.
 */
void qttune_session_dispatch_callbacks(
    qttune_session_t* session,
    const qttune_frame_t* frame)
{
    if (session == nullptr || frame == nullptr) {
        return;
    }
    if (session->closed.load()) {
        return;  /* Don't dispatch to closed session */
    }

    /* SNAPSHOT PATTERN: copy list, unlock, then invoke */
    std::vector<std::pair<qttune_frame_callback_t, void*>> snapshot;
    {
        std::lock_guard<std::mutex> lock(session->callback_mutex);
        snapshot = session->callbacks;  /* Copy iterators */
    }

    /* Invoke outside lock - callbacks must not reenter session APIs */
    for (const auto& [callback, user_data] : snapshot) {
        callback(frame, user_data);
    }
}


} // extern "C"
