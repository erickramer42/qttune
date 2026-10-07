# QtTune

> ⚠️ **WARNING:** This software may permanently damage your vehicle.  
> See [DISCLAIMER.md](DISCLAIMER.md) for full terms before use.

Cross-platform ECU reader/logger, evolving into a full tuning application.
Qt/QML frontend with a shared codebase targeting desktop (Windows, Linux,
macOS) and mobile (Android, iOS); desktop builds are validated, mobile
kits are pending first on-device build.

## Requirements

### Minimum for Desktop Build

| Component | Version | Notes |
|-----------|---------|-------|
| **CMake** | 3.21+ | Required for `qt_standard_project_setup()` |
| **Qt** | 6.12+ (LGPL) | Core, Gui, Quick modules |
| **C++ Compiler** | C++17 | MSVC 19.x (2019+), GCC 11+, Clang 14+ |

### Runtime

- **Windows:** VC++ 2019 Redistributable (auto-installed by MSVC workload)
- **Linux/macOS:** Qt runtime libraries (provided by package manager or Qt installer)

### Development (Optional)

- **Python 3.13+** (64-bit) — for future trace analysis tooling
- **J2534 driver** — optional, only needed for live vehicle testing in v0.3+

Mobile builds additionally require Qt Creator with Android/iOS kits configured.

## Build (Desktop)

    # Windows (adjust path to your Qt installation)
    cmake -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
    cmake --build build --config Release

    # Linux/macOS: use the Qt path from your package manager or Qt installer
    cmake -B build -DCMAKE_PREFIX_PATH=/path/to/qt
    cmake --build build

Run `build/app/Release/qttune.exe` on Windows (VS generator) or
`build/app/qttune` with single-config generators like Ninja.

## Build (Mobile) — unvalidated

Intended path: open in Qt Creator → select Android or iOS kit → Run. The
QML, bridge, and core are shared; only the kit differs. Not yet verified
on a device/emulator.

## Features

- Cross-platform: Windows, macOS, Linux; desktop validated, mobile unvalidated
- Qt-free C++ core behind a stable C ABI — UI layers are replaceable skins
- Live frame logging: mock transport streams synthetic CAN frames end-to-end
  — core worker thread → GUI-thread bridge → sort proxy → virtualized log table
- Signal decoding: POD signal definitions with scale/offset linear decode,
  LSB-first bitfield extraction, signed-type sign extension (core, unit-tested)
- Physically plausible mock data: idle RPM wobble with rev blips, coolant
  warm-up toward thermostat, trailing intake temps, coupled throttle/load —
  every mock frame is decodable by the built-in signal set (round-trip tested)
- Session lifecycle: create/start/stop/close with clean worker quiesce
- Thread-safe by construction: UI receives display values only; all parsing in core
- Backpressure: drop-at-source above 10k-frame cap with honest accounting
- Sort toggle: newest-first (live edge at viewport top) / oldest-first (chronological)

## Status / Roadmap

| Version | Milestone | Status |
|---------|-----------|--------|
| v0.1.0  | Skeleton: core ABI, Qt shell, responsive layout | ✅ Released |
| v0.2.0  | Callback API, mock transport, bridge marshaling, live LogPage | ✅ Released |
| v0.3.0  | Signals layer, mock signal emission, coalesced LiveView, J2534 transport, live vehicle read/logging | 🚧 Phases 1–1b done |
| v0.4.0  | Security access discovery, flash capability | Planned |
| v0.5.0+ | Multi-manufacturer plugins | Future |

[![CI](https://github.com/erickramer42/qttune/actions/workflows/ci.yml/badge.svg)](https://github.com/erickramer42/qttune/actions/workflows/ci.yml)

Currently 37 automated tests (18 core, 13 signals, 6 bridge), all green
locally on Windows/MSVC 2022 and on CI (3-platform matrix). Per-case ctest
granularity via `gtest_discover_tests()`. Mock frames and unit-level signal
decoding only — no real vehicle communication yet. See [STATUS.md](STATUS.md)
for verified test matrix and engineering notes.

Flash capabilities require discovering each manufacturer's secure gateway authentication. Vehicle communication research is tracked separately and outside this repo for now.

## Plugins

Reserved for manufacturer modules implementing
protocol/security layers.

## Dependencies

This application uses [Qt](https://www.qt.io), licensed under LGPL-3.0.
Qt is not bundled in this repository; see qt.io for sources and license text.

## License

MIT — see [LICENSE](LICENSE) file.

⚠️ Early infrastructure. Core callback API, mock transport, and signal
decoding implemented and unit-tested. No real vehicle communication yet.
Do not connect to a vehicle expecting functionality.
