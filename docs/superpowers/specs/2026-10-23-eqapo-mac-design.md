# eqapo — Equalizer APO + PC Equalizer for macOS: Design

Date: 2026-10-23
Status: approved by autonomous decision (user request: "build it from scratch ... work on this"; session is non-interactive)

## 1. Goal

Build, from scratch, a macOS equivalent of the Windows pair
**Equalizer APO** (system-wide audio processing engine, text config) and
**PC Equalizer** (20-band graphic/parametric front end with presets that
drives Equalizer APO). The result must:

- run in the background and process *all* audio going to the default
  output device, with no user interaction once installed;
- reproduce the exact filter mathematics and the exact configuration
  language of Equalizer APO 1.4 (same `config.txt` files work unchanged);
- ship PC Equalizer's 20 fixed bands and every one of its 46 EQ presets
  and 37 delay/echo presets, producing bit-identical config text;
- be very fast with very low CPU usage (target: under 1 % of one core for
  a 20-band stereo preset at 48 kHz; no allocations or locks on the audio
  thread);
- be verified on macOS with automated tests plus a live end-to-end
  measurement proving the output audio differs from the input as the
  filters dictate.

Assumption stated to the user: "works properly for Windows" in the
request is read as "behaves like the Windows original"; the deliverable
targets macOS only (macOS 14.2 or newer; developed and tested on
macOS 27.0.1, Apple silicon).

Reference material used: Equalizer APO 1.4 source (GPL v2, GitHub mirror
`mirror/equalizerapo`), its wiki "Configuration reference", and the
preset/data files inside "PC Equalizer Free v1.3.3.3 Portable". No code
is copied; the DSP is re-implemented in C++ following the published
formulas so results match. Our code is licensed GPL-2.0-or-later.

## 2. Non-goals (explicitly out of scope)

- Windows build.
- VST plugin hosting (`VSTPlugin:` lines are ignored, like unknown commands).
- Loudness correction (`LoudnessCorrection:`; depends on Windows volume API).
- Pre-mix stage / per-application processing. `Stage:` is parsed; only
  the `post-mix` stage is processed.
- Registry functions, regex functions and arrays inside expressions.
- Processing audio that applications send to a non-default output device.

## 3. Audio capture and playback architecture (macOS)

Chosen approach: **Core Audio process tap + private aggregate device**.

1. On start, and whenever the default output device changes, the daemon:
   - finds the default output device `D` (UID, name, nominal sample rate,
     output stream format, buffer frame size);
   - creates a `CATapDescription` with
     `initExcludingProcesses:[ownProcessObject] andDeviceUID:D.uid withStream:0`,
     `privateTap = YES`, `muteBehavior = CATapMutedWhenTapped`
     and calls `AudioHardwareCreateProcessTap`. The tap's stream format
     equals the device stream format (same channel count and rate).
   - creates a private aggregate device with sub-device `D` (main/clock)
     and the tap in its tap list (`tapautostart = 1`, drift comp off).
   - installs one IOProc (`AudioDeviceCreateIOProcIDWithBlock`, NULL queue
     so it runs on the HAL real-time thread). In each callback the input
     buffers hold the tapped mix of every other process; the daemon runs
     the filter engine and writes the result into the output buffers,
     which go to `D`.
2. Because the tap mutes the tapped processes while it is being read,
   users hear only the processed signal. Hardware volume/mute keys still
   act on `D`. If the daemon dies the tap is destroyed by the system and
   normal audio resumes (fail-safe).
3. Listeners: default output device change, device alive, nominal sample
   rate change, stream configuration change, processor overload (counted
   as xruns). Any structural change tears down and rebuilds the pipeline
   on a serial control queue.
4. Permission: the process-tap API requires the "System Audio Recording
   Only" TCC permission. The binary embeds an `Info.plist`
   (`-sectcreate __TEXT __info_plist`) with `CFBundleIdentifier` and
   `NSAudioCaptureUsageDescription`, and is ad-hoc code-signed so the
   grant persists across launches.

Rejected alternative: a HAL virtual-device driver (`AudioServerPlugIn`,
BlackHole/eqMac style). Needs `sudo` install into
`/Library/Audio/Plug-Ins/HAL`, a `coreaudiod` restart, forces users to
pick a virtual device, and needs manual volume forwarding. The tap gives
the same single-callback latency with none of that.

## 4. Components

```
eqapo (one Mach-O binary, Objective-C++ / C++20)
├── engine/      portable DSP + config language (no Core Audio)
│   ├── FilterEngine        loads config, builds FilterConfiguration, runs it
│   ├── FilterConfiguration channel buffers, in-place/out-of-place routing, crossfade
│   ├── factories           one per command (Device, Stage, If/Eval, Include, Channel,
│   │                       IIR, BiQuad, Preamp, Delay, Copy, Convolution, GraphicEQ)
│   ├── filters             BiQuad, BiQuadFilter, IIRFilter, PreampFilter, DelayFilter,
│   │                       CopyFilter, ChannelFilter, GraphicEQFilter, ConvolutionFilter
│   ├── dsp                 RealFFT (Accelerate vDSP), PartitionedConvolver, MinimumPhase
│   ├── ChannelHelper, StringHelper, GainIterator
│   └── expr                tiny expression evaluator for If/ElseIf/Else/EndIf/Eval/`inline`
├── audio/       AudioFile (ExtAudioFile read/write, WAV/AIFF/FLAC/CAF... via AudioToolbox)
├── daemon/      AudioPipeline (tap+aggregate+IOProc), DeviceMonitor, ConfigWatcher (FSEvents),
│                StatusFile, Daemon (lifecycle, signals)
└── cli/         sub-commands (see §7)
gui/             EqualizerAPO.app — Swift/SwiftUI menu bar app = PC Equalizer front end
presets/         eq/ (46) and delay/ (37) PC Equalizer presets, verbatim band values
tests/           C++ unit + integration tests (CTest), live self-test command
```

### 4.1 Engine data flow (mirrors Equalizer APO semantics)

- Channel names come from channel count exactly as Equalizer APO's
  `ChannelHelper::getDefaultChannelMask`: 1→`C`; 2→`L R`; 4→`L R RL RR`;
  6→`L R C LFE SL SR`; 8→`L R C LFE RL RR SL SR`; otherwise `1..N`.
  Name lookups accept numbers (1-based) and the aliases
  SL↔RL, SR↔RR, SUB→LFE.
- `FilterEngine::loadConfig` parses `config.txt` line by line
  (`Command: parameters`; command trimmed; unknown commands ignored;
  `#` lines are ignored because they have no recognised command; UTF-8
  with optional BOM; CRLF tolerated). Factories are consulted in the
  same order as the original so precedence rules match (Device → If →
  Expression → Include → Stage → Channel → IIR → BiQuad → Preamp →
  Delay → Copy → Convolution → GraphicEQ).
- Each filter is initialised with `(sampleRate, maxFrameCount,
  selectedChannelNames)` and may return a new channel list (Copy creates
  virtual channels; Channel changes the selection). Routing tables
  (`inChannels`, `outChannels`, in-place flag) are precomputed; the audio
  thread only does pointer indexing.
- Processing: deinterleave device input into planar `float` channel
  buffers, run each filter on its channel subset, interleave the first
  `outputChannelCount` channels back out. Virtual channels beyond the
  device count are zeroed at block start.
- Hot reload: a new `FilterConfiguration` is built on the control
  thread, published through an atomic pointer, and the audio thread
  crossfades old→new over `sampleRate/100` frames with a raised-cosine
  curve (Equalizer APO's 10 ms transition), then hands the old one back
  for deletion off the audio thread. A semaphore prevents a second load
  while a transition is in progress.
- Denormals: the audio thread enables flush-to-zero (FPCR.FZ on arm64,
  MXCSR FTZ/DAZ on x86_64) and biquads also zero denormal state after
  each block, like the original.

### 4.2 Filter mathematics (exact Equalizer APO 1.4)

Biquad (`Filter n: ON <type> ...`), RBJ cookbook as implemented in
`BiQuad.cpp`:

- `A = 10^(gain/40)` for PK/LS/HS, else `10^(gain/20)`;
  `ω = 2πf/fs`; `alpha = sin ω / (2Q)` for Q, `sin ω / 2 · sqrt((A+1/A)(1/S−1)+2)`
  for shelf slope S, `sin ω · sinh(ln2/2 · BW · ω / sin ω)` for bandwidth;
  `beta = 2 sqrt(A) alpha`. Coefficients per type exactly as the original
  (LP, HP, BP constant-0 dB peak, NO, AP, PK, LS, HS). State: direct form
  I in double precision, float in/out.
- Parser rules: types `PK PEQ Modal LP HP LPQ HPQ BP LS HS LSC HSC NO AP`;
  regexes `Fc <num> Hz` (also `H z`, non-breaking space stripped,
  comma→period, and "1.000"-style thousands: if the string has ≥5 chars,
  no exponent, and a `.` four from the end, multiply by 1000),
  `Gain <num> dB`, `Q <num>`, `BW Oct <num>`, and a leading `<num> dB`
  slope right after the type (e.g. `LS 6dB`, `LSC 10.8 dB`).
  Defaults when no Q/BW/slope: PK and AP → error (line ignored);
  LP/HP/BP → Q = 1/√2; LS/HS → S = 0.9; NO → Q = 30.
  Shelves with explicit slope use S = slope/12; shelves whose type does
  not end in `C` are corner-frequency shelves: the biquad centre
  frequency is shifted by `10^(|gain|/80/S)` (× for LS, ÷ for HS), with
  S derived from Q as `1/((1/Q²−2)/(A+1/A)+1)` when Q was given.
  Gain is ignored for LP/HP/NO/AP; BW ignored for shelves; slope ignored
  for non-shelves. `None` type is silently skipped.
- `Filter: ON IIR Order m Coefficients b0..bm a0..am` → generic direct
  form I, normalised by a0, double state.
- `Preamp: x dB` → multiply by `10^(x/20)` on selected channels
  (multiple preamps add in dB because they multiply).
- `Delay: t ms | n samples` → integer ring buffer of
  `round(fs·t/1000)` or `round(n)` samples per channel.
- `Copy: T=f*S+... T2=...` → sums with factors (plain, `dB` suffixed,
  or constants containing a decimal point / literal `0`), creating
  target channels that do not exist yet; parsing rules identical to
  `CopyFilterFactory` (split on space, `=`, `+`, `*`).
- `Channel: L R | 1 2 | all` → selection for following commands; restored
  at the end of an included file.
- `GraphicEQ: f g; f g; ...` → nodes sorted by frequency, gain
  interpolated linearly in log-frequency (flat outside), sampled at
  16384 bins over 0..fs/2 (FFT size 32768), converted to a
  minimum-phase impulse by the cepstral method (floor −100 dB), scaled,
  windowed by a half raised-cosine over 16384 taps, and convolved with
  the uniformly partitioned FFT convolver (partition = maxFrameCount).
- `Convolution: file` → impulse loaded with AudioToolbox; file sample
  rate must equal the device rate (else error, filter skipped); file
  channels assigned round-robin to selected channels; same partitioned
  convolver (`num_partitions = ceil(len/flen)`, FFT size 2·flen,
  frequency-domain delay line, overlap-add of the second half).
- `Include: file` → relative to the including file; recursion limit 100.
- `Device: pattern; pattern` → each pattern is space-separated words that
  must all occur (case-insensitive) in `"<device name> <device UID>"`;
  `all` matches always; a non-matching Device skips lines until the next
  Device line or end of the current file.
- `Stage: pre-mix post-mix capture` → lines are processed only while the
  selection includes `post-mix` (the only stage that exists here).
- Expressions (`If:`/`ElseIf:`/`Else:`/`EndIf:`/`Eval:` and
  `` `inline` ``): numbers, strings, booleans; `+ - * / ^`, comparisons,
  `and or xor`, `!`, ternary `?:`, assignment to variables, functions
  `abs sin cos tan sinh cosh tanh ln log log10 exp sqrt min max sum
  str2dbl strlen tolower toupper`; constants `e pi sampleRate
  inputChannelCount outputChannelCount deviceName connectionName
  deviceGuid stage`. Delivered after the core engine and GUI.

### 4.3 Daemon behaviour

- Config root: `~/Library/Application Support/eqapo/`
  - `config/config.txt` (watched, hot-reloaded, created with a default
    on first run), `config/presets/eq/*.txt`, `config/presets/delay/*.txt`.
  - `status.json` (device, rate, channels, buffer, filters, errors,
    xruns, pid, timestamps) rewritten on every state change.
  - `eqapo.pid`.
- Logs to stderr with timestamps; the LaunchAgent redirects to
  `~/Library/Logs/eqapo.log`.
- FSEvents watcher on the config directory tree, 50 ms latency, reload
  debounced to one load per burst (the original waits 10 ms for a second
  event).
- Signals: SIGTERM/SIGINT → orderly teardown (stop IOProc, destroy
  aggregate and tap). SIGHUP → reload config.
- Buffer size: default device value; `--buffer N` sets the aggregate's
  `kAudioDevicePropertyBufferFrameSize`.

## 5. PC Equalizer front end (GUI)

`EqualizerAPO.app`, SwiftUI `MenuBarExtra` (window style), menu bar only,
no Dock icon. It never processes audio; it writes `config.txt` exactly
as PC Equalizer does, and the daemon applies it instantly.

- 20 vertical sliders, −15…+15 dB in 0.1 dB steps, fixed centres
  22, 31, 44, 62, 88, 124, 176, 250, 354, 500, 700, 1000, 1400, 2000,
  2800, 4000, 5660, 8000, 11300, 16000 Hz (PC Equalizer's band set).
- Bandwidth per preset (BW Oct 0.5…3.0, default 1.2), preamp
  −20…+20 dB, master on/off, preset picker (46 built-in + user presets
  saved into `presets/eq/`), delay/echo effect picker (37 built-in),
  "Open config folder", "Edit config.txt", daemon status + start/stop,
  CPU/xrun readout from `status.json`.
- Output format: PC Equalizer's exact layout:
  `Filter  n: ON  PEQ      Fc   22.0 Hz  Gain   0.00 dB  BW Oct 01.200`
  with its thousands-separator frequencies (`1,000`, `11,300`), a
  `Preamp:` line, then `Include: presets/delay/<name>.txt` when an
  effect is selected.
- Built without Xcode: `swiftc`/SwiftPM + a script that assembles the
  `.app` bundle and ad-hoc signs it.

## 6. Testing

- Unit tests (C++, own minimal harness, run by CTest):
  biquad coefficients vs. closed-form response (`gainAt`) for every type;
  parser edge cases (REW thousands, comma decimals, slope syntax, missing
  Q errors, `None`); channel naming/aliases; Copy parsing and processing
  (constants, dB factors, new channels, self-reference); Delay (ms and
  samples, block boundaries); Preamp stacking; GraphicEQ response at
  nodes within 0.25 dB and flat outside; Convolution vs. direct
  convolution (1e-5 tolerance) across partition boundaries; Include
  nesting and channel restore; Device/Stage gating; transition
  crossfade; expression evaluator.
- Offline integration: `eqapo process in.wav out.wav --config c.txt`
  used by a test that renders a multi-tone signal through the PC
  Equalizer "Pure Bass" preset and asserts per-band RMS changes.
- Live macOS test: `eqapo selftest` starts the pipeline with a −30 dB
  notch at 1 kHz plus a +6 dB band at 100 Hz, plays a 100 Hz + 1 kHz tone
  from a child `afplay`, taps the daemon's own output with a second
  (non-muting) process tap, and reports measured attenuation/gain. Pass
  criterion: 1 kHz down by more than 20 dB, 100 Hz up by 5…7 dB relative
  to the source.
- Performance: `eqapo bench` runs the engine on synthetic audio for a
  given config and prints µs per 512-frame block and estimated CPU %.
  Target under 1 % for the 20-band preset; the live daemon's CPU is
  checked with `ps` during playback.

## 7. CLI

```
eqapo run [--buffer N] [--config DIR]   foreground daemon
eqapo install | uninstall               LaunchAgent (~/Library/LaunchAgents/com.eqapo.daemon.plist)
eqapo start | stop | restart | status   control the LaunchAgent / show status.json
eqapo reload                            SIGHUP running daemon
eqapo devices                           list output devices (name, UID, rate, channels)
eqapo preset list | apply <name> | show <name>
eqapo process in out [--config FILE]    offline render
eqapo bench [--config FILE]             engine throughput
eqapo selftest                          live end-to-end check (needs audio permission)
eqapo config path | edit | reset        config helpers
```

## 8. Error handling

- Config errors never stop audio: a bad line is logged and skipped; the
  previous configuration stays active until a new one loads.
- Pipeline build failures (permission denied, device gone, unsupported
  format) are logged, written to `status.json`, and retried with
  exponential back-off (1 s → 30 s) and on every device-list change.
- Tap/aggregate objects are always destroyed on teardown paths,
  including crashes via `atexit` + signal handlers where safe.
- Format guard: only `Float32` PCM streams are processed; anything else
  is logged and the pipeline passes audio straight through (copy).

## 9. Milestones

1. Engine + CLI (`process`, `bench`, `preset`) with unit tests passing.
2. Daemon (tap pipeline, watcher, status, LaunchAgent) + live selftest.
3. Presets imported; README; install path exercised end to end.
4. GUI menu bar app writing PC Equalizer-format configs.
5. Expression language subset (If/Eval/inline).
