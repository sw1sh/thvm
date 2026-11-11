# Metal backend - embedding strategy

See [cpu.md](cpu.md) for the codegen pipeline this backend shares
with the CPU JIT.  Both backends route through the same UOp-DAG
renderer: `cg_render_uop_kernel` (MSL) and `cg_render_uop_kernel_c`
(C99).

## TL;DR

The Metal backend mirrors `backend/cpu/` as Objective-C (`.m`) files
that expose plain C functions. The runtime stays single-TU C. Shader
source lives in `src/backend/metal/shaders/*.metal` and is compiled
offline by `xcrun metal` into a `default.metallib`. The runtime
loads the metallib at backend init and resolves kernel function
names by string lookup. It looks for `default.metallib` beside the
library first (`make wl` copies one there), then at
`build/default.metallib`. A backend whose init fails (no device, no
metallib, a build without Metal) computes nothing: a realize on it
fails with the reason, `TToDevice` to it gives a message and
`$Failed`, and `DEV=metal` leaves the CPU the default, as tinygrad
raises when a device fails to open (`tests/test_backend_unavailable.c`).

## Why not pure-C bindings

Metal's public API is Objective-C. To call it from C you have three
choices:

1. **`objc_msgSend` directly.** Possible — the Objective-C runtime
   ships with a C ABI. Doable but tedious and fragile (every selector
   needs the right cast for the argument types; mismatched casts
   crash at runtime, not compile time).

2. **`metal-cpp` (Apple's official C++ bindings).** Header-only,
   compiles as Objective-C++. Drags in a C++ runtime, which the rest
   of thvm doesn't need.

3. **Objective-C `.m` files exposing a C API.** Native to the
   platform, idiomatic, and the C runtime gets to stay C. This is
   what most pragmatic projects do (e.g. tinygrad's Metal driver
   uses `pyobjc`; ggml's Metal backend is `.m`).

Option 3 wins on readability + minimal footprint.

## File layout

```
src/backend/metal/
  init.m              -- backend init / shutdown; loads metallib
  buf_alloc.m         -- MTLBuffer allocator
  buf_free.m
  buf_incref.m
  buf_decref.m
  buf_read.m          -- blit MTLBuffer -> host
  buf_write.m         -- host -> MTLBuffer
  dispatch.m          -- kernel dispatch (the analogue of cpu/interpret.c)
  _.m                 -- assembles the Backend vtable
  shaders/
    binary.metal      -- ADD, MUL, CMPLT, CMPEQ, the DAG encoder's per-op kernels
    unary.metal       -- NEG, RECIP, SQRT, EXP2, LOG2
    const.metal       -- UOP_CONST (constant fill)
    aot_*.metal       -- AOT-on-Metal kernels
```

## Build wiring

Mirror the existing CPU backend's makefile entry. The `.m` files
compile with `clang` (no `cc`-vs-clang distinction on macOS):

```makefile
THVM_PLATFORM := $(shell uname -s)
ifeq ($(THVM_PLATFORM),Darwin)
  DEV_OBJS += build/backend_metal.o build/default.metallib
  LDFLAGS_METAL := -framework Metal -framework Foundation
endif

build/backend_metal.o: src/backend/metal/_.m
	clang -c -fobjc-arc -O2 $(CFLAGS) -o $@ $<

build/default.metallib: $(wildcard src/backend/metal/shaders/*.metal)
	xcrun -sdk macosx metal -c $^ -o build/shaders.air
	xcrun -sdk macosx metallib build/shaders.air -o $@
```

The `_.m` file `#include`s every other `.m` sibling (same pattern as
`backend/cpu/_.c`) so we still build as a single TU per backend.

## Backend selection

`thvm_init()` picks a backend at process start:

```c
const char *want = getenv("DEV");
if (want && strcmp(want, "metal") == 0) {
  CURRENT_BACKEND = &METAL_BACKEND;
} else {
  CURRENT_BACKEND = &CPU_BACKEND;
}
```

`METAL_BACKEND` is a Backend struct exported by `_.m`, populated
analogously to `CPU_BACKEND`. The struct is the only thing the rest
of the runtime sees from the backend, so swapping is one env var.

## Kernel dispatch

The Metal `dispatch` function takes the same `KernelEntry` the CPU
side does. Per opcode, it:

1. Looks up the right `MTLComputePipelineState` (cached at init via
   `[device newComputePipelineStateWithFunction:...]`).
2. Binds inputs / output buffers via
   `[encoder setBuffer:offset:atIndex:]`.
3. For shape-dependent kernels (REDUCE, EXPAND, RESHAPE, CONV2D),
   packs the shape parameters into a small `MTLBuffer` constant slot
   (matches the CPU `KProgOp.arg` packing — same encoding works).
4. `[encoder dispatchThreads:threadsPerThreadgroup:]` with
   threadgroup size taken from the pipeline state's
   `maxTotalThreadsPerThreadgroup`.
5. Commits the command buffer. Synchronous wait
   (`[cmdBuffer waitUntilCompleted]`) at the dispatch boundary; the
   batch / deferred-decref scaffolding for amortising encoder cost
   lives in `metal_dispatch_flush` (see `docs/NN/memory.md`).

## Buffers

`TenDesc.buffer.ptr` becomes opaque from the runtime's POV. On CPU
it's a `void *`. On Metal it'll be a CFRetain'd `id<MTLBuffer>`,
unwrapped only inside the backend `.m` files. The existing
refcount / free / read / write functions already abstract this —
swapping the implementation is mechanical.

## Scope notes

- **Single device.** One default `MTLDevice` selected at init.
- **Shader specialisation constants.** All shapes pack into the arg
  buffer; no recompilation per call.
- **Standard command encoders.** Indirect command buffers are not
  used.
- **Dtypes.** The generated MSL of the tile path takes the dtypes
  `rmu_msl_type_name` maps (f32, f16, bf16, fp8 bytes, i8 / u8 / i32 /
  i64); the shader-library DAG encoder takes f32 and i32 only
  (`metal_kernel_supported`). See `docs/NN/dtypes.md` for backend
  coverage.

## Build dependency

Shader compilation with `xcrun metal` requires the macOS SDK and
Xcode command-line tools. Without them, the build falls back to
CPU-only by skipping the Darwin block in the makefile.
