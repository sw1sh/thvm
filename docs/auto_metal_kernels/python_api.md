# Python API surface

`py/thvm/thvm.py` is a thin ctypes wrapper over `libthvm_py.dylib`.
Two main classes plus a `K` namespace of mirrored constants.

```python
from py.thvm import Thvm, Metal, K
```

## `K` constants (mirrored from C at import time)

The dylib exports `py_const_<NAME>()` getters; `K.<NAME>` reads each
once and caches.  Never drift from C -- any rebuild picks up new
values.

| Group | Members |
|---|---|
| dtypes | `INT32`, `FP32` |
| buffer scopes | `SCOPE_GLOBAL`, `SCOPE_LOCAL`, `SCOPE_REG` |
| FP unary/binary | `ADD`, `MUL`, `NEG`, `RECIP`, `EXP2`, `LOG2`, `SQRT`, `CMPLT`, `CMPEQ` |
| Integer binary | `IADD`, `ISUB`, `IMUL`, `IDIV`, `IMOD`, `ILT`, `IAND` |
| reduce kinds | `REDUCE_SUM`, `REDUCE_MAX` |
| axis types | `AXIS_LOOP`, `AXIS_REDUCE`, `AXIS_UPCAST`, `AXIS_UNROLL`, `AXIS_LOCAL`, `AXIS_GLOBAL`, `AXIS_GROUP_REDUCE` |
| OPT kinds | `OPT_UNROLL`, `OPT_UPCAST`, `OPT_TC`, `OPT_LOCAL`, `OPT_GROUP_REDUCE`, `OPT_CONV`, `OPT_FAST_MATH`, `OPT_SIMD_REDUCE`, `OPT_VEC_LOAD` |
| KOpt opcodes | `KOP_NONE`, `KOP_UPCAST`, `KOP_UNROLL`, `KOP_LOCAL`, `KOP_GROUP`, `KOP_GROUPTOP`, `KOP_SWAP`, `KOP_PADTO`, `KOP_NOLOCALS`, `KOP_TC`, `KOP_GLOBAL`, `KOP_FAST_MATH`, `KOP_SIMD_REDUCE`, `KOP_VEC_LOAD` |

## `Thvm` -- the runtime + UOp DAG builder

```python
h = Thvm()                     # init runtime; one per process
```

### UOp constructors (return raw integer Term IDs)

```python
buf  = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
m    = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=M)
n    = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=N)
k    = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=K_dim)
kc   = h.iconst(N)             # integer constant atom
addr = h.iadd(h.imul(m, kc), n)
ld   = h.index_e(buf, addr)
mul  = h.binary(K.MUL, ld_a, ld_b)
red  = h.reduce(mul, axis_id=2, kind=K.REDUCE_SUM)
root = h.store(out_buf, addr_out, red)
```

The `instance` arg on `buffer()` disambiguates input slots: 0 is the
output, 1.., are inputs.  This becomes `buffer(0)`, `buffer(1)`, ...
in the rendered MSL signature.

### Kernel slots + autotune

```python
kid = h.kernel_alloc()
h.kernel_set_cached_lift(kid, root, out_buf, [a_buf, b_buf])

cands = h.kernel_opts_propose(kid, cap=64)
# -> [(op, axis, arg), ...]   # tuples of three integers

new_root = h.kernel_apply_opt(kid, (K.KOP_TC, 0, 8))
# returns new root Term, or None if the KOpt was invalid for this DAG

msl = h.render(root, name="k")
# -> a string of MSL.  Compile via Metal.compile_msl below.
```

### DAG-only KOpt apply (no kernel slot)

```python
new_root = h.uop_dag_apply_kopt(root, (K.KOP_FAST_MATH, 0, 0))
```

Same effect as `kernel_apply_opt` but doesn't touch the
`KernelEntry.applied_opts[]` log.  Use it when you want to manipulate
a DAG without registering it with a kernel slot.

## `Metal` -- in-process MSL compile + dispatch

```python
m = Metal()                    # binds MTLCreateSystemDefaultDevice
pso = m.compile_msl(src, fn="k")
```

`src` is the full MSL source.  `fn` names the entry kernel.

```python
buf = m.buf_alloc(size_bytes)              # MTLBuffer with shared storage
m.buf_write(buf, raw_bytes)                # bytes -> buffer
m.buf_write_array(buf, np_array)           # numpy array -> buffer (zero copy where possible)
arr = m.buf_read_array(buf, shape, dtype)  # buffer -> new numpy array

m.dispatch(pso, [out_buf, in_buf, ...], grid=(Gx, Gy, Gz),
           threadgroup=(Tx, Ty, Tz))
wall_ns, gpu_ns = m.dispatch_timed(pso, bufs, grid=..., threadgroup=...)

m.buf_release(buf)
m.pso_release(pso)
```

`grid` is **total threads**, MLX-style -- same convention as
`mx.fast.metal_kernel`.  E.g. one threadgroup of 256 threads with
`R*256` total threads runs `R` threadgroups.  `threadgroup` is the
threadgroup size.

## A complete autotune loop (excerpted from `py/examples/matmul_beam_loop.py`)

```python
from py.thvm import Thvm, Metal, K

h, m = Thvm(), Metal()
M = N = K_dim = 64

# Build kernel + propose
root = build_matmul(h, M, N, K_dim, with_tc=False)   # see py/examples/matmul_demo.py
kid = h.kernel_alloc()
out_buf = h.buffer(K.SCOPE_GLOBAL, K.FP32, (M, N), instance=0)
a_buf = h.buffer(K.SCOPE_GLOBAL, K.FP32, (M, K_dim), instance=1)
b_buf = h.buffer(K.SCOPE_GLOBAL, K.FP32, (K_dim, N), instance=2)
h.kernel_set_cached_lift(kid, root, out_buf, [a_buf, b_buf])

cands = h.kernel_opts_propose(kid, cap=64)

results = []
for cand in cands:
    new_root = h.kernel_apply_opt(kid, cand)
    if new_root is None:
        continue
    msl = h.render(new_root, name="k")
    pso = m.compile_msl(msl, fn="k")
    # ... dispatch and time ...
    results.append((cand, p50_ns))
```

Each candidate is a fresh apply; no cumulative state across iterations.
For multi-KOpt sequences, fold them in order:

```python
root_after = root
for cand in [(K.KOP_TC, 0, 8), (K.KOP_GLOBAL, 0, 0), (K.KOP_GLOBAL, 1, 0)]:
    root_after = h.kernel_apply_opt(kid, cand)
```

## Where to look

- `py/thvm/thvm.py` -- the wrapper.  Read `class Thvm` (line 220+) and
  `class Metal` (line 431+) for the full API.
- `py/examples/matmul_beam_loop.py` -- end-to-end BEAM driver.
- `py/examples/matmul_demo.py` -- `build_matmul` reference fixture.
- `py/examples/agent_softmax_msl/score.py` -- raw-MSL scoring harness;
  bypasses `Thvm` entirely (only uses `Metal`).
