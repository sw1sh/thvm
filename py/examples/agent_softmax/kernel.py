"""Softmax via thvm templates -- one-line kernel build.

Default `templates.softmax` returns a parallel-shape DAG
(r=AXIS_GLOBAL, c=AXIS_LOCAL) that hits MLX parity at small shapes
out of the box.  Future iterations can override axis_types or apply
KOpts (h.uop_dag_apply_kopt) to retune.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(ROOT))
from py.thvm import Thvm, K, templates


def build(h: Thvm, R: int, C: int):
    root, out_buf, in_bufs = templates.softmax(h, R, C)
    return root, out_buf, in_bufs[0]


def dispatch(R: int, C: int) -> dict:
    # Match the parallel-default softmax: r-> tg, c -> tt.
    return dict(grid=(R * C, 1, 1), threadgroup=(C, 1, 1))
