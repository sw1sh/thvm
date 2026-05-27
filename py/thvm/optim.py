"""thvm.nn.optim -- SGD / Adam / Muon, mirroring tinygrad's nn.optim.

The contract matches tinygrad's Optimizer: zero_grad() clears the
parameter gradients and schedule_step() applies one update and returns
the tensors that still need realizing.  thvm's assign is eager (it
realizes the update immediately), so schedule_step performs the update
in place and returns the (already-updated) params -- `loss.realize(*sched)`
in the training loop then just realizes the loss.

Trainable params are those with requires_grad != False; an unset (None)
requires_grad is promoted to True (tinygrad rule), while an explicit
False (BatchNorm running stats) is a non-trained buffer.
"""
from __future__ import annotations

from .tensor import Tensor


def _dedup(xs):
    out, seen = [], set()
    for x in xs:
        if id(x) not in seen:
            seen.add(id(x))
            out.append(x)
    return out


class Optimizer:
    def __init__(self, params, lr: float):
        if lr < 0:
            raise ValueError(f"Invalid learning rate: {lr}")
        for x in params:
            if x.requires_grad is None:
                x.requires_grad_(True)
        # Params must be realized TAG_TEN leaves for requires_grad +
        # backward to register them (factory inits are lazy UOP graphs).
        for x in params:
            x.realize()
        self.params = _dedup([x for x in params if x.requires_grad])
        self.buffers = _dedup([x for x in params if not x.requires_grad])
        assert self.params, "optimizer must have at least one param"
        for p in self.params:
            p.requires_grad_(True)
        self.lr = lr

    def zero_grad(self):
        # Clear BOTH the Python .grad handle AND the canonical C-side
        # TENS[tid].grad accumulator -- thvm's backward() *accumulates*
        # into that accumulator (tinygrad semantics), so without clearing
        # it each step the gradients pile up across steps and blow up to
        # nan within a few iterations.
        from .tensor import _TH
        for p in self.params:
            p.grad = None
            try:
                _TH.ten_clear_grad(p.term)
            except Exception:
                pass

    def step(self):
        self.schedule_step()

    def schedule_step(self):
        """Build the (lazy, in-place) update assigns and return every
        tensor that must be realized to apply the step.  tinygrad model:
        `loss.realize(*opt.schedule_step())` fires them together -- which
        is also exactly what makes the whole step one JIT-capturable
        graph (no eager realize mid-step)."""
        if not Tensor.training:
            raise RuntimeError("Tensor.training must be True to step the optimizer")
        # Realize the grads first so the forward activations that read the
        # params are materialized BEFORE the in-place ASSIGN writes them.
        # Then the caller's loss reduction reads realized activations (not
        # the params) and can't race the param updates -- which is what
        # lets `loss.realize(*schedule_step())` stay correct with in-place
        # assigns (no explicit two-phase needed at the call site).
        grads = [p.grad for p in self.params if p.grad is not None]
        if grads:
            Tensor.realize(*grads)
        return self._step()

    def _step(self):
        raise NotImplementedError


class SGD(Optimizer):
    def __init__(self, params, lr=0.001, momentum=0.0, weight_decay=0.0,
                 nesterov=False, **_):
        super().__init__(params, lr)
        self.momentum, self.wd, self.nesterov = momentum, weight_decay, nesterov
        self.b = [Tensor.zeros(*(p.shape or (1,))) for p in self.params]
        for b in self.b:
            b.realize()

    def _step(self):
        out = []
        for i, p in enumerate(self.params):
            g = p.grad
            if g is None:
                continue
            g = g.reshape(*p.shape) if p.shape else g
            if self.wd:
                g = g + p * self.wd
            if self.momentum:
                self.b[i].assign(self.b[i] * self.momentum + g)
                out.append(self.b[i])
                g = (g + self.b[i] * self.momentum) if self.nesterov else self.b[i]
            p.assign(p - g * self.lr)
            out.append(p)
        return out


class Adam(Optimizer):
    def __init__(self, params, lr=0.001, b1=0.9, b2=0.999, eps=1e-8, **_):
        super().__init__(params, lr)
        self.b1, self.b2, self.eps = b1, b2, eps
        self.t = 0
        self.m = [Tensor.zeros(*(p.shape or (1,))) for p in self.params]
        self.v = [Tensor.zeros(*(p.shape or (1,))) for p in self.params]
        # Per-step bias-correction factors live as 1-element TENSORs (not
        # Python floats) so the rendered kernel source is step-INVARIANT
        # (the kernel reads `*bc1_inv_buf` instead of having a baked CONST
        # literal that changes every step).  Without this thvm's nvrtc JIT
        # cache misses on the F=10-or-so param-update kernels EVERY step
        # (different bc1/bc2 -> different source hash) -- 28 fresh
        # nvrtc compiles per step on V100, ~270 s/step at BS=128.
        import numpy as _np
        self._bc1_inv = Tensor(_np.array([1.0], dtype=_np.float32))
        self._bc2_inv = Tensor(_np.array([1.0], dtype=_np.float32))
        self._bc1_inv.realize()
        self._bc2_inv.realize()
        for x in [*self.m, *self.v]:
            x.realize()

    def _step(self):
        # Bias correction values live in the 1-element bc1_inv/bc2_inv
        # tensors (allocated in __init__).  Compute the new values
        # host-side, then ten_write them into the existing buffers.  The
        # buffer addresses + kernel source stay step-invariant so the
        # nvrtc JIT cache hits every step.
        self.t += 1
        bc1 = 1.0 - self.b1 ** self.t
        bc2 = 1.0 - self.b2 ** self.t
        import numpy as _np
        from .thvm import Thvm as _Thvm
        _th = _Thvm()
        _th.ten_write(self._bc1_inv.term,
                      _np.array([1.0/bc1], dtype=_np.float32).tobytes())
        _th.ten_write(self._bc2_inv.term,
                      _np.array([1.0/bc2], dtype=_np.float32).tobytes())
        # Phase 1: the in-place m/v moment updates, THEN realize them so
        # each m[i]/v[i].term collapses from its ASSIGN node to a TEN
        # before phase 2 reads it.  Without this split, m_hat = m[i] *
        # (1/bc1) captures the *unrealized* ASSIGN(m[i], ...) node, p's
        # update embeds it, and realizing p re-fires m[i]'s in-place
        # write against the already-updated buffer (m -> 1.9x, or m picks
        # up v's value).  Realizing m,v first is backend-agnostic and
        # needs no scheduler bundling.
        mv = []
        for i, p in enumerate(self.params):
            g = p.grad
            if g is None:
                continue
            g = g.reshape(*p.shape) if p.shape else g
            self.m[i].assign(self.m[i] * self.b1 + g * (1.0 - self.b1))
            self.v[i].assign(self.v[i] * self.b2 + (g * g) * (1.0 - self.b2))
            mv += [self.m[i], self.v[i]]
        if mv:
            Tensor.realize(*mv)
        # Phase 2: the param updates read the now-realized m,v (plain TENs).
        out = []
        for i, p in enumerate(self.params):
            if p.grad is None:
                continue
            m_hat = self.m[i] * self._bc1_inv
            v_hat = self.v[i] * self._bc2_inv
            p.assign(p - m_hat * self.lr
                     * (v_hat.sqrt() + self.eps).reciprocal())
            out.append(p)
        return out


# Muon needs Newton-Schulz orthogonalization; thvm has no batched matmul
# iteration helper yet, so alias to Adam (the script falls back unless
# MUON=1).  AdamW == Adam without the decoupled weight decay term here.
Muon = Adam
AdamW = Adam
