# Phase 15 -- wnf-only realize: nf out of the hot path, cubic cliff dead

The architectural shift the user pushed for: `wnf` becomes the
*only* reducer in the realize loop.  `nf` is demoted to its
file-header-original role -- inspector / step-debugger only.
materialize stops firing interactions; it's strictly graph -> kernel
compile.  The substitution-cascade-driven cubic cliff in per-step
grad disappears entirely (heap_replace cells per step: 533M -> 0).

## What lands

### 1. `nf` removed from `realize.c`

Old realize loop:
```c
res = nf(wnf(res));        // nf re-fires every redex via redex_fire's
                           // O(HEAP_NEXT) heap_replace cascade
```
New realize loop:
```c
res = wnf(res);            // wnf alone -- local SUB-bit substitution
                           // (heap_subst_var), no global cascade
```

`wnf/nf.c` and `wnf/redex.c` (the inspector primitives) are
untouched and still callable from WL via `TNf[]` / `TRedexes[]` /
`TInteract[]`.

### 2. Inline `interact_grad` removed from `materialize::visit`

Materialize never fires interactions.  The DP1_GRAD bail in
`schedule/materialize.c:608-621` is gone.  If a DP1_GRAD slips
through to `visit`, it falls through to `VISIT_BAIL` and the
realize loop iterates -- wnf fires it on the next pass.

### 3. New `TAG_F_UOP_CHILD` frame in wnf

The single-page mechanism that makes wnf-alone sufficient.  When
wnf reaches a TAG_UOP that has an active descendant (chain-rule
DP1_GRAD, DUP-projection from TGrad's leafTids nest, nested
UOP_ASSIGN, nested UOP_KERNEL), it pushes an `F_UOP_CHILD` frame
recording `(uop_loc, child_idx, parent_op)` and descends into the
active child.  Apply pops the frame, heap_sets the WHNF result
back in place, and:
- recursively re-drives the WHNF if it's itself a UOP-with-actives
  (e.g. DUP-UOP commute returns a UOP with fresh DUPs around its
  children) -- via reentrant wnf with synced `WNF_S_POS`,
- scans for the next active sibling and pushes a new F_UOP_CHILD
  frame for it,
- when no actives remain, the parent UOP is WHNF.

Active-detection is transitive (`uop_has_active_descendant_memo`
in `interact/uop_grad.c`): a passive UOP whose subgraph contains
any DP/KERNEL/ASSIGN counts as active so the descent finds buried
chain-rule grads through MUL/ADD/etc. spines.  Per-call gen-keyed
memo prevents exponential walks on shared sub-DAGs (the common
case in autodiff -- e.g. Adam's `gT` referenced from both `0.1*gT`
and `gT*gT`).

### 4. WNF_S_POS sync at every reentrant wnf call

The pre-existing reentrant `wnf(src)` / `wnf(dst)` calls in the
ASSIGN handler used to be safe because the outer wnf had no
pending stack frames at that point (ASSIGN was always at the
head).  With F_UOP_CHILD frames now possibly pushed by ancestors
before ASSIGN is reached, the outer's local `s_pos` had advanced
past the global `WNF_S_POS`; the inner wnf snapshotted the stale
value and pushed frames at positions that clobbered the outer's
frames.

Fix is mechanical: sync `WNF_S_POS = s_pos` before each reentrant
call, restore `s_pos = WNF_S_POS` on return.  Applied at:
- `wnf/_.c:160-167` -- ASSIGN's src/dst force.
- `wnf/_.c:560-563` -- F_UOP_CHILD apply's whnf re-drive.

## HotCounters tooling (also part of this arc)

Lands the per-context `HotCounters` block + WL surface
(`THotCounters` / `THotCountersReset` / `THotCountersDelta[label,
body]` / `THotCountersReport[ds]`) so any future perf claim can be
verified from a 30-line `.wls` script -- no rebuild, no printf
session.  Order in `$THotCounterNames` matches
`hot_counters_snapshot` in `src/instrument/hot_counters.c` -- keep
in sync when adding counters.

Counters captured: heap_replace calls/cells (the cubic-cliff
integral), is_redex calls, redex_enumerate calls/cells, wnf calls,
realize calls, materialize calls, kernel/grad fires.

## Cubic cliff: dead

3-layer MLP, three "training steps" (loss + g1/g2/g3 per step):

```
                BEFORE (with nf)             AFTER (wnf-only)
                WallMs   HeapReplaceCells    WallMs   HeapReplaceCells
s1.g1            160     533,108,986          6.4     0
s2.g1           1108   3,761,561,858          2.3     0
s3.g1           1970   6,989,896,723          2.3     0
```

`heap_replace` is never called from the realize hot path.  Per-step
time is FLAT instead of growing linearly.  ~700x speedup on g1 by
step 3, and the ratio gets worse (better) for longer training
runs.  `GradFires` per step constant (22), `WnfCalls` constant
(58), `MaterializeCalls` constant (3) -- realize is now genuinely
O(forward graph size) per step, not O(N x HEAP_NEXT).

## Files touched

- `src/schedule/realize.c` -- `nf(wnf(res))` -> `wnf(res)`.
- `src/schedule/materialize.c` -- inline `interact_grad` removed
  from `visit`; materialize is reduction-free.
- `src/wnf/_.c` -- TAG_UOP enter pushes F_UOP_CHILD; new
  TAG_F_UOP_CHILD apply case re-drives + scans next sibling;
  WNF_S_POS sync at ASSIGN handler's reentrant wnf calls.
- `src/interact/uop_grad.c` -- `uop_child_is_active` /
  `uop_has_active_descendant_memo` / `uop_next_active_child`
  scan helpers (per-call gen-keyed memo for sub-DAG sharing).
- `src/thvm.h` -- `TAG_F_UOP_CHILD = 28`, `TAG_COUNT = 29`;
  `HotCounters` struct + `HOT_*` macros; forward decls for
  `interact_dup_*` / `interact_kernel`.
- `src/instrument/hot_counters.c` (new) -- snapshot/reset helpers
  on `CURRENT_CTX->hot`.
- `wl/THVMLink/CSource/thvmlink.c` -- `thvm_wl_hot_counters` +
  `thvm_wl_hot_counters_reset` LibraryLink wrappers.
- `wl/THVMLink/Kernel/Profile.wl` -- `THotCounters[]`,
  `THotCountersReset[]`, `THotCountersDelta[label, body]`,
  `THotCountersReport[ds]`, `$THotCounterNames`.
- `wl/THVMLink/Kernel/THVMLink.wl` -- load $hotCountersFn /
  $hotCountersResetFn.
- `wl/THVMLink/Kernel/README.md` -- Profile.wl row updated.

WL grid: 393/393.

## Why it took so long

Three false starts before landing:
1. **SUB-bit substitution in `redex_fire`** (HVM4-style, the
   `magical-wondering-biscuit.md` Fix A) -- user rejected: `nf` is
   debugger-only by file-header design, fix the architecture not
   the cascade primitive.
2. **Hacky direct-dispatch descent (`drive_to_whnf` etc.)** --
   reimplemented wnf badly with C-recursion and segfaulted on
   shared WNF_STACK.  User: "this all looks super hacky, why not
   the stack machine with frames?"
3. **F_UOP_CHILD frame v1** -- right idea, but the F_UOP_CHILD
   apply didn't recursively re-drive the WHNF after firing, AND
   the existing ASSIGN-handler's reentrant wnf clobbered F_UOP_CHILD
   ancestor frames via stale `WNF_S_POS`.  Both fixes lifted the
   approach to 393/393.

The user's "no fucking bailing out and reverting" was the right
call -- the third iteration finally found the WNF_S_POS sync that
made everything work.

## Next

- LeNet 4-step convergence target (Phase 14 deferral): now
  unblocked by per-step perf.  Re-bench.
- `nf` is now strictly inspector code; its `redex_fire` cascade
  cost is fine (it's only called via WL `TNf[]` for one-off step
  debugging).  Could optionally retire `redex_fire`'s cascade in
  favor of SUB-bit too, but no bench delta -- that's polish, not
  perf.
