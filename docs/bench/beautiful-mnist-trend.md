# beautiful-mnist parity arc trend log

One line per parity-arc tick.  Format: ISO timestamp | commit short-hash | tick label | LeNet 4-step wallclock ms | LeNet final loss.

The LeNet 4-step run is the canary; beautiful-mnist's per-step time is
recorded once M1 lands and TConv2DIm2Col is the public path.

| timestamp | commit | tick | LeNet 4-step ms | LeNet final loss |
|-----------|--------|------|-----------------|------------------|
| 2026-04-30T14:31Z | c1a6dac | M1 step 1: TConv2DIm2Col landed + verified | ~3000 (TConv2D) | 0.3693 |
| 2026-04-30T14:45Z | c1a6dac | M1 step 2 bench: TConv2DIm2Col swap-in is 100x slower (PAD-and-sum explodes chain rule) | 341733 (TConv2DIm2Col) | 0.6789 |

## M1 outcome

TConv2DIm2Col stays as an opt-in alternate lowering -- not flipped
as the public `TConv2D` because the PAD-and-sum design causes a
100x end-to-end regression.  See plan doc for the tinygrad-style
view-only im2col redesign options (A/B/C) that would actually get
the speedup.
