# CMorseRunner

[![CI](https://github.com/IzumiChino/CMorseRunner/actions/workflows/ci.yml/badge.svg)](https://github.com/IzumiChino/CMorseRunner/actions/workflows/ci.yml)

A C++/Qt6 port of MorseRunner, a CW contest pile-up trainer by Alex Shovkoplyas VE3NEA.

The original MorseRunner was written in Delphi/Pascal for Windows. This repository ports the application to modern C++ with Qt6 and PortAudio, keeps the original contest model and signal-processing behavior, and adds a new desktop UI plus an RIT control.

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

## Releases

Pre-built binaries for Linux x86_64 are attached to each [GitHub Release](https://github.com/IzumiChino/CMorseRunner/releases).

To publish a new release:

```bash
# Push code to main.
# CI will automatically:
# 1) bump patch version in CMakeLists.txt
# 2) create and push tag v<NEW_VERSION>
# 3) trigger release workflow and upload package assets
```

If you need manual major/minor bumping locally:

```bash
bash scripts/bump-version.sh patch   # or minor / major
git push origin main --follow-tags
```

The release workflow (`release.yml`) builds in Release mode, packages the binary with LICENSE and README into a `.tar.gz`, and creates a GitHub Release automatically when a semantic-version tag like `v1.2.3` is pushed.

## Project structure

```
src/
  audio/      DSP pipeline (filters, AGC, modulator, QSB/QRM/QRN, WAV)
  core/       Contest logic, station state machines, call list, log
  ui/         Qt6 main window and score dialog
CMakeLists.txt
```

## Licensing

This repository is distributed under **GPL-2.0**.

- Files under `src/audio/**` and `src/core/**` adapt logic from the original MorseRunner source. They remain available under **MPL-2.0** and are additionally distributed under **GPL-2.0** pursuant to MPL-2.0 section 3.3.
- `.gitignore`, `CMakeLists.txt`, `README.md`, `src/main.cpp`, and files under `src/ui/**` were written for this port and are distributed under **GPL-2.0-only**.
- The original Delphi/Pascal files used for this port carry the standard MPL-2.0 header and do not include the Exhibit B "Incompatible With Secondary Licenses" notice.

See [LICENSE](LICENSE), [LICENSES/MPL-2.0.txt](LICENSES/MPL-2.0.txt), and [LICENSES/GPL-2.0-only.txt](LICENSES/GPL-2.0-only.txt) for the exact file map and full license texts.

## Acknowledgements

Original MorseRunner by Alex Shovkoplyas VE3NEA — https://www.dxatlas.com/MorseRunner/
