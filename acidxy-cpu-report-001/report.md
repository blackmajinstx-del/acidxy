# acidxy: hardware CPU measurement

Machine commit: `da1a15908d7b38668fe06a115f486caed1ce715b`. Benchmark: `opxy-synth-c8-sweep-v1`. Scope: whole voice, including oscillator/filter.

Heaviest of the midpoint run, 72 measured settings (each control through its positions with the others at their defaults) and every control at its heaviest position, each the median of 3 captures: **7.720%** of the stock 580,352-cycle block budget. Midpoint run: 6.076%. Every control at its heaviest position: 5.618%. Heaviest single setting: WAV DIST 62.5%.

| Setting | Median |
| --- | ---: |
| midpoint (stock) | 6.076% |
| DECAY 0.0% | 6.673% |
| DECAY 12.5% | 6.734% |
| DECAY 25.0% | 6.676% |
| DECAY 37.5% | 6.743% |
| DECAY 50.0% | 6.671% |
| DECAY 62.5% | 6.735% |
| DECAY 75.0% | 6.671% |
| DECAY 87.5% | 6.751% |
| DECAY 100.0% | 6.685% |
| CUTOFF 0.0% | 6.732% |
| CUTOFF 12.5% | 7.077% |
| CUTOFF 25.0% | 6.749% |
| CUTOFF 37.5% | 6.674% |
| CUTOFF 50.0% | 6.744% |
| CUTOFF 62.5% | 6.693% |
| CUTOFF 75.0% | 6.754% |
| CUTOFF 87.5% | 6.677% |
| CUTOFF 100.0% | 6.740% |
| RESO 0.0% | 6.679% |
| RESO 12.5% | 6.743% |
| RESO 25.0% | 6.680% |
| RESO 37.5% | 6.746% |
| RESO 50.0% | 6.683% |
| RESO 62.5% | 6.751% |
| RESO 75.0% | 6.734% |
| RESO 87.5% | 6.952% |
| RESO 100.0% | 7.262% |
| DRIVE 0.0% | 6.307% |
| DRIVE 12.5% | 6.693% |
| DRIVE 25.0% | 6.751% |
| DRIVE 37.5% | 6.683% |
| DRIVE 50.0% | 6.751% |
| DRIVE 62.5% | 6.682% |
| DRIVE 75.0% | 6.741% |
| DRIVE 87.5% | 6.679% |
| DRIVE 100.0% | 6.743% |
| OVERDRV 0.0% | 5.548% |
| OVERDRV 12.5% | 6.723% |
| OVERDRV 25.0% | 6.696% |
| OVERDRV 37.5% | 6.773% |
| OVERDRV 50.0% | 6.680% |
| OVERDRV 62.5% | 6.601% |
| OVERDRV 75.0% | 6.416% |
| OVERDRV 87.5% | 6.401% |
| OVERDRV 100.0% | 6.256% |
| ENV MOD 0.0% | 6.752% |
| ENV MOD 12.5% | 6.685% |
| ENV MOD 25.0% | 6.751% |
| ENV MOD 37.5% | 6.682% |
| ENV MOD 50.0% | 6.754% |
| ENV MOD 62.5% | 6.687% |
| ENV MOD 75.0% | 6.755% |
| ENV MOD 87.5% | 6.687% |
| ENV MOD 100.0% | 6.755% |
| WAV DIST 0.0% | 6.429% |
| WAV DIST 12.5% | 7.146% |
| WAV DIST 25.0% | 7.178% |
| WAV DIST 37.5% | 7.268% |
| WAV DIST 50.0% | 7.224% |
| WAV DIST 62.5% | 7.720% |
| WAV DIST 75.0% | 7.604% |
| WAV DIST 87.5% | 7.680% |
| WAV DIST 100.0% | 7.622% |
| TUNE 0.0% | 6.705% |
| TUNE 12.5% | 6.630% |
| TUNE 25.0% | 6.720% |
| TUNE 37.5% | 6.661% |
| TUNE 50.0% | 6.754% |
| TUNE 62.5% | 6.705% |
| TUNE 75.0% | 6.807% |
| TUNE 87.5% | 6.754% |
| TUNE 100.0% | 6.804% |
| combined (as read from the reports), differing between runs: DECAY 87.5%, CUTOFF 12.5%, RESO 100.0%, DRIVE 25.0%, OVERDRV 37.5%, ENV MOD 75.0%, WAV DIST 62.5%, TUNE 75.0%; DECAY 87.5%, CUTOFF 12.5%, RESO 100.0%, DRIVE 25.0%, OVERDRV 37.5%, ENV MOD 50.0%, WAV DIST 62.5%, TUNE 75.0%; DECAY 87.5%, CUTOFF 12.5%, RESO 100.0%, DRIVE 25.0%, OVERDRV 37.5%, ENV MOD 75.0%, WAV DIST 62.5%, TUNE 75.0% | 5.618% |

Recommended: at most 3.5%. **Warning: 7.720% of the block budget at WAV DIST 62.5% is above the recommended 3.5%; make it lighter where you can (docs/profiling.md).**

This is a synthetic measurement on the USB-MIDI command endpoint, not total device CPU, maximum polyphony or other combinations of control settings. Graphics are excluded. Callback tests exclude sample preparation, allocation, stock mixing and voice processing; they include stopwatch-call overhead and may include interrupt time. See docs/profiling.md for the versioned workloads.

Firmware SHA-256: `f84e2ad0b95da048176f39c10945aa50206bbcee2baf83881e46c62a5da45fa4`.

Compile report SHA-256: `48c76cd2b2b56d43f8ddb49aa4d76ff826de8a6cc4f7878a056c7483f21810f0`.

Attribution uses the user-confirmed installed package, expected case order and exact report count; the device does not attest its firmware hash. Raw captures remain private because they contain device identity.
