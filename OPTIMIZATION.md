# Darkmajin AcidXY memory optimization

Base: `blackmajinstx-del/acidxy` commit `f31530f02687540dcf57774d5fe3ebf96c26e3b4` (2026-10-31). These changes are local and have not been published to that GitHub repository.

## Result

Open-XY SDK 1.6 / OP-XY 1.2.1 reports 42,124 bytes of initialized AcidXY code and data plus 6,816 bytes of AcidXY state: **48,940 bytes (47.8 KiB)**. The earlier build measured about 265 KiB, so the engine allocation falls by about 82%.

## Changes

- Pack the exponential table as 24-bit values.
- Keep the filter curve coarse at very low coefficients and use denser segments from the tune range upward.
- Use the Blackfin Q31 multiply for fractional table interpolation.
- Reduce tanh, envelope and saturation-compensation tables and interpolate between entries.
- Set `voiceCapacity` to 1. Supplied presets already use mono mode; polyphonic user presets are limited to one simultaneous note per track in this build.

## Verification

`python sdk.py compile --machine . --out ... --profiling off` succeeded. `python sdk.py test --machine . --cxx <clang++> --host-cxx <clang++>` passed the preview ABI checks and all four host tests. SDK CPU estimation was unavailable for this candidate, so no CPU performance claim is made.
