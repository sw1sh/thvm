"""End-to-end: build matmul UOp via thvm-py, render to MSL, compile via
xcrun, dispatch via score_metallib, compare against MLX baseline.
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(ROOT))

from py.thvm import Thvm, K
from py.examples.matmul_demo import build_matmul

SCORE_METALLIB = str(ROOT / "bench" / "metal-problems" / "runner" / "score_metallib")


def render_to_metallib(msl_source: str, td: Path) -> Path | None:
    """MSL source -> .metallib via xcrun metal + xcrun metallib."""
    metal_path = td / "k.metal"
    air_path = td / "k.air"
    metallib_path = td / "k.metallib"
    metal_path.write_text(msl_source)

    cp = subprocess.run(
        ["xcrun", "metal", "-c", str(metal_path), "-o", str(air_path)],
        capture_output=True, text=True, timeout=60,
    )
    if cp.returncode != 0:
        print("compile_err:", cp.stderr.strip().split("\n")[0], file=sys.stderr)
        return None

    cp = subprocess.run(
        ["xcrun", "metallib", str(air_path), "-o", str(metallib_path)],
        capture_output=True, text=True, timeout=60,
    )
    if cp.returncode != 0:
        print("metallib_err:", cp.stderr.strip().split("\n")[0], file=sys.stderr)
        return None
    return metallib_path


def write_dispatch(metallib_path: Path, M: int, N: int, K_dim: int, *,
                   tc: bool):
    """Write the .dispatch sidecar score_metallib reads.

    Dispatch shape depends on what the renderer emits:
      plain (LOOP-nested):   1 thread covers everything       -> 1, 1
      OPT(_, TC) guarded:    1 simdgroup (32 threads)         -> 32, 32
                             (renderer wraps in `if sgi==0 && tg==0`,
                              but simdgroup_matrix needs all 32 lanes)
    """
    sidecar = metallib_path.with_suffix(".metallib.dispatch")
    if tc:
        with open(sidecar, "w") as f:
            f.write("grid=32,1,1 threadgroup=32,1,1\n")
    else:
        with open(sidecar, "w") as f:
            f.write("grid=1,1,1 threadgroup=1,1,1\n")


def main(argv: list[str]) -> int:
    M = int(argv[1]) if len(argv) > 1 else 64
    N = int(argv[2]) if len(argv) > 2 else 64
    K_dim = int(argv[3]) if len(argv) > 3 else 64
    use_tc = ("--tc" in argv)

    h = Thvm()
    root = build_matmul(h, M, N, K_dim, with_tc=use_tc)
    msl = h.render(root, name="k")

    print(f"=== rendered MSL (M={M} N={N} K={K_dim} tc={use_tc}) ===")
    print(msl)
    print("===")

    with tempfile.TemporaryDirectory() as td:
        td_path = Path(td)
        metallib = render_to_metallib(msl, td_path)
        if metallib is None:
            return 1
        write_dispatch(metallib, M, N, K_dim, tc=use_tc)
        cp = subprocess.run(
            [SCORE_METALLIB, str(metallib),
             str(M), str(N), str(K_dim), "10", "2"],
            capture_output=True, text=True, timeout=120,
        )
        print("=== score_metallib output ===")
        print(cp.stdout, end="")
        if cp.returncode != 0:
            print("STDERR:", cp.stderr, file=sys.stderr)
        return cp.returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv))
