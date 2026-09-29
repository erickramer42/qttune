#include "qttune/version.h"
#include "qttune/core.h"

#include <atomic>
#include <cstring>


// Defines the struct *forward-declared* in core.h. Must be at global
// scope with the exact same name — anonymous namespace would create
// a distinct type and trigger C2872 ambiguity.
struct qttune_session {
    std::atomic<bool> closed{false};
    // transport pointer lands here in phase 2
};

namespace {

std::atomic<bool> g_initialized{false};

} // namespace

extern "C" {

qttune_status_t qttune_init(void)
{
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        return QT_OK; // idempotent
    }
    return QT_OK;
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
        default:                      return "unknown status";
    }
}

qttune_status_t qttune_session_create(const char* /*transport_uri*/,
                                      qttune_session** out_session)
{
    if (out_session == nullptr) {
        return QT_ERR_NULL_ARGUMENT;
    }
    *out_session = nullptr;

    // Phase 2: parse URI, instantiate transport, return real handle.
    return QT_ERR_NOT_IMPLEMENTED;
}

qttune_status_t qttune_session_close(qttune_session* session)
{
    if (session == nullptr) {
        return QT_ERR_NULL_ARGUMENT;
    }
    bool expected = false;
    if (!session->closed.compare_exchange_strong(expected, true)) {
        return QT_ERR_SESSION_CLOSED;
    }
    delete session;
    return QT_OK;
}

qttune_status_t qttune_session_send(qttune_session* session,
                                    const qttune_frame* frame)
{
    if (session == nullptr || frame == nullptr) {
        return QT_ERR_NULL_ARGUMENT;
    }
    if (frame->data_length > sizeof(frame->data)) {
        return QT_ERR_NULL_ARGUMENT;
    }
    return QT_ERR_NOT_IMPLEMENTED;
}

qttune_status_t qttune_session_receive(qttune_session* /*session*/,
                                       qttune_frame* /*out_frame*/,
                                       uint32_t /*timeout_ms*/)
{
    return QT_ERR_NOT_IMPLEMENTED;
}

} // extern "C"
