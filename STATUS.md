# QtTune — Project Status

**Date:** October 7, 2026
**Version:** v0.2.0 (released, tagged); v0.3.0 Phases 1–1b complete, Phase 2 substantially complete (strip chart remaining)
**Repository:** https://github.com/erickramer42/qttune
**CI:** [![CI](https://github.com/erickramer42/qttune/actions/workflows/ci.yml/badge.svg)](https://github.com/erickramer42/qttune/actions/workflows/ci.yml)

QtTune is a cross-platform ECU reader/logger, evolving into a full tuning
application. The guiding architecture principle: **a Qt-free core library
behind a stable C ABI, with UI layers as replaceable skins around it.**

## Architecture

+----------------------------+
|   Qt Quick UI (desktop)    |   Responsive QML shell — sidebar above
|   Main / Dashboard / Live /|   720 px width, bottom TabBar below
|   Log / Settings pages     |   LogPage: live frame table, sort toggle
|   LiveViewPage: signal     |   subscription checkboxes + live values
|   table (strip chart       |   (chart = next increment)
|   pending)                 |
+---------------+------------+
                | QtTuneBridge (thread-marshaling seam)
                |   worker→GUI via QMetaObject::invokeMethod (QueuedConnection)
                |   + 100ms coalesce timer draining decode staging buffer
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

## Data Path

    [MockTransport worker thread]         std::thread, 50ms tick
              │  qttune_frame_t (stack-owned)
              ▼
    core: snapshot-dispatch callbacks    (register/unregister safe mid-dispatch)
              │  QtTuneBridge::onFrame   — copies POD, checks in-flight gate,
              │                           posts QueuedConnection lambda
              ▼
    GUI thread: handleFrameInternal      — hex display values to FrameListModel
              │                           + qttune_decode_signal per frame
              │                             into staging buffer
              ▼
    100ms coalesce timer ──drain──▶ SignalListModel.updateValues()
              │                        one dataChanged per window
              │                        (contiguous rowrange, ValueRole)
              ▼
    QML LiveViewPage: value table (strip chart pending)

Frame-log path (unchanged from v0.2.0):

    handleFrameInternal → FrameListModel (arrival order, 10k ring,
    back-eviction) → FrameSortProxy (newest/oldest toggle) → LogPage

Contract highlights:

- UI receives final display values, never raw frames (rule 2).
- Backpressure bounds IN-FLIGHT queue depth (posted-but-undrained
  lambdas, cap 1000), NOT cumulative frame count. Model ring eviction
  handles memory. Stream never freezes.
- GUI emissions for the LiveView path: one batch per 100ms window
  (10 Hz), decoupled from frame rate (mock 20 fps, future J2534 500 Hz).

## Signals Layer (landed v0.3.0 Phases 1 / 1b)

POD signal metadata + linear decode: `display = raw * scale + offset`.

- `QttuneSignalDef` — POD struct: byte/bit position, type enum (UINT8..INT32,
  FLOAT32), scale/offset, unit string, display hints, reserved fields for
  ABI-stable growth.
- `qttune_decode_signal(def, payload, len, out)` — Intel (LSB-first) bit
  extraction, sign extension for INT types, IEEE-754 passthrough for
  FLOAT32. Standardized error codes (NULL=1, bounds=2, bit params=3,
  unsupported type=4).
- `qttune_validate_signal(def)` — structural validation; fixed-width strings
  must terminate within buffer.
- `QttuneSignalSet` — swappable definition collections; set origin is
  opaque to the decoder. Mock ships a built-in set
  (`qttune_mock_signal_set`); future providers (DBC loader, manufacturer
  plugins) construct sets through the same struct.
- **Motorola (MSB-first) bit numbering is a deliberate non-goal** until a
  flags bit selects it — first customer of a reserved flags bit is byte
  order (DBC mixes Motorola and Intel freely).
- **Mock emission (Phase 1b):** tick-driven simulation (idle RPM wobble +
  rev blips, coolant warm-up toward thermostat, trailing IAT, coupled
  throttle/load), encoded as the inverse of the signal definitions.
  Transport never decodes (rule 2); round-trip enforced by integration
  test because byte-order/scale inversions pass structural checks but
  produce absurd physics — only range assertions catch that class.

## Coalesced Signal Path (landed v0.3.0 Phase 2)

- `handleFrameInternal` decodes the mock signal set per frame into
  `m_stagingBuffer` (GUI thread; decode ABI is pure computation).
- 100ms `QTimer` (`m_coalesceTimer`) drains staging into
  `SignalListModel::updateValues()` — one contiguous-rowrange
  `dataChanged` for the whole batch, started on connect, stopped on
  disconnect.
- `SignalListModel` — QAbstractListModel over the signal set: name, unit,
  value, subscribed roles; `toggleSubscribed(row)` Q_INVOKABLE drives the
  LiveView checkboxes. Value updates filter on `subscribed`.
- Bridge metrics: `guiNotifyCount` (per-frame instrumentation, baseline
  1:1 with frames) and `signalBatchCount` (~10/s at any frame rate).
  Baseline measured before coalescing: 200 notify emissions / 10 s at
  mock rate → LiveView path now 10/s. **~20× reduction, decoupled from
  frame rate.**
- LiveViewPage QML: subscription checkboxes + tabular-digit value labels,
  wired into sidebar/TabBar/StackLayout at index 2 (Home, Dash, **Live**,
  Log, Setup).
- Strip chart NOT yet built — needs per-signal rolling history ring
  (timestamp+value) in SignalListModel + QML Canvas redraw on
  `valuesUpdated`. This is the remaining Phase 2 increment.
- Known inefficiency (deliberate, mock-rate harmless): decode loop runs
  all signals per frame regardless of subscription; filter decode on
  `isSubscribed(id)` when signal count grows.

## Backpressure (corrected 2026-10-07)

Original v0.2.0 design gated delivery on `m_currentFrameCount >= 10k`
(cumulative lifetime count) — once 10k frames had EVER been processed,
all delivery stopped permanently and the UI froze while the counter
climbed. The model's ring eviction never engaged because nothing reached
it. Corrected: the gate now bounds **in-flight** posted-but-undrained
queued lambdas (`MaxInFlightFrames = 1000`, decremented in the lambda
before processing). At mock rate in-flight sits at 0–2; drop counter
climbs only under genuine backlog (GUI thread stalled under burst).
Regression sentinel: `StreamingContinuesPastTenThousandFrames`, using
test-only seam `forceFrameCountForTesting(int)` (Q_INVOKABLE, never
called by app code). Lesson: a drop statistic must be paired with a
liveness test — cumulative-vs-in-flight conflation is invisible until
someone lets the app run 8+ minutes.

## Callback API Design (landed v0.2.0)

Session-scoped, pair-identity registration — no opaque registration
handles:

- `qttune_register_frame_callback(session, callback, user_data)`
  — duplicate pairs silently deduplicated (idempotent success).
- `qttune_unregister_frame_callback(session, callback, user_data)`
  — idempotent.
- `qttune_session_dispatch_callbacks(session, frame)` — **internal**,
  snapshot-dispatch pattern (copy list under mutex, invoke unlocked) —
  callbacks may register/unregister mid-dispatch without deadlock.
- **Callback contract:** worker thread, must not block, must not call
  `qttune_session_close()`.
- **Close discipline:** atomic closed flag flipped before teardown;
  detach joins the worker.

## What Works (verified)

| Capability | Verified by |
|---|---|
| Core builds standalone with zero Qt deps | `core-and-tests` CI job (Ubuntu, Qt absent) |
| 40 tests pass: 18 core + 13 signals + 9 bridge | GTest suites; 40/40 locally on Windows/MSVC 2022, green on 3-platform CI matrix |
| Mock transport delivers physically plausible, signal-set-decodable frames | `MockFramesDecodeAgainstMockSignalSet`: real worker frames decoded under every mock definition, physical ranges asserted |
| Encoder/decoder layout contract | Round-trip integration test catches byte-order/scale inversions invisible to structural validation |
| Bridge marshal: worker → GUI thread | `FramesMarshalFromWorkerToGuiThread` |
| Coalesced signal updates at 10 Hz regardless of frame rate | `SignalBatchesAreCoalesced` (bounded ranges, not exact counts); manual: header `batch:` climbs ~10/s while `frames:` climbs ~20/s |
| Notify instrumentation baseline (1:1 per frame) | `NotifyCountTracksFrameProcessing` |
| Streaming continues past 10k cumulative frames (no freeze) | `StreamingContinuesPastTenThousandFrames` via `forceFrameCountForTesting` seam |
| LiveView page shows subscribed decoded values updating in ~100ms steps | Manual smoke: RPM wobble/rev blips visible, coolant rising, throttle/load synced |
| Sort toggle, clear, mid-stream disconnect, lifecycle idempotency | Existing 6 bridge behaviors + tests |
| Full app builds Windows (MSVC 2022) + macOS (AppleClang, Qt 6.12.0) | CI jobs |
| Responsive layout (sidebar ↔ TabBar at 720px) incl. new Live entry | Local run |
| Version banner `core 0.2.0` from generated version.h | Local run; single source of truth in root CMake `project()` |

## In Progress

v0.3.0 phases:

- [x] **Phase 1 — Signals layer:** POD ABI, scale/offset decode, LSB-first
      bit extraction, sign extension, validator
- [x] **Phase 1b — Mock signal emission:** tick-driven simulation;
      round-trip integration test on real worker-thread frames
- [x] **Phase 2 (mostly) — Coalesced LiveView:** 100ms batch window,
      SignalListModel, subscription checkboxes, LiveView value table,
      in-flight backpressure fix, notify/batch metrics.
      REMAINING: strip chart (per-signal rolling history + QML Canvas)
- [ ] **Phase 3 — J2534 hybrid loader:** registry discovery (dual WOW64
      views), in-process x64 load, surrogate host for x86-only DLLs
- [ ] **Phase 4 — Record-replay transport:** fixture capture +
      deterministic playback through the same frame API

## Known Issues / Technical Debt

1. **aqtinstall pinned to git master on Windows CI.** Sporadic py7zr
   `Bad7zFile` extraction bug seen once (clean re-run passed). If it
   recurs: pin a known-good post-#1000 commit + `py7zr==1.1.0`; revert
   to PyPI release once aqtinstall 3.4+ ships the layout fix.
2. **Mobile builds unvalidated.** Responsive layout only; no kit build
   attempted yet.
3. **Linux full-app build not in CI.** Deliberate (core invariant is the
   Ubuntu job's purpose); add when Linux is a ship target.
4. **Strip chart absent** — Phase 2 remainder; needs history ring in
   SignalListModel.
5. **Decode ignores subscription state** (all signals decoded per
   frame, filtered at model). Harmless at mock rate; filter decode when
   signal counts grow.
6. **Ascending (oldest-first) log mode lacks auto-follow UX.** v0.3
   candidate.
7. **Session API single-threaded by contract** (bridge is sole caller).
8. **Drop-counter emit is still per-frame in handleFrameInternal**
   (TODO in code from v0.2); move into the coalesce batch for real-data
   rates.
9. **`SessionStartWithoutTransportFails` is a `SUCCEED()` stub** — weak
   test, either implement or delete.

## Roadmap

| Phase | Goals | Status |
|-------|-------|--------|
| v0.1.0 | Skeleton: core ABI, Qt shell, responsive layout | ✅ Released |
| v0.2.0 | Callback API, mock transport, CI + tests | ✅ Released (tagged) |
| v0.3.0 | Signals layer, coalesced LiveView, J2534 transport, live vehicle read/logging | 🚧 Phases 1–1b done; Phase 2 mostly done |
| v0.4.0 | Security access discovery, flash capability | Planned |
| v0.5.0+ | Multi-manufacturer plugin modules | Planned |

## Recent Engineering Notes

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
resolved it cleanly. Windows then hit an aqtinstall bug constructing
doubled `qt6_xxxx/qt6_xxxx/` paths for the Qt 6.11+ repository layout;
worked around by invoking aqt from master directly. Both fixes are
temporary scaffolding to be reverted when upstreams ship tagged releases.
(Recurring CI-noise, ignorable: `Qt6TaskTree`/`QmlAssetDownloaderPrivate`
not-found warnings and missing Vulkan headers — optional Quick plugins,
never used by this app.)

**2026-10-04 — callback API round:** dropped never-implemented registration
handle (pair-identity instead), duplicate registration idempotent-success,
tests assert actual behavior, snapshot-dispatch mandated so callback
reentry can't deadlock. MSVC caught an implicit int→enum conversion
(C2664) — three-compiler matrix earns its keep.

**2026-10-05 — bridge + log round:** (1) captureless lambdas unsafe as
pair-identity C callbacks needing self-reference — named free functions
only. (2) Thread-identity tests must exercise real execution paths
(session_start), not manual dispatch. (3) Assertions encode contracts,
not timing coincidences. Q_PROPERTY NOTIFY is a promise; skipping emit
leaves QML serving stale values. Windows DLL deployment for test targets
via `$<TARGET_RUNTIME_DLLS>` copy, not PATH injection (semicolon fragility).

**2026-10-06 — signals round (Phase 1):** (1) error-code precedence is a
contract — type validation before bit-parameter validation. (2)
zero-initialized PODs mask string-termination test bugs — corrupt the
ENTIRE buffer. Also migrated all suites to `gtest_discover_tests()`
(per-case granularity, free empty-suite detection); DISCOVERY runs the
test binary at BUILD time on Windows, so runtime DLL copying must be
ordered before discovery (we hit exit 0xc0000135 until reordered).

**2026-10-07 — emission + coalescing round (Phases 1b/2):** three lessons.
(1) Designated initializers are C++20 — MSVC accepts permissively, GCC on
Ubuntu rejects; C++17 code needs plain constructor helpers. (2)
`strncpy_s`/`_TRUNCATE` are MSVC-only Annex K — absent from glibc —
"fix a warning" edits can compile locally and fail only CI; direction of
travel matters (MSVC-green ≠ portable). Use a small bounded-copy helper.
(3) **Cumulative count is not backpressure**: gating delivery on lifetime
frame count froze the stream permanently at 10k while the model's ring
eviction sat unused. Bound in-flight queue depth, not totals; pair drop
statistics with liveness tests. Also: measuring before optimizing — the
guiNotifyCount instrumentation locked the 20× coalescing-reduction claim
with a baseline number. New-QML-file linker failures (QmlCacheGeneratedCode
unresolved externals) mean stale QML cache — clean reconfigure fixes.

---

*(This document describes development state only. Safety, regulatory,
and warranty caveats are in [DISCLAIMER.md](DISCLAIMER.md).)*
