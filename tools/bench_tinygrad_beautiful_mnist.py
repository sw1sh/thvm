#!/usr/bin/env python3
"""Profile tinygrad's beautiful_mnist training loop from the thvm tree.

This does not modify the sibling tinygrad checkout.  It imports it via
TinyHVM/tinygrad, builds the same beautiful_mnist architecture on a
synthetic fixed batch, and reports warmup/capture/replay counters in the
same shape as wl/Examples/beautiful-mnist/bench-train.wls.
"""

from __future__ import annotations

import collections
import os
import sys
import time
from pathlib import Path
from typing import Callable


REPO_ROOT = Path(__file__).resolve().parents[1]
TINYGRAD_ROOT = REPO_ROOT / "TinyHVM" / "tinygrad"

if not TINYGRAD_ROOT.exists():
  raise SystemExit(f"tinygrad checkout not found: {TINYGRAD_ROOT}")

if "DEVICE" in os.environ and "DEV" not in os.environ:
  os.environ["DEV"] = os.environ["DEVICE"]
if "TRAIN_BEAM" in os.environ and "JITBEAM" not in os.environ:
  os.environ["JITBEAM"] = os.environ["TRAIN_BEAM"]

sys.path.insert(0, str(TINYGRAD_ROOT))

from tinygrad import Tensor, TinyJit, nn, GlobalCounters, function, Device, dtypes  # noqa: E402
from tinygrad.helpers import BEAM, DEBUG, JIT, getenv  # noqa: E402
from tinygrad.uop.ops import Ops  # noqa: E402


def env_int(name: str, default: int) -> int:
  raw = os.environ.get(name, "")
  return int(raw) if raw.strip().isdigit() else default


def fmt_ms(seconds: float) -> str:
  return f"{seconds * 1e3:.1f}ms"


def fmt_bytes(nbytes: int) -> str:
  units = ("B", "KiB", "MiB", "GiB")
  value = float(nbytes)
  for unit in units:
    if abs(value) < 1024.0 or unit == units[-1]:
      return f"{value:.1f}{unit}" if unit != "B" else f"{int(value)}B"
    value /= 1024.0
  raise AssertionError("unreachable")


def op_name(op) -> str:
  return getattr(op, "name", str(op).split(".")[-1])


def iter_leaf_calls(linear):
  for call in linear.src:
    ast = call.src[0]
    if ast.op is Ops.CUSTOM_FUNCTION and ast.arg == "graph":
      yield from ast.src[0].src
    else:
      yield call


def captured_summary(jit: TinyJit) -> tuple[str, str]:
  cap = jit.captured
  if cap is None:
    return "captured=none", "captured_ops={}"

  calls = list(cap.linear.src)
  leaf_calls = list(iter_leaf_calls(cap.linear))
  ast_hist = collections.Counter()
  program_hist = collections.Counter()
  graph_calls = 0

  for call in calls:
    ast = call.src[0]
    name = op_name(ast.op)
    if ast.op is Ops.CUSTOM_FUNCTION:
      name = f"{name}:{ast.arg}"
      if ast.arg == "graph":
        graph_calls += 1
    ast_hist[name] += 1

  for call in leaf_calls:
    ast = call.src[0]
    if ast.op is Ops.PROGRAM:
      program_hist[ast.arg.name] += 1

  hist = ",".join(f"{k}={v}" for k, v in ast_hist.most_common())
  top_programs = ",".join(f"{k}={v}" for k, v in program_hist.most_common(8))
  summary = (
    f"captured_calls={len(calls)} captured_leaf_calls={len(leaf_calls)} "
    f"graph_calls={graph_calls}"
  )
  detail = f"captured_ops={{{hist}}}"
  if top_programs:
    detail += f" top_programs={{{top_programs}}}"
  return summary, detail


class Model:
  def __init__(self):
    self.layers: list[Callable[[Tensor], Tensor]] = [
      nn.Conv2d(1, 32, 5), Tensor.relu,
      nn.Conv2d(32, 32, 5), Tensor.relu,
      nn.BatchNorm(32), Tensor.max_pool2d,
      nn.Conv2d(32, 64, 3), Tensor.relu,
      nn.Conv2d(64, 64, 3), Tensor.relu,
      nn.BatchNorm(64), Tensor.max_pool2d,
      lambda x: x.flatten(1), nn.Linear(576, 10),
    ]

  @function
  def __call__(self, x: Tensor) -> Tensor:
    return x.sequential(self.layers)


def make_synthetic_batch(bs: int) -> tuple[Tensor, Tensor]:
  x = Tensor.rand(bs, 1, 28, 28, dtype=dtypes.float32)
  y = Tensor([i % 10 for i in range(bs)], dtype=dtypes.int32)
  return x.realize(), y.realize()


def main() -> None:
  bs = env_int("BS", 32)
  warmup_steps = env_int("WARMUP_STEPS", 2)
  n_steps = env_int("N_STEPS", 1)
  read_loss = env_int("READ_LOSS", 0) != 0
  sync_step = env_int("SYNC", 1) != 0
  bench_mode = os.environ.get("BENCH_MODE", "train").lower()

  Tensor.manual_seed(env_int("SEED", 42))

  setup_t0 = time.perf_counter()
  x, y = make_synthetic_batch(bs)
  model = Model()
  params = nn.state.get_parameters(model)
  opt = nn.optim.Adam(params)

  opt_state = [opt.b1_t, opt.b2_t, *opt.m, *opt.v] if hasattr(opt, "m") else []
  Tensor.realize(x, y, *params, *opt_state)
  setup_dt = time.perf_counter() - setup_t0
  GlobalCounters.reset()

  @TinyJit
  @Tensor.train()
  def train_step(batch_x: Tensor, batch_y: Tensor) -> Tensor:
    opt.zero_grad()
    loss = model(batch_x).sparse_categorical_crossentropy(batch_y).backward()
    return loss.realize(*opt.schedule_step())

  @TinyJit
  def forward_step(batch_x: Tensor, batch_y: Tensor) -> Tensor:
    return model(batch_x).sparse_categorical_crossentropy(batch_y).realize()

  step_fn = forward_step if bench_mode == "forward" else train_step

  print("tinygrad beautiful_mnist benchmark")
  print(f"  tinygrad_root={TINYGRAD_ROOT}")
  print(
    f"  device={Device.DEFAULT} BS={bs} WARMUP_STEPS={warmup_steps} "
    f"N_STEPS={n_steps} BENCH_MODE={bench_mode} SYNC={1 if sync_step else 0}"
  )
  print(
    f"  JIT={JIT.value} BEAM={BEAM.value} JITBEAM={getenv('JITBEAM', BEAM.value)} "
    f"IGNORE_JIT_FIRST_BEAM={getenv('IGNORE_JIT_FIRST_BEAM', 0)} "
    f"TRAIN_BEAM={os.environ.get('TRAIN_BEAM', '') or 'unset'} DEBUG={DEBUG.value}"
  )
  print(
    f"  params={sum(p.numel() for p in params)} setup={fmt_ms(setup_dt)} "
    f"mem={fmt_bytes(GlobalCounters.mem_used)}"
  )

  def run_step(step: int, label: str) -> float:
    GlobalCounters.reset()
    t0 = time.perf_counter()
    loss = step_fn(x, y)
    if sync_step:
      Device[Device.DEFAULT].synchronize()
    wall = time.perf_counter() - t0
    read_wall = 0.0
    if read_loss:
      read_t0 = time.perf_counter()
      _ = loss.item()
      read_wall = time.perf_counter() - read_t0
    summary, _ = captured_summary(step_fn)
    print(
      f"{label} step {step}: wall={fmt_ms(wall)}"
      f" read={fmt_ms(read_wall)}"
      f" kernels={GlobalCounters.kernel_count}"
      f" gpu={GlobalCounters.time_sum_s * 1e3:.1f}ms"
      f" ops={GlobalCounters.global_ops}"
      f" bytes={fmt_bytes(GlobalCounters.global_mem)}"
      f" mem={fmt_bytes(GlobalCounters.mem_used)}"
      f" jit_cnt={step_fn.cnt}"
      f" {summary}"
    )
    return wall

  for i in range(1, warmup_steps + 1):
    run_step(i, "warmup")

  timed = [run_step(warmup_steps + i, "timed") for i in range(1, n_steps + 1)]
  if timed:
    avg = sum(timed) / len(timed)
    print(f"steady_ms_per_step={avg * 1e3:.1f}")

  _, detail = captured_summary(step_fn)
  print(detail)


if __name__ == "__main__":
  main()
