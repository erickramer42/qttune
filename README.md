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

## Roadmap

| Phase | Goals | Status |
|-------|-------|--------|
| v0.1.0 | Skeleton: core ABI, Qt shell, responsive layout | ✅ Released |
| v0.2.0 | Callback API, mock transport, CI + tests | 🚧 In Progress |
| v0.3.0 | J2534 transport, live vehicle read/logging | Future |
| v0.4.0 | Security access discovery, flash capability | Future |
| v0.5.0+ | Multi-manufacturer plugins | Future |

[![CI](https://github.com/erickramer42/qttune/actions/workflows/ci.yml/badge.svg)](https://github.com/erickramer42/qttune/actions/workflows/ci.yml)

Flash capabilities require discovering each manufacturer's secure
gateway authentication. Vehicle communication research is tracked
separately and outside this repo for now.

⚠️ Early skeleton. No vehicle communication yet. Do not connect to a
vehicle expecting functionality.

## Plugins

Reserved for manufacturer modules implementing
protocol/security layers.

## Dependencies

This application uses [Qt](https://www.qt.io), licensed under LGPL-3.0.
Qt is not bundled in this repository; see qt.io for sources and license text.

## License

MIT — see [LICENSE](LICENSE) file.