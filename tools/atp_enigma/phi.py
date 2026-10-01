#!/usr/bin/env python3
"""Problem features phi(p): the Python side of FindProof --problem-features.

The selection layer's trainer reads phi(p) through load(), never by
column position: the header must name exactly the columns pinned here,
in this order, and every row must carry meta_phi_version ==
ATP_PHI_VERSION.  Values are float32, as in the C struct (AtpPhi in
src/thvm.h); a trainer must not recompute any of them, so that training
and the deployed layer read the same numbers (plan gate G2.4).

  python3 tools/atp_enigma/phi.py FILE...

checks FindProof --problem-features --header output (a header line, then
rows; header lines repeated by concatenation are skipped) and that every
value reads back as the float32 C printed (%.9g).  Exit 1 on a mismatch.
"""

import sys

import numpy as np

# Must change with ATP_PHI_VERSION and PHI_NAMES / PHI_META_NAMES
# (tools/atp_cli/problem_features.c).
ATP_PHI_VERSION = 1

PHI_COLUMNS = (
    "goal_existential", "equals_goal", "ifeq_dispatcher", "horn_slice",
    "def_unfold", "ax_ground_compound", "conj_cone_partial", "distrib_law",
    "fe_live", "sine",
    "ac_pairs", "comm_syms", "assoc_syms", "idem_syms", "unit_syms",
    "inverse_syms", "distrib_syms",
    "axioms", "hypotheses", "goals", "syms", "syms_ar0", "syms_ar1",
    "syms_ar2", "syms_ar3", "syms_ar4p", "max_arity", "var_names",
    "ax_size_max", "ax_size_mean", "ax_depth_max", "ax_depth_mean",
    "ax_vars_max", "ax_vars_mean", "ax_ground",
    "goal_size_max", "goal_size_mean", "goal_depth_max", "goal_vars_max",
    "conj_sym_frac",
)

# Never selection inputs: the layout version and the TPTP header fields
# (training files have them, deployed problems do not).
META_COLUMNS = (
    "meta_phi_version", "meta_rating", "meta_spc_known", "meta_spc_uns",
    "meta_spc_sat", "meta_spc_open", "meta_spc_ueq",
)

HEADER = ("problem",) + PHI_COLUMNS + META_COLUMNS


class PhiError(ValueError):
    pass


def load(path):
    """(names, phi, meta) of a --problem-features file: phi is a float32
    array of shape (rows, len(PHI_COLUMNS)), meta (rows, len(META_COLUMNS))."""
    names, phi, meta = [], [], []
    seen_header = False
    with open(path) as fp:
        for n, line in enumerate(fp, 1):
            f = line.rstrip("\n").split("\t")
            if f == [""]:
                continue
            if f[0] == "problem":
                if tuple(f) != HEADER:
                    raise PhiError(f"{path}:{n}: the header is not the pinned "
                                   f"phi(p) layout version {ATP_PHI_VERSION}")
                seen_header = True
                continue
            if not seen_header:
                raise PhiError(f"{path}:{n}: a row before the header")
            if len(f) != len(HEADER):
                raise PhiError(f"{path}:{n}: {len(f)} columns, want {len(HEADER)}")
            v = np.array(f[1:], dtype=np.float32)
            for text, x in zip(f[1:], v):
                if "%.9g" % float(x) != text:
                    raise PhiError(f"{path}:{n}: {text} does not read back as "
                                   "the float32 C printed")
            m = v[len(PHI_COLUMNS):]
            if m[0] != np.float32(ATP_PHI_VERSION):
                raise PhiError(f"{path}:{n}: meta_phi_version {m[0]:g}, want "
                               f"{ATP_PHI_VERSION}")
            names.append(f[0])
            phi.append(v[:len(PHI_COLUMNS)])
            meta.append(m)
    shape = (0, len(PHI_COLUMNS))
    return (names,
            np.stack(phi) if phi else np.zeros(shape, dtype=np.float32),
            np.stack(meta) if meta else np.zeros((0, len(META_COLUMNS)),
                                                 dtype=np.float32))


def main():
    if len(sys.argv) < 2:
        print(__doc__.strip())
        sys.exit(2)
    ok = True
    for path in sys.argv[1:]:
        try:
            names, phi, _meta = load(path)
        except (OSError, PhiError) as e:
            print(f"phi.py: {e}")
            ok = False
            continue
        print(f"phi.py: {path}: {len(names)} rows, {phi.shape[1]} phi(p) "
              f"columns, layout version {ATP_PHI_VERSION}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
