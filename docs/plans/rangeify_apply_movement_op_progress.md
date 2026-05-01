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
- per-USE PAD identity-pass: a PAD reading an input, or a short chain
  ending in an input, emits a fresh use-local `S_INDEX_E` + `S_LOAD`
  from that edge's own `RngsCtx`.  This avoids the old per-input-slot
  first-writer-wins bug where one consumer's movement chain could
  mis-address another consumer's load.
- PAD chain peeling currently accepts `LOAD`, `EXPAND`, `BITCAST`,
  `SHRINK`, and RESHAPE in two cases: size-1-axis insertion/removal,
  or same-rank same-numel reshape with leading source extent > 1.
  The leading-1 case is deliberately excluded because it corrupts the
  LeNet first-conv fanout.
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

Bail-count delta on focused suites after the per-USE PAD path:
- `conv_im2col.wlt`: 6/0, no rangeify bails reported.
- `grad.wlt`: 62/0, only the older RESHAPE rank-mismatch bail remains.
- `nn.wlt`: 49/0, one deliberate leading-1 PAD bail remains in LeNet
  first conv, plus the load-bearing attention pre-INDEX mismatch bail.

## Remaining bails

### Leading-1 PAD fanout

`nn.wlt` still reports:

```
SHRINK/PAD-rank: opcode=PAD out_dims=[1,25,576]
src_dims=[1,1,576] os.dims=[20,576] chain: RESHAPE -> INPUT
```

Broad RESHAPE peeling closes this bail but breaks
`nn/lenet-end-to-end-forward-shape-and-softmax-sum`: the first conv
image fanout mis-addresses its patch views and the softmax output
becomes invalid.  Keep this bail until the view-addressing model for
leading-1 flatten-then-PAD chains is understood.

### RESHAPE rank-mismatch on chain body

`grad.wlt` still has:

```
rangeify bail (mid-emit): RESHAPE shape-change ndim cap or != os->ndim
```

The `S_RESHAPE_V` path still mainly handles direct input loads.  A
non-input chain body needs a general iter-shape transform around the
body expression rather than synthesizing a fresh DEFINE_PARAM load.

### Attention pre-INDEX mismatch

The remaining `nn/attention-identity-q-row-selection` pre-INDEX
mismatch is still load-bearing:

```
pre-INDEX-mismatch: v.shape=[2,3] strides=[3,1] os=[2,2]
rngs=[S_IDIV,S_IMOD]
```

Those rngs encode RESHAPE flat-roundtrip intent, not a safe materializer
addressing pattern.  Relaxing this gate previously broke the attention
row-selection test.

## Cumulative progress

Commits driving the earlier reductions (newest first):

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

## Reference

- tinygrad: `apply_movement_op` and `_apply_reshape` in
  `/Users/swish/src/tinygrad/tinygrad/schedule/indexing.py:113-145`.
- thvm IR: ScalarOp enum + S_I* family in `src/thvm.h:520-630`.
- Backward walk: `src/schedule/rangeify.c:680-975` (rngs[] +
  per-input-slot capture).
- pre-INDEX rngs path: `src/schedule/rangeify.c:998-1244`.
- Forward identity-pass: `src/schedule/rangeify.c:1449-1676`
  (RESHAPE), `1740-1844` (PAD/SHRINK).
