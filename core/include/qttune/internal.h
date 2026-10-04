#ifndef QTTUNE_INTERNAL_H
#define QTTUNE_INTERNAL_H

#include "qttune/core.h"

#ifdef __cplusplus
extern "C" {
#endif

void qttune_session_dispatch_callbacks(qttune_session_t* session,
                                       const qttune_frame_t* frame);

qttune_status_t qttune_mock_transport_attach(qttune_session_t* session,
                                            const char* uri);
void qttune_mock_transport_detach(qttune_session_t* session);
qttune_status_t qttune_mock_transport_start(qttune_session_t* session);

#ifdef __cplusplus
}
#endif

#endif /* QTTUNE_INTERNAL_H */
