# QtTune — Project Status

**Date:** October 4, 2026
**Version:** v0.1.0 (v0.2.0 in progress)
**Repository:** https://github.com/erickramer42/qttune
**CI:** [![CI](https://github.com/erickramer42/qttune/actions/workflows/ci.yml/badge.svg)](https://github.com/erickramer42/qttune/actions/workflows/ci.yml)

QtTune is a cross-platform ECU reader/logger, evolving toward a full tuning
application. The guiding architecture principle: **a Qt-free core library
behind a stable C ABI, with UI layers as replaceable skins around it.**

## Architecture

+----------------------------+
|   Qt Quick UI (desktop)    |   Responsive QML shell — sidebar above
|   Main / Dashboard / Log / |   720 px width, bottom TabBar below
|   Settings pages           |
+---------------+------------+
                | QtTuneBridge (thread-marshaling seam, partial)
+---------------v------------+
|         qttune-core        |   C++, C++17, C ABI exports
|  * Session lifecycle       |   ZERO Qt dependencies (CI-enforced)
|  * Version/status reporting|
|  * Frame callbacks NEW     |
|  * Transports (in progress)|
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
| 15 unit tests pass: status strings, version format, init/shutdown idempotency, session arg validation, callback register/unregister, duplicate dedup, distinct-userdata registration, dispatch invocation, null-pointer paths, mock transport delivery, unknown-uri rejection | GTest suite; locally verified 15/15 on Windows/MSVC 2022 |
| Mock transport delivers frames from background thread | Integration test (`MockTransportDeliversFrames`) verifies callbacks fire on worker thread, session_close joins worker without hang |
| Session-scoped frame callback API with snapshot dispatch | Unit tests (dispatch invocation + dedup verified at dispatch time); real second-thread exercise arrives with mock transport |
| Full app builds on Windows (MSVC 2022, Qt 6.12.0) | `full-build` CI job |
| Full app builds on macOS (AppleClang, Xcode 26 SDK, Qt 6.12.0) | `macos-build` CI job |
| Responsive QML layout (desktop sidebar ↔ mobile TabBar breakpoint) | Local run, Windows |
| Version string surfaces in UI (`QtTune — core 0.1.0 [ready]`) | Local run |

## In Progress (v0.2.0)

- [x] **Mock transport** — `mock://` URI scheme, timer-driven synthetic
      frame generation on a `std::thread` worker in core; dispatches via
      the snapshot path; close must stop-and-join before session
      destruction (quiesce discipline)
- [x] **Rx callback API** — landed: pair-based register/unregister,
      silent dedup, snapshot dispatch, thread contract documented
- [ ] **Bridge thread marshaling** — `QMetaObject::invokeMethod` in
      QtTuneBridge to move frames from core thread to UI thread
- [ ] **LogPage implementation** — virtualized TableView bound to the
      bridge model

Done in v0.2.0 so far:

- [x] GitHub Actions CI: 3-job matrix (see above), all green
- [x] GTest unit test suite wired into CMake via FetchContent
- [x] `QTTUNE_BUILD_CORE_ONLY` CMake option enabling headless builds
- [x] Qt upgraded to 6.12.0 LTS across all platforms
- [x] Session-scoped callback API + internal dispatch hook +
      11 new unit tests (including mock transport integration)

## Known Issues / Technical Debt

1. **aqtinstall pinned to git master on Windows CI.** aqt 3.3.0 mishandles
   the Qt 6.11+ repository layout; the Windows job installs aqt from
   `git+https://github.com/miurahr/aqtinstall` directly. Revert to a
   released version (or `install-qt-action`) once a fix ships — see
   aqtinstall issues #959 / #1007.
2. **Mobile builds unvalidated.** The QML/bridge/core are shared and the
   layout is responsive, but no Android/iOS kit build has been attempted.
   README says so explicitly.
3. **Bridge is a seam, not yet a pump.** Construction/version wiring
   works; frame marshaling does not exist yet.
4. **Linux full-app build not in CI.** Deliberate: the Ubuntu runner
   exists to prove the Qt-free core invariant. A Linux full-build job is
   a v0.3+ candidate when Linux becomes a ship target.
5. **No vehicle communication exists.** This is still infrastructure;
   the callback API fires from the test harness, not from a live
   transport yet. See DISCLAIMER.md before connecting anything.
6. **Per-case ctest granularity pending.** Suite registers as one ctest
   entry; `gtest_discover_tests()` (or CI `--gtest_list_tests` check) is
   a small hardening task.

## Roadmap

| Phase | Goals | Status |
|-------|-------|--------|
| v0.1.0 | Skeleton: core ABI, Qt shell, responsive layout | ✅ Released |
| v0.2.0 | Callback API, mock transport, CI + tests | 🚧 In progress |
| v0.3.0 | J2534 transport, live vehicle read/logging | Planned |
| v0.4.0 | Security access discovery, flash capability | Planned |
| v0.5.0+ | Multi-manufacturer plugin modules | Planned |

## Recent Engineering Notes (cross-platform CI saga, condensed)

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

---

*(This document describes development state only. Safety, regulatory,
and warranty caveats are in [DISCLAIMER.md](DISCLAIMER.md).)*
