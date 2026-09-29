# DISCLAIMER — READ BEFORE USING

**This software may damage your vehicle's control modules.**

QtTune is a cross-platform ECU diagnostic and tuning application in
early development. When vehicle communication and flashing features
are added, errors in this software, its configuration, or your use of
it may result in:

- Corrupted firmware or calibration ("bricked" modules)
- Engine or transmission damage
- Disabled or degraded safety systems (ABS, TCS, stability control)

**Back up your ECU contents before any session where writes are
possible. Never flash a vehicle with an unstable power supply.**

## Current State

This project is a skeleton. It does **not** communicate with any
vehicle, read any ECU data, or write any data. Do not connect it to
a vehicle expecting functionality.

## Intended Purpose

- **Diagnostics:** Read and log vehicle data
- **Calibration:** View and edit maps for supported vehicles
- **Research:** Understand diagnostic protocol behavior

## Not Intended For

- Defeating emissions controls or safety interlocks
- Circumventing manufacturer security in violation of any law,
  license agreement, or regulation
- Road use of modified calibrations where not street-legal

## Regulatory Notice

Modifying engine control software may violate:

- US: 40 CFR Part 86 (Clean Air Act prohibitions on emissions
  defeat devices)
- EU: type-approval requirements
- Other jurisdictions: local emissions and vehicle-modification laws

Vehicle modifications may void your powertrain warranty.

## Warranty & Liability

THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
The authors accept no responsibility for vehicle damage, warranty
denials, regulatory violations, or any consequential losses arising
from use of this software. See LICENSE for full terms.

---

By downloading or using this software, you acknowledge that you have
read and understood these warnings.
