# Heap discipline: SUB-bit substitution + Cheney GC (HVM4 alignment)

Status: **deferred**, not retired. Both fixes are nice-to-haves for
HVM4 alignment; neither is on the critical path for tinygrad parity
or LeNet/beautiful-mnist work.

## What this plan covers

Two HVM4-alignment fixes, ordered by isolation:

1. **Fix A: SUB-bit substitution** -- replace the `heap_replace`
   cascade in the *inspector* path (`redex_fire`) with HVM4-style
   O(1) cell-marking.  **Not a perf-cliff fix** (see "Scope
   correction" below).
2. **Fix B: Cheney-style copying GC** -- bound heap growth for
   long-running training loops.  Solves an OOM ceiling, not a
   wallclock cliff.

A is small and self-contained; B is the bigger lift.

## Scope correction (2026-05-01)

An earlier draft of this plan claimed the `heap_replace` cascade in
[src/wnf/redex.c:106-113](../../src/wnf/redex.c#L106-L113) was the
cause of the bound-w `sgd_loop` cubic cliff.  **That diagnosis was
wrong.**  The file's own header is explicit
([src/wnf/redex.c:1-2](../../src/wnf/redex.c#L1-L2)):

> wnf/redex.c -- redex enumeration + single-redex firing for the
> debugger / step interface.

And [src/wnf/redex.c:24-25](../../src/wnf/redex.c#L24-L25):

> Cost is O(HEAP_NEXT) per scan -- linear over the live heap.
> For the inspector workflow this is fine.

`redex_fire` is the path called by WL `TInteract` / `TRedexes` /
`TStep` for one-redex-at-a-time inspection.  Normal evaluation goes
through `wnf` in [src/wnf/_.c](../../src/wnf/_.c), which doesn't
cascade-replace -- it already substitutes via SUB-bit and walks the
chain via `term_resolve` on read.

So Fix A's actual value is:
- HVM4 alignment (one fewer architectural divergence).
- Faster step-by-step debugger / inspector workflow when the heap
  is large.
- Nice-to-have if/when we expose multi-step inspector loops to WL
  for long training-run debugging.

The bound-w cubic cliff (if it still reproduces) is somewhere else.
Likely candidates worth re-investigating before any cliff-chasing:
the per-iter ALO realize cost, the LAM bound-var shape table
allocation pattern, or the kernel program cache hit rate under
recursive bound-w.  Those audits are out of scope here.

---

## Fix A: SUB-bit substitution in the inspector path

### Current state

[src/wnf/redex.c:106-113](../../src/wnf/redex.c#L106-L113):

```c
static void heap_replace(Term old, Term new_) {
  if (old == new_) return;
  HOT_HEAP_REPLACE_CALLS++;
  HOT_HEAP_REPLACE_CELLS += HEAP_NEXT;
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    if (heap_read(i) == old) heap_set(i, new_);
  }
}
```

Called once per `redex_fire` at line 373.  Patches every cell
holding the old redex.  Cost: O(HEAP_NEXT) per inspector step.

### HVM4 reference

`/Users/swish/src/HVM4/clang/wnf/_.c:105-107`: read a cell, check
the SUB bit, follow the substituted pointer in O(1).  No heap scan.

### Approach

Replace the cascade with single-cell SUB-bit set on the redex's own
val.  Every reader path needs to go through `term_resolve` after
`heap_read` (most already do; a handful in `is_redex` and a few
interaction sites pattern-match on tag without resolving).

### Implementation

| File | Change |
|------|--------|
| [src/wnf/redex.c:106-113](../../src/wnf/redex.c#L106-L113) | Replace cascade with `heap_set(term_val(old), term_sub_set(result, 1))`. ~10 LOC. |
| [src/wnf/redex.c:27-99](../../src/wnf/redex.c#L27-L99) (`is_redex`) | Most child reads already go through `term_resolve`; double-check the few that don't. ~5-10 LOC. |
| `src/interact/app_bri.c, app_sup.c, dup_sup.c, uop_kernel.c` | Insert `term_resolve` on body reads that pattern-match on tag. ~15 LOC. |
| `src/schedule/materialize.c` | Insert `term_resolve` on TAG_TEN tag-checks at lines ~228, ~407. ~5 LOC. |
| [src/wnf/redex.c](../../src/wnf/redex.c) | Drop `HOT_HEAP_REPLACE_*` counters (no longer relevant). |

Total: ~50 LOC.

### Verification

1. `make wl` clean.
2. Full WL grid: 599/0.
3. Inspector workflow tests (`TInteract`, `TStep`, `TRedexes`)
   round-trip identically.
4. Optional: micro-bench `TInteract` on a heap with 100K cells;
   expect substantial speedup vs current cascade.

### Out of scope

- The eval `wnf` path is already SUB-bit-aware; no change needed.
- Threading / per-thread heap banks (HVM4 has them; thvm is
  single-threaded for now).

---

## Fix B: Cheney-style copying GC

### Current state

[src/heap/alloc.c:1-11](../../src/heap/alloc.c#L1-L11) is a pure
bump allocator: no deallocation, no compaction.  Per-iter cell
growth is linear; long training loops eventually exhaust the heap.

### HVM4 reference

`/Users/swish/src/HVM4/clang/heap/collect.c` (130 lines): two
semi-spaces, Cheney evacuate from a root set, forwarding pointers,
flip spaces.  Cost proportional to live set only.

### Approach

Mirror HVM4's collect.c.  Trigger from `wnf`'s entry block when
`HEAP_NEXT + GC_MARGIN >= HEAP_END`; bail to `wnf_rebuild`, run
`gc_collect()` from the realize loop, retry.

Roots:
- The current realize term (`thvm_realize`'s `res`).
- `EXTERN_PINS`'s pinned Terms ([src/schedule/extern_pin.c](../../src/schedule/extern_pin.c)).
- `DEFS` (book references).
- Every `KernelEntry`'s `output_tid` -> TenDesc -> producer chain;
  `input_terms` for symbolic inputs; `source_uop`.
- `BOUNDARY_TERM[]` if a materialize pass is in flight.

Side tables that hold heap_loc and need post-collect remap:
- [src/lam/shape.c](../../src/lam/shape.c) (re-key by lam_loc).
- `uop_const_cache` / `uop_mov_cache`: clear at GC time
  (rebuildable).
- `WNF_LAST_STACK[]`: pending bail frames; remap.

Side tables that don't move: book locs (permanent region).

### Implementation

| File | Change |
|------|--------|
| `src/heap/collect.c` (new) | Cheney evacuate + gc_collect.  ~150 LOC, structurally close to HVM4's `collect.c`. |
| [src/heap/alloc.c](../../src/heap/alloc.c) | Initialize `GC_BASE`, `GC_FROM_START`, `GC_TO_START`; alloc returns from current from-space. ~30 LOC. |
| [src/wnf/_.c](../../src/wnf/_.c) | Add GC_NEEDED check in `enter:` (mirror HVM4 line 82-90).  Bail to `wnf_rebuild`. ~20 LOC. |
| `src/schedule/realize.c` | Wrap the `nf(wnf(res))` + materialize loop with `if (GC_NEEDED) { res = gc_collect(res); }`. ~10 LOC. |
| [src/lam/shape.c](../../src/lam/shape.c) | Add `lam_shape_remap(old_loc, new_loc)` callback. ~20 LOC. |
| `src/uop/const.c, mov_cache.c` | Add reset hooks called from gc_collect. ~10 LOC each. |
| [src/thvm.c](../../src/thvm.c) | `thvm_init` calls `gc_init(space_words)` to set up semi-spaces. ~15 LOC. |

Total: ~250-300 LOC.

### Verification

1. `make wl` clean.
2. Full WL grid: 599/0 (profile.wlt baselines may need re-baselining
   if cell counts shift).
3. **New** test in `wl/THVMLink/Tests/heap_compact.wlt`:
   - Run 100 iterations of an ASSIGN-loop computation.
   - Assert `THeapPos[]` stays bounded (e.g. <2x the per-iter peak).
   - Assert `gc_collect_count > 0`.
4. n=10000 ASSIGN-loop completes without OOM.
5. linear-train + Newton + sgd_loop tests still pass.

### Rollback

Gate via `THVM_GC=0` env var.  Disabling restores the bump allocator.

### Out of scope

- Per-thread heap banks (parallel reduction; thvm is single-threaded).
- Generational GC (Cheney is enough for batch-style ML workloads).
- Refcount-based incremental GC (Cheney's amortized cost is fine).

---

## Sequencing

Both fixes are deferred behind higher-priority work (closing scalar
uops + fusion gaps; see [scalar_uops_lowering.md](scalar_uops_lowering.md)
and [beautiful_mnist_parity.md](beautiful_mnist_parity.md)).  When
picked up:

1. **Fix A first.**  Smaller, well-scoped, isolated to inspector
   path.  ~50 LOC.
2. **Fix B second.**  Bigger lift but mostly mechanical given HVM4
   as a reference.

## Critical files

- **HVM4 reference**: `/Users/swish/src/HVM4/clang/heap/collect.c`,
  `/Users/swish/src/HVM4/clang/wnf/_.c:105-107`,
  `/Users/swish/src/HVM4/clang/heap/subst_var.c`.
- **thvm cascade (inspector path)**: [src/wnf/redex.c:106-113, 373](../../src/wnf/redex.c#L106-L113).
- **thvm SUB infrastructure**: `src/term/resolve.c`,
  `src/heap/subst_var.c`, `term_sub_get/set` in [src/thvm.h](../../src/thvm.h).
- **thvm allocator**: [src/heap/alloc.c](../../src/heap/alloc.c),
  [src/thvm.c](../../src/thvm.c) `thvm_init`.
- **thvm side tables to remap**: [src/lam/shape.c](../../src/lam/shape.c),
  `src/uop/const.c`, `src/uop/mov_cache.c`,
  [src/schedule/extern_pin.c](../../src/schedule/extern_pin.c).
