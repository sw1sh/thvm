# Benchmark History

This file is the consolidated benchmark log for the tinygrad-parity,
training, kernelization, Metal, and autotune arcs.

The old `phase6.md` through `phase16.md` files were useful while the
work was being done, but they mixed current measurements with stale
plans, false starts, and file-touch lists.  The original details remain
in git history.  This document keeps the measurements, decisions, and
open gaps that still help future work.

## Current Snapshot

Measured on Apple M3 Max unless noted.

### Validation

Current broad checks from the Metal tile/autotune arc:

- `make test`: passing, including `test_tile_graph` and
  `test_metal_real`.
- Full WL suite before the final Conv2D proposer cleanup:
  `669 passed, 0 failed`.
- Focused WL after the Conv2D proposer cleanup:
  `kernel_opts.wlt` `36 passed, 0 failed`,
  `metal_dtypes.wlt` `15 passed, 0 failed`.
- `git diff --check`: clean for the committed iterations.

### LeNet Training

LeNet/Adam CPU and Metal training now run end to end for the current
smoke size:

```text
N_STEPS=4 train.wls
losses: {2.6071, 1.8054, 1.1324, 0.6546, 0.3559}
```

The same sequence was observed for:

```bash
wolframscript -f wl/Examples/lenet-mnist/train.wls
THVM_BACKEND=metal THVM_TILE=1 N_STEPS=4 wolframscript -f wl/Examples/lenet-mnist/train.wls
```

The old `Conv2D + ReLU + MaxPool2d` weight-gradient shape bug is fixed
in the current tree; the minimal repro returns the weight shape
`{2, 1, 3, 3}`.

### beautiful_mnist Forward And Training Loop

The latest canary uses the tinygrad-style `beautiful_mnist` forward
example with diagnostic backend specializations disabled:

```bash
THVM_BACKEND=metal THVM_TILE=1 THVM_METAL_SPECIALIZED=0 \
  wolframscript -f wl/Examples/beautiful-mnist/forward.wls
```

Latest generic Metal tile numbers:

```text
sample walls: 36.0ms / 8.6ms / 9.0ms
```

With fire-time autotune forced and the autotune cache disabled:

```bash
THVM_BACKEND=metal THVM_TILE=1 THVM_METAL_SPECIALIZED=0 \
THVM_AUTOTUNE=1 THVM_AUTOTUNE_CACHE=0 \
  wolframscript -f wl/Examples/beautiful-mnist/forward.wls
```

Latest autotuned run:

```text
sample walls: 85.3ms / 5.4ms / 5.2ms
```

After adding the Conv2D `UPCAST`/`outputs_per_thread` schedule knob:

```text
generic tile, no autotune:                  75.2ms / 7.3ms / 7.6ms
fire-time autotune, cache disabled:        397.0ms / 6.5ms / 6.6ms
```

After adding opt-sequence search and gating generic Metal tile away
from PAD-heavy single-channel im2col Conv2D:

```text
generic tile, no autotune:                 107.1ms / 4.9ms / 4.1ms
fire-time autotune, cache disabled:        125.8ms / 5.8ms / 5.9ms
```

After dropping non-semantic tile JIT hash versioning and retiring the
diagnostic Metal GEMV shortcut:

```text
generic tile, no autotune:                  33.2ms / 8.6ms / 10.2ms
fire-time autotune, cache disabled:        137.2ms / 4.4ms / 4.9ms
autotune fresh tmp cache:                  118.6ms / 6.4ms / 5.7ms
autotune tmp cache replay:                 112.4ms / 4.5ms / 4.6ms
```

After adding the conservative Conv2D reduce-unroll candidate:

```text
generic tile, no autotune:                  95.0ms / 10.4ms / 6.6ms
fire-time autotune, cache disabled:        167.6ms / 5.4ms / 4.4ms
```

After switching focus from first-sample overhead to the full
beautiful_mnist loop, `bench-train.wls` now builds the actual
tinygrad-style architecture with rank-4 batches:

```bash
THVM_BACKEND=metal THVM_TILE=1 THVM_METAL_SPECIALIZED=0 \
BENCH_MODE=forward BS=512 WARMUP_STEPS=1 N_STEPS=3 \
  wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls
```

Current forward-only result after rank-4 Conv2D tile recognition and
single-channel patch-input tile support:

```text
BS=1 forward replay:      7.1ms / 6.9ms / 4.6ms
BS=16 forward replay:    67.6ms / 20.8ms / 16.0ms
BS=512 forward replay:  314.3ms / 298.8ms / 292.0ms
```

After broadening rangeify/Metal tile coverage for `PERMUTE` and
`S_RESHAPE_V`, and skipping host pre-materialization for tile kernels
that consume view indexing directly:

```text
BS=512 forward, no autotune:
  timed_ms={74.2, 73.5, 69.1, 63.0, 61.5, 56.0, 57.4, 51.4, 49.3, 49.1}
  steady_ms_per_step=60.5
  dispatch=metal-tile=36, none=3

BS=512 forward, post-autotuned:
  timed_ms={28.6, 29.7, 29.9, 29.9, 29.4, 29.0, 30.6, 30.5}
  steady_ms_per_step=29.7
  dispatch=metal-tile=36, none=3
```

Tinygrad comparison now uses the THVM-side harness
`tools/bench_tinygrad_beautiful_mnist.py`, which imports the sibling
checkout read-only and forces a device synchronize after each step so
the timed replay is not enqueue-only.

Safe beamed small-batch run:

```bash
PYTHONPATH=TinyHVM/tinygrad DEVICE=METAL BS=32 WARMUP_STEPS=2 \
N_STEPS=1 IGNORE_JIT_FIRST_BEAM=1 JITBEAM=1 DEBUG=0 \
  DUMP_IR=1 IR_TOP=10 \
  python3 tools/bench_tinygrad_beautiful_mnist.py
```

```text
BS=32 tinygrad train, JITBEAM=1:
  warmup no-jit:       2374.1ms, kernels=107
  warmup capture:      2051.1ms, kernels=3
  steady replay:          6.2ms, kernels=3
  captured_calls=3, captured_leaf_calls=110, graph_calls=3
  mem=9.9MiB
  graph leaf batches: 32, 64, 14
  repeated leaf shapes include E_8_8, E_2_8_4, E_2_8_4n1,
  E_16_4n12, E_32n1, E_32n2, E_4_8, E_2_2_8,
  plus grouped reductions such as r_64_36_32 and
  r_32_400_32n1.

BS=32 thvm train, THVM_TILE=1, before replay lifetime audit:
  warmup capture:      1949.0ms
  warmup replay:       1397.2ms
  steady replay:        465.6ms
  jit-ops=2243, JitReplayDispatches=2201
  dispatch=metal-tile=1247, none=35, metal-alias=117
  retained=743.1MB after timed replay
  captured replay: 2201 DISPATCH + 42 ASSIGN records
  dispatch routes: metal-tile=2084, metal-alias=117
  dispatch runs split by ASSIGNs: 42 runs
  largest runs: 138, 137, 125, 123, 121, 120 dispatches

BS=32 thvm train, THVM_TILE=1, after replay lifetime audit:
  warmup capture:      1941.7ms
  warmup replay:        752.8ms
  steady replay:        751.5ms
  jit-ops=2243, captured dispatches=2201, live replay dispatches=2067
  JitReplayDispatches=2067, JitReplayAssigns=42
  live=5.26GB, retained=6.32GB, freelist=1.06GB

BS=32 thvm train, THVM_TILE=1, THVM_METAL_GRAPH_REPLAY=1:
  warmup capture:      1903.5ms
  warmup replay:        728.2ms
  steady replay:        711.1ms
  live/replay dispatch counts match the non-graph audited run

BS=32 thvm train, after graph replay chunking, safe ASSIGN sinking,
and rootless replay slot packing:
  warmup capture:     12417.3ms
  steady replay:       179.4ms
  jit-ops=2243, live replay dispatches=2067
  JitReplayDispatches=2013, JitReplayAssigns=0
  JitGraphRuns=30, JitGraphDispatches=1969
  graph_run_count=18, graph_encoded_dispatches=2054
  replay_packed_dispatches=1098
  live=3.32GB, retained=3.58GB, freelist=258MB
  memory_plan peak=1.99GB, total=3.56GB
  captured replay: 2201 DISPATCH + 42 ASSIGN records,
                   176 skipped dispatch records
```

The replay lifetime audit found that the older `465.6ms` number was
not a trustworthy parity baseline: captured records could point at
Metal buffers freed by post-realize rollback, and the plain TJit
replay path counted backend dispatch failures as if they had run.  The
current bounded small-batch replay gap is roughly `29x` wall time
versus tinygrad's `6.2ms` beamed replay.  Metal ICB replay and replay
slot packing moved the gap down from the audited `~115x` baseline, but
they do not solve the larger problem: THVM still preserves gigabytes
of backward/update intermediates and encodes about two thousand leaf
dispatches across 18 graph chunks.

The current IR-level comparison:

- Tinygrad compiles the stable train step into three Metal graph
  submissions backed by an indirect-command-buffer replay.  The graph
  still contains about one hundred leaf kernels, but per-step host
  work is three graph launches.
- THVM captures the step as a flat sequence of per-kernel dispatches.
  Capture liveness skips dead dispatch records, safe rootless
  assignment copies are sunk into producer kernels, and replay slot
  packing reuses non-overlapping temporary buffers.  The live replay
  set is still about two thousand dispatches.
- The next Metal parity gap is therefore stronger fusion and fewer
  graph chunks over lowered primitives, not another one-off custom
  kernel.  The `TJitCaptureSummary`/`DUMP_JIT_CAPTURE=1` surface is
  the guardrail: the goal is to reduce graph launches, dispatch count,
  and multi-gigabyte live intermediates together.

Bounded BS=512 tinygrad run without JIT beam:

```bash
PYTHONPATH=TinyHVM/tinygrad DEVICE=METAL BS=512 WARMUP_STEPS=2 \
N_STEPS=1 IGNORE_JIT_FIRST_BEAM=1 JITBEAM=0 DEBUG=2 \
  python3 tools/bench_tinygrad_beautiful_mnist.py
```

```text
BS=512 tinygrad train, JITBEAM=0:
  warmup no-jit:       4221.0ms, kernels=119, gpu=88.0ms
  warmup capture:      1406.9ms, kernels=3,   gpu=74.0ms
  steady replay:         96.8ms, kernels=3,   gpu=92.5ms
  captured_calls=3, captured_leaf_calls=122, graph_calls=3
  graph batches: 32, 64, 26 leaf kernels
  mem=160.1MiB
```

The earlier `30-33ms` note should not be used as the live baseline
until it is reproduced through this synchronized harness.  In
particular, `TRAIN_BEAM=1` is not consumed by standalone
`examples/beautiful_mnist.py`; this harness maps it to tinygrad's
`JITBEAM` only as a convenience.  A BS=512 `JITBEAM=1` capture was
stopped after more than two minutes because Metal compiler callbacks
started timing out; RSS stayed modest, but it is not a safe default
profiling command.

Gradient status:

```text
BENCH_MODE=grad-w5 BS=1: capture 173.9ms, replay 13.6ms
BENCH_MODE=grad-14 BS=1: capture 110.1ms, replay 13.1ms
BENCH_MODE=grad-12 BS=1: capture 170.6ms, replay 14.4ms
BENCH_MODE=grad-9  BS=1: capture 224.8ms, replay 17.6ms
BENCH_MODE=grad-7  BS=1: capture 319.4ms, replay 163.3ms
BENCH_MODE=grad-7  BS=1 after Metal reshape/permute coverage:
  capture 241.4ms, replay 105.6ms, dispatch=metal-tile=82, none=2, metal-op=127
BENCH_MODE=grad-1  BS=1: still too slow to use as a loop canary
```

After adding generated Metal expression JIT for heavy f32
movement/ALU graphs and keeping tile kernels ahead of reduce-expression
JIT, the BS=512 steady replay status is:

```text
BENCH_MODE=forward BS=512:
  steady_ms_per_step=66.0
  dispatch=metal-tile=36, none=3

BENCH_MODE=grad-1 BS=512:
  steady_ms_per_step=4368.0
  dispatch=metal-tile=171, none=6, metal-jit=137, metal-op=3

BENCH_MODE=grad-3 BS=512:
  steady_ms_per_step=2578.1
  dispatch=metal-tile=170, none=6, metal-jit=73, metal-op=3

BENCH_MODE=grad-7 BS=512:
  steady_ms_per_step=126.9
  dispatch=metal-tile=73, none=5, metal-jit=25, metal-op=2

BENCH_MODE=grad-9 BS=512:
  steady_ms_per_step=186.1
  dispatch=metal-tile=72, none=4, metal-jit=9, metal-op=2

BENCH_MODE=train BS=512:
  steady_ms_per_step=41315.9
  dispatch=metal-tile=2138, none=51, metal-jit=1048, metal-op=48
```

Current bounded small-batch canary after Metal rollback, rangeify
reshape coverage, and alias-reshape replay refcount cleanup:

```text
BENCH_MODE=train BS=32:
  steady_ms_per_step=386.5
  jit-ops=4179
  dispatch=metal-tile=2928, none=51, metal-alias=318
  hot counters: JitReplayCalls=1, JitReplayDispatches=4137,
                JitReplayAssigns=42
  metal memory after timed: live=20.3MB, retained=851.4MB,
                            deferred=0
```

After adding contiguous private reduce-chain fusion, the same bounded
canary is:

```text
BENCH_MODE=train BS=32:
  steady_ms_per_step=500.4
  jit-ops=2243
  dispatch=metal-tile=1238, none=35, metal-alias=126
  hot counters: JitReplayCalls=1, JitReplayDispatches=2201,
                JitReplayAssigns=42
  metal memory after timed: live=20.2MB, retained=830.6MB,
                            deferred=0
```

This proves replay granularity is addressable, but it is not yet a
performance win.  The current policy fuses only direct/boundary
sources so movement-heavy reductions stay tile-lowered instead of
falling to `metal-jit`; the next useful optimization is to make those
flattened-coordinate movement/reduce programs tile-lower safely, then
re-open broader chain fusion.

After enabling Metal graph replay by default, sinking safe rootless
ASSIGN records into producer kernels, and packing rootless replay
temporaries by captured lifetime:

```text
BENCH_MODE=train BS=32:
  steady_ms_per_step=179.4
  jit-ops=2243
  dispatch=metal-tile=1220, none=35, metal-alias=144
  hot counters: JitReplayCalls=1, JitReplayDispatches=2013,
                JitReplayAssigns=0, JitGraphRuns=30,
                JitGraphDispatches=1969
  graph replay: 18 chunks, 2054 encoded dispatches
  replay_packed_dispatches=1098
  metal memory after timed: live=3.32GB, retained=3.58GB,
                            freelist=258MB
  memory plan: peak=1.99GB, total=3.56GB
```

This is the current small-batch training baseline for parity work.  It
is still far behind tinygrad's `~6.2ms` beamed BS=32 replay because
the lowered primitive graph is much less fused, but the bottleneck has
moved from broken replay ownership to visible fusion/memory-plan
quality.

This is a real improvement over the previous `grad-3` replay
(`~10.6s`) and removes the worst `PAD`/`SHRINK` materializer from
the profile, but it is still roughly `35x` behind tinygrad on the
bounded BS=32 beamed canary and roughly `400x` behind tinygrad's
measured BS=512 no-beam captured train step.  The current full-loop
gap is the repeated all-target early-conv backward graph: thousands
of kernels are emitted for one Adam replay, and the slowest kernels
are repeated movement-heavy partial-gradient materializers over
`{512,32,20,20}`/pool-shaped inputs.

Interpretation for the hackathon track:

- First-sample overhead is not the next gating metric.
- Rank-4 forward now works and the autotuned forward replay is in the
  same `~30ms` class as tinygrad's full captured train step.
- Target-pruned gradients make final-layer and late-block gradients
  replayable, but early conv-weight gradients still explode into many
  generic movement/reduction kernels.
- The next useful work is backward Conv/BN/pool tile recognition and
  fusion, plus row-wise/group reductions for the large batch-stat and
  flatten/linear movement kernels.

Interpretation:

- The old generic Metal tile path was roughly `64-69ms` steady-state
  for the second Conv2D.
- The diagnostic `THVM_METAL_SPECIALIZED=1` oracle was roughly
  `7-10ms`.
- The shared tile Conv2D template plus pre-materialization fix moved
  the generic path into the same class as the diagnostic oracle.
- The widened `LOCAL` proposer, `UPCAST` output-per-thread knob, and
  reduce-axis `UNROLL` knob give autotune real schedule choices.  The
  current tuned steady-state is in the `5-7ms` range depending on run
  noise and selected variants.
- Tile JIT cache keys now ignore `tile_axes_version`, an internal
  mutation counter, so autotune reset/apply cycles can reuse compiled
  kernels when the generated graph and applied opts are identical.
- The old opt-in direct Metal GEMV shader is retired; rank-1
  `TMatVec` stays on the shared `TILE_MMA`/`metal-gemm` route even
  when `THVM_METAL_SPECIALIZED=1` is enabled.
- The single-channel first Conv2D now uses a generated Metal tile
  patch-input template instead of the generic Metal fallback.
- This is not tinygrad parity yet.  The schedule space and backward
  graph lowering are still much narrower than tinygrad's search.

## Rerun Commands

Useful canaries:

```bash
make test
make wl-test

wolframscript -f wl/Examples/beautiful-mnist/forward.wls
THVM_BACKEND=metal THVM_TILE=1 THVM_METAL_SPECIALIZED=0 \
  wolframscript -f wl/Examples/beautiful-mnist/forward.wls
THVM_BACKEND=metal THVM_TILE=1 THVM_METAL_SPECIALIZED=0 \
THVM_AUTOTUNE=1 THVM_AUTOTUNE_CACHE=0 \
  wolframscript -f wl/Examples/beautiful-mnist/forward.wls
THVM_BACKEND=metal THVM_TILE=1 THVM_METAL_SPECIALIZED=0 \
BENCH_MODE=forward BS=512 WARMUP_STEPS=1 N_STEPS=3 \
  wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls

wolframscript -f wl/Examples/lenet-mnist/train.wls
THVM_BACKEND=metal THVM_TILE=1 N_STEPS=4 \
  wolframscript -f wl/Examples/lenet-mnist/train.wls

wolframscript -f wl/Examples/metal-gemm-autotune.wls
```

Bounded CPU LeNet autotune comparison:

```bash
TRAIN_BENCH_MODE=both N_STEPS=1 WARMUP_STEPS=1 MAX_TUNE_KERNELS=3 \
THVM_TILE=1 wolframscript -f wl/Examples/lenet-mnist/bench-train.wls
```

Observed in the Phase 16 arc:

```text
1543 live candidates -> 68 representative schedule keys
tuned 3 reps in 0.4ms
timed step: 33.6s baseline vs 29.2s autotune
```

That result is a harness smoke, not a strong speedup claim.  The
bounded sample was too small and no candidate clearly beat baseline.

## Open Performance Gaps

Current work should prioritize these in order:

1. Fuse or recompute the huge multi-consumer conv/backward
   `RESHAPE/PAD/ADD/EXPAND` producers.  The largest remaining live
   BS=32 Metal buffer is `1.31GB` and feeds 8 consumers.
2. Reduce graph replay chunk count from 18 toward tinygrad's 3 graph
   submissions, without regressing the bounded memory profile.
3. Lower row-wise/group reductions and softmax-like patterns as
   generated tile kernels instead of scalar reducer loops.
4. Broaden the generated Conv2D schedule beyond threadgroup size,
   output-per-thread, and reduce-unroll.  Next useful knobs are output
   axis mapping, cooperative reduction shape, local-memory staging, and
   vector width.
5. Reduce first-sample overhead for Metal once steady replay is in the
   right class.
6. Add Metal `simdgroup_matrix`/MMA variants after reductions and
   local-memory tiling are stable.
7. Keep improving multi-grad structural sharing.  It is not the top
   bottleneck in the latest LeNet measurements, but it will matter once
   larger training loops become kernel-dispatch bound.

## Timeline

### Phase 6: Initial beautiful_mnist Training Pipeline

The first BS=1 `beautiful_mnist` training example exercised:

- `TConv2D`, then lowered as `kh * kw` partial sums;
- `TReLU`, `TMaxPool2d`, reshape/flatten;
- `TMatVec` plus bias;
- sparse categorical cross entropy;
- `TGrad` and tensor-land Adam updates.

CPU forward plus loss was about `0.9s`, while individual gradients
through the conv and pool stack took minutes.  The important lesson was
that the forward path was usable, but the chain-rule expansion and
per-sample conv lowering made training impractical.

### Phase 7: TJit Capture/Replay

`TJit` landed as the thvm analogue of tinygrad `TinyJit`.

Measurements:

```text
64x64 matmul + bias, 50 eager iters: 440.7ms total, 8.8ms/iter
64x64 matmul + bias, 50 TJit iters: 0.5ms total

linear train step:
  step 1 capture: 2602.6ms
  step 2 replay: 0.2ms
  steps 3-5 replay: 0.1ms each
  captured ops: 114
```

This proved that scheduler/materialize overhead dominated small stable
loops, and capture/replay could skip it.

### Phase 8: Memory Plan Feedback

The memory planner recorded per-kernel allocation depth and last-use
depth, gated behind `THVM_REUSE_BUFS=1`.

Decision: keep it opt-in.  It was safe for forward-only flat graphs but
unsafe for chain-rule graphs because later WNF passes could still read a
buffer that the within-pass planner had already returned to the free
list.  End-of-realize rollback remained the safe default.

### Phase 9: Smarter Dispatch Planning

Four possible wins were analyzed:

1. Matmul plus bias fused dispatch.
2. Movement-prefix BLAS recognition for attention `K^T`.
3. Conv as im2col plus one GEMM.
4. Lifetime tracking strong enough to make `THVM_REUSE_BUFS` default.

Most code changes were deferred.  The useful correction was that
im2col did not require a new opcode; it can be expressed through
movement/view operations.  The hard part is producing a good lowered
shape that does not explode materialization or chain-rule work.

### Phase 10: BEAM Scaffold

`TBeamPick` landed as a WL-side version of candidate calibration and
winner replay.

Measurements:

```text
sum-of-squares BEAM replay 200x: 1.6ms
eager winner 200x: 32ms

4096-element reduction:
  calibration: mkA 414ms, mkB 743ms -> mkA wins
  replay 500x: beam 17.25ms, eager mkA 76.66ms, eager mkB 104.54ms
```

This was useful as a surface experiment, but the real system moved
toward C-side `TOpt` proposal and autotune instead of WL-only BEAM.

### Phase 11: GPT-2 Building Blocks

The GPT-2 layer surface landed in `NN.wl`: embeddings, GELU, causal
mask, layer norm affine, multi-head attention, linear, and ones.

Per-layer eager profile for a tiny config:

```text
TLayerNormAffine[{seq, dim}]:   2.16ms
TGELU[{seq, dim}]:            266.67ms
TMultiHeadAttention:            3.61ms
TLinear:                        0.87ms
TEmbeddingMatrix:               0.11ms
```

The major finding was a constant-argument kernel-program-cache miss:
numeric scalar ops such as `x + 1` or `1 + Exp[x]` produced fresh
constant tensors and re-JITed repeatedly.  That made tiny GPT-2
end-to-end forward cost about `100s`.

### Phase 12: TFromNet Surface

`TFromNet` learned enough NetGraph/NetChain structure to convert GPT-2
subgraphs.  The smoke compared against Wolfram reference evaluation:

```text
embedding subgraph: 31ms, shape {4, 768}, max abs diff 0
FFN subgraph: 4879ms, shape {4, 768}, max abs diff 2.35e-6
synthetic mini-GPT-2 single forward: 201927ms
```

The FFN was numerically correct but still much too slow because of the
Phase 11 constant-argument cache miss.

### Phase 13: Constant-Arg Fixes and Optimizer Surface

The frontend began pre-expanding scalar numeric operands so binop
kernels reused cache-friendly shapes.  Adam was reshaped around lazy
in-backend updates.

Headline numbers:

| Case | Before | After |
|------|--------|-------|
| `x + 1`, shape `{8,64}` | 60ms | 0.17ms |
| `TGELU[x]`, shape `{8,64}` | 270ms | 4ms |
| mini GPT-2 `forward.wls` | 100000ms | 694ms |
| `inference.wls` Part 2 single-token forward | 200000ms | 653ms |
| GPT-2 NetModel FFN block, dim 768 | 6000ms | 3500ms |

Later in the same arc, pre-transposing Wolfram `LinearLayer` weights
removed repeated materialization:

```text
linear1 768->3072: 191ms -> 0.35ms
linear2 3072->768: 191ms -> 0.35ms
one transformer FFN block: 3100ms -> 970ms
```

The 10-token synthetic generation loop showed TJit correctness:

```text
eager: 5181ms, 518ms/token
jit:   3904ms, 390ms/token
```

### Phase 14: Single-Realize Assign and Faster Grad Helpers

Nested `ASSIGN` terms began draining in one materialize/realize path,
so Adam could update `m`, `v`, and `w` in one realize per parameter.

The recursive WL helper for collecting UOP leaf tids was replaced by a
C walk:

```text
LeNet 8-weight forward graph:
  WL recursive walk:  ~1600ms/call
  C iterative walk:   ~1ms/call
  TGradMany:          13270ms -> 4ms
```

Synthetic 100-token GPT-2 generation:

```text
eager: 53916ms, 539.2ms/token
jit:   40396ms, 404.0ms/token
captured ops/step: 30472
```

The default heap was also raised to avoid LeNet training exhaustion.

### Phase 15: WNF-Only Realize

`nf` left the hot path.  `wnf` became the only reducer in the realize
loop, with `TAG_F_UOP_CHILD` frames to descend into active UOP
children.

The cubic heap replacement cliff disappeared:

| Case | Before wall | Before heap_replace cells | After wall | After cells |
|------|-------------|---------------------------|------------|-------------|
| MLP step 1 grad 1 | 160ms | 533108986 | 6.4ms | 0 |
| MLP step 2 grad 1 | 1108ms | 3761561858 | 2.3ms | 0 |
| MLP step 3 grad 1 | 1970ms | 6989896723 | 2.3ms | 0 |

After this change, `GradFires`, `WnfCalls`, and `MaterializeCalls`
were flat per step instead of growing with heap size.

### Phase 16: Kernel Opts, Autotune, Tile, and Metal

The kernel optimization stack moved into C:

```text
TKernelProposed[kid]       -> kernel_opts_propose
TKernelAutotune[kid]       -> benchmark candidates/sequences and apply winner
TKernelAutotuneUnique[]    -> tune one representative per schedule key
TKernelVariants[kid]       -> inspect baseline/candidate timings
THVM_AUTOTUNE=1            -> fire-time autotune trigger
THVM_AUTOTUNE_DEPTH=N      -> maximum opt-sequence depth
THVM_AUTOTUNE_BEAM=N       -> sequence expansion width
```

Implemented opt classes:

| Opt | Current role |
|-----|--------------|
| `UNROLL` | reduce-tail C/JIT unroll hint |
| `UPCAST` | elementwise output-loop unroll hint |
| `LOCAL` | Metal tile local thread binding |
| `GLOBAL` | Metal tile grid binding paired with `LOCAL` |
| `GROUP` / `GROUPTOP` | reduce-axis group metadata |
| `TC` | Metal GEMM tile-size metadata for `TILE_MMA` |
| `SWAP`, `PADTO`, `NOLOCALS` | reserved or not yet consumed |

Metal improvements in this arc:

- `TILE_MMA` recognition for f32 matmul and rank-1 matvec.
- Direct `metal-gemm` route fed by validated tile metadata.
- `TC` candidates for 32/16/8 tiled GEMM variants.
- Generated Metal tile route for f32 elementwise and reduction plans.
- Scalar UOp structural CSE before tile planning.
- Scalar reduce axes appended into `KernelAxes` so `GROUP` candidates
  become reachable.
- Shared `tile_analyze_conv2d_flat` template for im2col-fused Conv2D
  reduce graphs.
- Generated runtime-configured Metal Conv2D tile kernel.
- Conv2D tile dispatch reads original strided views directly instead
  of pre-materializing non-contiguous inputs.
- Conv2D `LOCAL` proposer scans every splittable loop axis.
- Conv2D `UPCAST` proposer exposes an output-per-thread knob for the
  generated Metal tile kernel.
- Conv2D `UNROLL` proposer exposes a reduce-axis unroll knob for the
  generated Metal tile kernel.
- Autotune can expand the best single candidates into short opt
  sequences, so Conv2D search can test combinations such as
  `UPCAST + LOCAL/GLOBAL`.
- Generic Metal tile declines PAD-heavy KProg graphs for now; this
  preserves correctness for single-channel im2col Conv2D while the
  direct template and generic movement renderer mature.

Important canary progression:

| State | beautiful_mnist forward sample walls |
|-------|--------------------------------------|
| Generic Metal tile, no autotune, early Phase 16 | `144.4ms / 66.9ms / 66.0ms` |
| Diagnostic specialized oracle | `58.6ms / 8.2ms / 10.4ms` |
| After scalar-UOp CSE | `146.2ms / 69.2ms / 66.3ms` |
| After reduce-axis/GROUP plumbing | `95.0ms / 63.3ms / 67.7ms` |
| Generated Conv2D tile before pre-materialization fix | `125.2ms / 65.1ms / 66.3ms` |
| Generated Conv2D tile after fix | `81.4ms / 9.8ms / 11.1ms` |
| Post-regression generic tile rerun | `36.0ms / 8.6ms / 9.0ms` |
| Fire-time autotune, cache disabled | `85.3ms / 5.4ms / 5.2ms` |
| After output-per-thread knob, no autotune | `75.2ms / 7.3ms / 7.6ms` |
| After output-per-thread knob, fire-time autotune | `397.0ms / 6.5ms / 6.6ms` |
| After sequence search + PAD fallback, no autotune | `107.1ms / 4.9ms / 4.1ms` |
| After sequence search + PAD fallback, fire-time autotune | `125.8ms / 5.8ms / 5.9ms` |

Scalar CSE helped compile input size but did not close the throughput
gap by itself:

```text
first conv reduce graph:  1615 -> 590 scalar nodes
second conv reduce graph: 1415 -> 563 scalar nodes
```

The throughput step change came from moving the Conv2D pattern into the
shared tile path and avoiding the incorrect pre-materialized view path.

## Historical beautiful_mnist Trend

The old trend file tracked an early failed attempt to flip
`TConv2DIm2Col` into the public path:

| timestamp | commit | tick | LeNet 4-step ms | final loss |
|-----------|--------|------|-----------------|------------|
| 2026-04-30T14:31Z | `c1a6dac` | `TConv2DIm2Col` landed + verified | ~3000 | 0.3693 |
| 2026-04-30T14:45Z | `c1a6dac` | im2col swap-in regressed | 341733 | 0.6789 |

Outcome: the old PAD-and-sum im2col lowering stayed opt-in because it
caused a roughly 100x end-to-end regression.  The later generated
Metal tile Conv2D template is the replacement direction.
