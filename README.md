# CMorseRunner

A C++/Qt6 rewrite of MorseRunner, a CW contest pile-up trainer by Alex Shovkoplyas VE3NEA.

The original was written in Delphi/Pascal for Windows only. This project adapts the DSP pipeline, station state machines, and contest logic into modern C++, targeting Linux (and any platform supported by Qt6 + PortAudio), with a redesigned Qt6 UI and added features.

## Features

- Simulated CW pile-up contest (DX station calling CQ, multiple callers)
- Realistic audio DSP pipeline
  - Moving-average bandpass filter with configurable bandwidth
  - AGC (automatic gain control)
  - QSB (signal fading simulation)
  - QRM (co-channel interference stations)
  - QRN (atmospheric noise)
- **RIT** (Receiver Incremental Tuning) — tune the receive passband independently of transmit frequency
- CW speed and pitch controls
- Contest log with sent/received exchange tracking
- Score dialog at end of session

## Dependencies

| Library | Version |
|---------|---------|
| Qt6     | Core, Widgets, Multimedia |
| PortAudio | 2.0 |
| CMake   | ≥ 3.28 |
| C++ compiler | C++26 (GCC 14 / Clang 18+) |

On Debian/Ubuntu:
```bash
sudo apt install qt6-base-dev portaudio19-dev cmake g++
```

On Arch:
```bash
sudo pacman -S qt6-base portaudio cmake gcc
```

## Building

```bash
git clone https://github.com/IzumiChino/CMorseRunner.git
cd CMorseRunner
cmake -B build
cmake --build build -j$(nproc)
./build/MorseRunner
```

## Usage

1. Set your callsign, contest exchange (serial number / zone), WPM and bandwidth in the left panel.
2. Click **Start** to begin a pile-up session.
3. Type callsigns and exchanges in the entry fields; press **Enter** to log a QSO.
4. Use the RIT spinbox to shift the receive frequency without moving the transmit pitch.
5. Press **Esc** to clear the entry or abort a partial QSO.
6. Click **Stop** to end the session and view the score.

## Project structure

```
src/
  audio/      DSP pipeline (filters, AGC, modulator, QSB/QRM/QRN, WAV)
  core/       Contest logic, station state machines, call list, log
  ui/         Qt6 main window and score dialog
CMakeLists.txt
```

## License

This project is dual-licensed. See [LICENSE](LICENSE) for full details.

- DSP algorithms, state-machine logic, and numeric constants are derived from MorseRunner by Alex Shovkoplyas VE3NEA and remain under the **Mozilla Public License 2.0** (Incompatible With Secondary Licenses).
- New contributions (Qt6 UI, C++ architecture, RIT feature, build system) are Copyright (C) 2026 Izumi Chino \<BI6DX\> and additionally available under **GPL-2.0**.

## Acknowledgements

Original MorseRunner by Alex Shovkoplyas VE3NEA — https://www.dxatlas.com/MorseRunner/
