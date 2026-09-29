# QtTune

Cross-platform ECU reader/logger, evolving into a full tuning application.
Qt/QML frontend (desktop + mobile) over a Qt-free C ABI core library.

## Build (desktop)

    cmake -B build
    cmake --build build
    ./build/app/qttune

Requires Qt 6.5+ (install via `aqtinstall`, Qt online installer, or your
package manager).

## Mobile

Open the repo in Qt Creator, select the Android or iOS kit, and build.
The QML, bridge, and core are shared; only the kit differs.

⚠️ Early skeleton. No vehicle communication yet. Do not connect to a
vehicle expecting functionality. See DISCLAIMER.md before any future use.
