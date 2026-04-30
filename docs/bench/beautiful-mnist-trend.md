# beautiful-mnist parity arc trend log

One line per parity-arc tick.  Format: ISO timestamp | commit short-hash | tick label | LeNet 4-step wallclock ms | LeNet final loss.

The LeNet 4-step run is the canary; beautiful-mnist's per-step time is
recorded once M1 lands and TConv2DIm2Col is the public path.

| timestamp | commit | tick | LeNet 4-step ms | LeNet final loss |
|-----------|--------|------|-----------------|------------------|
| 2026-04-30T14:31Z | (pre-commit) | M1 step 1: TConv2DIm2Col landed + verified | (canary green pre-tick) | 0.3693 |
