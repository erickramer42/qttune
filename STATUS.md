# QtTune — Project Status

**Date:** October 7, 2026
**Version:** v0.2.0 (released); v0.3.0 Phases 1–1b (signals layer + mock emission) complete
**Repository:** https://github.com/erickramer42/qttune
**CI:** [![CI](https://github.com/erickramer42/qttune/actions/workflows/ci.yml/badge.svg)](https://github.com/erickramer42/qttune/actions/workflows/ci.yml)

QtTune is a cross-platform ECU reader/logger, evolving into a full tuning
application. The guiding architecture principle: **a Qt-free core library
behind a stable C ABI, with UI layers as replaceable skins around it.**

## Architecture

+----------------------------+
|   Qt Quick UI (desktop)    |   Responsive QML shell — sidebar above
|   Main / Dashboard / Log / |   720 px width, bottom TabBar below
|   Settings pages           |   LogPage: live frame table, sort toggle
+---------------+------------+
                | QtTuneBridge (thread-marshaling seam, COMPLETE for
                |   frame flow; signal decode consumption = v0.3 Phase 2)
                |   worker→GUI via QMetaObject::invokeMethod (QueuedConnection)
+---------------v------------+
|         qttune-core        |   C++, C++17, C ABI exports
|  * Session lifecycle       |   ZERO Qt dependencies (CI-enforced)
|  * Version/status reporting|
|  * Frame callbacks (done)  |
|  * Transports (mock done)  |
|  * Signals: decode + mock  |   signal_sets.cpp, SimState emission
|  *   set + emission (done) |
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
    GUI thread: handleFrameInternal      — decode to display values
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

## Signals Layer (landed v0.3.0 Phases 1 / 1b)

POD signal metadata + linear decode: `display = raw * scale + offset`.

- `QttuneSignalDef` — POD struct: byte/bit position, type enum (UINT8..INT32,
  FLOAT32), scale/offset, unit string, display hints, reserved fields for
  ABI-stable growth.
- `qttune_decode_signal(def, payload, len, out)` — Intel (LSB-first) bit
  extraction, sign extension for INT types, IEEE-754 passthrough for
  FLOAT32. Standardized error codes (NULL=1, bounds=2, bit params=3,
  unsupported type=4).
- `qttune_validate_signal(def)` — structural validation: num_bytes span,
  bit_offset/bit_length consistency, null-termination of fixed-width
  strings (last byte must be '\0').
- `QttuneSignalSet` — swappable definition collections; the set's origin
  is opaque to the decoder. Mock ships a built-in set
  (`qttune_mock_signal_set`); future providers (DBC loader, manufacturer
  plugins) construct sets through the same struct — decode ABI is
  definition-source-agnostic by design.
- **Motorola (MSB-first) bit numbering is a deliberate non-goal** until a
  flags bit selects it. First customer of a reserved flags bit is byte
  order (DBC mixes Motorola and Intel freely) — documented contract,
  not an omission.
- **Mock emission (Phase 1b):** transport payloads come from a
  tick-driven simulation — idle RPM wobble with periodic rev blips,
  coolant warming from ambient toward thermostat, trailing IAT, coupled
  throttle/load — encoded as the inverse of the signal definitions. The
  transport never decodes (rule 2); it guarantees only "emitted payloads
  are decodable by the mock set," enforced by integration test.
- Live consumer is Phase 2 (coalesced LiveView); decoder currently
  exercised by unit + integration tests.

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
  `closed` flag before teardown; transports observe it and stop
  dispatching; detach joins the worker.

## What Works (verified)

| Capability | Verified by |
|---|---|
| Core builds standalone with zero Qt deps | `core-and-tests` CI job (Ubuntu, Qt absent) |
| 37 tests pass: 18 core (lifecycle, callbacks, mock transport, worker-thread identity, self-unregister reentry, signal-set decode round-trip) + 13 signals (decode paths, sign extension, scale/offset, error codes, validator, mock set validation/unique IDs, set plausibility) + 6 bridge | GTest suites (`test_core`, `test_signals`, `test_bridge`); 37/37 locally on Windows/MSVC 2022, green on 3-platform CI matrix |
| Mock transport delivers physically plausible, signal-set-decodable frames | `MockFramesDecodeAgainstMockSignalSet`: captures real worker-thread frames, decodes each under every mock definition, asserts physical ranges |
| Signal decode layer: scale/offset, LSB-first bitfields, sign extension, bounds checking | `test_signals` suite (13 tests) |
| Encoder/decoder layout contract holds | Round-trip integration test catches byte-order/scale inversions invisible to structural validation |
| Mock transport delivers frames from background thread | `MockTransportDeliversFrames` verifies callbacks fire on worker thread, session_close joins worker without hang |
| Per-case ctest granularity for all suites | `gtest_discover_tests()` on all three test targets |
| Bridge marshals frames from worker thread to GUI thread | `FramesMarshalFromWorkerToGuiThread` asserts model mutation thread == GUI thread |
| End-to-end frame flow: mock → core → bridge → proxy → QML | 6 bridge tests + manual smoke (log streaming, sort toggle, clear, disconnect mid-stream) |
| Ordered frame log via sort proxy (newest/oldest-first toggle) | `SortProxyOrdersNewestFirstByDefault`, `SortProxyTogglesToOldestFirst` |
| Full app builds on Windows (MSVC 2022, Qt 6.12.0) | `full-build` CI job |
| Full app builds on macOS (AppleClang, Xcode 26 SDK, Qt 6.12.0) | `macos-build` CI job |
| Responsive QML layout (desktop sidebar ↔ mobile TabBar breakpoint) | Local run, Windows |
| Version string surfaces in UI (`QtTune — core 0.2.0 [ready]`) | Local run; single source of truth via root CMake `project()` → generated `version.h` |
| Self-unregistering callbacks survive live dispatch (snapshot pattern) | `SelfUnregisterDuringLiveDispatch`: exactly 1 self-invocation, sibling survives |
| Callback thread identity via real worker path | `CallbackExecutesOnTransportWorkerThread` |
| Bridge test target builds/runs on Windows (DLLs auto-deployed) | `$<TARGET_RUNTIME_DLLS>` post-build copy; ctest + direct run |

## In Progress

v0.3.0 phases:

- [x] **Phase 1 — Signals layer:** POD signal metadata ABI, scale/offset
      decode, LSB-first bit extraction, sign extension, validator
- [x] **Phase 1b — Mock signal emission:** tick-driven simulation encodes
      payloads per the mock signal set; integration test proves the
      decoder round-trips real worker-thread frames
- [ ] **Phase 2 — Coalesced LiveView:** 100ms batched GUI updates,
      LiveView page with subscription checkboxes, 2D strip chart
- [ ] **Phase 3 — J2534 hybrid loader:** registry discovery (dual WOW64
      views), in-process x64 load, surrogate host for x86-only DLLs
- [ ] **Phase 4 — Record-replay transport:** fixture capture + deterministic
      playback through the same frame API

## Known Issues / Technical Debt

1. **aqtinstall pinned to git master on Windows CI.** aqt 3.3.0 mishandles
   the Qt 6.11+ repository layout; the Windows job installs aqt from
   `git+https://github.com/miurahr/aqtinstall` directly. A dev-master
   snapshot hit the documented sporadic py7zr `Bad7zFile` extraction bug
   once (install-qt-action#344 / aqtinstall#995); a clean re-run passed,
   confirming sporadic-tool behavior. If flakiness recurs: pin a
   known-good post-#1000 commit with `py7zr==1.1.0` alongside, and
   revert to a PyPI release once one ships containing the fix.
2. **Mobile builds unvalidated.** The QML/bridge/core are shared and the
   layout is responsive, but no Android/iOS kit build has been attempted.
3. **Linux full-app build not in CI.** Deliberate: the Ubuntu runner
   exists to prove the Qt-free core invariant. A Linux full-build job is
   a v0.3+ candidate when Linux becomes a ship target.
4. **Per-frame NOTIFY signals and dynamic proxy sort are mock-rate only
   (20 fps).** J2534 rates need coalesced GUI updates (~100ms batches).
   TODO(v0.3) marked in code — Phase 2 of v0.3.0.
5. **Ascending (oldest-first) mode has no auto-follow/jump-to-latest UX
   yet** — newest-first default needs none. v0.3 candidate.
6. **Session API is single-threaded by contract** (bridge is sole caller);
   a session-level mutex is future hardening if that changes.
7. **Signals layer has no UI consumer yet.** Decode is proven end-to-end
   to the core boundary (integration test on real worker-thread frames);
   bridge consumption + LiveView is Phase 2. UI appearance is unchanged
   so far — Phase 1b was core-side only.

## Roadmap

| Phase | Goals | Status |
|-------|-------|--------|
| v0.1.0 | Skeleton: core ABI, Qt shell, responsive layout | ✅ Released |
| v0.2.0 | Callback API, mock transport, CI + tests | ✅ Released |
| v0.3.0 | Signals layer, coalesced LiveView, J2534 transport, live vehicle read/logging | 🚧 Phases 1–1b done |
| v0.4.0 | Security access discovery, flash capability | Planned |
| v0.5.0+ | Multi-manufacturer plugin modules | Planned |

## Recent Engineering Notes (cross-platform CI saga, condensed)

example of full build and testing workflow:
rm -rf build
cmake -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
cmake --build build --config Release
./build/tests/Release/test_core.exe
./build/tests/Release/test_signals.exe
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

**2026-10-06 — signals round (v0.3.0 Phase 1):** two failure-mode
lessons surfaced by the first red run. (1) **Error-code precedence is a
contract** — type validation must precede bit-parameter validation,
otherwise an unsupported type with degenerate bit fields returns the
wrong error code (3 instead of 4). Order your checks in the order your
error enum promises. (2) **Zero-initialized PODs mask string-termination
test bugs** — a test that `memset`s 31 of 32 name bytes leaves the last
byte '\0' from aggregate initialization, so the "unterminated name"
case is silently well-formed. When testing failure modes on fixed-width
buffers, corrupt the *entire* buffer, not most of it. Also migrated all
three suites from single-entry `add_test()` to `gtest_discover_tests()`,
closing the per-case-granularity debt and giving free
silent-empty-suite detection; the legacy PATH-environment properties
were dropped alongside (build-tree RPATH covered non-Windows cases, the
DLL copy step covers Windows).

**2026-10-07 — signals emission round (v0.3.0 Phase 1b):** the C
standard bites again — designated initializers are C++20, not C++17,
and MSVC accepts them permissively while GCC/Clang on the Ubuntu CI job
will reject them; both struct-array definitions were rebuilt through
plain constructor helpers. Lesson reinforced twice over this round:
*code that compiles on your machine is not code that compiles* — the
three-compiler matrix is the only authority. Design note worth keeping:
the mock transport encodes simulation values as the **inverse** of the
signal definitions but never decodes them; the encoder/decoder contract
is enforced by a round-trip integration test because byte-order and
scale inversions pass all structural checks while producing physically
absurd values — only range assertions catch that failure class.

---

*(This document describes development state only. Safety, regulatory,
and warranty caveats are in [DISCLAIMER.md](DISCLAIMER.md).)*
