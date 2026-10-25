"""A dead Tensor's backend buffer is freed; a buffer something can still read is not.

tinygrad frees a Buffer when its last Python reference dies (device.py:160-163
Buffer.__del__ -> deallocate).  thvm traces instead (py_reclaim): allocating a
tensor and finishing a realize reclaim once resident bytes outgrow the last
reclaim's survivors.  Before, only GlobalCounters.reset() freed anything, so a
loop that built a fresh input tensor every step without it grew without bound
(the ATP ENIGMA trainer reached 12 GB in a 20,000-step TinyJit soak).

The survival cases run with THVM_RECLAIM_MB=0, which reclaims at every
allocation and realize after any growth, and churn 1 MB buffers (freed ones are
unmapped) between building a reader and reading it.

Each case runs in a child process, so a crash fails the test, not the run.
"""
import unittest

import helpers

_PRELUDE = r"""
import os, warnings
import numpy as np
from thvm import GlobalCounters, Tensor, TinyJit, nn
from thvm.tensor import _TH
MB = 1 << 20
"""

_FRESH = _PRELUDE + r"""
MODE = os.environ["MODE"]
w = Tensor(np.ones((30, 1), np.float32)).realize()

@TinyJit
def f(x):
    return (x @ w).sum().realize()

rng = np.random.default_rng(0)
for i in range(1, 3001):
    a = rng.standard_normal((4096, 30)).astype(np.float32)
    if MODE == "plain":
        t = Tensor(a)
    elif MODE == "realize":
        t = Tensor(a).realize()
    elif MODE == "nojit":
        v = (Tensor(a).realize() @ w).sum().numpy()
    elif MODE == "jit":
        v = f(Tensor(a).realize()).numpy()
    if i % 1000 == 0:
        print("AT", i, _TH.buf_live_count(), _TH.cpu_live_bytes() // MB, flush=True)
"""

_TRAIN = _PRELUDE + r"""
from thvm import Context
RESET = os.environ["RESET"] == "1"
rng = np.random.default_rng(7)
l1, l2 = nn.Linear(30, 16), nn.Linear(16, 1)
l1.weight = Tensor((rng.standard_normal((16, 30)) * 0.2).astype(np.float32))
l1.bias = Tensor(np.zeros(16, np.float32))
l2.weight = Tensor((rng.standard_normal((1, 16)) * 0.25).astype(np.float32))
l2.bias = Tensor(np.zeros(1, np.float32))
params = [l1.weight, l1.bias, l2.weight, l2.bias]
opt = nn.optim.Adam(params, lr=1e-3)

@TinyJit
def step(x, y):
    loss = (l2(l1(x).relu()) - y).square().mean()
    opt.zero_grad()
    loss.backward()
    loss.realize()
    opt.step()
    return loss

data = np.random.default_rng(11)
teacher = data.standard_normal((30, 1)).astype(np.float32)
with Context(TRAINING=1):
    for i in range(1, 3001):
        xa = data.standard_normal((4096, 30)).astype(np.float32)
        x, y = Tensor(xa).realize(), Tensor(xa @ teacher).realize()
        step(x, y).numpy()
        if RESET:
            GlobalCounters.reset()
        if i % 500 == 0:
            print("AT", i, _TH.buf_live_count(), _TH.cpu_live_bytes() // MB, flush=True)
print("PARAMS", b"".join(p.numpy().tobytes() for p in params).hex(), flush=True)
"""

_SURVIVE = _PRELUDE + r"""
warnings.simplefilter("ignore")
N = 1 << 18
x = np.arange(N, dtype=np.float32)

def churn():
    junk = [Tensor(np.full(N, 7.0, np.float32)).realize() for _ in range(4)]
    GlobalCounters.reset()
    junk += [Tensor(np.full(N, 9.0, np.float32)).realize() for _ in range(4)]
    del junk
    GlobalCounters.reset()

def check(name, got, want):
    print(name, bool(np.array_equal(got, want)), flush=True)

# A lazy consumer keeps the buffer of a realized tensor that died.
z = Tensor(x).realize() * 2.0
churn()
check("LAZY", z.numpy(), 2 * x)

# A lazy movement view and realized views of a dead tensor.
v1 = Tensor(x).realize().reshape(512, 512)
v2 = Tensor(x).realize().reshape(512, 512).realize()
v3 = Tensor(x).realize().reshape(512, 512).permute(1, 0).realize()
v4 = Tensor(x).realize()[1000:3000]
churn()
check("VIEW_LAZY", v1.numpy(), x.reshape(512, 512))
check("VIEW_REAL", v2.numpy(), x.reshape(512, 512))
check("VIEW_PERM", v3.numpy(), x.reshape(512, 512).T)
check("VIEW_SHRINK", v4.numpy(), x[1000:3000])

# TinyJit: the captured input of the capture call and the shared output
# buffer survive the inputs' deaths; every replay reads its fresh input.
w = Tensor(np.full(N, 0.5, np.float32)).realize()

@TinyJit
def f(a):
    return (a * w + 1.0).realize()

ok = True
for k in range(6):
    out = f(Tensor(x + k).realize())
    churn()
    ok &= bool(np.array_equal(out.numpy(), (x + k) * 0.5 + 1.0))
print("JIT", ok, flush=True)

# Two TinyJits read one leaf that Python rebinds; one of them re-captures (a
# changed int argument) and drops its old capture.  The other's replay still
# reads the leaf: the reclaim marks what live captures retain.
W = Tensor(np.full(N, 0.5, np.float32)).realize()

def mk():
    @TinyJit
    def g(a, k):
        return (a * W + float(k)).realize()
    return g

A, B = mk(), mk()
for _ in range(3):
    A(Tensor(x).realize(), 1)
    B(Tensor(x).realize(), 1)
W = Tensor(np.full(N, 2.0, np.float32)).realize()
churn()
A(Tensor(x).realize(), 2)
churn()
check("JIT_SHARED", B(Tensor(x).realize(), 1).numpy(), x * 0.5 + 1)

# A gradient of a dead loss, and one taken again after .grad = None, which starts
# over (tinygrad tensor.py:465-466).
p = Tensor(x)
(p * p).sum().backward()
g = p.grad
churn()
check("GRAD", g.numpy(), 2 * x)
q = Tensor(x)
(q * Tensor(x + 1)).sum().backward()
q.grad = None
(q * Tensor(x + 1)).sum().backward()
churn()
check("GRAD_ACC", q.grad.numpy(), x + 1)

# A detached copy and a clone of a dead tensor.
d = Tensor(x).realize().detach()
c = Tensor(x).realize().clone()
churn()
check("DETACH", d.numpy(), x)
check("CLONE", c.numpy(), x)
"""


def _run(src, **env):
    out = helpers.run_child(src, 1800, DEV="cpu", **env)
    if out.returncode != 0:
        raise AssertionError(f"child died rc={out.returncode}:\n{out.stdout}\n{out.stderr[-3000:]}")
    return out.stdout


def _at(stdout):
    """{iteration: (live buffers, resident MB)} from the child's AT lines."""
    rows = [ln.split() for ln in stdout.splitlines() if ln.startswith("AT ")]
    return {int(r[1]): (int(r[2]), int(r[3])) for r in rows}


class TestTensorFree(unittest.TestCase):
    def _bounded(self, at):
        # 3000 dead 480 KB inputs are 1.4 GB; the reclaim bound is 64 MB of
        # garbage over the survivors, and the live set stays the same.
        first, last = at[min(at)], at[max(at)]
        self.assertLess(last[1], 160, at)
        self.assertLess(last[0], first[0] + 300, at)

    def test_fresh_tensors_free(self):
        for mode in ("plain", "realize", "nojit", "jit"):
            with self.subTest(mode=mode):
                self._bounded(_at(_run(_FRESH, MODE=mode)))

    def test_jit_training_without_reset(self):
        free = _run(_TRAIN, RESET="0")
        reset = _run(_TRAIN, RESET="1")
        self._bounded(_at(free))
        params = [ln for ln in free.splitlines() if ln.startswith("PARAMS")]
        self.assertEqual(params, [ln for ln in reset.splitlines() if ln.startswith("PARAMS")])
        self.assertEqual(len(params), 1)

    def test_readable_buffers_survive(self):
        out = _run(_SURVIVE, THVM_RECLAIM_MB="0").split()
        names = ["LAZY", "VIEW_LAZY", "VIEW_REAL", "VIEW_PERM", "VIEW_SHRINK", "JIT",
                 "JIT_SHARED", "GRAD", "GRAD_ACC", "DETACH", "CLONE"]
        self.assertEqual(out, [w for n in names for w in (n, "True")])


if __name__ == "__main__":
    unittest.main()
