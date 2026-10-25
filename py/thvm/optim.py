"""SGD, Adam, AdamW, LARS and Muon, as tinygrad's nn/optim.py: schedule_step() returns
the tensors whose realize applies one step and step() realizes them.  Tensors with
is_param False (BatchNorm running stats) are realized but not updated."""
from __future__ import annotations

from .helpers import TRAINING
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
        # backward() finds a param by its TAG_TEN leaf, so a lazy factory init
        # (a UOp graph) is realized first.
        for x in params:
            x.realize()
        # tinygrad nn/optim.py:13-15: params are updated, buffers only realized.
        self.params = _dedup([x for x in params if x.is_param])
        self.buffers = _dedup([x for x in params if not x.is_param])
        assert self.params, "optimizer must have at least one param"
        self.lr = lr

    def zero_grad(self):
        for p in self.params:
            p.grad = None

    def step(self):
        # tinygrad nn/optim.py:39.
        Tensor.realize(*self.schedule_step())

    def schedule_step(self):
        """The tensors whose realize applies one step, its in-place assigns lazy
        (tinygrad nn/optim.py:41-57); the grads are realized first."""
        if not TRAINING:
            raise RuntimeError(
                f"TRAINING={TRAINING.value}, TRAINING must be enabled to use the "
                "optimizer.  Consider using Context(TRAINING=1) before calling "
                "Optimizer.step().")
        # The grads realize first, materializing the activations that read the params
        # before the assigns overwrite them (for loss.realize(*opt.schedule_step())).
        grads = [p.grad for p in self.params if p.grad is not None]
        if grads:
            Tensor.realize(*grads)
        # tinygrad nn/optim.py:57: the buffers (BatchNorm running stats) ride
        # the step's realize, so their assigns land every step.
        return self._step() + self.buffers

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
        self.b1, self.b2, self.eps, self.wd, self.adam = b1, b2, eps, weight_decay, True
        self.m = [Tensor.zeros(*(p.shape or (1,))) for p in self.params]
        self.v = [Tensor.zeros(*(p.shape or (1,))) for p in self.params]
        # b1**t and b2**t are on-graph Tensors that _step advances by an assign, so
        # a TinyJit replay advances them too (tinygrad nn/optim.py:157, 163-164).
        import numpy as _np
        self.b1_t = Tensor(_np.array([1.0], dtype=_np.float32))
        self.b2_t = Tensor(_np.array([1.0], dtype=_np.float32))
        self.b1_t.realize()
        self.b2_t.realize()
        for x in [*self.m, *self.v]:
            x.realize()

    def _step(self):
        self.b1_t.assign(self.b1_t * self.b1)
        self.b2_t.assign(self.b2_t * self.b2)
        # The moments realize before the param updates read them: an update built
        # on an unrealized ASSIGN(m[i], ...) would fire that write again with p.
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
        # tinygrad nn/optim.py:169-171, 178.
        out = []
        for i, p in enumerate(self.params):
            if p.grad is None:
                continue
            m_hat = self.m[i] / (1.0 - self.b1_t)
            v_hat = self.v[i] / (1.0 - self.b2_t)
            if self.adam:
                upd = m_hat * self.lr * (v_hat.sqrt() + self.eps).reciprocal()
                if self.wd:
                    # Decoupled (AdamW) decay, up += wd * p (tinygrad nn/optim.py:171);
                    # gated so plain Adam keeps its exact expression.
                    upd = upd + p.detach() * (self.wd * self.lr)
            else:
                # LAMB scales the update by the trust ratio ||p|| / ||up|| (nn/optim.py:171-178).
                up = m_hat / (v_hat.sqrt() + self.eps) + p.detach() * self.wd
                r1, r2 = p.detach().square().sum().sqrt(), up.square().sum().sqrt()
                upd = up * (r1 > 0).where((r2 > 0).where(r1 / r2, 1.0), 1.0) * self.lr
            p.assign(p - upd)
            out.append(p)
        # tinygrad nn/optim.py:179 returns b1_t and b2_t with the step's tensors.
        return out + [self.b1_t, self.b2_t]


class LAMB(Adam):
    """tinygrad's LAMB (nn/optim.py:147-179): Adam's moments, the update scaled by the trust
    ratio ||p|| / ||up||; adam=True drops the ratio."""

    def __init__(self, params, lr=0.001, b1=0.9, b2=0.999, eps=1e-6, weight_decay=0.0,
                 adam=False, **_):
        if weight_decay < 0:
            raise ValueError(f"Invalid weight_decay value: {weight_decay}")
        super().__init__(params, lr, b1, b2, eps, weight_decay)
        self.adam = adam


# tinygrad nn/optim.py:132-138: Adam with decoupled weight decay, default 0.01.
def AdamW(params, lr=0.001, b1=0.9, b2=0.999, eps=1e-8, weight_decay=0.01, **_):
    return Adam(params, lr=lr, b1=b1, b2=b2, eps=eps, weight_decay=weight_decay)


# tinygrad's SGD is LARS(tcoef=0) (nn/optim.py:74-80); thvm's SGD is its own class.
class LARS(Optimizer):
    """Layer-wise Adaptive Rate Scaling (tinygrad nn/optim.py:95-129): tcoef=0 makes
    it momentum SGD, ns_coefficients Muon."""

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
        self.b = ([Tensor.zeros(*(p.shape or (1,))) for p in self.params]
                  if self.momentum else [])
        for b in self.b:
            b.realize()

    def _step(self):
        # tinygrad nn/optim.py:109-129.  The momentum buffers realize with the
        # params, except under Newton-Schulz: its deep chain over an unrealized
        # ASSIGN stalls realize, so they realize first.
        out = []
        ns = bool(self.ns_coefficients)
        pre = []          # Newton-Schulz path: (i, p, r, g before its momentum term)
        moments = []
        for i, p in enumerate(self.params):
            g = p.grad
            if g is None:
                pre.append(None)
                continue
            g = g.reshape(*p.shape) if p.shape else g
            if self.tcoef != 0:
                # trust ratio (tinygrad nn/optim.py:112-116)
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
            if ns:
                pre.append((i, p, r, g))
                continue
            if self.momentum:
                g = (g + self.b[i] * self.momentum) if self.nesterov else self.b[i]
            # popular momentum applies the learning rate after momentum.
            if not self.classic:
                g = (g * r * self.lr) if isinstance(r, Tensor) else (g * (r * self.lr))
            g = self._post_wd(p, g)
            p.assign(p - g)
            if self.momentum:
                out.append(self.b[i])
            out.append(p)
        if not ns:
            return out
        # The nesterov term reads b[i] after this realize: built on the unrealized
        # ASSIGN, it would make p's realize fire the momentum write again.
        if moments:
            Tensor.realize(*moments)
        out = list(moments)
        for entry in pre:
            if entry is None:
                continue
            i, p, r, g = entry
            if self.momentum:
                g = (g + self.b[i] * self.momentum) if self.nesterov else self.b[i]
            # orthogonalize the update as a 2-D matrix (tinygrad nn/optim.py:123)
            gshape = g.shape
            g = g.reshape(gshape[0], -1).newton_schulz(
                self.ns_steps, self.ns_coefficients).reshape(*gshape)
            # popular momentum applies the learning rate after momentum.
            if not self.classic:
                g = (g * r * self.lr) if isinstance(r, Tensor) else (g * (r * self.lr))
            g = self._post_wd(p, g)
            p.assign(p - g)
            out.append(p)
        return out

    def _post_wd(self, p, g):
        # Muon's post-momentum decay, after the lr scaling so the trust ratio
        # leaves it alone: p <- p*(1 - lr*wd) - g (tinygrad nn/optim.py:126-127).
        if not self.pre_wd and self.wd > 0:
            g = g + p.detach() * (self.wd * self.lr)
        return g


# tinygrad nn/optim.py:83-93: LARS with Newton-Schulz on the momentum update and
# post-momentum weight decay.
def Muon(params, lr=0.001, momentum=0.95, weight_decay=0.1, ns_steps=5,
         ns_coefficients=(3.4445, -4.775, 2.0315), nesterov=True, **_):
    return LARS(params, lr=lr, momentum=momentum, weight_decay=weight_decay,
                ns_steps=ns_steps, ns_coefficients=ns_coefficients,
                nesterov=nesterov, classic=False, pre_wd=False, tcoef=0.0)
