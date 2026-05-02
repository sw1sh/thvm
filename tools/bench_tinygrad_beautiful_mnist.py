#!/usr/bin/env python3
"""Profile tinygrad's beautiful_mnist training loop from the thvm tree.

This does not modify the sibling tinygrad checkout.  It imports it via
TinyHVM/tinygrad, builds the same beautiful_mnist architecture on a
synthetic fixed batch, and reports warmup/capture/replay counters in the
same shape as wl/Examples/beautiful-mnist/bench-train.wls.
"""

from __future__ import annotations

import collections
import hashlib
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


def env_bool(name: str, default: bool = False) -> bool:
  raw = os.environ.get(name, "")
  if raw == "":
    return default
  return raw.lower() not in {"0", "false", "no", "off"}


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


def short(text: object, limit: int = 96) -> str:
  s = str(text).replace("\n", "\\n")
  return s if len(s) <= limit else s[:limit - 3] + "..."


def source_hash(src: str) -> str:
  return hashlib.blake2b(src.encode("utf-8"), digest_size=6).hexdigest()


def format_hist(hist: collections.Counter, limit: int = 12) -> str:
  if not hist:
    return "{}"
  parts = [f"{k}={v}" for k, v in hist.most_common(limit)]
  if len(hist) > limit:
    parts.append(f"...={sum(hist.values()) - sum(v for _, v in hist.most_common(limit))}")
  return "{" + ",".join(parts) + "}"


def format_estimates(est) -> str:
  if est is None:
    return "{}"
  return f"{{ops={short(est.ops)},lds={short(est.lds)},mem={short(est.mem)}}}"


def iter_leaf_calls(linear):
  for call in linear.src:
    ast = call.src[0]
    if ast.op is Ops.CUSTOM_FUNCTION and ast.arg == "graph":
      yield from ast.src[0].src
    else:
      yield call


def call_kind(call) -> str:
  ast = call.src[0]
  if ast.op is Ops.CUSTOM_FUNCTION:
    return f"{op_name(ast.op)}:{ast.arg}"
  return op_name(ast.op)


def program_leaf_summary(call) -> dict:
  ast = call.src[0]
  if ast.op is not Ops.PROGRAM:
    return {
      "kind": call_kind(call),
      "name": call_kind(call),
      "key": call_kind(call),
      "count": 1,
      "uops": 0,
      "source": "",
      "source_hash": "",
      "op_hist": collections.Counter(),
    }

  sink = ast.src[0]
  linear = ast.src[2] if len(ast.src) > 2 and ast.src[2].op is Ops.LINEAR else None
  source = ast.src[3].arg if len(ast.src) > 3 and ast.src[3].op is Ops.SOURCE else ""
  kinfo = sink.arg
  op_hist = collections.Counter(op_name(u.op) for u in linear.src) if linear is not None else collections.Counter()
  dtype_hist = collections.Counter(str(u.dtype) for u in linear.src) if linear is not None else collections.Counter()
  return {
    "kind": "PROGRAM",
    "name": ast.arg.name,
    "key": getattr(ast, "key", source_hash(source)),
    "function": ast.arg.function_name,
    "global": ast.arg.global_size,
    "local": ast.arg.local_size,
    "vars": tuple(v.expr for v in ast.arg.vars),
    "globals": ast.arg.globals,
    "outs": ast.arg.outs,
    "ins": ast.arg.ins,
    "uops": len(linear.src) if linear is not None else 0,
    "axis_types": tuple(op_name(a) for a in getattr(kinfo, "axis_types", ())),
    "applied_opts": tuple(str(o) for o in getattr(kinfo, "applied_opts", ())),
    "estimates": getattr(kinfo, "estimates", None),
    "source": source,
    "source_hash": source_hash(source) if source else "",
    "op_hist": op_hist,
    "dtype_hist": dtype_hist,
    "linear": linear,
  }


def dump_program_detail(prefix: str, summary: dict, source_lines: int, linear_lines: int) -> None:
  print(
    f"{prefix}name={summary['name']} count={summary.get('count', 1)} "
    f"uops={summary['uops']} global={summary.get('global', ())} "
    f"local={summary.get('local', None)} outs={summary.get('outs', ())} "
    f"ins={summary.get('ins', ())} source_hash={summary.get('source_hash', '')}"
  )
  print(
    f"{prefix}  estimates={format_estimates(summary.get('estimates'))} "
    f"axis_types={summary.get('axis_types', ())} "
    f"opts={summary.get('applied_opts', ())}"
  )
  print(
    f"{prefix}  ops={format_hist(summary.get('op_hist', collections.Counter()))} "
    f"dtypes={format_hist(summary.get('dtype_hist', collections.Counter()), 8)}"
  )

  source = summary.get("source", "")
  if source_lines > 0 and source:
    print(f"{prefix}  source_head:")
    for line in source.splitlines()[:source_lines]:
      print(f"{prefix}    {short(line, 140)}")

  linear = summary.get("linear")
  if linear_lines > 0 and linear is not None:
    print(f"{prefix}  linear_head:")
    for idx, u in enumerate(linear.src[:linear_lines]):
      src_ops = tuple(op_name(s.op) for s in u.src)
      print(
        f"{prefix}    {idx}: op={op_name(u.op)} dtype={short(u.dtype, 40)} "
        f"src={src_ops} arg={short(u.arg, 100)}"
      )


def dump_captured_ir(jit: TinyJit, top: int, source_lines: int, linear_lines: int) -> None:
  cap = jit.captured
  if cap is None:
    print("captured_ir=none")
    return

  print("captured_ir:")
  print(f"  top_level_calls={len(cap.linear.src)}")
  for idx, call in enumerate(cap.linear.src):
    ast = call.src[0]
    if ast.op is Ops.CUSTOM_FUNCTION and ast.arg == "graph":
      leaf = list(ast.src[0].src)
      leaf_names = collections.Counter(program_leaf_summary(c)["name"] for c in leaf)
      print(
        f"  graph[{idx}]: leaf_calls={len(leaf)} "
        f"programs={format_hist(leaf_names, 10)}"
      )
    else:
      print(f"  call[{idx}]: kind={call_kind(call)}")

  grouped: dict[object, dict] = {}
  for call in iter_leaf_calls(cap.linear):
    summary = program_leaf_summary(call)
    key = (
      summary["kind"],
      summary["name"],
      summary.get("global", ()),
      summary.get("local", None),
      summary.get("source_hash", ""),
    )
    if key not in grouped:
      grouped[key] = dict(summary, count=0)
    grouped[key]["count"] += 1

  rows = sorted(grouped.values(), key=lambda x: (-x["count"], -x["uops"], x["name"]))
  print("  top_leaf_program_shapes:")
  for i, row in enumerate(rows[:top], 1):
    dump_program_detail(f"    {i}. ", row, source_lines, linear_lines)


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
  dump_ir = env_bool("DUMP_IR", False)
  ir_top = env_int("IR_TOP", 8)
  source_lines = env_int("SOURCE_LINES", 0)
  linear_lines = env_int("LINEAR_LINES", 0)
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
    f"N_STEPS={n_steps} BENCH_MODE={bench_mode} SYNC={1 if sync_step else 0} "
    f"DUMP_IR={1 if dump_ir else 0} IR_TOP={ir_top}"
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
  if dump_ir:
    dump_captured_ir(step_fn, ir_top, source_lines, linear_lines)


if __name__ == "__main__":
  main()
