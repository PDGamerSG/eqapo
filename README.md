# eqapo — Equalizer APO + PC Equalizer for macOS

A from-scratch macOS port of the Windows pair **Equalizer APO** (system-wide
parametric equalizer driven by a text configuration) and **PC Equalizer**
(20-band front end with presets). It runs in the background, processes every
sound that goes to the default output device, uses the exact same filter
mathematics and `config.txt` language as Equalizer APO 1.4, and ships all
PC Equalizer presets.

* macOS 14.2 or newer, Apple silicon or Intel. No kernel extension, no
  virtual audio driver, no `sudo`.
* CPU: about 0.3 % of one core for a 20-band stereo preset at 48 kHz.
* Latency: one device buffer (default 512 frames = 10.7 ms at 48 kHz).

## How it works

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

Requires the Command Line Tools (`xcode-select --install`) and CMake 3.25+.

```sh
cmake -S . -B build && cmake --build build -j8
./build/eqapo_tests          # unit tests (engine, filters, parser)
./build/eqapo selftest       # live end-to-end check (needs the permission)
./build/eqapo selftest --synthetic   # live check that does not need the permission
sudo cmake --install build   # installs /usr/local/bin/eqapo and the presets
```

## Use

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

Configuration lives in `~/Library/Application Support/eqapo/config/config.txt`
and uses Equalizer APO's syntax, for example:

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

Not supported (Windows specific): VST plugins, loudness correction, the
pre-mix/capture stages, registry and regex expression functions.

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

## Offline tools

```sh
eqapo process in.wav out.wav --config my.txt   # render a file through a configuration
eqapo bench --config my.txt                    # µs per block and CPU percentage
eqapo gen tone.wav --tone 100:0.25 --tone 1000:0.25
eqapo analyze out.wav 100 1000                 # dBFS of each tone
```

## Project layout

```
src/engine   portable DSP + configuration language (C++20, Accelerate FFT)
src/daemon   Core Audio tap pipeline, device monitor, FSEvents config watcher, status
src/cli      command line interface (single `eqapo` binary)
src/audio    audio file reading/writing (AudioToolbox)
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
