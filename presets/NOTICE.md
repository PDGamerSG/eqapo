# Presets

`eq/` holds the 46 equalizer presets and `delay/` the 37 delay/echo effect
presets shipped with **PC Equalizer Free 1.3.3.3** (Bulent Sener, freeware),
reproduced verbatim as Equalizer APO configuration text so the macOS engine
yields the same curves. The two PC Equalizer-specific include lines
(`pce_chnl.txt`, `pce_delay.txt`) were removed because eqapo manages channel
and effect selection itself.

Band centres (20 bands): 22, 31, 44, 62, 88, 124, 176, 250, 354, 500, 700,
1000, 1400, 2000, 2800, 4000, 5660, 8000, 11300, 16000 Hz. All bands are
`PEQ` peaking filters with a per-preset `BW Oct` bandwidth.
