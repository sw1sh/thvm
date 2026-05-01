# apply_movement_op port: progress + next steps

## What landed (this session, ~14 commits)

Backward-walk infrastructure mirroring tinygrad's `apply_movement_op`
([indexing.py:128-145](../../../tinygrad/tinygrad/schedule/indexing.py#L128-L145)):

- `RngsCtx` per-prog_value tracking (per-axis iter expressions
  + `valid_mask` side channel for PAD bounds).
- All 7 movement-op transforms correctly implemented in the backward
  pass: REDUCE (with axis insertion at the right position via
  `reduce_inner` walk), SHRINK (S_IADD), FLIP (S_ISUB from extent-1),
  EXPAND (ICONST(0) for broadcast axes), PAD (S_ISUB shift +
  S_IAND'd bounds-check into `valid_mask`), RESHAPE (flat-roundtrip
  via S_IDIV/S_IMOD), PERMUTE (axis reorder).
- Per-input-slot `input_rngs_pre/post` captured during backward walk.

Forward-walk consumers wired:

- pre-INDEX rngs fallback: when no per-pattern shape branch matches,
  build the load address symbolically from rngs via `S_INDEX_E` +
  optional `S_IWHERE` wrap on `valid_mask`.
- UOP_RESHAPE identity-pass: when source `via_rngs`, the chain's
  RESHAPE transform was already baked into the load → propagate
  source value unchanged.
- UOP_SHRINK identity-pass: same logic for SHRINK.
- ALU `all_via_rngs` propagation: chain marker flows through ADD/MUL
  but only when ALL sources are via_rngs (mixed-source ops would mix
  iter coords).
- post-INDEX shape-mismatch bail dropped: emit with view strides
  (works for SHRINK-alias / RESHAPE'd views).

Other:
- New scalar IR primitives: S_ICONST, S_IADD, S_ISUB, S_IMUL,
  S_IDIV, S_IMOD, S_ILT, S_IAND, S_IWHERE, S_INDEX_E (commit
  e271003).
- Identity-mod-size-1-axes RESHAPE fast path (commit 16ebcaa).
- S_RESHAPE_V split-src form for rank-mismatch RESHAPE (commit
  67495cf, refined in 64ec316/199b326).

Bail-count delta on conv_im2col + bn_grad + grad:
- Session start: 12 mid-emit bails
- Session end: 11 mid-emit bails
- (broader baseline pre-session: 56 → 11 = 80% reduction)
Grid: 621 passed, 0 failed (excl beautiful_mnist + pending_*).

## What's blocked: PAD identity-pass

The remaining 9 SHRINK/PAD ndim>3 bails (all PAD on conv-backward
chains) would close if forward-walk could identity-pass UOP_PAD when
source is via_rngs.  Multiple attempts all break
`nn/lenet-end-to-end-forward-shape-and-softmax-sum`:

| restriction | bails | tests fail |
|---|---:|---:|
| no restriction | 4 | 1 (lenet) |
| `+ input_n_uses[i] == 1` | 4 | 1 (lenet) |
| `+ ndim > os->ndim` | 4 | 1 (lenet) |
| `+ valid_mask != 0` | 4 | 2 (lenet + others) |

**Diagnosis**: the IMAGE input in lenet's chain gets `via_rngs_pre[i]
= 1` because rngs.ndim matches v.shape.ndim, but no PAD upstream
means no `valid_mask`, so no `S_IWHERE` wrap.  The bare `S_LOAD`
then computes addresses via `S_INDEX_E(rngs * strides)` where the
rngs were correctly computed for the WEIGHT TILE chain interpretation
but not for the IMAGE chain.  Result: OOB loads (off=784 vs
in_numel=576), garbage values, NaN propagation through the softmax.

Concrete OOBs traced (commit ee2ee72 + later attempts):
- op_id=600 slot=1 off=784 in_numel=576
- op_id=613 slot=2 off=784 in_numel=576
- ... 25 inputs total

The OOB happens BECAUSE the rngs flow assumes one chain interpretation
across multiple inputs that have DIFFERENT chain structures.  The
backward walk's "first writer wins" captured one consumer's
view of rngs; other consumers see those rngs as if they were their
own — wrong for the image which doesn't have the PAD-shifted
addressing pattern.

## What's needed to unblock PAD identity-pass

Per-USE rngs instead of per-slot.  Each consumer of an input gets its
own `RngsCtx` reflecting that consumer's specific chain transforms.
The input_load is emitted per-USE (one `S_INDEX_E` + optional
`S_IWHERE` per consumer) rather than once-per-slot.

This requires restructuring the materializer to:
1. Re-emit input loads at consumer-time (currently hoisted up-front).
2. Track rngs per (slot, consumer) tuple, not per-slot.
3. Handle the dispatcher's input_load_pre/post arrays differently
   (since they were per-slot caches).

Estimated ~200-300 LOC rework across rangeify.c.  Doable but didn't
fit in this session.

## Cumulative progress (session arc: 56 -> 16 bails, 71% reduction)

```
13 SHRINK/PAD ndim > os->ndim (rank-promoted intermediate)
 2 RESHAPE shape-change ndim cap or != os->ndim
 1 pre-INDEX shape mismatch (non-broadcast)
```

Commits driving the recent reductions (newest first):

- `d65e4a6` PAD fusion accepts S_IWHERE-wrapped LOAD + S_INDEX_E
  expression-walk for reduce_range independence (infrastructure;
  no current bail closes since all current SHRINK/PAD bails have
  loads that genuinely depend on reduce_range).
- `5b73a73` narrow PAD-as-size1-inflation scaffold for source==S_LOAD.
- `2e4bbc1` PAD-fusion classifier extends to loop_ranges for non-
  reduce axes (closed [1,9,4] case).
- `2c84b06` PAD fusion via S_RESHAPE_V routes new axis through
  reduce_range when out_dim==reduce_size (closed 6 conv-chain bails).
- `08c431e` clarify pre-INDEX rngs gate + diagnostic (no count
  change; doc only).
- `b170c08` pre-INDEX rngs fallback for same-rank shape mismatch
  (extent-gated; no current bail closes but infrastructure ready).
- `a03dd6e` post-INDEX rngs-based fallback via S_INDEX_E (closed 2
  fusion_count.wlt broadcast-unmatched bails + 1 transitive).
- `3586273` bump SHRINK/PAD ndim cap 3->4 + rename rank-promoted
  bail msg.
- `0beada6` RESHAPE-V matches reduce_range when no os axis matches
  (closed 4 RESHAPE bails + 4 transitive SHRINK/PAD).
- `db88cbc` and earlier: backward walk + per-input rngs + first
  RESHAPE-V emission + REDUCE axis fix (28 bails closed earlier
  in session).

### What's left and why

**13 SHRINK/PAD ndim > os->ndim (rank-promoted intermediate)**: All
have `src.op = S_IWHERE` wrapping S_LOAD where the LOAD's S_INDEX_E
address provably uses reduce_range (the chain is reading reduce-
axis-varying input data that PAD relayouts).  My narrow fusion at
`d65e4a6` correctly identifies these as not-fuseable: bounds-AND'ing
without addressing remap would gate the same data, not reposition.
Closing requires per-USE rngs to remap addressing.

**RESHAPE rank-mismatch on chain body**:
- `out=[1] src=[2] os=[2,4]` (collapse with rank mismatch)
- `out=[4] src=[2,2] os=[4]` (flat-decompose with non-input source)

The `S_RESHAPE_V` path requires `KSRC_IS_INPUT(raw)` because the
emit synthesizes a fresh DEFINE_PARAM + INDEX + LOAD chain.
Wrapping a non-input chain body needs an iter-shape transform
(flat-roundtrip into the body's own iter shape).

**pre-INDEX shape mismatch (non-broadcast)** (1 bail in nn.wlt
attention test path): `v.shape=[2,3] strides=[3,1] os=[2,2]
rngs=[S_IDIV, S_IMOD]`.  The chain rngs are S_IDIV/S_IMOD
expressions encoding a RESHAPE flat-roundtrip's INTENT, not the
addressing the materializer wants.  The bail is load-bearing:
forces fallback to a different lowering path that addresses
correctly.  Verified by `08c431e`'s diagnostic + the relaxed-gate
attempt that broke `nn/attention-identity-q-row-selection`.

## Reference

- tinygrad: `apply_movement_op` and `_apply_reshape` in
  `/Users/swish/src/tinygrad/tinygrad/schedule/indexing.py:113-145`.
- thvm IR: ScalarOp enum + S_I* family in `src/thvm.h:520-630`.
- Backward walk: `src/schedule/rangeify.c:609-720` (rngs[] +
  per-input-slot capture).
- pre-INDEX rngs path: `src/schedule/rangeify.c:984-1030`.
- Forward identity-pass: `src/schedule/rangeify.c:1186-1198`
  (RESHAPE), `1474-1480` (SHRINK).
