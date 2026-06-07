# Symbolic shapes in thvm — how it integrates

Read [01-overview](01-overview.md) first for *what* and *why*. This is the
mechanism: the few pieces that carry a `Variable` dim from a shape through the
scheduler to a kernel that loops the right number of times.

## The kvar: a shape variable

`src/schedule/kvar.c` is a tiny registry. `kvar_alloc(name, lo, hi)` returns a
`var_id`; the kvar remembers `[lo, hi]`.

A dimension extent is a plain `u32`. Bit 31 — `KVAR_FLAG` — distinguishes a
*packed kvar* from a literal:

```
kvar_pack_extent(id)   -> KVAR_FLAG | id      (bit 31 set, id in the low bits)
kvar_extent_is_var(e)  -> (e & KVAR_FLAG) != 0
kvar_extent_var_id(e)  -> e & KVAR_ID_MASK
```

So a `Shape` whose `dims[i] = kvar_pack_extent(id)` is symbolic on axis `i`.
**That is the producer** — the single act that puts a variable into a shape.
Everything downstream just has to read extents through the right resolver.

## Two resolvers — size at the bound, loop at the value

```
kvar_extent_static(e)   -> hi  for a kvar, the literal otherwise   // SIZING
kvar_extent_runtime(e)  -> the bound value (kvar_runtime(id), hi   // LOOPING
                           if unbound) for a kvar, the literal otherwise
kvar_set_runtime(id, v) -> set the per-run binding (caller binds before realize)
```

- **Sizing** uses `kvar_extent_static`, so buffers + the dispatch shape are the
  worst case (`hi`). `shape_numel` (`src/view/shape_numel.c`) multiplies
  `kvar_extent_static` per dim, so a `{S}` tensor's buffer is `hi` elements;
  `view_create` strides size at `hi` too.
- **Looping** uses `kvar_extent_runtime`, so the kernel runs exactly `value`
  iterations.

This split is the crux. A symbolic buffer is allocated ONCE at `hi` — so it fits
any length and its buffer id is stable across TJit replays — but the kernel
touches only the first `value` rows. Read-back returns `value` valid rows.
Confusing the two breaks it: loop-at-`hi` reads garbage past the data;
size-at-`value` reallocates per length and defeats the purpose.

## The flow, end to end (a `{S}` reduce)

1. **Producer.** A `Shape` with `dims[0] = kvar_pack_extent(s)`. `tensor_alloc`
   sizes the buffer at `shape_numel = hi`.
2. **Schedule.** `rangeify` (`src/schedule/rangeify_unified.c`) turns each output
   / reduce axis into a `UOP_RANGE` via `ru_new_range(extent, …)`. It is already
   kvar-aware — `ru_extent_is_one` treats a kvar as "not 1" so the axis is never
   collapsed — so the `RANGE` carries the packed kvar extent unchanged.
3. **Execute.**
   - *CPU interpreter* (`src/backend/cpu/uop_walk.c`): each RANGE / reduce loop
     reads its extent through `kvar_extent_runtime`, so it iterates the bound
     value. **Wired (M1).**
   - *Codegen* (`src/codegen/render_uop.c`): for a var extent it emits
     `for (…; i < V_<name>; …)` — a symbolic loop bound referencing a kernel
     variable, not the literal. **Emission ready.**
4. **Bind.** `kvar_set_runtime(s, value)` before `thvm_realize`. The interpreter
   reads it directly. For the *compiled* kernels the launch must declare and
   pass `V_<name> = value` — **pending (M2).**

## What's wired vs pending

| piece | where | status |
|---|---|---|
| producer (a kvar in a `Shape` dim) | `kvar_pack_extent` | wired |
| sizing at the upper bound | `shape_numel`, `view_create` | wired |
| rangeify emits a kvar RANGE | `rangeify_unified.c` `ru_new_range` | wired (already kvar-aware) |
| interpreter loops the bound value | `uop_walk.c` `kvar_extent_runtime` | wired (M1) |
| codegen emits a symbolic bound | `render_uop.c` `V_<name>` | wired (emission) |
| op coverage: elementwise, multi-axis reduce | inherit the RANGE path | wired (interpreter) |
| matmul (GEMM) `M = bound` | `blas_try_gemm` `kvar_extent_runtime` | wired (M2) |
| compiled elementwise/reduce kernel gets the bound | cpu-jit / Metal / CUDA dispatch | **M2** (pending) |
| WL surface (a symbolic-dim constructor) | — | **M2** |
| JIT rebinds the dim on replay | `jit` capture/replay | **M3** |

The compiled-backend row is the next real work: `render_uop` already *names* the
bound (`V_s`); the cpu-jit / Metal / CUDA launchers must declare that variable in
the kernel signature/preamble and pass `kvar_runtime(id)` at dispatch (the
analog of binding an input buffer in the TJit `input_replace` port). Until then,
symbolic dims run on the interpreter only.

## Tests

- `tests/test_uop_symbolic_shape.c` (M1): a `{S}` sum over its symbolic axis →
  `S` at `S=4`/`8`; rebinding the SAME dim → `S=3` then `S=7` (the generation
  pattern; the fire-memo re-fires correctly).
- `tests/test_sym_m2.c`: symbolic elementwise (`{S}+{S}`) and a multi-axis
  reduce over `{S, d}` — op coverage threads through the RANGE path with no
  per-op work.
