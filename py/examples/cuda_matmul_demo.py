"""End-to-end CUDA matmul: thvm-py builds a UOp DAG, renders CUDA,
compiles it with nvrtc in-process, dispatches on the GPU, verifies vs
numpy.  The CUDA counterpart of matmul_inproc.py.

Runs only on a Linux+CUDA host with a GPU (the pod is a V100 / SM70).
On a host without the CUDA bridge it prints a skip line and exits 0.

  python3 py/examples/cuda_matmul_demo.py
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(ROOT))

from py.thvm import Thvm, Cuda, K
from py.examples.matmul_demo import build_matmul


def run_matmul(h: Thvm, c: Cuda, M: int, N: int, K_dim: int,
               *, reps: int = 20, warmup: int = 3) -> dict:
    """Build + render CUDA + nvrtc-compile + dispatch a matmul; verify
    against a numpy reference; return stats.
    """
    t0 = time.perf_counter_ns()
    # Plain (LOOP-only, no TC): on the V100 WMMA is fp16-only, so an
    # fp32 matmul takes the scalar tiled-accumulator path -- the CUDA
    # renderer promotes the two output LOOP axes (m, n) onto `tid`.
    root = build_matmul(h, M, N, K_dim, with_tc=False)
    cu = h.render_cuda(root, name="k")
    t_render = time.perf_counter_ns() - t0

    t0 = time.perf_counter_ns()
    fn = c.compile(cu, fn="k")
    t_compile = time.perf_counter_ns() - t0

    rng = np.random.default_rng(42)
    a_np = rng.uniform(-1, 1, (M, K_dim)).astype(np.float32)
    b_np = rng.uniform(-1, 1, (K_dim, N)).astype(np.float32)
    c_ref = a_np @ b_np

    # Renderer's discovered buffer order: out (instance 0), a (1), b (2).
    out_buf = c.buf_alloc(M * N * 4)
    a_buf = c.buf_alloc(M * K_dim * 4)
    b_buf = c.buf_alloc(K_dim * N * 4)
    c.buf_write_array(a_buf, a_np)
    c.buf_write_array(b_buf, b_np)

    # 1-D launch: total threads = M*N (one output element per thread),
    # the renderer guards `tid >= M*N`.  Block of 256, warp-multiple.
    total = M * N
    block = 256 if total >= 256 else max(32, ((total + 31) // 32) * 32)
    grid = (total + block - 1) // block

    for _ in range(warmup):
        c.dispatch(fn, [out_buf, a_buf, b_buf], grid=grid, block=block)
    c_got = c.buf_read_array(out_buf, (M, N), np.float32)
    max_abs = float(np.max(np.abs(c_got - c_ref)))
    max_rel = float(np.max(np.abs(c_got - c_ref) / (np.abs(c_ref) + 1e-30)))

    samples = []
    for _ in range(reps):
        wall, gpu = c.dispatch_timed(fn, [out_buf, a_buf, b_buf],
                                     grid=grid, block=block)
        samples.append((wall, gpu))
    samples.sort()
    p50 = samples[reps // 2]

    c.buf_release(out_buf)
    c.buf_release(a_buf)
    c.buf_release(b_buf)
    c.fn_release(fn)

    return dict(
        M=M, N=N, K=K_dim,
        render_us=t_render / 1e3,
        compile_us=t_compile / 1e3,
        max_abs=max_abs, max_rel=max_rel,
        p50_wall_us=p50[0] / 1e3, p50_gpu_us=p50[1] / 1e3,
        gflops_p50=(2.0 * M * N * K_dim) / (p50[1] * 1e-9) / 1e9 if p50[1] else 0,
        correct=(max_abs <= 1e-3),
    )


def run_reduce(h: Thvm, c: Cuda, length: int) -> dict:
    """Build + render CUDA + dispatch a full vector reduce-sum; verify
    against numpy.  out[0] = sum(in[k]).
    """
    out_buf_t = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(1,), instance=0)
    in_buf_t = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(length,), instance=1)
    k_axis = h.range(axis_id=0, axis_type=K.AXIS_REDUCE, extent=length)
    red = h.reduce(K.REDUCE_SUM, axis=0, src=h.index_e(in_buf_t, k_axis))
    root = h.store(out_buf_t, h.iconst(0), red)

    cu = h.render_cuda(root, name="k")
    fn = c.compile(cu, fn="k")

    rng = np.random.default_rng(7)
    in_np = rng.uniform(-1, 1, (length,)).astype(np.float32)
    ref = float(in_np.sum())

    bi = c.buf_alloc(length * 4)
    bo = c.buf_alloc(4)
    c.buf_write_array(bi, in_np)
    # Scalar output: every thread re-runs the serial sum; launch one.
    c.dispatch(fn, [bo, bi], grid=1, block=1)
    got = float(c.buf_read_array(bo, (1,), np.float32)[0])
    c.buf_release(bi)
    c.buf_release(bo)
    c.fn_release(fn)
    return dict(length=length, got=got, ref=ref,
                max_abs=abs(got - ref), correct=(abs(got - ref) <= 1e-2))


def main() -> int:
    if not Cuda.available():
        print("SKIP: this libthvm_py build has no CUDA bridge "
              "(build `make py` on a Linux+CUDA host).")
        return 0

    h = Thvm()
    c = Cuda()
    print(f"CUDA device compute capability: sm_{c.device_sm()}")
    print("=" * 92)

    ok = True
    for M, N, K_dim in [(8, 8, 4), (64, 64, 64), (128, 128, 128)]:
        d = run_matmul(h, c, M, N, K_dim)
        ok &= d["correct"]
        print(
            f"matmul {d['M']:>4d}x{d['N']:<4d}x{d['K']:<4d}  "
            f"render={d['render_us']:7.1f}us  compile={d['compile_us']:9.1f}us  "
            f"p50_gpu={d['p50_gpu_us']:8.1f}us  gflops={d['gflops_p50']:6.1f}  "
            f"abs={d['max_abs']:.2e} rel={d['max_rel']:.2e}  "
            f"{'OK' if d['correct'] else 'FAIL'}"
        )

    for length in [64, 1024]:
        d = run_reduce(h, c, length)
        ok &= d["correct"]
        print(
            f"reduce len={d['length']:<6d} got={d['got']:+.5f} "
            f"ref={d['ref']:+.5f}  abs={d['max_abs']:.2e}  "
            f"{'OK' if d['correct'] else 'FAIL'}"
        )

    print("=" * 92)
    if ok:
        print("PASS: thvm-CUDA matches numpy within fp32 tolerance.")
        return 0
    print("FAIL: a result diverged from numpy.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
