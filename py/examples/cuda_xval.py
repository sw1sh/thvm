"""CUDA cross-validation harness: thvm-CUDA vs tinygrad-CUDA vs numpy.

Stage 4 of the CUDA backend slice (docs/plans/cuda_backend.md).  For each
of matmul / softmax / row-reduce, at several shapes, it:

  1. builds the UOp DAG via py.thvm, renders CUDA, nvrtc-compiles, and
     dispatches on the GPU through the `Cuda` class -- GPU time from
     CUDA events (`dispatch_timed`'s gpu ns);
  2. runs the equivalent tinygrad op with CUDA=1, default and BEAM=2 --
     GPU time parsed from tinygrad's own DEBUG=2 per-kernel `tm` (also
     CUDA-event time), in a fresh subprocess per config;
  3. checks both against a numpy fp32 reference.

Reports, per op+shape: correctness (max abs / rel error vs numpy) and
GPU-time p50 + p10, plus the thvm/tinygrad speed ratio (the "gap").

Runs only on a Linux+CUDA host (the pod is a V100 / SM70).  Without the
CUDA bridge it prints a SKIP line and exits 0.

  python3 py/examples/cuda_xval.py                  # full sweep
  python3 py/examples/cuda_xval.py --quick           # small shapes only
  python3 py/examples/cuda_xval.py --md out.md       # also write a doc

The methodology mirrors docs/auto_metal_kernels/profiling.md: many
samples, percentiles, warmup, GPU-event time on both sides so the
comparison is apples-to-apples.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(ROOT))

from py.thvm import Thvm, Cuda, K

# Sample budgets.  >=50 reps per profiling.md; 200 is cheap with GPU
# events and tightens the percentiles for sub-100us kernels.
REPS = 200
WARMUP = 20
TG_REPS = 60          # tinygrad subprocess reps (one kernel set per rep)


# ====================================================================
# percentile helper
# ====================================================================
def pctl(samples, q):
    """q-th percentile (q in [0,1]) of a sorted-or-unsorted list."""
    s = sorted(samples)
    if not s:
        return 0.0
    i = min(len(s) - 1, max(0, int(q * (len(s) - 1) + 0.5)))
    return s[i]


# ====================================================================
# thvm-CUDA DAG builders -- all lift cleanly (STORE over LOOP axes,
# REDUCE in the body), so the modern CUDA dispatch path runs them.
# ====================================================================
def build_matmul(h, M, N, Kd):
    """C[m,n] = sum_k A[m,k] * B[k,n].  Output axes m,n -> tid."""
    out_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
    a_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, Kd), instance=1)
    b_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Kd, N), instance=2)
    m = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=M)
    n = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=N)
    k = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=Kd)
    Kc, Nc = h.iconst(Kd), h.iconst(N)
    a_ld = h.index_e(a_b, h.iadd(h.imul(m, Kc), k))
    b_ld = h.index_e(b_b, h.iadd(h.imul(k, Nc), n))
    red = h.reduce(K.REDUCE_SUM, axis=2, src=h.mul(a_ld, b_ld))
    return h.store(out_b, h.iadd(h.imul(m, Nc), n), red)


def build_row_reduce(h, R, C):
    """out[r] = sum_c in[r,c].  Output axis r -> tid, c reduced."""
    out_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R,), instance=0)
    in_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)
    r = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=R)
    c = h.range(axis_id=1, axis_type=K.AXIS_REDUCE, extent=C)
    Cc = h.iconst(C)
    ld = h.index_e(in_b, h.iadd(h.imul(r, Cc), c))
    red = h.reduce(K.REDUCE_SUM, axis=1, src=ld)
    return h.store(out_b, r, red)


def build_softmax(h, R, C):
    """Row-wise softmax: out[r,c] = exp(in[r,c]-rmax) / sum_c exp(...).
    Output axes r,c -> tid; two REDUCE axes recompute the row max and
    the row sum (each output thread does its own row reduction -- naive
    but correct, and it lifts).
    """
    out_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=0)
    in_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)
    r = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=R)
    c = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=C)
    Cc = h.iconst(C)
    k1 = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=C)
    rmax = h.reduce(K.REDUCE_MAX, axis=2,
                    src=h.index_e(in_b, h.iadd(h.imul(r, Cc), k1)))
    k2 = h.range(axis_id=3, axis_type=K.AXIS_REDUCE, extent=C)
    row2 = h.index_e(in_b, h.iadd(h.imul(r, Cc), k2))
    rsum = h.reduce(K.REDUCE_SUM, axis=3,
                    src=h.exp(h.add(row2, h.neg(rmax))))
    this = h.index_e(in_b, h.iadd(h.imul(r, Cc), c))
    val = h.mul(h.exp(h.add(this, h.neg(rmax))), h.recip(rsum))
    return h.store(out_b, h.iadd(h.imul(r, Cc), c), val)


# ====================================================================
# inputs + numpy reference (float64 = the true value) + a width-aware
# fp32 tolerance.  A GPU reduction over W values accumulates serially
# in fp32; that legitimately differs from numpy's pairwise fp32 sum.
# Comparing to the float64 reference with a ~W*eps relative band is
# the honest correctness gate -- it must be the SAME band for thvm and
# tinygrad so the comparison stays fair.
# ====================================================================
FP32_EPS = 2.0 ** -23           # ~1.19e-7


def make_inputs(op, shape, width):
    """Returns (input arrays fp32, reference fp64, tol_abs, tol_rel)."""
    rng = np.random.default_rng(42)
    if op == "matmul":
        M, N, Kd = shape
        a = rng.uniform(-1, 1, (M, Kd)).astype(np.float32)
        b = rng.uniform(-1, 1, (Kd, N)).astype(np.float32)
        ref = a.astype(np.float64) @ b.astype(np.float64)
        # dot of Kd terms, |term|<=1: serial fp32 round-off ~ Kd*eps.
        return [a, b], ref, 4.0 * width * FP32_EPS, 1e-2
    if op == "row_reduce":
        R, C = shape
        x = rng.uniform(-1, 1, (R, C)).astype(np.float32)
        ref = x.astype(np.float64).sum(axis=1)
        # sum of C terms, |term|<=1: round-off ~ C*eps absolute.
        return [x], ref, 4.0 * width * FP32_EPS, 1e-2
    if op == "softmax":
        R, C = shape
        x = rng.uniform(-4, 4, (R, C)).astype(np.float32)
        xd = x.astype(np.float64)
        e = np.exp(xd - xd.max(axis=1, keepdims=True))
        ref = e / e.sum(axis=1, keepdims=True)
        # outputs in [0,1]; exp + normalize round-off stays ~1e-5.
        return [x], ref, 1e-4, 1e-3
    raise ValueError(op)


# ====================================================================
# thvm-CUDA runner -- render, compile, dispatch, time
# ====================================================================
def _grid_block(total):
    """1-D launch: total threads cover the output; block is warp-multiple."""
    block = 256 if total >= 256 else max(32, ((total + 31) // 32) * 32)
    grid = (total + block - 1) // block
    return grid, block


def run_thvm(h, c, op, shape):
    """Build + render + compile + dispatch one op on thvm-CUDA.
    Returns dict with correctness vs numpy and GPU p50/p10 (us).
    """
    M = N = Kd = R = C = 0
    if op == "matmul":
        M, N, Kd = shape
        root = build_matmul(h, M, N, Kd)
        out_shape, total = (M, N), M * N
        flops = 2.0 * M * N * Kd
        width = Kd
    elif op == "row_reduce":
        R, C = shape
        root = build_row_reduce(h, R, C)
        out_shape, total = (R,), R
        flops = float(R * C)
        width = C
    elif op == "softmax":
        R, C = shape
        root = build_softmax(h, R, C)
        out_shape, total = (R, C), R * C
        flops = float(R * C * 5)
        width = C
    else:
        raise ValueError(op)

    arrs, ref, tol_abs, tol_rel = make_inputs(op, shape, width)

    t0 = time.perf_counter_ns()
    cu = h.render_cuda(root, name="k")
    render_us = (time.perf_counter_ns() - t0) / 1e3
    if not cu:
        return dict(status="render_err", op=op, shape=shape)

    t0 = time.perf_counter_ns()
    fn = c.compile(cu, fn="k")
    compile_us = (time.perf_counter_ns() - t0) / 1e3

    # Renderer's buffer order: out (instance 0) first, then inputs.
    out_buf = c.buf_alloc(int(np.prod(out_shape)) * 4)
    in_bufs = []
    for arr in arrs:
        b = c.buf_alloc(arr.nbytes)
        c.buf_write_array(b, arr)
        in_bufs.append(b)
    bufs = [out_buf] + in_bufs
    grid, block = _grid_block(total)

    for _ in range(WARMUP):
        c.dispatch(fn, bufs, grid=grid, block=block)
    got = c.buf_read_array(out_buf, out_shape, np.float32).astype(np.float64)
    diff = np.abs(got - ref)
    max_abs = float(diff.max())
    max_rel = float((diff / (np.abs(ref) + 1e-30)).max())
    correct = (max_abs <= tol_abs) or (max_rel <= tol_rel)

    gpu = []
    for _ in range(REPS):
        _, g = c.dispatch_timed(fn, bufs, grid=grid, block=block)
        gpu.append(g / 1e3)            # ns -> us

    c.buf_release(out_buf)
    for b in in_bufs:
        c.buf_release(b)
    c.fn_release(fn)

    p50, p10 = pctl(gpu, 0.50), pctl(gpu, 0.10)
    return dict(
        status="ok", op=op, shape=shape,
        render_us=render_us, compile_us=compile_us,
        max_abs=max_abs, max_rel=max_rel, correct=correct,
        tol_abs=tol_abs, tol_rel=tol_rel,
        gpu_p50_us=p50, gpu_p10_us=p10,
        gflops_p50=(flops / (p50 * 1e-6) / 1e9) if p50 else 0.0,
        cu=cu, cu_lines=cu.count(chr(10)) + 1,
    )


# ====================================================================
# tinygrad-CUDA runner -- a fresh subprocess per config so CUDA= /
# BEAM= env, the JIT cache and the device are clean.  DEBUG=2 prints a
# per-kernel `tm` (GPU time, CUDA events); we parse the relevant line.
# ====================================================================
TG_DRIVER = r'''
import os, sys, json, re
import numpy as np
from tinygrad import Tensor, Device
from tinygrad.helpers import Context

op      = os.environ["XV_OP"]
shape   = json.loads(os.environ["XV_SHAPE"])
reps    = int(os.environ["XV_REPS"])
tol_abs = float(os.environ["XV_TOL_ABS"])
tol_rel = float(os.environ["XV_TOL_REL"])

rng = np.random.default_rng(42)

# Same inputs + float64 reference as the thvm side (make_inputs).
def make():
    if op == "matmul":
        M, N, Kd = shape
        a = rng.uniform(-1, 1, (M, Kd)).astype(np.float32)
        b = rng.uniform(-1, 1, (Kd, N)).astype(np.float32)
        return [a, b], a.astype(np.float64) @ b.astype(np.float64)
    if op == "row_reduce":
        R, C = shape
        x = rng.uniform(-1, 1, (R, C)).astype(np.float32)
        return [x], x.astype(np.float64).sum(axis=1)
    if op == "softmax":
        R, C = shape
        x = rng.uniform(-4, 4, (R, C)).astype(np.float32)
        xd = x.astype(np.float64)
        e = np.exp(xd - xd.max(axis=1, keepdims=True))
        return [x], e / e.sum(axis=1, keepdims=True)
    raise ValueError(op)

def compute(ts):
    if op == "matmul":
        return ts[0] @ ts[1]
    if op == "row_reduce":
        return ts[0].sum(axis=1)
    if op == "softmax":
        return ts[0].softmax(axis=1)
    raise ValueError(op)

arrs, ref = make()

# Correctness: one realized run, compared to the float64 reference
# under the same width-aware tolerance the thvm side uses.
ts = [Tensor(a) for a in arrs]
got = compute(ts).numpy().astype(np.float64)
diff = np.abs(got - ref)
max_abs = float(diff.max())
max_rel = float((diff / (np.abs(ref) + 1e-30)).max())
correct = bool((max_abs <= tol_abs) or (max_rel <= tol_rel))

# Timing: DEBUG=2 prints, per realized kernel, a `tm  X.YZus/...` field
# (GPU time from CUDA events).  We sum the *compute* kernels only --
# `copy ... CUDA <- NPY` HtoD lines also carry a `tm`, but those are
# host->device transfers, not the op.  To isolate the op, the inputs
# are pre-staged onto CUDA buffers ONCE (outside the timed loop), so a
# timed realize launches only the compute kernel(s).  Warm up first so
# JIT/codegen cost is excluded.
import io, contextlib

# Stage inputs onto the device once: .realize() forces the HtoD copy
# now; subsequent ops on these tensors reuse the resident buffer.
staged = [Tensor(a).realize() for a in arrs]
Device[Device.DEFAULT].synchronize()

tm_re = re.compile(r"\btm\s+([0-9.]+)(us|ms)")

def realize_and_time():
    # compute() builds a fresh lazy graph each call, so each realize
    # re-runs the compute kernel(s) -- nothing is cached across reps.
    out = compute(staged)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
        out.realize()
        Device[Device.DEFAULT].synchronize()
    total_us = 0.0
    n = 0
    for line in buf.getvalue().splitlines():
        # skip host<->device copy lines; count compute kernels only
        if " copy " in line or "<- NPY" in line or "<- CUDA" in line:
            continue
        m = tm_re.search(line)
        if m:
            v = float(m.group(1))
            total_us += v * (1000.0 if m.group(2) == "ms" else 1.0)
            n += 1
    return total_us, n

# warmup
for _ in range(8):
    realize_and_time()

samples = []
kcount = 0
for _ in range(reps):
    t, n = realize_and_time()
    if n > 0:
        samples.append(t)
        kcount = n

print("XVRESULT " + json.dumps(dict(
    status="ok", max_abs=max_abs, max_rel=max_rel, correct=correct,
    samples=samples, n_kernels=kcount,
)))
'''


def run_tinygrad(op, shape, beam, tol_abs, tol_rel):
    """Run the tinygrad equivalent in a subprocess; return dict with
    correctness + GPU-time samples.  `beam`=0 default, 2 enables BEAM.
    `tol_abs`/`tol_rel` are the same width-aware fp32 band the thvm
    side uses, so the correctness gate is identical for both.
    """
    env = dict(os.environ)
    env["CUDA"] = "1"
    env["DEBUG"] = "2"
    env["XV_OP"] = op
    env["XV_SHAPE"] = json.dumps(list(shape))
    env["XV_REPS"] = str(TG_REPS)
    env["XV_TOL_ABS"] = repr(tol_abs)
    env["XV_TOL_REL"] = repr(tol_rel)
    if beam:
        env["BEAM"] = str(beam)
    else:
        env.pop("BEAM", None)
    try:
        r = subprocess.run([sys.executable, "-c", TG_DRIVER],
                           env=env, capture_output=True, text=True,
                           timeout=900)
    except subprocess.TimeoutExpired:
        return dict(status="timeout", op=op, shape=shape, beam=beam)
    out = r.stdout + "\n" + r.stderr
    for line in out.splitlines():
        if line.startswith("XVRESULT "):
            d = json.loads(line[len("XVRESULT "):])
            s = d.get("samples", [])
            d["op"], d["shape"], d["beam"] = op, shape, beam
            d["gpu_p50_us"] = pctl(s, 0.50)
            d["gpu_p10_us"] = pctl(s, 0.10)
            return d
    tail = "\n".join(out.splitlines()[-12:])
    return dict(status="error", op=op, shape=shape, beam=beam, log=tail)


# ====================================================================
# orchestration
# ====================================================================
def fmt_us(x):
    return f"{x:.2f}" if x else "-"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true",
                    help="small shapes only (fast smoke run)")
    ap.add_argument("--md", default="", help="write a results markdown doc")
    ap.add_argument("--json", default="", help="dump raw results as JSON")
    ap.add_argument("--no-tinygrad", action="store_true",
                    help="skip the tinygrad baseline (thvm-only)")
    args = ap.parse_args()

    if not Cuda.available():
        print("SKIP: this libthvm_py build has no CUDA bridge "
              "(build `make py` on a Linux+CUDA host).")
        return 0

    h = Thvm()
    c = Cuda()
    sm = c.device_sm()
    print(f"# CUDA cross-validation -- device sm_{sm}")
    print(f"# thvm reps={REPS} warmup={WARMUP}  |  tinygrad reps={TG_REPS}")
    print("=" * 100)

    if args.quick:
        plan = [
            ("matmul", [(256, 256, 256), (512, 512, 512)]),
            ("softmax", [(1024, 1024)]),
            ("row_reduce", [(4096, 4096)]),
        ]
    else:
        plan = [
            ("matmul", [(256, 256, 256), (512, 512, 512), (1024, 1024, 1024)]),
            ("softmax", [(4096, 4096)]),
            ("row_reduce", [(4096, 4096)]),
        ]

    rows = []
    all_correct = True
    for op, shapes in plan:
        for shape in shapes:
            label = f"{op} {'x'.join(str(s) for s in shape)}"
            print(f"\n## {label}")
            tv = run_thvm(h, c, op, shape)
            if tv["status"] != "ok":
                print(f"  thvm-CUDA: {tv['status']}")
                rows.append(dict(label=label, op=op, shape=shape, thvm=tv))
                all_correct = False
                continue
            thvm_ok = tv["correct"]
            all_correct &= thvm_ok
            print(f"  thvm-CUDA : abs={tv['max_abs']:.2e} rel={tv['max_rel']:.2e} "
                  f"{'OK' if thvm_ok else 'FAIL'}  "
                  f"gpu p50={fmt_us(tv['gpu_p50_us'])}us "
                  f"p10={fmt_us(tv['gpu_p10_us'])}us  "
                  f"({tv['gflops_p50']:.1f} GFLOP/s, {tv['cu_lines']}-line .cu)")

            tg0 = tg2 = None
            if not args.no_tinygrad:
                tg0 = run_tinygrad(op, shape, beam=0,
                                   tol_abs=tv["tol_abs"], tol_rel=tv["tol_rel"])
                if tg0.get("status") == "ok":
                    tg0_ok = tg0["correct"]
                    all_correct &= tg0_ok
                    print(f"  tinygrad  : abs={tg0['max_abs']:.2e} "
                          f"rel={tg0['max_rel']:.2e} "
                          f"{'OK' if tg0_ok else 'FAIL'}  "
                          f"gpu p50={fmt_us(tg0['gpu_p50_us'])}us "
                          f"p10={fmt_us(tg0['gpu_p10_us'])}us  "
                          f"({tg0['n_kernels']} kernels)")
                else:
                    print(f"  tinygrad  : {tg0.get('status')} "
                          f"{tg0.get('log','')}")
                tg2 = run_tinygrad(op, shape, beam=2,
                                   tol_abs=tv["tol_abs"], tol_rel=tv["tol_rel"])
                if tg2.get("status") == "ok":
                    print(f"  tinygrad B2: gpu p50={fmt_us(tg2['gpu_p50_us'])}us "
                          f"p10={fmt_us(tg2['gpu_p10_us'])}us  "
                          f"({tg2['n_kernels']} kernels)")
                else:
                    print(f"  tinygrad B2: {tg2.get('status')} "
                          f"{tg2.get('log','')}")

            ratio0 = ratio2 = None
            if tg0 and tg0.get("status") == "ok" and tg0["gpu_p50_us"]:
                ratio0 = tv["gpu_p50_us"] / tg0["gpu_p50_us"]
            if tg2 and tg2.get("status") == "ok" and tg2["gpu_p50_us"]:
                ratio2 = tv["gpu_p50_us"] / tg2["gpu_p50_us"]
            if ratio0:
                print(f"  GAP       : thvm is {ratio0:.1f}x tinygrad-default, "
                      f"{ratio2:.1f}x tinygrad-BEAM2"
                      if ratio2 else
                      f"  GAP       : thvm is {ratio0:.1f}x tinygrad-default")
            rows.append(dict(label=label, op=op, shape=shape,
                             thvm=tv, tg0=tg0, tg2=tg2,
                             ratio0=ratio0, ratio2=ratio2,
                             thvm_ok=thvm_ok))

    print("\n" + "=" * 100)
    print("PASS: thvm-CUDA matches numpy everywhere"
          if all_correct else
          "FAIL: a result diverged from its numpy reference")

    if args.md:
        write_md(Path(args.md), sm, rows, all_correct)
        print(f"wrote {args.md}")

    if args.json:
        slim = []
        for r in rows:
            tv = r["thvm"]
            tv2 = {k: v for k, v in tv.items() if k != "cu"}
            slim.append(dict(label=r["label"], op=r["op"], shape=r["shape"],
                             thvm=tv2, tg0=r.get("tg0"), tg2=r.get("tg2"),
                             ratio0=r.get("ratio0"), ratio2=r.get("ratio2")))
        Path(args.json).write_text(json.dumps(
            dict(sm=sm, reps=REPS, tg_reps=TG_REPS, rows=slim), indent=1))
        print(f"wrote {args.json}")

    return 0 if all_correct else 1


def write_md(path, sm, rows, all_correct):
    L = []
    L.append("# CUDA cross-validation: thvm-CUDA vs tinygrad-CUDA vs numpy")
    L.append("")
    L.append(f"Device: Tesla V100-SXM2-16GB (sm_{sm}). "
             f"thvm reps={REPS} (warmup {WARMUP}); tinygrad reps={TG_REPS}. "
             "All GPU times are CUDA-event time (thvm: `dispatch_timed` "
             "gpu ns; tinygrad: parsed DEBUG=2 per-kernel `tm`).")
    L.append("")
    L.append("| op + shape | thvm correct | thvm GPU p50/p10 (us) | "
             "tinygrad p50/p10 (us) | tinygrad BEAM2 p50/p10 (us) | "
             "gap (thvm / tg-default) | gap (thvm / tg-BEAM2) |")
    L.append("|---|---|---|---|---|---|---|")
    for r in rows:
        tv = r["thvm"]
        if tv.get("status") != "ok":
            L.append(f"| {r['label']} | {tv.get('status')} | - | - | - | - | - |")
            continue
        tg0, tg2 = r.get("tg0"), r.get("tg2")
        corr = (f"abs={tv['max_abs']:.1e} rel={tv['max_rel']:.1e} "
                f"{'OK' if r.get('thvm_ok') else 'FAIL'}")
        thvm_t = f"{tv['gpu_p50_us']:.2f} / {tv['gpu_p10_us']:.2f}"
        tg0_t = (f"{tg0['gpu_p50_us']:.2f} / {tg0['gpu_p10_us']:.2f}"
                 if tg0 and tg0.get("status") == "ok" else "-")
        tg2_t = (f"{tg2['gpu_p50_us']:.2f} / {tg2['gpu_p10_us']:.2f}"
                 if tg2 and tg2.get("status") == "ok" else "-")
        g0 = f"{r['ratio0']:.1f}x" if r.get("ratio0") else "-"
        g2 = f"{r['ratio2']:.1f}x" if r.get("ratio2") else "-"
        L.append(f"| {r['label']} | {corr} | {thvm_t} | {tg0_t} | "
                 f"{tg2_t} | {g0} | {g2} |")
    L.append("")
    L.append("PASS: thvm-CUDA matches numpy everywhere."
             if all_correct else
             "FAIL: a result diverged from its numpy reference.")
    L.append("")
    path.write_text("\n".join(L) + "\n")


if __name__ == "__main__":
    sys.exit(main())
