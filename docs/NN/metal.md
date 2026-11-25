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
fails with the reason, `TToDevice` to it gives the message
`TToDevice::nodev` and a `Failure`, and `DEV=metal` leaves the CPU the default, as tinygrad
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

### Weights from disk

A disk tensor (`thvm_tensor_mmap`, tinygrad's DISK device: a view into a
mapping of a file) reaches Metal in `thvm_metal_buf_wrap_external`. By
default its bytes are read from the file straight into a driver-owned
`MTLBuffer` with parallel `pread`s (`metal_read_file_into`), as tinygrad's
`DiskAllocator._copyout` does on macOS in place of a copy out of the mapping
(`runtime/ops_disk.py:91-97`). From an internal disk that is 9.7 GB/s,
against 1.1 to 1.7 GB/s for faulting the mapping in from eight threads and
copying it. The reads bypass the page cache (`F_NOCACHE`), where the bytes
would be a second copy of the device buffer, and the mapping is never
touched. The file is read through one descriptor per mapped file, kept open
while its tensors live (`thvm_disk_buf_file_info`), so the load reads the
file the mapping shows even after its path was replaced. Without a
descriptor, or when a read fails, the bytes are copied from the mapping.

`THVM_METAL_MMAP_WRAP=1` wraps the mapping's pages in place instead
(`newBufferWithBytesNoCopy`): no copy and a fraction of the footprint, but a
process killed while the graphics processor still reads those pages can leave
it spinning on them. Only that mode, and the CPU, use a weight through its
mapping; there a caller may warm the files in the background
(`TDiskPrefetchAsync`, `TDiskWarmAsync`). With the default load such a
warm-up competes for the disk and holds up the mapping calls on the same
files.

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
- **bf16 conversions.** A cast between float and bf16, a bf16 reduce body
  added to its float accumulator, and a store into a bf16 buffer convert by
  the bit pattern (`thvm_bf2f`, `thvm_f2bf`, emitted by
  `rmu_emit_bf16_helpers` into a kernel that touches bf16). This is tinygrad's
  `pm_manual_bf16_cast` (`renderer/cstyle.py:94-104`, taken by its Metal
  renderer at `:376`). The Metal compiler's own conversion of a computed
  `bfloat` value gives wrong numbers inside an unrolled reduce loop: a 1 by 1
  convolution of 32 channels lost 20 of its 32 terms.
- **Matmul launch shape.** `cg_tile_metal_dispatch_shape` sizes the launch
  for the kernel `rmu_emit_matmul_tc` renders, so the two decide from the
  same facts. With the first operand read row-major (`{M, K}`) the kernel
  stages tiles in threadgroup memory and the launch is one threadgroup per
  tile; with it read transposed (stored `{K, M}`, as an attention block over
  a channels-first feature map builds its scores) the kernel is one simdgroup
  per 8 by 8 output tile and the launch is `(M / 8) * (N / 8)` threadgroups
  of 32 threads (`rmt_matmul_a_layout`). A launch sized for staged tiles
  under the second kernel writes only the first rows of the output.
- **Replay zero fill.** A JIT replay reuses buffers, and a kernel that
  writes only part of a reused buffer (a windowed store) must find zeros in
  the rest, as it did in the fresh buffer of the first run. The replay
  zeroes a reused output first unless `uop_dag_store_full_bytes` proves that
  the kernel's STORE writes every element of a buffer of exactly that size
  (`jit_replay_zero_first`, decided once per recorded kernel). tinygrad's
  planner reuses buffers with no fill (`schedule/memory.py:24-53`). The fill
  itself is a compute kernel on the replay's serial encoder that clears 256
  bytes a thread (`metal_zero_encode_on`).

## Build dependency

Shader compilation with `xcrun metal` requires the macOS SDK and
Xcode command-line tools. Without them, the build falls back to
CPU-only by skipping the Darwin block in the makefile.
