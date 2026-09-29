#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Status codes: mirror J2534 convention of 0 = success ---- */
typedef int32_t qttune_status_t;

enum {
    QT_OK                  = 0,
    QT_ERR_NULL_ARGUMENT   = -1,
    QT_ERR_NOT_IMPLEMENTED = -2,
    QT_ERR_NO_TRANSPORT    = -3,
    QT_ERR_OUT_OF_MEMORY   = -4,
    QT_ERR_SESSION_CLOSED  = -5,
};

/* Opaque handle. Internals never leak past this header. */
typedef struct qttune_session qttune_session;

/* Frame: sized for CAN-FD (64 bytes) so we never truncate later. */
typedef struct qttune_frame {
    uint32_t protocol_id;     /* enum later: CAN, CANFD, ISO15765, ... */
    uint32_t arbitration_id;
    uint8_t  data[64];
    uint8_t  data_length;
    uint64_t timestamp_us;    /* monotonic, from core clock */
} qttune_frame;

/* Library lifecycle. qttune_init() must succeed before session creation. */
qttune_status_t qttune_init(void);
void            qttune_shutdown(void);

/* Human-readable status, never NULL. */
const char* qttune_status_string(qttune_status_t status);
const char* qttune_version_string(void);

/* Session lifecycle.
 * transport_uri examples (future):
 *   "j2534:<driver_dll>"
 *   "ble:<mac_address>"
 *   "wifi:<host:port>"
 */
qttune_status_t qttune_session_create(const char* transport_uri,
                                      qttune_session** out_session);
qttune_status_t qttune_session_close(qttune_session* session);

/* Frame I/O. Stubbed in v0.1 — will dispatch through the transport layer. */
qttune_status_t qttune_session_send(qttune_session* session,
                                    const qttune_frame* frame);
qttune_status_t qttune_session_receive(qttune_session* session,
                                       qttune_frame* out_frame,
                                       uint32_t timeout_ms);

#ifdef __cplusplus
} /* extern "C" */
#endif
