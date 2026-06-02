"""thvm.nn.optim -- SGD / Adam / AdamW / LARS / Muon, mirroring tinygrad's nn.optim.

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
    def __init__(self, params, lr=0.001, b1=0.9, b2=0.999, eps=1e-8,
                 weight_decay=0.0, **_):
        super().__init__(params, lr)
        self.b1, self.b2, self.eps, self.wd = b1, b2, eps, weight_decay
        self.m = [Tensor.zeros(*(p.shape or (1,))) for p in self.params]
        self.v = [Tensor.zeros(*(p.shape or (1,))) for p in self.params]
        # Bias-correction state lives ON-GRAPH as 1-element Tensors b1_t,
        # b2_t initialised to ones, advanced INSIDE _step by a captured
        # kernel (b1_t.assign(b1_t * b1)).  Because b1_t/b2_t are in the
        # realize set the step returns, TinyJit captures their advance
        # kernel and re-fires it every replay -- so t (== the b1_t/b2_t
        # power) advances under JIT, matching tinygrad nn/optim.py:161-183.
        # A host-side `self.t += 1; ten_write(1/bc1)` would be DEAD on
        # replay (TinyJit re-fires only captured kernels, not the Python
        # body), pinning the correction at the step-1 value forever.
        import numpy as _np
        self.b1_t = Tensor(_np.array([1.0], dtype=_np.float32))
        self.b2_t = Tensor(_np.array([1.0], dtype=_np.float32))
        self.b1_t.realize()
        self.b2_t.realize()
        for x in [*self.m, *self.v]:
            x.realize()

    def _step(self):
        # Advance the on-graph bias-correction state (tinygrad
        # nn/optim.py:167-168: self.b1_t *= self.b1).  These assigns are
        # captured by TinyJit and re-fired each replay, so b1_t == b1**t
        # advances every step.
        self.b1_t.assign(self.b1_t * self.b1)
        self.b2_t.assign(self.b2_t * self.b2)
        # Phase 1: the in-place m/v moment updates plus the b1_t/b2_t
        # advance, THEN realize them so each m[i]/v[i]/b1_t/b2_t.term
        # collapses from its ASSIGN node to a TEN before phase 2 reads it.
        # Without this split, m_hat = m[i] / (1 - b1_t) captures the
        # *unrealized* ASSIGN(m[i], ...) node, p's update embeds it, and
        # realizing p re-fires m[i]'s in-place write against the already-
        # updated buffer (m -> 1.9x, or m picks up v's value).  Realizing
        # the moments + correction state first is backend-agnostic and
        # needs no scheduler bundling.
        mv = [self.b1_t, self.b2_t]
        for i, p in enumerate(self.params):
            g = p.grad
            if g is None:
                continue
            g = g.reshape(*p.shape) if p.shape else g
            self.m[i].assign(self.m[i] * self.b1 + g * (1.0 - self.b1))
            self.v[i].assign(self.v[i] * self.b2 + (g * g) * (1.0 - self.b2))
            mv += [self.m[i], self.v[i]]
        Tensor.realize(*mv)
        # Phase 2: the param updates read the now-realized m,v + b1_t,b2_t
        # (plain TENs).  tinygrad nn/optim.py:173-174,182: m_hat = m/(1-b1_t),
        # v_hat = v/(1-b2_t), p -= lr * m_hat / (sqrt(v_hat) + eps).
        out = []
        for i, p in enumerate(self.params):
            if p.grad is None:
                continue
            m_hat = self.m[i] / (1.0 - self.b1_t)
            v_hat = self.v[i] / (1.0 - self.b2_t)
            upd = m_hat * self.lr * (v_hat.sqrt() + self.eps).reciprocal()
            if self.wd:
                # Decoupled (AdamW) weight decay -- add wd*p to the update so
                # p -= lr*(m_hat/(sqrt(v_hat)+eps) + wd*p), matching tinygrad
                # nn/optim.py:175,182 (up += wd*t.detach()).  Gated on wd!=0 so
                # plain Adam (wd=0) keeps the exact m_hat*lr*recip expression
                # and stays byte-identical.  p.detach() so the decay carries no
                # autograd tape.
                upd = upd + p.detach() * (self.wd * self.lr)
            p.assign(p - upd)
            out.append(p)
        # b1_t/b2_t are advanced+realized in phase 1; they must ALSO be in
        # the returned realize set so TinyJit's capture-liveness pass keeps
        # their advance kernel (otherwise jit_capture_finalize marks it
        # replay_skip=1 and the correction freezes again under replay).
        return out + [self.b1_t, self.b2_t]


# AdamW = Adam with decoupled weight decay (tinygrad nn/optim.py:136-142:
# AdamW(...) = LAMB(weight_decay, adam=True)); default wd 0.01 like tinygrad.
def AdamW(params, lr=0.001, b1=0.9, b2=0.999, eps=1e-8, weight_decay=0.01, **_):
    return Adam(params, lr=lr, b1=b1, b2=b2, eps=eps, weight_decay=weight_decay)


# LARS / Muon share one trust-ratio + momentum + Newton-Schulz core,
# faithfully porting tinygrad nn/optim.py:99-133 (class LARS).  thvm's
# SGD/Adam/AdamW above stay in their own byte-stable eager paths; LARS is
# a separate class so adding it cannot perturb them.  In tinygrad SGD is
# LARS(tcoef=0) and Muon is LARS(ns_coefficients=...); thvm keeps the fast
# specialized SGD and exposes Muon as the LARS(NS) wrapper below.
class LARS(Optimizer):
    """Layer-wise Adaptive Rate Scaling (tinygrad nn/optim.py:99-133).

    Trust ratio ``r = tcoef * |w| / (|g| + wd*|w|)`` gated on |w|>0 & |g|>0
    (else 1.0) scales the momentum-SGD update.  With ``tcoef=0`` it is plain
    momentum SGD; with ``ns_coefficients`` set it is Muon (the momentum
    buffer is orthogonalized by Newton-Schulz before the update)."""

    def __init__(self, params, lr=0.001, momentum=0.9, weight_decay=1e-4,
                 ns_steps=0, ns_coefficients=None, nesterov=False,
                 classic=True, pre_wd=True, tcoef=0.001, **_):
        if momentum < 0:
            raise ValueError(f"Invalid momentum value: {momentum}")
        super().__init__(params, lr)
        self.momentum, self.wd = momentum, weight_decay
        self.ns_steps, self.ns_coefficients = ns_steps, ns_coefficients
        self.nesterov, self.classic = nesterov, classic
        self.pre_wd, self.tcoef = pre_wd, tcoef
        # Momentum buffers, zero-initialised + realized (tinygrad's
        # self.b = self._new_optim_param(); zeros on the first run).
        self.b = ([Tensor.zeros(*(p.shape or (1,))) for p in self.params]
                  if self.momentum else [])
        for b in self.b:
            b.realize()

    def _step(self):
        # The trust ratio, weight decay, momentum update, Newton-Schulz
        # orthogonalization, and the in-place param assign, ported from
        # tinygrad LARS._step (nn/optim.py:113-133).
        #
        # The non-NS path stays fully eager-inline like thvm's SGD: build g,
        # update self.b[i] in place, p.assign(p - g), and return both the
        # momentum buffers and the params so the caller realizes them TOGETHER
        # (one fire scope).  An eager `Tensor.realize` of the momentum buffer
        # mid-step (before reading it back into the param update) desyncs the
        # next step's momentum recurrence and freezes the param -- SGD's inline
        # one-shot realize is the correct shape.
        #
        # Newton-Schulz is the exception: it reads self.b[i] through a deep
        # matmul chain, and a chain over an unrealized in-place ASSIGN node
        # stalls realize (the same hazard the Adam class splits around with its
        # `Tensor.realize(*mv)` barrier).  So when ns_coefficients is set we
        # first build + realize the momentum buffers, then run NS on the now-
        # plain-TEN buffers in a second pass.
        out = []
        ns = bool(self.ns_coefficients)
        pre = []          # NS path scratch: (p, r, g_after_momentum)
        moments = []
        for i, p in enumerate(self.params):
            g = p.grad
            if g is None:
                pre.append(None)
                continue
            g = g.reshape(*p.shape) if p.shape else g
            if self.tcoef != 0:
                # trust ratio (tinygrad nn/optim.py:117-119)
                r1 = p.detach().square().sum().sqrt()
                r2 = g.square().sum().sqrt()
                r = (r1 > 0).where(
                    (r2 > 0).where(
                        (r1 * self.tcoef) * (r2 + r1 * self.wd).reciprocal(),
                        1.0),
                    1.0)
            else:
                r = 1.0
            if self.pre_wd and self.wd > 0:
                g = g + p.detach() * self.wd
            # classic momentum applies the learning rate before momentum.
            if self.classic:
                g = (g * r * self.lr) if isinstance(r, Tensor) else (g * (r * self.lr))
            if self.momentum:
                self.b[i].assign(self.b[i] * self.momentum + g)
                moments.append(self.b[i])
                g = (g + self.b[i] * self.momentum) if self.nesterov else self.b[i]
            if ns:
                pre.append((p, r, g))
                continue
            # popular momentum applies the learning rate after momentum.
            if not self.classic:
                g = (g * r * self.lr) if isinstance(r, Tensor) else (g * (r * self.lr))
            p.assign(p - g)
            if self.momentum:
                out.append(self.b[i])
            out.append(p)
        if not ns:
            return out
        # NS path: realize the momentum buffers, then orthogonalize.
        if moments:
            Tensor.realize(*moments)
        out = list(moments)
        for entry in pre:
            if entry is None:
                continue
            p, r, g = entry
            # orthogonalize the (reshaped-2D) momentum update, reshape back
            # (tinygrad nn/optim.py:127).
            gshape = g.shape
            g = g.reshape(gshape[0], -1).newton_schulz(
                self.ns_steps, self.ns_coefficients).reshape(*gshape)
            # tinygrad nn/optim.py:129 reassigns its LOCAL `t = t.detach() *
            # (1 - wd*lr)` for the "muon post-momentum weight decay", but that
            # decayed `t` is then used ONLY for `g.cast(t.dtype)` (line 132) --
            # the param update applied by schedule_step is `tt.assign(tt.detach()
            # - up)` against the ORIGINAL param `tt` (line 60, _apply_update),
            # so the decay never reaches the param value.  Porting the actual
            # tinygrad behaviour (thvm is f32-only so the dtype hook is moot):
            # no param decay -- p <- p - g.  (Verified: applying the decay puts
            # thvm off tinygrad by exactly |p|*wd*lr.)
            # popular momentum applies the learning rate after momentum.
            if not self.classic:
                g = (g * r * self.lr) if isinstance(r, Tensor) else (g * (r * self.lr))
            p.assign(p - g)
            out.append(p)
        return out


# Muon applies Newton-Schulz orthogonalization to the momentum update --
# tinygrad nn/optim.py:86-97 (Muon(...) = LARS(pre_wd=False, classic=False,
# nesterov=True, ns_coef)).  Coefficients/steps default to the Muon quintic
# (3.4445,-4.775,2.0315), 5 steps; momentum 0.95, weight_decay 0.1, matching
# tinygrad's defaults.  (tinygrad's "post-momentum weight decay" is dead code
# -- see LARS._step -- so weight_decay does not affect the Muon param update.)
def Muon(params, lr=0.001, momentum=0.95, weight_decay=0.1, ns_steps=5,
         ns_coefficients=(3.4445, -4.775, 2.0315), nesterov=True, **_):
    return LARS(params, lr=lr, momentum=momentum, weight_decay=weight_decay,
                ns_steps=ns_steps, ns_coefficients=ns_coefficients,
                nesterov=nesterov, classic=False, pre_wd=False, tcoef=0.0)
