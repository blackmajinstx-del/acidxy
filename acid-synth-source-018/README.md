# ACID for OP-XY

ACID is an acid bass synth for OP-XY OS 1.2.1, built against the public Open-XY
SDK 1.6. This release promotes the hardware-auditioned FAST 014 sound to the
main `acidxy` engine. All five supplied presets, including the init preset, use
the OP-XY's native mono playback mode.

## Controls

Press any encoder to switch the edited page. Both parameter rows stay visible.
Shift-turn uses the host's fine adjustment.

| Encoder | Page 1 | Page 2 |
|---|---|---|
| 1 | Decay | Overdrv |
| 2 | Cutoff | Env Mod |
| 3 | Reso | Wav Dist |
| 4 | Drive | Tune |

- **Decay (0–100):** controls how quickly the filter envelope falls after a
  note. Lower values make short, plucky sweeps; higher values sustain the sweep.
  At 100 the envelope holds until another note starts. This is filter-envelope
  decay, not the host amplitude-envelope release.
- **Cutoff (0–100):** opens the resonant low-pass filter. Low settings darken the
  bass; high settings reveal harmonics and bite. Its seven-octave coefficient
  mapping is an amount, not a calibrated Hz readout. It also brightens the two
  wall faces, capped at roughly 50% brightness.
- **Reso (0–100):** raises filter feedback and emphasizes the cutoff region,
  progressing from a rounded tone to ringing acid squelch. It also controls
  the overhead beams and floor-light circles; at zero this stage-light effect
  is hidden. Both beam layers remain distinguishable at low nonzero settings.
- **Drive (0–100):** applies hard clipping after the filter, adding aggression,
  upper harmonics and compression. Zero bypasses it. This FAST release uses 1x
  processing rather than the older 4x FIR oversampling, so bright/high notes
  can sound different and can have more aliasing. Drive also controls the
  alternating left/right note-and-particle strobes; zero hides them.
- **Overdrv (0–100):** boosts and saturates the oscillator before the filter.
  It changes how hard the filter is driven and how resonance responds. This
  differs from the post-filter Drive and WAV/DIST distortion stages. Zero
  bypasses it. Overdrv also brightens the angry eyebrows, capped at roughly 50%.
- **Env Mod (0–100):** sets how strongly each note's filter envelope opens the
  cutoff. Zero removes this envelope modulation; larger values make a wider
  note-triggered sweep. Decay controls the sweep's duration.
- **Wav Dist (−50…+50):** a combined bipolar control. −50 is square, moving
  towards 0 morphs square into saw, and **0 is clean saw with this distortion
  stage off**. Turning above 0 keeps saw and increases post-filter tanh
  distortion, reaching full distortion at +50. Other Drive/Overdrv settings
  can still distort the sound at WAV/DIST 0.
- **Tune (−24.0…+24.0 semitones):** shifts oscillator pitch by up to two
  octaves in either direction. +0.0 preserves the host note's pitch. Host pitch
  modulation remains active.

## Mono operation and presets

Supplied presets: Acid Spark (`default.json`), Lime Phreak, Neon Bass, V7ACID
and Acid Rush. Night Shift and Copper Wire are not included.

V7ACID: Tune +0.0, Cutoff 30, Reso 80, Env Mod 20, Decay 90, Overdrv 50,
Wav Dist 0, Drive 50.

Mono is a native OP-XY per-track preset setting, not a global one-voice pool.
Independent tracks can use ACID independently. Previously saved projects or
user presets retain their saved play mode; load a supplied preset or select
mono in the host. Stock OP-XY owns amplitude envelopes, velocity, pitch, glide
and downstream mixing/effects. This engine does not implement a second
sequencer or its own accent/slide flags.

The display retains the original three-pose dancer, perspective room, side
faces, eyebrows, note strobes and stage lights. Motion follows note activity;
reduced-motion mode freezes the pose and disables flashes. No visual waveform
is generated.

## Performance and size

Release 018 native build, OP-XY 1.2.1 / pinned SDK tools:

| Native size | Bytes |
|---|---:|
| Engine-attributed initialized code/data | 250,212 |
| Engine-attributed zero-initialized state | 17,284 |
| Shared SDK initialized code/data | 49,184 |
| Shared SDK zero-initialized state | 17,640 |
| Total linked allocation span, engine plus adapter | 334,320 (326.5 KiB) |
| Initialized native module binary | 299,396 |
| External display SVG | 50,488 |
| One voice state | 408 |

The SDK reserves one shared 2 MiB module region; the 334,320-byte figure is
this build's used allocation span, not an extra reservation per voice. The
native voice pool reserves 28 voice states for the declared capacity, while
the supplied presets use mono per track. Engine size is not the size of the
complete stock OS image, and these byte counts are not CPU percentages.

Calibrated desktop CPU estimates, median of three runs, in instruction-equivalent
units per one voice / 64-sample block:

| Workload | Approximate cost |
|---|---:|
| SDK independent-control sweep, worst setting | 16,259 |
| Joint corners and random static patches, worst | 19,868 |
| Continuously moving all controls, synthetic stress | 21,588 |
| Acid Spark | 16,158 |
| Lime Phreak | 14,491 |
| Neon Bass | 16,199 |
| V7ACID | 18,402 |
| Acid Rush | 16,040 |

The moving-control stress result exceeds 20,000; this release does not claim
to stay below that cost for every workload. It deliberately retains the
user-approved FAST 014 audio, rather than substituting later experiments.
CPU figures are estimates from the SDK's calibrated desktop benchmark, **not
measured Blackfin instructions, device cycles, or CPU percentages**. They cover
one voice's audio callback and exclude graphics and stock host mixing/effects.
Mono reduces simultaneous notes per track; additional tracks and effects still
add CPU load. A catalog-ready hardware profiling record remains pending.

The DSP uses fixed-point arithmetic, bounded callback work, no heap allocation
in audio, a pBLEP saw/square oscillator, two state-variable sections updated
twice, oscillator overdrive, 1x post-filter Drive, and WAV/DIST distortion.
FAST 014 uses reduced-precision coefficients and eight-sample interpolation
of settled filter-envelope coefficients. Its sound is intentionally not
sample-identical to the earlier 4x reference. Peak lookahead is 64 samples
(about 1.45 ms); the pre-host voice ceiling is approximately 0.475 full scale.

## Build and test

No stock firmware, SDK checkout, generated binaries, private captures or local
development history is included. The machine lives at this repository's root.
Use Open-XY SDK staging snapshot `3cc73e6a7131bcf1dfa67d316cf80e1e03a011b7`
(SDK 1.6), or verify compatibility with a later public SDK before updating it.

```sh
python generators/presets.py
python generators/tables.py
python ../open-xy/sdk.py test --machine .
python ../open-xy/sdk.py compile --machine . --out ../acid-build --profiling off
```

The SDK needs its documented Python dependencies, a host C++ compiler and its
pinned native tools. On Windows, use the compiler paths appropriate to your
installation. The SDK's chrono-based CPU benchmark may require C++17 with
current Microsoft headers; this release's desktop measurement used C++17 only
for that host harness. Firmware is built with the SDK's pinned Blackfin tools.
The four C++ host tests cover sound/lifecycle, analytic DSP approximation
bounds, display routing, and peak protection. The original high-precision 4x
tests are not claimed as bit-exact acceptance criteria for this 1x release.

Use your own legally obtained stock OS for firmware assembly. Preserve the
previous build's `layout.json` so existing projects keep ACID's engine/page
numbers. Build outputs must stay outside this source repository.

## Credits

vibecoded by darkmajin

implemented by one and only [V7](https://github.com/V714)

DT303 synthesis reference: [srtuss, DT303 (2022)](https://dittytoy.net/ditty/0029103012).
The fixed-point Open-XY adaptation and FAST optimizations modify that reference
structure and its supplied ACID-XY adaptation. These credits do not imply
endorsement by srtuss, V7, Open-XY or Teenage Engineering.

## License and sharing

The inherited DSP/adaptation notice is **CC BY-NC-SA 4.0**: retain attribution,
identify modifications, follow the noncommercial and ShareAlike terms. The
SDK-generated scaffold retains its MIT notice. See `LICENSE` for both notices
and the MIT text. This package is source-available; its inherited
noncommercial restriction means it must not be advertised as an unrestricted
MIT project or as OSI-style open source.

The supplied SVG/artwork came from the development material. Its authorship
and redistribution license have not been established. Inclusion for review
does not resolve those rights or grant an artwork license. Confirm permission
or replace those assets before treating the complete visual package as cleared
for public redistribution. No stock OS or third-party sample library is included.

## Sharing with Open-XY

Publish the extracted source files at the root of your own GitHub repository,
then send its URL to the maintainer for review. Do not upload the firmware ZIP
as source and do not commit `os.ldr`.

For official builder inclusion, follow the current
[Open-XY catalog submission guide](https://github.com/etcetc000/open-xy/blob/main/docs/catalog.md).
The engine source remains in your own repository. A catalog-only pull request
reserves `acidxy`, then pins an approved tag/commit and the hardware-generated
CPU record. Ordinary submissions target `main`; work requiring unreleased SDK
changes targets `staging`. Check that the ID is still available before filing.
The host estimates in this README cannot replace that hardware record.

