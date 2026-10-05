# QtTune — Project Status

**Date:** October 5, 2026
**Version:** v0.2.0 (complete, pending tag)
**Repository:** https://github.com/erickramer42/qttune
**CI:** [![CI](https://github.com/erickramer42/qttune/actions/workflows/ci.yml/badge.svg)](https://github.com/erickramer42/qttune/actions/workflows/ci.yml)

QtTune is a cross-platform ECU reader/logger, evolving toward a full tuning
application. The guiding architecture principle: **a Qt-free core library
behind a stable C ABI, with UI layers as replaceable skins around it.**

## Architecture

+----------------------------+
|   Qt Quick UI (desktop)    |   Responsive QML shell — sidebar above
|   Main / Dashboard / Log / |   720 px width, bottom TabBar below
|   Settings pages           |   LogPage: live frame table, sort toggle
+---------------+------------+
                | QtTuneBridge (thread-marshaling seam, COMPLETE)
                |   worker→GUI via QMetaObject::invokeMethod (QueuedConnection)
+---------------v------------+
|         qttune-core        |   C++, C++17, C ABI exports
|  * Session lifecycle       |   ZERO Qt dependencies (CI-enforced)
|  * Version/status reporting|
|  * Frame callbacks (done)  |
|  * Transports (mock done)  |
|  * Frame model (qtab)      |
+----------------------------+

Rules the project is built around:

1. **Core never links Qt.** Enforced in CI on a runner with no Qt installed.
2. **UI layers receive final display values, never raw frames.** All
   parsing, decoding, and unit conversion lives in core. (Applies to the
   future mobile client too: the C ABI supports a native or Flutter
   front end without changes.)
3. **The C ABI is the FFI boundary** — stable handles, flat functions,
   POD structs. Enum values evolve append-only; anything above the ABI
   is disposable.

## Data Path (landed v0.2.0)

    [MockTransport worker thread]         std::thread, 50ms tick
              │  qttune_frame_t (stack-owned)
              ▼
    core: snapshot-dispatch callbacks    (register/unregister safe mid-dispatch)
              │  QtTuneBridge::onFrame   — copies POD, checks backpressure,
              │                           posts QueuedConnection lambda
              ▼
    GUI thread: handleFrameInternal       — decode to display values
              │                           (timestamp/DLC/ext/payloadHex)
              ▼
    FrameListModel (canonical, arrival   ← FrameSortProxy (newest-first /
    order, 10k cap, front-eviction)         oldest-first, toggleable)
              │
              ▼
    QML LogPage ListView (virtualized)

Contract highlights:

- UI receives final display values, never raw frames (rule 2 enforced
  at handleFrameInternal).
- Backpressure: drop-at-source above 10k in-flight; surfaced honestly
  via the droppedFrames property.
- Signals emitted only on the GUI thread; worker touches atomics only.

## Callback API Design (landed v0.2.0)

Session-scoped, pair-identity registration — no opaque registration
handles (avoids the dangling-handle crash class):

- `qttune_register_frame_callback(session, callback, user_data)`
  — duplicate (callback, user_data) pairs are silently deduplicated
  (idempotent success); same function with different user_data is a
  distinct registration.
- `qttune_unregister_frame_callback(session, callback, user_data)`
  — idempotent; removing a nonexistent registration is success.
- `qttune_session_dispatch_callbacks(session, frame)` — **internal**
  (`qttune/internal.h`), transport-facing only. Uses the
  **snapshot-dispatch pattern**: copies the callback list under the
  mutex, releases it, then invokes. This permits callbacks to call
  register/unregister (self-unregistration teardown idiom) without
  deadlock.
- **Callback contract:** runs on the transport worker thread; must not
  block; must not call `qttune_session_close()`.
- **Close discipline:** `qttune_session_close()` flips the atomic
  `closed` flag before teardown; transports must observe it and stop
  dispatching (full join-the-worker-thread discipline lands with the
  mock transport).

## What Works (verified)

| Capability | Verified by |
|---|---|
| Core builds standalone with zero Qt deps | `core-and-tests` CI job (Ubuntu, Qt absent) |
| 17 core unit tests pass: status strings, version format, init/shutdown idempotency, session arg validation, callback register/unregister, duplicate dedup, distinct-userdata registration, dispatch invocation, null-pointer paths, mock transport delivery, unknown-uri rejection, worker-thread identity, self-unregister reentry | GTest suite; 17/17 locally on Windows/MSVC 2022, CI matrix to confirm |
| Mock transport delivers frames from background thread | Integration test (`MockTransportDeliversFrames`) verifies callbacks fire on worker thread, session_close joins worker without hang |
| Bridge marshals frames from worker thread to GUI thread | `FramesMarshalFromWorkerToGuiThread` asserts model mutation thread == GUI thread |
| End-to-end frame flow: mock → core → bridge → proxy → QML | 6 bridge tests + manual smoke (log streaming, sort toggle, clear, disconnect mid-stream) |
| Ordered frame log via sort proxy (newest/oldest-first toggle) | `SortProxyOrdersNewestFirstByDefault`, `SortProxyTogglesToOldestFirst` |
| Full app builds on Windows (MSVC 2022, Qt 6.12.0) | `full-build` CI job |
| Full app builds on macOS (AppleClang, Xcode 26 SDK, Qt 6.12.0) | `macos-build` CI job |
| Responsive QML layout (desktop sidebar ↔ mobile TabBar breakpoint) | Local run, Windows |
| Version string surfaces in UI (`QtTune — core 0.1.0 [ready]`) | Local run |
| Self-unregistering callbacks survive live dispatch (snapshot pattern) | `SelfUnregisterDuringLiveDispatch`: exactly 1 self-invocation, sibling survives |
| Callback thread identity via real worker path | `CallbackExecutesOnTransportWorkerThread` |
| Bridge test target builds/runs on Windows (DLLs auto-deployed) | `$<TARGET_RUNTIME_DLLS>` post-build copy; ctest + direct run |

## In Progress

(none — v0.2.0 complete)

Done in v0.2.0:

- [x] GitHub Actions CI: 3-job matrix, all green
- [x] GTest suite (FetchContent) — 17 core + 6 bridge tests
- [x] `QTTUNE_BUILD_CORE_ONLY` headless option
- [x] Qt 6.12.0 LTS across all platforms
- [x] Session-scoped callback API (pair identity, snapshot dispatch)
- [x] Mock transport (`mock://`), timer-driven worker, quiesce-on-close
- [x] QtTuneBridge thread marshaling (QueuedConnection, POD copy)
- [x] FrameListModel + FrameSortProxy (newest-first toggle)
- [x] LogPage live table, HomePage session control, header chrome
- [x] QQuickStyle Basic pinned (fixes native-style customization warnings)
- [x] DLL deployment for test targets via `$<TARGET_RUNTIME_DLLS>`

## Known Issues / Technical Debt

1. **aqtinstall pinned to git master on Windows CI.** aqt 3.3.0 mishandles
   the Qt 6.11+ repository layout; the Windows job installs aqt from
   `git+https://github.com/miurahr/aqtinstall` directly. Revert to a
   released version (or `install-qt-action`) once a fix ships — see
   aqtinstall issues #959 / #1007.
2. **Mobile builds unvalidated.** The QML/bridge/core are shared and the
   layout is responsive, but no Android/iOS kit build has been attempted.
3. **Linux full-app build not in CI.** Deliberate: the Ubuntu runner
   exists to prove the Qt-free core invariant. A Linux full-build job is
   a v0.3+ candidate when Linux becomes a ship target.
4. **Per-frame NOTIFY signals and dynamic proxy sort are mock-rate only
   (20 fps).** J2534 rates need coalesced GUI updates (~100ms batches).
   TODO(v0.3) marked in code.
5. **Ascending (oldest-first) mode has no auto-follow/jump-to-latest UX
   yet** — newest-first default needs none. v0.3 candidate.
6. **Session API is single-threaded by contract** (bridge is sole caller);
   a session-level mutex is future hardening if that changes.
7. **Per-case ctest granularity pending.** Suite registers as one ctest
   entry; `gtest_discover_tests()` (or CI `--gtest_list_tests` check) is
   a small hardening task.

## Roadmap

| Phase | Goals | Status |
|-------|-------|--------|
| v0.1.0 | Skeleton: core ABI, Qt shell, responsive layout | ✅ Released |
| v0.2.0 | Callback API, mock transport, CI + tests | ✅ Released |
| v0.3.0 | J2534 transport, live vehicle read/logging | Planned |
| v0.4.0 | Security access discovery, flash capability | Planned |
| v0.5.0+ | Multi-manufacturer plugin modules | Planned |

## Recent Engineering Notes (cross-platform CI saga, condensed)

example of full build and testing workflow:
rm -rf build
cmake -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
cmake --build build --config Release
./build/tests/Release/test_core.exe
./build/tests/Release/test_bridge.exe
   (alternate test method command: ctest --test-dir build -C Release --output-on-failure)
./build/app/Release/qttune.exe (for full app end to end and manual testing)


For the record and for anyone hitting the same walls: the macOS job
initially failed because Apple removed the AGL framework from the Xcode 26
SDK while Qt ≤6.8 still referenced it transitively. Upgrading to Qt 6.12.0
resolved it cleanly (no weak-link flag, current runner). Windows then hit
an aqtinstall bug constructing doubled `qt6_xxxx/qt6_xxxx/` paths for the
Qt 6.11+ repository layout; worked around by invoking aqt from master
directly instead of via `install-qt-action` (whose `aqtversion` input
cannot express a git URL). Both fixes are temporary scaffolding to be
reverted when upstreams ship tagged releases.

**2026-10-04 — callback API round:** first design iteration was reviewed
and corrected before merging: dropped a never-implemented registration
handle from the signature (pair-based identity instead), made duplicate
registration idempotent-success rather than an error enum, rewrote tests
to assert actual behavior rather than a wished-for path, and mandated the
snapshot-dispatch pattern so callback reentry can't deadlock. MSVC also
caught an implicit int→enum conversion the clang-tuned test relied on
(C2664) — reminder that the three-compiler matrix earns its keep.

**2026-10-05 — bridge + log round:** three test-design lessons worth
recording. (1) Captureless lambdas are unsafe as pair-identity C
callbacks when self-reference is needed — the reentry test originally
unregistered a *different* lambda than the one registered; named free
functions are the only self-referential form. (2) Thread-identity
assertions must exercise the real execution path (session_start),
not manual dispatch calls that trivially run on the test thread.
(3) Assertions must encode contracts, not timing coincidences — the
sort-toggle test initially demanded a strict inequality that only held
when a new frame happened to arrive between two reads. Also: Q_PROPERTY
NOTIFY is a promise to the binding engine; incrementing a member
without emitting leaves QML serving stale values indefinitely (the
frozen frameCount binding). Windows DLL deployment for test targets
resolved via `$<TARGET_RUNTIME_DLLS>` (CMake 3.21+) rather than PATH
injection — PATH-based set_tests_properties approaches break on
semicolons in the environment value.

---

*(This document describes development state only. Safety, regulatory,
and warranty caveats are in [DISCLAIMER.md](DISCLAIMER.md).)*
