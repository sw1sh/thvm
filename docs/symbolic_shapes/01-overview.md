# Symbolic shapes in thvm — what & why

> Symbolic *shapes* (a tensor dimension that is a runtime variable), NOT
> neuro-symbolic AI. For the latter see [docs/symbolic/](../symbolic/).

## What a symbolic shape is

A **symbolic shape dimension** is a tensor axis whose extent is a *runtime
variable* (a "kvar") rather than a compile-time literal. A tensor `{S, dim}`
with `S` symbolic has a real fixed `dim` and a variable `S` that is *bound* to
an actual length each time the graph runs. This mirrors tinygrad's
`Variable("s", lo, hi)` symbolic shapes.

Every symbolic dim carries two numbers:

- **upper bound** `hi` — the worst-case size. Buffers and the dispatch shape
  are allocated at `hi`.
- **bound value** — the actual length for THIS run (`lo <= value <= hi`). The
  kernel loops `value` times; only the buffer's first `value` rows are
  meaningful.

> **The whole model in one line: _size at the upper bound, loop at the bound
> value._** A symbolic dim's buffer is `hi` (worst case); its loop count is the
> per-run binding. Almost everything else falls out of that distinction.

## Why thvm wants them

### The fixed-window problem

thvm's GPT-2 generation ([Gpt2 tutorial](../../wl/THVMLink/docs/Tutorials/Gpt2.md))
lifts the 12-block transformer ONCE into a fixed-shape forward over a
`{maxSeq, vocab}` one-hot input, padded to `maxSeq`, recomputed each step as a
`TJit` replay. `maxSeq` is a **fixed window**: it must be a constant so the
graph's shape stays constant, because TJit captures a *fixed* kernel set and
replays it. That constant forces three costs:

1. you pick an arbitrary window up front (`Length[ids] + nGenerated`);
2. you pad the one-hot to it every step;
3. you recompute the whole `{maxSeq, …}` forward (incl. the `{maxSeq, maxSeq}`
   attention) even though only the last real position's logits are used.

### What symbolic dims buy

Build the forward over a **symbolic seq dim** `S` instead. Capture it once, and
each step *bind* `S` to the running length — no padding, no per-step re-lift,
no `maxSeq`:

```
step = TJit[ ... TFromNet[net, ids] ... ]          (* S symbolic *)
Nest[g |-> Append[g, argmax @ step[g]], ids, n]    (* binds S = Length[g] *)
```

It also unlocks a real incremental **KV-cache** — the cache length `t` is
itself a symbolic dim — which the fixed window can't express. See
[decode_roadmap](../plans/decode_roadmap.md) for the milestone ladder
(symbolic-seq M1→M3, then KV-cache on top).

### The general lesson

This is how ONE compiled kernel serves MANY sizes. A symbolic batch `BS` or
sequence `S` compiles once and dispatches at the actual value — the foundation
for variable-length inference and dynamic batching without recompiling, exactly
as tinygrad does it (`render` a kernel with a `Variable` bound, pass the value
at launch).

## Status (2026-06)

The kvar machinery was ported from tinygrad but sat **dormant** (no producers,
no binders) until it was wired end-to-end on the CPU interpreter (milestone M1).
A symbolic dim now realizes, executes at its bound length, and *rebinds* across
runs (the generation pattern). The compiled backends (cpu-jit, Metal, CUDA)
still need the bound threaded into the launched kernel — that is M2. See
[02-integration](02-integration.md) for how the pieces fit, and
[decode_roadmap](../plans/decode_roadmap.md) for what remains.
