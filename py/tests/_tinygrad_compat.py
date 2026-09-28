"""Version shims for the tinygrad reference side of the parity tests."""


def tg_training():
    """tinygrad training mode, as a context manager / decorator.

    tinygrad reads the TRAINING ContextVar (#16703; the Tensor.training alias
    was removed in #16774, after which assigning it silently does nothing).
    Checkouts older than #16703 read Tensor.training via Tensor.train().
    """
    import tinygrad
    from tinygrad import helpers
    if hasattr(helpers, "TRAINING"):
        return helpers.Context(TRAINING=1)
    return tinygrad.Tensor.train()


def tg_muon_decays() -> bool:
    """Whether this tinygrad applies Muon's post-momentum weight decay.

    Before #17709 LARS computed the decayed param and dropped it, so a
    wd>0 Muon matched wd=0; thvm ports the fixed behaviour.
    """
    import numpy as np
    from tinygrad import Tensor
    from tinygrad.nn import optim
    w0 = np.ones((2, 2), np.float32)
    got = []
    with tg_training():
        for wd in (0.0, 1.0):
            w = Tensor(w0.copy())
            opt = optim.Muon([w], lr=0.1, weight_decay=wd)
            opt.zero_grad()
            (w * Tensor(np.arange(4, dtype=np.float32).reshape(2, 2))).sum().backward()
            opt.step()
            got.append(w.numpy())
    return bool(np.abs(got[1] - got[0]).max() > 1e-3)
