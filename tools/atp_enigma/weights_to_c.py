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
#   Families  name:lane:mode:k:tau:coop[,...]  mode topk | lazy, k in
#             1..64, tau >= 0, coop >= 0 (every coop-th weight pick to the
#             plain engine, 0 never); the name * matches any --guide family
# A v1 head has n_x = 17 + 3 + Phi + Lanes inputs (src/atp/guide.c lists
# them), a topk18 head 18.
#
# Usage:
#   weights_to_c.py build ID=head.safetensors [ID=head.safetensors ...]
#                   [-o src/atp/guide_models.c]
#   weights_to_c.py example out.safetensors
#   weights_to_c.py parity head.safetensors dump.tsv [out.txt]
# build writes the C file of static const float arrays (hex float
# literals, exact) and the registry --guide-model=<id> selects from; the
# first head is the default.  example writes the tiny head the shipped
# src/atp/guide_models.c is built from:
#   weights_to_c.py example ex.safetensors
#   weights_to_c.py build example=ex.safetensors -o guide_models.c
# reproduces it byte for byte.  parity rescores a THVM_ATP_GUIDE_DUMP file
# with the head in float64: every candidate's correction (to 1e-9) and
# score, and every pick's decision (gate G2.4).

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
NAME_RE = re.compile(r"^[A-Za-z0-9_.*+-]+$")


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
        if not NAME_RE.match(name):
            die(f"family name '{name}'")
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
    if layout == "v1":
        n_phi, n_lane = int(meta.get("Phi", "0")), int(meta.get("Lanes", "0"))
        if not (0 <= n_phi <= PHI_MAX and 0 <= n_lane <= LANE_MAX and n_x == NC + NS + n_phi + n_lane):
            die(f"{path}: v1 wants {NC} + {NS} + Phi + Lanes inputs, has {n_x} (Phi {n_phi}, Lanes {n_lane})")
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
            "b2": t["b2"][1][0], "kappa": t["kappa"][1][0], "fam": parse_families(fam_spec)}
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
        if not sep or not NAME_RE.match(mid) or "*" in mid:
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
    hdr, rows = {}, []
    with open(args[1]) as f:
        for line in f:
            if line.startswith("#"):
                hdr = dict(tok.split("=", 1) for tok in line[1:].split() if "=" in tok)
                continue
            if line.strip():
                rows.append([float(v) for v in line.split("\t")])
    if not rows:
        die(f"{args[1]}: no candidate rows")
    if int(hdr.get("n_x", n_x)) != n_x:
        die(f"{args[1]}: dumped n_x {hdr.get('n_x')} but the head has {n_x}")
    a = np.array(rows, np.float64)
    idx, chosen, x = a[:, 1].astype(np.int64), a[:, 3].astype(np.int64), a[:, 4:4 + n_x]
    corr_c, score_c = a[:, 4 + n_x], a[:, 5 + n_x]
    z = (x - mu) / sd
    corr_py = np.maximum(z @ w1.T + b1, 0.0) @ w2 + b2
    zero = hdr.get("zero", "0") == "1"
    score_py = -kappa * x[:, 0] + (0.0 if zero else corr_py)
    d_corr = float(np.max(np.abs(corr_py - corr_c) / np.maximum(1.0, np.abs(corr_c))))
    d_score = float(np.max(np.abs(score_py - score_c) / np.maximum(1.0, np.abs(score_c))))
    lazy, tau = hdr.get("mode") == "lazy", float(hdr.get("tau", "0"))
    starts = list(np.nonzero(idx == 0)[0]) + [len(idx)]
    picks = bad = 0
    for s0, s1 in zip(starts[:-1], starts[1:]):
        sc = score_py[s0:s1]
        if lazy:
            ok = np.nonzero(sc >= -tau)[0]
            want = int(ok[0]) if len(ok) else int(np.argmax(sc))
        else:
            want = int(np.argmax(sc))
        got = np.nonzero(chosen[s0:s1])[0]
        picks += 1
        bad += int(len(got) != 1 or int(got[0]) != want)
    ok = d_corr <= 1e-9 and d_score <= 1e-9 and bad == 0
    lines = [f"parity: {len(rows)} candidate rows over {picks} picks ({args[1]}, "
             f"mode {hdr.get('mode', '?')}, zero {int(zero)})",
             f"max relative |C - Python| correction {d_corr:.3e}, score {d_score:.3e}",
             f"picks whose decision differs from the head's: {bad}",
             "PARITY " + ("OK" if ok else "FAIL")]
    txt = "\n".join(lines)
    print(txt)
    if len(args) == 3:
        with open(args[2], "w") as f:
            f.write(txt + "\n")
    sys.exit(0 if ok else 1)


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("build", "example", "parity"):
        die("usage: weights_to_c.py build|example|parity ...")
    {"build": cmd_build, "example": cmd_example, "parity": cmd_parity}[sys.argv[1]](sys.argv[2:])


if __name__ == "__main__":
    main()
