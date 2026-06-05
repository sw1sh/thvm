"""Local stand-in for tinygrad's extra/bench_log.py (BenchEvent +
WallTimeEvent). tinygrad's lives under its repo `extra/` (not the
tinygrad frontend surface), so the gpt2 example carries a minimal
no-influxdb copy here -- same API, same wall-time accumulation."""
import time
from enum import Enum


class BenchEvent(Enum):
  LOAD_WEIGHTS = "load_weights"
  STEP = "step"
  FULL = "full"


_events = {e: {"wall": []} for e in BenchEvent}


class WallTimeEvent:
  def __init__(self, event: BenchEvent):
    self.event = event

  def __enter__(self):
    self.start = time.monotonic()
    return self

  def __exit__(self, *_):
    self.time = time.monotonic() - self.start
    _events[self.event]["wall"].append(self.time)
    return False
