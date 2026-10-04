#ifndef QTTUNE_INTERNAL_H
#define QTTUNE_INTERNAL_H

/*
 * Internal, non-ABI-stable interfaces for transport implementations.
 * Consumers of qttune-core must NOT include this header. Signatures
 * here may change without notice between versions.
 */

#include "qttune/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dispatch a received frame to all registered callbacks.
 *
 * Called by transport implementations from their worker thread.
 * Takes a snapshot of the callback list, then invokes callbacks
 * with NO lock held. Callbacks therefore may safely reenter
 * register/unregister.
 *
 * Preconditions:
 *   - session != NULL, frame != NULL (checked, no-op on violation)
 *   - frame must remain valid for the duration of the call
 *
 * Postcondition: all callbacks registered at snapshot time
 * have been invoked, unless session was observed closed.
 */
void qttune_session_dispatch_callbacks(qttune_session_t* session,
                                       const qttune_frame_t* frame);

#ifdef __cplusplus
}
#endif

#endif /* QTTUNE_INTERNAL_H */
