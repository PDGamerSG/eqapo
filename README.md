# eqapo — Equalizer APO + PC Equalizer for macOS, Linux, and Windows

A from-scratch macOS port of the Windows pair **Equalizer APO** (system-wide
parametric equalizer driven by a text configuration) and **PC Equalizer**
(20-band front end with presets). It runs in the background, processes every
sound that goes to the default output device, uses the exact same filter
mathematics and `config.txt` language as Equalizer APO 1.4, and ships all
PC Equalizer presets.

* macOS 14.2 or newer, Apple silicon or Intel. No kernel extension, no
  virtual audio driver, no `sudo`.
* Linux and Windows builds provide the offline CLI for 32-bit float WAV processing,
  presets, tone generation, analysis, and benchmarks. Live system-wide capture
  uses Core Audio on macOS.
* CPU: about 0.3 % of one core for a 20-band stereo preset at 48 kHz.
* Latency: one device buffer (default 512 frames = 10.7 ms at 48 kHz).

## Install

Download the package for your platform from the [latest release](https://github.com/PDGamerSG/eqapo/releases/latest).

- **Debian / Ubuntu (64-bit Intel or AMD)** — Install with `sudo apt install ./eqapo_0.1.5_amd64.deb`, then run `eqapo --help`.
- **Windows (64-bit Intel or AMD)** — Run the `.exe` installer. In PowerShell, run `& "$env:ProgramFiles\eqapo\bin\eqapo.exe" --help` for the default install location, or select the installer’s PATH option and open a new terminal.
- **macOS (Apple silicon)** — Open the `.dmg`, drag `eqapo.app` into Applications, then open it from Applications.

## How it works on macOS

The daemon creates a Core Audio *process tap* on the default output device
that captures the mix of every other process and mutes it while tapped, wraps
the output device and the tap in a private aggregate device, and runs one
real-time IOProc: tap in → filter engine → device out. Hardware volume keys
still work. If the daemon stops for any reason the tap disappears and normal
audio resumes.

macOS asks once for the **System Audio Recording Only** permission when the
daemon first starts. Click *Allow*. (`eqapo permission` reopens the settings
pane if you need to change it later.)

## Build

Requires CMake 3.25+. macOS also needs the Command Line Tools
(`xcode-select --install`). Linux builds need a C++20 compiler and pthreads.

```sh
cmake -S . -B build && cmake --build build -j8
./build/eqapo_tests          # unit tests (engine, filters, parser)
./build/eqapo selftest       # live end-to-end check (needs the permission)
./build/eqapo selftest --synthetic   # live check that does not need the permission
sudo cmake --install build   # installs /usr/local/bin/eqapo and the presets
```

On Linux and Windows, CMake builds the offline CLI. Run `cpack -G DEB --config
build/CPackConfig.cmake` on Debian or Ubuntu with `dpkg-dev` installed. For the
Windows x64 installer, build with a Windows x64 compiler (such as MinGW-w64) and
run `cpack -G NSIS --config build/CPackConfig.cmake` with NSIS installed. Run
`ctest --test-dir build` to exercise the native build before packaging.

## Use

The daemon, live device controls, and menu-bar app commands below are
macOS-only. Linux and Windows support the offline commands and presets.

```sh
eqapo install                # LaunchAgent: starts at login, restarts if it exits
eqapo status                 # device, sample rate, filters, CPU load, errors
eqapo preset list            # 46 PC Equalizer EQ presets + 36 delay/echo effects
eqapo preset apply rock      # writes config.txt; the daemon reloads instantly
eqapo preset apply "Headphones 01" --delay "Echo - small room"
eqapo config edit            # open config.txt in your editor
eqapo config off             # pass audio through unchanged
eqapo uninstall              # remove the LaunchAgent
```

Run `eqapo config path` for the active configuration file. The default roots
are `~/Library/Application Support/eqapo` on macOS, `$XDG_CONFIG_HOME/eqapo`
(or `~/.config/eqapo`) on Linux, and `%LOCALAPPDATA%\eqapo` on Windows.
Set `EQAPO_HOME` to override the root. The file uses Equalizer APO syntax:

```
Preamp: -6 dB
Filter 1: ON PK Fc 100 Hz Gain -3 dB Q 1.41
Filter 2: ON LS 12dB Fc 80 Hz Gain 4 dB
Channel: L
Delay: 2 ms
Channel: all
GraphicEQ: 25 6; 40 4.5; 63 3; 100 1.5; 160 0; 1000 0; 6300 1.5; 10000 3; 16000 3
Include: presets/eq/[build] Rock.txt
Convolution: room.wav
Device: MacBook Pro Speakers
Preamp: -3 dB
```

Supported commands: `Preamp`, `Filter` (PK/PEQ/Modal, LP, HP, LPQ, HPQ, BP,
LS, HS, LSC, HSC, `LS 6dB`/`LS 12dB`, `HS 6dB`/`HS 12dB`, NO, AP, IIR),
`Delay`, `Copy`, `Channel`, `GraphicEQ`, `Convolution`, `Include`, `Device`,
`Stage`, `If`/`ElseIf`/`Else`/`EndIf`, `Eval` and `` `inline` `` expressions.
Room EQ Wizard exports (both "Generic" and "FBQ2496" formats, comma decimals,
"1.000" thousands) load unchanged. See the
[Equalizer APO configuration reference](https://sourceforge.net/p/equalizerapo/wiki/Configuration%20reference/).

Not implemented in the portable engine: VST plugins, loudness correction, the
pre-mix/capture stages, registry functions, and regex expression functions.

## Installable macOS app

`eqapo.app` is a SwiftUI menu bar app with PC Equalizer's 20 bands (22 Hz … 16
kHz), bandwidth, preamp, visible preset and effect menus, the supplied Midnight
Pulse logo, startup-at-login control, and daemon status. It writes `config.txt`
in PC Equalizer's exact format; the daemon applies it immediately. A final
stereo-linked output limiter keeps hot EQ and volume boosts below digital full
scale to prevent clipping distortion. The limiter is allocation-free on the
audio thread and typical processing is under 1% of one CPU core.

```sh
./scripts/install_app.sh     # builds, tests, installs /Applications/eqapo.app, and starts the daemon
./scripts/package_dmg.sh     # creates build/eqapo.dmg for drag-and-drop installation
```

The app registers `com.eqapo.gui` as a per-user LaunchAgent so the menu bar
control returns at login. The daemon has its own `com.eqapo.daemon` LaunchAgent
and restarts independently if it exits. Grant macOS's System Audio Recording
permission the first time the daemon starts.

## Releases

### eqapo 0.1.5

- Added a `.deb` package for Debian and Ubuntu and a Windows `.exe` installer.
- Linux and Windows packages include the offline CLI and bundled presets.
- The macOS `.dmg` includes the menu-bar app and its Core Audio daemon.

Release assets:

- `eqapo_0.1.5_amd64.deb` installs the offline CLI and presets on Debian and Ubuntu x64.
- `eqapo-0.1.5-windows-x64-setup.exe` installs the offline CLI and presets on Windows x64.
- `eqapo-0.1.5-macos-arm64.dmg` installs the macOS menu-bar app and Core Audio daemon.

### eqapo 0.1.4

- Restored the project under `Data/Random projects/eqapo` after the original
  `Equalizer apo for mac` source folder was removed.
- Added a Linux build for the portable engine and offline CLI, including WAV
  processing, presets, benchmarks, and the existing test suite.
- Added Linux CI coverage and a Linux tarball packaging script.
- Updated the macOS app and daemon version to `0.1.4`.

Release assets:

- `eqapo-0.1.4-macos-arm64.dmg` is the installable macOS menu-bar app.
- `eqapo-0.1.4-linux-arm64.tar.gz` contains the Linux CLI and presets.

The Linux package does not include the macOS menu-bar app or live Core Audio
process-tap daemon.

### eqapo 0.1.3

- Fixed the menu-bar popover so clicks outside the panel close it immediately,
  including clicks delivered to another application, while clicks inside the
  panel keep it open.
- Made **Headphones 06** the first-run default preset. The app writes that
  preset into a missing or first-run placeholder configuration so the daemon
  uses it immediately.
- Preserved existing saved configurations and user-selected presets on later
  launches.
- Updated the packaged app and command-line daemon version to `0.1.3`.
- Verified the installed app, launch agents, bundled preset resources, release
  build, preset round-trip check, and native test suite.

## Offline tools

```sh
eqapo process in.wav out.wav --config my.txt   # render a file through a configuration
eqapo bench --config my.txt                    # µs per block and CPU percentage
eqapo gen tone.wav --tone 100:0.25 --tone 1000:0.25
eqapo analyze out.wav 100 1000                 # dBFS of each tone
```

## Project layout

```
src/engine   portable DSP + configuration language (C++20, Accelerate on macOS)
src/daemon   Core Audio tap pipeline, device monitor, FSEvents config watcher, status
src/cli      command line interface (single `eqapo` binary)
src/audio    audio file reading/writing (AudioToolbox on macOS, WAV elsewhere)
presets      PC Equalizer presets (see presets/NOTICE.md)
gui          menu bar application (Swift)
tests        unit tests (CTest)
docs         design spec and implementation plan
```

License: GPL-2.0-or-later (Equalizer APO is GPL-2.0; this is an independent
re-implementation of its documented behaviour).

## Credits and inspiration

eqapo is inspired by the design, configuration language, and filter behaviour
of [Equalizer APO](https://sourceforge.net/projects/equalizerapo/) and the
20-band front end and preset collection from **PC Equalizer Free** by Bulent
Sener. Their names, documentation, and preset data are credited here for the
work that made this compatible macOS implementation possible; eqapo's engine
code is an independent implementation.

The **Midnight Pulse Equalizer Emblem** supplied with this project is used as
eqapo's application icon and in-app branding, with credit to the supplied
artwork source/project owner.
