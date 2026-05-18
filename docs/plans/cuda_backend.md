# CUDA backend slice

Goal: thvm renders and runs kernels on **both Metal and CUDA**, so the
codegen can be tested against, and its gap to tinygrad closed on, a
second architecture.  This is not hackathon work (the HuggingFace
hackathon was cancelled); it is parity/test infrastructure.

## Framing: "port tinygrad's CUDA codegen" is really "build a CUDA backend"

tinygrad's `renderer/cstyle.py` `CUDARenderer` walks tinygrad's UOp
graph; thvm renders its own UOp DAG through `render_uop.c`'s `rmu_emit_*`
body emitter.  Different IRs -- the renderer code does not transfer.
What transfers is the *reference*: tinygrad's CUDA idioms (`extern "C"
__global__`, `blockIdx`/`threadIdx`, WMMA intrinsics) and its nvrtc +
driver-API runtime shape (`ops_cuda.py`).  We write thvm's own,
modelled on it.

## Current state (the parts that make this tractable)

- `src/codegen/render_uop.c` (3115 lines) already has **two** render
  targets: Metal (`cg_render_uop_kernel_root`) and C99
  (`cg_render_uop_kernel_c_root`), switched by a `static int
  RMU_TARGET_C` -- only **23 use-sites**.  The body emitter `rmu_emit_*`
  is shared; targets differ in preamble/signature + a handful of
  branches.
- Backends live in `src/backend/{cpu,metal,dispatch}/`.  A backend =
  `init.c` + `buf_*.c` (alloc/copy/free/read/write/pool/refcount) +
  `jit.c` (compile rendered source) + dispatch.  `cpu/` is the cleanest
  template; `metal/_.m` shows the GPU shape.
- `Makefile` already has a Darwin-only Metal section (conditional
  object, framework link, shader compile).  A Linux+CUDA section
  mirrors it.

So this is an *extension* of a renderer that already retargets, not a
rewrite.

## Stages

### Stage 0 -- `cg_target` abstraction (local, no CUDA HW)

Replace `RMU_TARGET_C` with an enum `cg_target { CG_TARGET_METAL,
CG_TARGET_C, CG_TARGET_CUDA }`.  The work is not purely mechanical:
each existing `!RMU_TARGET_C` site must be classified as

- **GPU-generic** (Metal *and* CUDA): thread/block positions, the
  `FAST_MATH` / `VEC_LOAD` / `SIMD_REDUCE` OPT lowerings -- CUDA has
  `__expf`, `float4`, `__shfl_down_sync`.  These become `target !=
  CG_TARGET_C`.
- **Metal-only**: `#include <metal_stdlib>`, `simdgroup_matrix`, the
  `[[thread_position_in_grid]]` attribute syntax.  These become
  `target == CG_TARGET_METAL`.

Exit check: Metal and CPU renders byte-identical to before (`make`,
the `test_render_uop_metal` / CPU JIT tests).  This stage is
foundational and ships first.

### Stage 1 -- CUDA renderer (local, render-only tests)

`cg_render_uop_kernel_cuda_root` + `cg_emit_cuda(ke)` (mirror of
`cg_emit_via_uop` in `render_metal.c`).

- Preamble: no `metal_stdlib`; `extern "C" __global__ void k(...)`.
- Signature: pointer args, builtins via `blockIdx`/`blockDim`/
  `threadIdx` instead of `[[thread_position_in_grid]]`.
- OPT lowerings (CUDA branch in the body emitter):
  - `OPT_TC` -> WMMA (`wmma::fragment`, `wmma::mma_sync`), natural
    fragment 16x16x16.  This is the handcoded tile optimization we
    already have for Metal `simdgroup_matrix`, re-emitted for WMMA.
  - `OPT_SIMD_REDUCE` -> `__shfl_down_sync` warp reduction.
  - `OPT_FAST_MATH` -> `__expf` / `__exp2f` / `__logf`.
  - `OPT_VEC_LOAD` -> `float4` reinterpret (same as Metal).

Tests: render the canonical matmul / softmax / reduce DAGs to `.cu`
strings and string-check structure.  No GPU needed.

### Stage 2 -- `src/backend/cuda/` runtime (needs the pod)

Mirror `cpu/` and `metal/`:

- `init.c` -- `cuInit`, `cuDeviceGet`, `cuCtxCreate`.
- `buf_*.c` -- `cuMemAlloc` / `cuMemcpyHtoD` / `cuMemcpyDtoH` /
  `cuMemFree`, plus the buffer pool/refcount mirroring `cpu/buf_pool.c`.
- `jit.c` -- nvrtc compiles the rendered `.cu` string to PTX,
  `cuModuleLoadData`, `cuModuleGetFunction`, `cuLaunchKernel`.
- `Makefile`: a Linux-and-CUDA-present section mirroring the Darwin
  Metal block; link `-lcuda -lnvrtc`; guard so the Mac build is
  untouched.

### Stage 3 -- bridge + dispatch integration (needs the pod)

- `src/backend/dispatch/` backend selection so `THVM_BACKEND=cuda`
  routes here.
- `py/csource/thvm_py_cuda.c` -- mirror of `thvm_py_metal.m`:
  `compile_cuda`, buffer alloc/write/read, `dispatch_timed` (CUDA
  events for GPU time -- the honest analogue of the Metal
  `GPUEndTime` fix in `py_metal_dispatch_timed`).
- End-to-end: build a matmul DAG in Python, render CUDA, compile,
  dispatch, check against numpy -- on the pod.

### Stage 4 -- cross-validation vs tinygrad (needs the pod)

- Run matmul / softmax / reduce through thvm-CUDA and tinygrad-CUDA on
  the same A6000; compare correctness and speed.  This is the "close
  the gap to tinygrad" measurement.
- Wire CUDA-appropriate `KOP_TC` tile proposals (WMMA 16x16x16) into
  `src/codegen/propose.c`.

## Test machine

Prime Intellect pod, RTX A6000 48 GB, `ubuntu_22_cuda_12` image.
`ssh thvm-pod` (ControlMaster; IP changes per pod -- update the
`Hostname` in `~/.ssh/config`).  Provision under the Wolfram Institute
team.  Bills ~$0.54/hr while ACTIVE; `prime pods terminate` to stop.

**The image is driver-only -- no `nvcc`, no `libnvrtc`.**  Before
Stage 2 can be tested, install nvrtc: `uv pip install
nvidia-cuda-nvrtc-cu12`, symlink `libnvrtc.so.12` +
`libnvrtc-builtins.so.*` into `/usr/lib/x86_64-linux-gnu/`, `sudo
ldconfig` (or set `NVRTC_PATH`).  See the `reference_tinygrad_cuda_pod`
memory note.

Stages 0-1 are local (Mac); 2-4 need the pod.  Provision the pod when
Stage 2 starts; terminate between sessions.

## Non-goals / risks

- **Not** a full CUDA-optimal kernel library -- the goal is a correct,
  measurable CUDA target, then close the gap iteratively.
- The Mac build must never break: all CUDA build wiring is guarded.
- WMMA has rigid shape constraints (16x16x16, specific dtypes); a
  non-conforming `OPT_TC` must fall back to a tiled scalar accumulator,
  exactly as the Metal path falls back from `simdgroup_matrix`.
- nvrtc version vs driver version skew on the pod image -- pin the
  nvrtc package to the CUDA 12 line the driver supports.
