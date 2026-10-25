# Symbolic shapes

A tensor dimension whose extent is a runtime variable (a "kvar"), as
tinygrad's `Variable("s", lo, hi)`. Not neuro-symbolic AI; for that see
[../symbolic/](../symbolic/).

## What a symbolic dimension is

A tensor `{S, dim}` with `S` symbolic has a fixed `dim` and a variable `S`
that is bound to an actual length each time the graph runs. Every symbolic
dimension carries two numbers:

- **upper bound** `hi`: the worst-case size. Buffers and the dispatch shape
  are allocated at `hi`.
- **bound value**: the length for this run (`lo <= value <= hi`). The kernel
  loops `value` times; only the buffer's first `value` rows are meaningful.

The whole model in one line: size at the upper bound, loop at the bound
value. A symbolic buffer is allocated once at `hi`, so it fits any length and
its buffer id stays stable across JIT replays, while the kernel touches only
the first `value` rows. Loop at `hi` and the kernel reads garbage past the
data; size at `value` and every length reallocates.

This is how one compiled kernel serves many sizes: GPT-2 decoding over a
symbolic sequence length `S` captures its forward once and binds `S` to the
running length each step, with no padding to a fixed window and no
recomputation of the whole window, and the KV-cache length is itself a
symbolic dimension (see the [GPT-2 tutorial](../../wl/THVMLink/docs/Tutorials/Gpt2.md)).

## The kvar

[src/schedule/kvar.c](../../src/schedule/kvar.c) is a small registry.
`kvar_alloc(name, lo, hi)` returns a `var_id`; the kvar remembers `[lo, hi]`.

A dimension extent is a plain `u32`. Bit 31 (`KVAR_FLAG`) marks a packed kvar:

```
kvar_pack_extent(id)   -> KVAR_FLAG | id      (bit 31 set, id in the low bits)
kvar_extent_is_var(e)  -> (e & KVAR_FLAG) != 0
kvar_extent_var_id(e)  -> e & KVAR_ID_MASK
```

A `Shape` whose `dims[i] = kvar_pack_extent(id)` is symbolic on axis `i`. That
is the only producer; everything downstream reads extents through one of two
resolvers:

```
kvar_extent_static(e)   -> hi for a kvar, the literal otherwise         // sizing
kvar_extent_runtime(e)  -> the bound value (hi if unbound) for a kvar,  // looping
                           the literal otherwise
kvar_set_runtime(id, v) -> set the binding (before realize)
```

- **Sizing** uses `kvar_extent_static`: `shape_numel`
  ([src/view/shape_numel.c](../../src/view/shape_numel.c)) and `view_create`
  size a `{S}` tensor at `hi`.
- **Looping** uses `kvar_extent_runtime`, so the kernel runs exactly `value`
  iterations.

## The flow, end to end (a `{S}` reduce)

1. **Producer.** A `Shape` with `dims[0] = kvar_pack_extent(s)`;
   `tensor_alloc` sizes the buffer at `hi`.
2. **Schedule.** rangeify
   ([src/schedule/rangeify_unified.c](../../src/schedule/rangeify_unified.c))
   turns each output or reduce axis into a `UOP_RANGE` with the packed kvar
   extent; `ru_extent_is_one` treats a kvar as not 1, so the axis is never
   collapsed.
3. **Render.** [src/codegen/render_uop.c](../../src/codegen/render_uop.c)
   emits `for (...; i < V_<name>; ...)` for a kvar extent and declares
   `unsigned V_<name> = kvar_vals[i]` per kvar, in `kvar_collect_from_dag`
   order.
4. **Bind.** `kvar_set_runtime(s, value)` before `thvm_realize`. The UOp
   walker ([src/backend/cpu/uop_walk.c](../../src/backend/cpu/uop_walk.c))
   reads the binding directly; the CPU JIT
   ([src/backend/cpu/jit.c](../../src/backend/cpu/jit.c)) and the Metal
   launcher ([src/backend/metal/_.m](../../src/backend/metal/_.m)) pass the
   bound values to the compiled kernel; BLAS GEMM
   ([src/backend/cpu/blas.c](../../src/backend/cpu/blas.c)) takes `M` from
   the binding. A JIT replay ([src/jit/capture.c](../../src/jit/capture.c))
   reads the live binding, so an append at a symbolic row offset lands at the
   current row.

## Surfaces

- WL: `TKVarAlloc[lo, hi]`, `TKVarSet[vid, value]`, `TKVarHi[vid]`,
  `TSymbolicAxis[t, axis, vid]` and the KV-cache append
  `TAppendAt[cache, src, posVid]`
  ([wl/THVMLink/Kernel/THVMLink.wl](../../wl/THVMLink/Kernel/THVMLink.wl)).
- Python: a `Variable` is a bound int ([py/thvm/_misc.py](../../py/thvm/_misc.py));
  the py layer builds no symbolic kernels.
- CUDA: the launcher does not pass kvar bounds.

## Tests

`tests/test_uop_symbolic_shape.c` (a `{S}` sum, rebinding the same dimension
across runs), `tests/test_sym_m2.c` (elementwise and multi-axis reduce), and
the rest of `tests/test_sym_*.c` (matmul, pad, reshape, gather, attention,
decode step, KV-cache append and its JIT replay).
