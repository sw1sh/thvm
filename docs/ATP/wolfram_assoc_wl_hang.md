# WolframAxioms/AndAssociativity WL path hang

## Symptom

Running `TFindProof["AndAssociativity", "WolframAxioms", Method ->
{...config...}, TimeConstraint -> 180]` from a `wolframscript`
kernel **hangs at 99% CPU** with no progress / no abort on
TimeConstraint expiry.  Tested with:

* `Method -> Automatic` -- timeout after 15s, 60s
* `Method -> "Waldmeister"` -- timeout after 15s, 60s
* `Method -> "WaldmeisterLazy"` -- timeout after 15s, 60s
* `Method -> {"Completion", "Ordering" -> "LPO", "SkolemHighest"
  -> True, "UnfailingCP" -> True, "CPSetInterreduce" -> True,
  "CriticalPairWeight" -> "Mix", "RecordNorm" -> False,
  "FifoTiebreak" -> True, "RHSInterreduce" -> True}` -- ran 88
  min at 99% CPU after TimeConstraint -> 180 without exiting.

`THVM_HEAP_CELLS=536870912` (4GB heap) → still aborts with
`heap_alloc: from-space exhausted; HEAP_NEXT=268435456,
cap=268435456` -- because cells split semi-space.
`THVM_HEAP_CELLS=1073741824` (1G cells / 8GB heap) → hangs
indefinitely at 99% CPU, no abort, no output, no completion.

## Context

Per `~/.claude/projects/-Users-swish-src-thvm/memory/project_atp_wm_sheffer_lpo.md`,
the C bench binary (`bin/test_atp_wolfram_bench andassoc 400000
200` with `THVM_ATP_WALDMEISTER=1`) DOES prove this theorem at
**6287 steps / 560 rules / 119s wall / 3GB peak RSS**.  So the C
saturation engine handles it; the WL `TFindProof` path does not.

Hypotheses (untested):

1. The WL-side `buildCplDataset` / `emitNorm` reconstruction
   path doesn't scale to 6287 saturation steps and hangs
   somewhere in the proof-walk DFS or the BFS bridge.
2. `RecordNorm -> False` (the fast-search path that bypasses
   per-step trace recording) may not be plumbed for all the
   configs above -- the engine still tries to record traces
   and OOMs at heap exhaustion.
3. `TimeConstraint` abort signals don't propagate into the
   C engine's tight inner loop when RecordNorm is False --
   so the timeout fires but the engine keeps running.

## Reaping orphan kernels

A side-effect of the hang: kernels spawned for probe runs do NOT
clean up when TimeConstraint expires.  After several probe
attempts I had 28 orphan `WolframKernel` processes consuming
40-130MB RSS each at 0-99% CPU, dating back 7+ hours.  Matching
`/tmp/probe_(sheffer|lazy)` in the wrapper command line + ETIME
> 1h filters them safely (the user's notebook + VsCodeWolfram +
WolframLLMUtilities kernels stay intact).

## Open work

1. Add a `TimeConstrained[...]` wrapper around the C-engine call
   in `TFindProof` that fires a hard abort on TimeConstraint
   expiry (vs the current cooperative check).
2. Investigate why the WL path doesn't complete what the C bench
   does in 119s.  Likely a bug in `buildCplDataset`'s trace walk
   for deep saturation runs OR a regression in `RecordNorm ->
   False` propagation.
3. Cap the heap aggressively for WL-path TFindProof to fail
   FAST when heap is exhausted, rather than running silent at
   99% CPU until the user kills the orphan.
