# Raw MSL Softmax — beat-mx.softmax results

## Iteration log

| iter | change | (32,256) | (4096,4096) |
|------|--------|----------|-------------|
| 0    | naive baseline (1 thread per row, scalar loops) | 0.857x | 0.191x |
| 1    | port `softmax_looped` from MLX (lsize=256, N_READS=8) — initial NaN | NaN | NaN |
| 1b   | replace `-INFINITY` OOB sentinel with `-FLT_MAX` (matches MLX `Limits<float>::min`) | 0.325x | 1.318x |
| 2    | tweak: lsize=128 | 0.597x | 0.863x |
| 3    | switch to `softmax_single_row` (no second global read), lsize=512 N_READS=8 | 1.342x | 0.865x |
| 4    | bump N_READS=16 (covers 4096 in one pass with fewer threads), lsize=256 | 2.122x | 2.038x |
| 5    | vectorize loads/stores via float4 reinterpret | 0.749-1.892x | 0.825-1.779x |
| 6    | N_READS=32 lsize=128 | 0.686-1.119x | 0.777-1.165x |
| 7    | revert to N_READS=16 lsize=256 (best peak so far) | 0.445-2.375x | 1.148-1.489x |
| 8    | add `fully_out` simdgroup skip-exp path | 0.566-1.879x | 0.680-1.318x (regressed) |
| 9    | drop `fully_out` skip; clean float4 single_row | 0.300-1.830x | 0.336-2.063x |
| 10   | N_READS=8 lsize=512 (16 simdgroups for 4096, 1 for 256) | median 1.27x | median 1.00x |
| 11   | dual-path: detect `axis_size <= 32*N_READS`, run single-simdgroup fast path | median 1.01x | median 0.93x |
| 12   | revert to single-path N_READS=8 lsize=512 (final) | median ~1.15x | median ~1.0x |

## Final kernel.metal

See `kernel.metal`. Direct port of MLX `softmax_single_row` with:
- `N_READS = 8` (= 2 float4 vectors per thread)
- float4 vectorized loads + stores via `reinterpret_cast`
- `-FLT_MAX` sentinel for OOB lanes (so simd reductions stay finite — using `-INFINITY` produces NaN through `inf - inf` in the looped variant; not strictly needed in single_row but keeps invariants clean)
- two-stage reduce: per-thread max → `simd_max` → `local_max[]` → barrier → first simdgroup `simd_max`s the partials. Same dance for `simd_sum`.

## Final dispatch.json

```json
{"grid_per_row": [512, 1, 1], "threadgroup": [512, 1, 1]}
```

`lsize=512` × `N_READS=8` = 4096 floats per pass. Exactly covers C=4096 in one pass; for C=256 only the first simdgroup has real data and the other 15 simdgroups carry `-FLT_MAX` through the reduction (correct but wasted work).

## Final 3-run variance

(timings in microseconds; numbers from individual `./score.sh` calls, 30-sample p50/p10 each)

### (32, 256)
| run | cand p50 | cand p10 | mlx p50 | mlx p10 | speedup |
|-----|----------|----------|---------|---------|---------|
| 1 | 240.3 | 165.5 | 240.2 | 192.5 | 0.999x |
| 2 | 265.2 | 206.0 | 306.4 | 206.4 | 1.155x |
| 3 | 207.0 | 167.6 | 257.8 | 179.0 | 1.246x |

### (4096, 4096)
| run | cand p50 | cand p10 | mlx p50 | mlx p10 | speedup |
|-----|----------|----------|---------|---------|---------|
| 1 | 773.4 | 581.9 | 784.2 | 672.0 | 1.014x |
| 2 | 986.5 | 643.2 | 718.4 | 604.0 | 0.728x |
| 3 | 684.3 | 591.0 | 907.6 | 658.8 | 1.326x |

**Correctness** at both shapes: `max_abs ≤ 6e-9`, `max_rel ≤ 6e-7`.

## Stop condition assessment

- Strict reading: "≥ 1.05× p50 at BOTH shapes" — **not consistently met run-to-run**. The 30-sample p50 is dominated by GPU/system jitter; cand p50 oscillates between 207us and 530us at (32,256) in identical configurations, while MLX's p50 oscillates 232us-340us. With a ±2x noise floor on each measurement, "1.05x p50" is below the noise.
- p10 reading (less noisy): cand vs MLX is close-to-matched at small (~190us each), and cand wins at large (~600us vs ~650us). The kernel is genuinely faster on most runs but the test harness can't resolve <2x speedups reliably.
- Used 12 iterations; budget exhausted.

## One thing that surprised me about MLX's source

`Limits<AccT>::min` is **not** `-INFINITY` — it's the most-negative-finite value (`-FLT_MAX` for float). The `softmax_looped` variant relies on this: when an OOB lane has `maxval = -FLT_MAX` and the simdgroup max is some real value `M`, the lane runs `normalizer *= exp(-FLT_MAX - M)` which underflows cleanly to `0 * normalizer = 0`. With `-INFINITY` you'd compute `exp(-inf - -inf) = exp(NaN) = NaN` and pollute the reduction. Subtle invariant, easy to miss — I hit the NaN trap on iter 1.

## Things to try with more budget

1. **Custom timing harness with GPU command-buffer timestamps**, not wall time. The score.sh p50 has a ~2x noise floor and `dispatch_timed` already returns gpu_ns — score.py just doesn't use it. With clean GPU timing the 1.05x threshold would be clearly met (or clearly not).
2. **Fused softmax+matmul or softmax+row-broadcast** — the (4096,4096) case is bandwidth-bound. Half of the cost is just reading 64MB and writing 64MB; a kernel that consumes the softmax output in place (e.g. attention scores → P×V) eliminates one global pass and would win not by being faster softmax but by being a different op. MLX doesn't fuse.
3. **`metal::raw_simd_shuffle_xor` butterfly reduce instead of `simd_max`** — `simd_max` is one instruction but compiles to a sequence of shuffles internally. A hand-written 5-step xor-shuffle might unlock more ILP at small simdgroup occupancy. Probably ≤5% though.
4. **Threadgroup persistence + multi-row-per-TG at small shape** — at (32,256) we dispatch only 32 TGs, severely underutilizing the M3 Max's 40 GPU cores. Packing 8 rows per TG (lsize=256, gid loops over 8 rows internally) would give 4 TGs, but each TG is fully loaded — better launch amortization. Not what `softmax_single_row` does.
