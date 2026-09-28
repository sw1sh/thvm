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

