# Setup

Mac M-series (M1/M2/M3/M4) only.  All kernels here use the Metal
compute pipeline; nothing in this directory runs on CUDA / WebGPU /
ROCm.

## Clone

```
git clone https://github.com/swishvm/thvm.git
cd thvm
```

(If you were spawned with a worktree path, the worktree was created
for you -- skip the clone, `cd` to the worktree, follow the rest.)

## System deps

- **Xcode command-line tools**: `xcode-select --install`
- **Python 3.11+**: for `py/` and the score harnesses.

That's it -- C + Python only for this work.

## Python virtualenv

The bench harnesses expect a venv at `bench/metal-problems/.venv`:

```
cd bench/metal-problems
python3 -m venv .venv
source .venv/bin/activate
pip install -U pip
pip install numpy mlx torch
deactivate
```

`mlx` is the production-grade Apple-Silicon ML framework -- our
baseline.  `torch` is needed only for the `pytorch_mps_eager` baseline
in some `runner/sweep.py` configs.

## Build

```
make py        # builds py/thvm/libthvm_py.dylib (ctypes bridge)
```

You only need `make py` for this work.  If you change C code under
`src/` (rare for kernel-tuning agents -- you'll mostly stay in MSL +
Python), `make py` rebuilds the dylib so the ctypes wrapper picks up
the changes.

`make test` runs the C test suite (~115 binaries; minutes).  For a
focused test on a specific symbol:

```
grep -l <symbol> tests/         # find the test file
./bin/test_<name>               # run that one binary
```

## Verify the toolchain

```
python3 -c 'from py.thvm import Thvm, Metal, K; m = Metal(); print("ok")'
```

Should print `ok` with no errors.  If it fails:

- `libthvm_py.dylib not found` -> `make py` first.
- `ImportError: dlopen(...): symbol not found` -> stale build; `make
  py` again after `git clean -fd py/thvm/`.
- `MTLCreateSystemDefaultDevice() returned nil` -> Metal unavailable
  (you're not on Mac, or running under a non-GPU environment).

## Known good versions

The infrastructure is regularly exercised against:

- Apple M3 Max, macOS 14+
- Python 3.11
- mlx 0.31.x
- torch 2.11+ (mps backend)

Older Mac generations (M1) work but have lower threadgroup memory
(~32 KB vs ~64 KB on M3+).  Pick conservative tile sizes if you're
on M1.

## Where to put your work

- **Raw MSL track**: copy `py/examples/agent_softmax_msl/` to a new
  dir, edit `kernel.metal` + `dispatch.json`, run `./score.sh R C`.
- **UOp DAG track**: write a Python file under your run directory
  with `SOURCE` (MSL string) + `def dispatch(M, N, K)`, run
  `python3 bench/metal-problems/runner/score_one.py <kernel.py>`.
- **Autotune track**: drive `kernel_apply_opt` from Python in a loop
  over candidates, render MSL each step, time, pick the winner.  See
  [autotuning.md](autotuning.md) and `py/examples/matmul_beam_loop.py`.

See [agent_brief.md](agent_brief.md) for which track applies to your
mission.
