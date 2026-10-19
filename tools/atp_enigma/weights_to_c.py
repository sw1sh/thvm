#!/usr/bin/env python3
# Compiled-in heads for the ENIGMA selection layer (src/atp/guide.c).
#
# A head is a safetensors file of f32 tensors mu, sd [n_x], w1 [hid, n_x]
# (row-major), b1 [hid], w2 [hid] or [1, hid], b2 [1], kappa [1], whose
# __metadata__ names the input layout and the integrations:
#   Layout    v1 | topk18 (absent: topk18, a tools/atp_enigma/topk_net.py
#             export, whose K gives the one family *:0:topk:K:0:0)
#   Phi       v1: problem-feature columns
#   Lanes     v1: lane one-hot columns
#   LaneNames v1, optional: the Lanes lane names in one-hot order, comma
#             separated, as FindProof --list-lanes prints them; a twin
#             running lane L (FINDPROOF_SLICE, "enigma-<L>" reads as L)
#             takes L's column, any other lane its family's
#   Families  name:lane:mode:k:tau:coop[,...]  mode topk | lazy, k in
#             1..64, tau >= 0, coop >= 0 (every coop-th weight selection
#             to the plain engine, 0 never); the name * matches any
#             --guide family; lane is the fallback one-hot index
# A v1 head has n_x = 17 + 3 + Phi + Lanes inputs (src/atp/guide.c lists
# them), a topk18 head 18.  Model ids, family and lane names match
# [A-Za-z0-9_.-]{1,47} (a family name may be *).
#
# Usage:
#   weights_to_c.py build ID=head.safetensors [ID=head.safetensors ...]
#                   [-o src/atp/guide_models.c]
#   weights_to_c.py example out.safetensors
#   weights_to_c.py harvest out.safetensors
#   weights_to_c.py parity head.safetensors dump.tsv [out.txt]
#   weights_to_c.py featparity run.npz dump.tsv [out.txt]
#   weights_to_c.py lanes head.safetensors listed.txt
# build writes the C file of static const float arrays (hex float
# literals, exact) and the registry --guide-model=<id> selects from; the
# first head is the default.  example writes the tiny head that keeps the
# build compiling, harvest the loop-0 harvest head (every phi(p) column,
# zero correction, top-64); the shipped src/atp/guide_models.c holds both
# and the trained loop-0 head:
#   weights_to_c.py example ex.safetensors
#   weights_to_c.py harvest hv.safetensors
#   weights_to_c.py build example=ex.safetensors harvest=hv.safetensors \
#     loop0=tools/atp_enigma/models/loop0.safetensors -o guide_models.c
# reproduces it byte for byte.  parity rescores a THVM_ATP_GUIDE_DUMP file
# with the head in float64: every candidate's correction (to 1e-9) and
# score, and every pick's decision (gate G2.4); a lazy dump must carry
# rank input 0.  featparity rebuilds the inputs of the dump's first
# session from the same run's history export (np0_extract.py .npz of a
# THVM_ATP_HISTORY_RATE=1 THVM_ATP_HISTORY_CPFEAT=1 run, no negative
# subsampling): columns 0-12 and 15-18 and the top-K candidate sets, to
# 1e-9, against the engine's queue (a dead parent's CP stays queued until
# a pop or a 'D' deletion removes it).  Columns 13, 14 (goal symbols), 19 (term-heap growth), phi and
# the lane are not in the export; training reads them from the dump.
# lanes checks a head's LaneNames against FindProof --list-lanes output.

import json
import math
import re
import struct
import sys

NC, NS = 17, 3
KMAX, HMAX, PHI_MAX, LANE_MAX = 64, 128, 64, 64
MAX_BYTES = 2 * 1024 * 1024
LAYOUTS = {"v1": "ATP_GUIDE_LAYOUT_V1", "topk18": "ATP_GUIDE_LAYOUT_TOPK18"}
MODES = {"topk": "ATP_GUIDE_TOPK", "lazy": "ATP_GUIDE_LAZY"}
NAME_RE = re.compile(r"^[A-Za-z0-9_.-]{1,47}$")
FAMILY_RE = re.compile(r"^(\*|[A-Za-z0-9_.-]{1,47})$")


def die(msg):
    sys.exit("weights_to_c: " + msg)


def read_safetensors(path):
    with open(path, "rb") as f:
        n = struct.unpack("<Q", f.read(8))[0]
        head = json.loads(f.read(n))
        data = f.read()
    meta = head.pop("__metadata__", None) or {}
    out = {}
    for name, d in head.items():
        if d["dtype"] != "F32":
            die(f"{path}: tensor {name} is {d['dtype']}, not F32")
        a, b = d["data_offsets"]
        out[name] = (list(d["shape"]), list(struct.unpack("<%df" % ((b - a) // 4), data[a:b])))
    return out, meta


def write_safetensors(path, tensors, meta):
    head, blobs, off = {}, [], 0
    for name, (shape, vals) in tensors.items():
        raw = struct.pack("<%df" % len(vals), *vals)
        head[name] = {"dtype": "F32", "shape": shape, "data_offsets": [off, off + len(raw)]}
        blobs.append(raw)
        off += len(raw)
    head["__metadata__"] = meta
    js = json.dumps(head, separators=(",", ":")).encode()
    js += b" " * ((8 - len(js) % 8) % 8)
    with open(path, "wb") as f:
        f.write(struct.pack("<Q", len(js)))
        f.write(js)
        f.write(b"".join(blobs))


def parse_families(spec):
    fams = []
    for item in spec.split(","):
        parts = item.split(":")
        if len(parts) != 6:
            die(f"family '{item}' is not name:lane:mode:k:tau:coop")
        name, lane, mode, k, tau, coop = parts
        if not FAMILY_RE.match(name):
            die(f"family name '{name}' is not * or [A-Za-z0-9_.-]{{1,47}}")
        if mode not in MODES:
            die(f"family {name}: mode '{mode}' is not topk or lazy")
        lane, k, coop, tau = int(lane), int(k), int(coop), float(tau)
        if lane < 0 or not 1 <= k <= KMAX or coop < 0 or not (math.isfinite(tau) and tau >= 0.0):
            die(f"family {name}: lane {lane}, k {k}, tau {tau}, coop {coop} out of range")
        fams.append({"name": name, "lane": lane, "mode": mode, "k": k, "tau": tau, "coop": coop})
    if len({f["name"] for f in fams}) != len(fams):
        die(f"duplicate family names in '{spec}'")
    return fams


def load_head(path):
    t, meta = read_safetensors(path)
    for name in ("mu", "sd", "w1", "b1", "w2", "b2", "kappa"):
        if name not in t:
            die(f"{path}: no tensor {name}")
    mu, sd = t["mu"][1], t["sd"][1]
    n_x, hid = len(mu), len(t["b1"][1])
    layout = meta.get("Layout", "topk18")
    if layout not in LAYOUTS:
        die(f"{path}: layout '{layout}'")
    lane_names = None
    if layout == "v1":
        n_phi, n_lane = int(meta.get("Phi", "0")), int(meta.get("Lanes", "0"))
        if not (0 <= n_phi <= PHI_MAX and 0 <= n_lane <= LANE_MAX and n_x == NC + NS + n_phi + n_lane):
            die(f"{path}: v1 wants {NC} + {NS} + Phi + Lanes inputs, has {n_x} (Phi {n_phi}, Lanes {n_lane})")
        if meta.get("LaneNames"):
            lane_names = meta["LaneNames"].split(",")
            if len(lane_names) != n_lane or len(set(lane_names)) != n_lane:
                die(f"{path}: LaneNames wants {n_lane} distinct names, has {lane_names}")
            for ln in lane_names:
                if not NAME_RE.match(ln):
                    die(f"{path}: lane name '{ln}' is not [A-Za-z0-9_.-]{{1,47}}")
        fam_spec = meta.get("Families")
        if fam_spec is None:
            die(f"{path}: a v1 head names its Families")
    else:
        n_phi, n_lane = 0, 0
        if n_x != 18:
            die(f"{path}: topk18 wants 18 inputs, has {n_x}")
        fam_spec = meta.get("Families") or "*:0:topk:%d:0:0" % int(meta.get("K", "32"))
    if not 1 <= hid <= HMAX:
        die(f"{path}: hidden {hid} not in 1..{HMAX}")
    if t["mu"][0] != [n_x] or t["sd"][0] != [n_x] or t["w1"][0] != [hid, n_x]:
        die(f"{path}: mu / sd / w1 shapes {t['mu'][0]} {t['sd'][0]} {t['w1'][0]}")
    if t["w2"][0] not in ([hid], [1, hid]) or t["b2"][0] != [1] or t["kappa"][0] != [1]:
        die(f"{path}: w2 / b2 / kappa shapes {t['w2'][0]} {t['b2'][0]} {t['kappa'][0]}")
    head = {"layout": layout, "n_x": n_x, "n_phi": n_phi, "n_lane": n_lane, "hid": hid,
            "mu": mu, "sd": sd, "w1": t["w1"][1], "b1": t["b1"][1], "w2": t["w2"][1],
            "b2": t["b2"][1][0], "kappa": t["kappa"][1][0], "fam": parse_families(fam_spec),
            "lanes": lane_names}
    for name in ("mu", "sd", "w1", "b1", "w2"):
        if not all(math.isfinite(v) for v in head[name]):
            die(f"{path}: {name} has a non-finite value")
    if not all(v > 0.0 for v in sd):
        die(f"{path}: sd has a value <= 0")
    if not (math.isfinite(head["b2"]) and math.isfinite(head["kappa"]) and head["kappa"] >= 0.0):
        die(f"{path}: b2 {head['b2']} / kappa {head['kappa']} (kappa must be >= 0)")
    return head


def c_float(v):
    if not math.isfinite(v):
        die(f"non-finite value {v}")
    h = float(v).hex()
    sign = ""
    if h[0] == "-":
        sign, h = "-", h[1:]
    mant, exp = h[2:].split("p")
    ip, _, fp = mant.partition(".")
    fp = fp.rstrip("0")
    return sign + "0x" + ip + ("." + fp if fp else "") + "p" + exp


def c_array(name, vals):
    return ([f"static const float {name}[{len(vals)}] = {{"]
            + [f"  {c_float(v)}f," for v in vals] + ["};", ""])


def emit(heads):
    total = sum(4 * (2 * h["n_x"] + h["hid"] * h["n_x"] + 2 * h["hid"] + 2) for _, h in heads)
    if total > MAX_BYTES:
        die(f"heads total {total} bytes, over the {MAX_BYTES}-byte budget")
    out = ["// GENERATED by tools/atp_enigma/weights_to_c.py -- do not edit.",
           "//",
           "// The compiled-in heads of the ENIGMA selection layer (src/atp/guide.c);",
           "// --guide-model=<id> selects one, the first is the default.  Hex float",
           "// literals hold each f32 exactly."]
    for mid, h in heads:
        out.append(f"//   {mid}: layout {h['layout']}, {h['n_x']} inputs ({h['n_phi']} phi, "
                   f"{h['n_lane']} lanes), hidden {h['hid']}, families "
                   + " ".join(f["name"] for f in h["fam"]))
    out.append("")
    for i, (mid, h) in enumerate(heads):
        p = f"atp_guide_m{i}"
        for name in ("mu", "sd", "w1", "b1", "w2"):
            out += c_array(f"{p}_{name}", h[name])
        out.append(f"static const AtpGuideFamily {p}_fam[{len(h['fam'])}] = {{")
        for f in h["fam"]:
            out.append(f"  {{\"{f['name']}\", {f['lane']}u, {MODES[f['mode']]}, {f['k']}u, "
                       f"{c_float(f['tau'])}, {f['coop']}u}},")
        out += ["};", ""]
        if h["lanes"] is not None:
            out.append(f"static const char *const {p}_lanes[{len(h['lanes'])}] = {{")
            out += [f"  \"{ln}\"," for ln in h["lanes"]]
            out += ["};", ""]
    out.append(f"static const AtpGuideModel atp_guide_model_table[{len(heads)}] = {{")
    for i, (mid, h) in enumerate(heads):
        p = f"atp_guide_m{i}"
        out += ["  {",
                f"    .id     = \"{mid}\",",
                f"    .layout = {LAYOUTS[h['layout']]},",
                f"    .n_x    = {h['n_x']}u,",
                f"    .n_phi  = {h['n_phi']}u,",
                f"    .n_lane = {h['n_lane']}u,",
                f"    .hid    = {h['hid']}u,",
                f"    .mu     = {p}_mu,",
                f"    .sd     = {p}_sd,",
                f"    .w1     = {p}_w1,",
                f"    .b1     = {p}_b1,",
                f"    .w2     = {p}_w2,",
                f"    .b2     = {c_float(h['b2'])}f,",
                f"    .kappa  = {c_float(h['kappa'])}f,",
                f"    .n_fam  = {len(h['fam'])}u,",
                f"    .fam    = {p}_fam,",
                f"    .lanes  = {p + '_lanes' if h['lanes'] is not None else 'NULL'},",
                "  },"]
    out += ["};",
            "",
            "// The compiled-in heads; the first is the default.",
            "static const AtpGuideModel *atp_guide_models(u32 *n) {",
            f"  *n = {len(heads)}u;",
            "  return atp_guide_model_table;",
            "}"]
    return "\n".join(out) + "\n"


def cmd_build(args):
    out_path = "src/atp/guide_models.c"
    specs = []
    i = 0
    while i < len(args):
        if args[i] == "-o":
            out_path = args[i + 1]
            i += 2
            continue
        mid, sep, path = args[i].partition("=")
        if not sep or not NAME_RE.match(mid):
            die(f"'{args[i]}' is not ID=head.safetensors")
        specs.append((mid, path))
        i += 1
    if not specs:
        die("build needs at least one ID=head.safetensors")
    if len({mid for mid, _ in specs}) != len(specs):
        die("duplicate model ids")
    heads = [(mid, load_head(path)) for mid, path in specs]
    with open(out_path, "w") as f:
        f.write(emit(heads))
    print(f"weights_to_c: {len(heads)} head(s) -> {out_path}")


def example_head():
    n_x, hid = NC + NS + 0 + 2, 4
    w1 = [[0.0] * n_x for _ in range(hid)]
    w1[0][2] = -0.5     # smaller than the problem's typical pick
    w1[1][8] = -0.25    # lower goal weight
    w1[2][13] = 0.5     # goal symbols
    w1[3][12] = 0.125   # selections since push
    tensors = {
        "mu": ([n_x], [0.0] * n_x),
        "sd": ([n_x], [1.0] * n_x),
        "w1": ([hid, n_x], [v for row in w1 for v in row]),
        "b1": ([hid], [0.0] * hid),
        "w2": ([hid], [0.5] * hid),
        "b2": ([1], [0.0]),
        "kappa": ([1], [1.0]),
    }
    meta = {"Layout": "v1", "Phi": "0", "Lanes": "2", "Families": "*:0:topk:16:0:0"}
    return tensors, meta


def cmd_example(args):
    if len(args) != 1:
        die("example needs out.safetensors")
    tensors, meta = example_head()
    write_safetensors(args[0], tensors, meta)
    print(f"weights_to_c: example head -> {args[0]}")


# ATP_PHI_N (src/thvm.h), the length of phi.py's PHI_COLUMNS.
PHI_N = 40
HARVEST_K = 64


def harvest_head():
    """The loop-0 harvest head: layout v1 with every phi(p) column and no
    lane columns (a dump names its lane), one hidden unit, w1 = w2 = 0 and
    kappa 1.  Its correction is 0 with or without --guide-zero, so it picks
    the engine's root, and its top-64 dump records the engine's 64 best
    candidates at every scored pick."""
    n_x, hid = NC + NS + PHI_N, 1
    tensors = {
        "mu": ([n_x], [0.0] * n_x),
        "sd": ([n_x], [1.0] * n_x),
        "w1": ([hid, n_x], [0.0] * (hid * n_x)),
        "b1": ([hid], [0.0]),
        "w2": ([hid], [0.0]),
        "b2": ([1], [0.0]),
        "kappa": ([1], [1.0]),
    }
    meta = {"Layout": "v1", "Phi": str(PHI_N), "Lanes": "0",
            "Families": "*:0:topk:%d:0:0" % HARVEST_K}
    return tensors, meta


def cmd_harvest(args):
    if len(args) != 1:
        die("harvest needs out.safetensors")
    tensors, meta = harvest_head()
    write_safetensors(args[0], tensors, meta)
    print(f"weights_to_c: harvest head -> {args[0]}")


def read_dump(path):
    """The sessions of a THVM_ATP_GUIDE_DUMP file (one '# guide' header per
    armed engine, and a '# guide end picks=P rows=R' footer when the
    engine was freed): [(header dict, float64 rows)].  A session's footer,
    when present, is the header's "end" entry (a dict); one without it was
    cut short and its last line may be partial, so a line of the wrong
    width ends its rows."""
    import numpy as np
    sessions = []
    with open(path) as f:
        for line in f:
            if line.startswith("# guide end"):
                if sessions:
                    sessions[-1][0]["end"] = dict(tok.split("=", 1) for tok in line[11:].split()
                                                  if "=" in tok)
                continue
            if line.startswith("#"):
                hdr = dict(tok.split("=", 1) for tok in line[1:].split() if "=" in tok)
                sessions.append((hdr, []))
                continue
            if line.strip():
                if not sessions:
                    die(f"{path}: candidate rows before the first '# guide' header")
                hdr, rows = sessions[-1]
                fields = line.rstrip("\n").split("\t")
                want = 6 + int(hdr.get("n_x", len(fields) - 6))
                if len(fields) != want or hdr.get("cut"):
                    hdr["cut"] = "1"
                    continue
                rows.append([float(v) for v in fields])
    return [(hdr, np.array(rows, np.float64)) for hdr, rows in sessions if rows]


def pick_starts(idx):
    """Row ranges of the scored picks (each starts at evaluation index 0)."""
    import numpy as np
    starts = list(np.nonzero(idx == 0)[0]) + [len(idx)]
    return list(zip(starts[:-1], starts[1:]))


def cmd_parity(args):
    import numpy as np
    if len(args) not in (2, 3):
        die("parity needs head.safetensors dump.tsv [out.txt]")
    h = load_head(args[0])
    n_x = h["n_x"]
    mu, sd = np.array(h["mu"], np.float64), np.array(h["sd"], np.float64)
    w1 = np.array(h["w1"], np.float64).reshape(h["hid"], n_x)
    b1, w2 = np.array(h["b1"], np.float64), np.array(h["w2"], np.float64).reshape(-1)
    b2, kappa = float(h["b2"]), float(h["kappa"])
    sessions = read_dump(args[1])
    if not sessions:
        die(f"{args[1]}: no candidate rows")
    lines, all_ok = [], True
    for si, (hdr, a) in enumerate(sessions):
        if int(hdr.get("n_x", n_x)) != n_x:
            die(f"{args[1]}: session {si} dumped n_x {hdr.get('n_x')} but the head has {n_x}")
        idx, chosen, x = a[:, 1].astype(np.int64), a[:, 3].astype(np.int64), a[:, 4:4 + n_x]
        corr_c, score_c = a[:, 4 + n_x], a[:, 5 + n_x]
        z = (x - mu) / sd
        corr_py = np.maximum(z @ w1.T + b1, 0.0) @ w2 + b2
        # The layer reads a non-finite correction as 0 (and counts it).
        corr_py = np.where(np.isfinite(corr_py), corr_py, 0.0)
        zero = hdr.get("zero", "0") == "1"
        score_py = -kappa * x[:, 0] + (0.0 if zero else corr_py)
        d_corr = float(np.max(np.abs(corr_py - corr_c) / np.maximum(1.0, np.abs(corr_c))))
        d_score = float(np.max(np.abs(score_py - score_c) / np.maximum(1.0, np.abs(score_c))))
        lazy, tau = hdr.get("mode") == "lazy", float(hdr.get("tau", "0"))
        # A lazy candidate is the engine's next one when it is scored.
        rank_bad = int(np.count_nonzero(x[:, 0] != 0.0)) if lazy else 0
        picks = bad = 0
        for s0, s1 in pick_starts(idx):
            sc = score_py[s0:s1]
            if lazy:
                ok = np.nonzero(sc >= -tau)[0]
                want = int(ok[0]) if len(ok) else int(np.argmax(sc))
            else:
                want = int(np.argmax(sc))
            got = np.nonzero(chosen[s0:s1])[0]
            picks += 1
            bad += int(len(got) != 1 or int(got[0]) != want)
        ok = d_corr <= 1e-9 and d_score <= 1e-9 and bad == 0 and rank_bad == 0
        all_ok = all_ok and ok
        lines += [f"parity session {si}: {len(a)} candidate rows over {picks} picks ({args[1]}, "
                  f"mode {hdr.get('mode', '?')}, zero {int(zero)}, lane {hdr.get('lane', '?')})",
                  f"  max relative |C - Python| correction {d_corr:.3e}, score {d_score:.3e}",
                  f"  picks whose decision differs from the head's: {bad}",
                  f"  lazy rows with a non-zero rank input: {rank_bad}"]
    lines.append("PARITY " + ("OK" if all_ok else "FAIL"))
    txt = "\n".join(lines)
    print(txt)
    if len(args) == 3:
        with open(args[2], "w") as f:
            f.write(txt + "\n")
    sys.exit(0 if all_ok else 1)


NORM_WIN, GROWTH_WIN = 256, 64
# The normalized columns of x and their raw sources, in the layer's norm
# order (src/atp/guide.c ATP_GUIDE_NORM_COL / atp_guide_norm_raw).
NORM_COL = (1, 2, 3, 4, 5, 6, 7, 8, 11, 15)


def cmd_featparity(args):
    """The first dump session's inputs against the run's history export."""
    import numpy as np
    if len(args) not in (2, 3):
        die("featparity needs run.npz dump.tsv [out.txt]")
    d = np.load(args[0])
    seq = d["seq"].astype(np.int64)
    if len(seq) > 1 and np.any(np.diff(seq) <= 0):
        die(f"{args[0]}: seq is not strictly increasing")
    has_push = d["has_push"].astype(bool)
    pri0 = np.where(has_push, d["pri_push"], d["pri"]).astype(np.int64)
    push_step = d["push_step"].astype(np.int64)
    selected_at = d["selected_at"].astype(np.int64)
    if "d_seq" not in d:
        die(f"{args[0]}: no queue deletions (re-extract with this np0_extract.py)")
    # The engine's queue, not end_step's liveness: a CP whose parent died
    # stays queued (and a top-K candidate, counted in n_cps) until a pop
    # or a deletion removes it.
    q_end = np.where(selected_at >= 0, selected_at, np.int64(1) << 40)
    d_seq = d["d_seq"].astype(np.int64)
    jd = np.searchsorted(seq, d_seq)
    okd = (jd < len(seq)) & (seq[np.minimum(jd, len(seq) - 1)] == d_seq)
    np.minimum.at(q_end, jd[okd], d["d_step"].astype(np.int64)[okd])
    depth = d["depth"].astype(np.float64)
    bounds = d["bounds"].astype(np.int64)
    if "cpfeat" not in d:
        die(f"{args[0]}: no cpfeat (run the export with THVM_ATP_HISTORY_CPFEAT=1)")
    cpfeat = d["cpfeat"].astype(np.float64)
    has_cf = d["has_cpfeat"].astype(bool)
    w_seq, w_step, w_pri = (d["w_seq"].astype(np.int64), d["w_step"].astype(np.int64),
                            d["w_pri"].astype(np.int64))
    sessions = read_dump(args[1])
    if not sessions:
        die(f"{args[1]}: no candidate rows")
    hdr, a = sessions[0]
    if hdr.get("layout") != "v1":
        die(f"{args[1]}: layout {hdr.get('layout')}; topk18 dumps: topk_net.py featparity")
    lazy = hdr.get("mode") == "lazy"
    t_col, idx, seq_c, chosen = (a[:, 0].astype(np.int64), a[:, 1].astype(np.int64),
                                 a[:, 2].astype(np.int64), a[:, 3].astype(np.int64))
    X = a[:, 4:]

    def pri_at(t):
        pri = pri0.copy()
        m = w_step <= t - 1
        if m.any():
            ix = np.searchsorted(seq, w_seq[m])
            okm = (ix < len(seq)) & (seq[np.minimum(ix, len(seq) - 1)] == w_seq[m])
            pri[ix[okm]] = w_pri[m][okm]
        return pri

    def raw(j, pri_t):
        f = cpfeat[j]
        return np.log1p(np.array([pri_t[j], f[0], f[1], f[2], f[3], f[5], f[6], f[7], f[13],
                                  depth[j]], np.float64))

    norm_m, norm_n = np.zeros(len(NORM_COL)), np.zeros(len(NORM_COL), np.int64)
    dmax = {c: 0.0 for c in list(range(13)) + [15, 16, 17, 18]}
    missing = set_bad = pick_bad = no_s = 0
    blocks = pick_starts(idx)
    last_block = {}
    for bi, (s0, s1) in enumerate(blocks):
        last_block[int(t_col[s0])] = bi
    # An orphan popped while pick t is retried is deleted at step t - 1
    # (selections so far), after the blocks of t that scored it: it is
    # queued for its own block and the earlier blocks of t.
    popped_from = {}
    for bi, (s0, s1) in enumerate(blocks):
        hit = np.nonzero(chosen[s0:s1])[0]
        if len(hit) == 1:
            popped_from[int(seq_c[s0 + hit[0]])] = bi
    for bi, (s0, s1) in enumerate(blocks):
        t = int(t_col[s0])
        pri_t = pri_at(t)
        queued = (push_step <= t - 1) & (q_end >= t)
        late = np.nonzero((push_step <= t - 1) & (q_end == t - 1))[0]
        for j in late:
            bj = popped_from.get(int(seq[j]))
            if bj is not None and bj >= bi and int(t_col[blocks[bj][0]]) == t:
                queued[j] = True
        alive = np.nonzero(queued)[0]
        js = np.searchsorted(seq, seq_c[s0:s1])
        found = (js < len(seq)) & (seq[np.minimum(js, len(seq) - 1)] == seq_c[s0:s1])
        if not found.all():
            missing += int(np.count_nonzero(~found))
            continue
        j0 = int(js[0])
        v0 = raw(j0, pri_t)
        for i in range(len(NORM_COL)):
            if not has_cf[j0] and i not in (0, len(NORM_COL) - 1):
                continue
            norm_n[i] = min(norm_n[i] + 1, NORM_WIN)
            norm_m[i] += (v0[i] - norm_m[i]) / norm_n[i]
        if not lazy:
            key = (pri_t[alive].astype(np.uint64) << np.uint64(32)) | seq[alive].astype(np.uint64)
            top = alive[np.argsort(key, kind="stable")[:s1 - s0]]
            set_bad += int(not np.array_equal(seq[top], seq_c[s0:s1]))
        n = t - 1
        w = min(n, GROWTH_WIN)
        growth = None
        if w == 0:
            growth = 0.0
        elif n < len(bounds):
            growth = float(bounds[n] - bounds[n - w]) / float(w)
        else:
            no_s += 1
        for r in range(s0, s1):
            j = int(js[r - s0])
            x = np.zeros(19)
            v = raw(j, pri_t)
            for i, c in enumerate(NORM_COL):
                x[c] = v[i] - norm_m[i]
            f = cpfeat[j]
            x[0] = 0.0 if lazy else math.log1p(r - s0)
            x[9], x[10] = f[11], f[12]
            x[12] = math.log1p(t - push_step[j])
            x[16] = 0.0
            if not has_cf[j]:
                for c in range(2, 15):
                    if c != 12:
                        x[c] = 0.0
                x[16] = 1.0
            x[17] = math.log1p(len(alive))
            if growth is not None:
                x[18] = math.log1p(growth)
            for c in dmax:
                if c == 18 and growth is None:
                    continue
                dmax[c] = max(dmax[c], abs(x[c] - X[r, c]))
        if last_block[t] == bi:
            hit = np.nonzero(chosen[s0:s1])[0]
            sel = np.nonzero(selected_at == t)[0]
            pick_bad += int(len(hit) != 1 or len(sel) != 1 or seq[sel[0]] != seq_c[s0 + hit[0]])
    worst = max(dmax.values())
    ok = missing == 0 and set_bad == 0 and pick_bad == 0 and worst <= 1e-9
    lines = [f"featparity: {len(blocks)} scored picks, {len(a)} rows of session 0 of {args[1]} "
             f"(mode {hdr.get('mode', '?')}) against {args[0]}",
             f"rows whose seq the export lacks: {missing}",
             f"top-K picks whose candidate set differs from the rebuilt queue's: {set_bad}",
             f"picks whose choice is not the export's selection: {pick_bad}",
             f"picks past the export's last selection (column 18 unchecked): {no_s}",
             "max |C - rebuilt| per column: " + " ".join(f"x{c}={v:.3g}" for c, v in dmax.items()),
             "columns 13, 14, 19, phi and lane: dump only (not in the export)",
             "FEATPARITY " + ("OK" if ok else "FAIL")]
    txt = "\n".join(lines)
    print(txt)
    if len(args) == 3:
        with open(args[2], "w") as f:
            f.write(txt + "\n")
    sys.exit(0 if ok else 1)


def cmd_lanes(args):
    """A head's LaneNames are FindProof lanes, in --list-lanes order."""
    if len(args) != 2:
        die("lanes needs head.safetensors listed.txt")
    h = load_head(args[0])
    with open(args[1]) as f:
        listed = [ln.strip() for ln in f if ln.strip()]
    if h["lanes"] is None:
        die(f"{args[0]}: no LaneNames")
    pos = []
    for ln in h["lanes"]:
        if ln not in listed:
            die(f"{args[0]}: lane '{ln}' is not a FindProof lane")
        pos.append(listed.index(ln))
    if pos != sorted(pos):
        die(f"{args[0]}: LaneNames {h['lanes']} are not in --list-lanes order")
    for fam in h["fam"]:
        if fam["lane"] >= len(h["lanes"]) and fam["name"] != "*":
            print(f"weights_to_c: family {fam['name']} falls back to no lane column")
    print(f"LANES OK: {len(h['lanes'])} lanes, in --list-lanes order")


def main():
    cmds = {"build": cmd_build, "example": cmd_example, "harvest": cmd_harvest,
            "parity": cmd_parity, "featparity": cmd_featparity, "lanes": cmd_lanes}
    if len(sys.argv) < 2 or sys.argv[1] not in cmds:
        die("usage: weights_to_c.py " + "|".join(cmds) + " ...")
    cmds[sys.argv[1]](sys.argv[2:])


if __name__ == "__main__":
    main()
