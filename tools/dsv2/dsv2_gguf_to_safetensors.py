#!/usr/bin/env python3
"""Convert an ollama DeepSeek-V2-Lite q8_0 GGUF into a HuggingFace-layout bf16
safetensors file that thvm's dsvHfInit (wl/.../DeepSeekV2/Decode.wl) can load.

GGUF stores deepseek2 tensors as reversed-ne, which loads as the HF [out, in]
layout dsvHfWeights already consumes -- so the only work is: dequantize Q8_0 ->
f32 -> bf16 (norms stay f32), rename GGUF -> HF, and split the pre-stacked
expert tensors into per-expert tensors (dsvHfWeights re-stacks them).  Streaming:
each GGUF tensor is dequantized once, so peak RAM is one tensor.

Usage:
  python3 tools/dsv2/dsv2_gguf_to_safetensors.py \
      --gguf ~/.ollama/models/blobs/sha256-458d... \
      --out  ~/.cache/thvm/dsv2_q8_hf.safetensors [--layers N]
"""
import argparse, json, os, struct
import numpy as np

QK8 = 32  # Q8_0 block: 1 f16 scale + 32 int8

GLOBAL = {
    "token_embd.weight": ("model.embed_tokens.weight", "bf16"),
    "output.weight": ("lm_head.weight", "bf16"),
    "output_norm.weight": ("model.norm.weight", "f32"),
}
PER = {
    "attn_norm.weight": ("input_layernorm.weight", "f32"),
    "attn_q.weight": ("self_attn.q_proj.weight", "bf16"),
    "attn_kv_a_mqa.weight": ("self_attn.kv_a_proj_with_mqa.weight", "bf16"),
    "attn_kv_a_norm.weight": ("self_attn.kv_a_layernorm.weight", "f32"),
    "attn_kv_b.weight": ("self_attn.kv_b_proj.weight", "bf16"),
    "attn_output.weight": ("self_attn.o_proj.weight", "bf16"),
    "ffn_norm.weight": ("post_attention_layernorm.weight", "f32"),
    "ffn_gate.weight": ("mlp.gate_proj.weight", "bf16"),
    "ffn_up.weight": ("mlp.up_proj.weight", "bf16"),
    "ffn_down.weight": ("mlp.down_proj.weight", "bf16"),
    "ffn_gate_inp.weight": ("mlp.gate.weight", "f32"),
    "ffn_gate_shexp.weight": ("mlp.shared_experts.gate_proj.weight", "bf16"),
    "ffn_up_shexp.weight": ("mlp.shared_experts.up_proj.weight", "bf16"),
    "ffn_down_shexp.weight": ("mlp.shared_experts.down_proj.weight", "bf16"),
}
EXP = {"ffn_gate_exps.weight": "gate_proj", "ffn_up_exps.weight": "up_proj",
       "ffn_down_exps.weight": "down_proj"}


def entries(gname, shape):
    """gname + HF shape (reversed ne) -> [(hf_name, dtype, out_shape, expert_idx)]."""
    if gname in GLOBAL:
        hf, dt = GLOBAL[gname]
        return [(hf, dt, shape, None)]
    _, n, role = gname.split(".", 2)
    p = f"model.layers.{n}."
    if role in PER:
        hf, dt = PER[role]
        return [(p + hf, dt, shape, None)]
    if role in EXP:
        # keep experts pre-stacked as one {e, inter, dim} tensor -- dsvHfWeights
        # flattens it with a single reshape (thvm has no native CAT, so the
        # per-expert Join path is O(e^2)).
        return [(p + f"mlp.experts.{EXP[role]}.weight", "bf16", shape, None)]
    raise ValueError(f"unmapped {gname}")


def bf16_bytes(f32):
    """f32 -> bf16 raw bytes (top 16 bits round-to-nearest-even), matching
    thvm src/dtype/fp_convert.c."""
    u = np.ascontiguousarray(f32, dtype="<f4").view(np.uint32)
    nan = ((u & 0x7F800000) == 0x7F800000) & ((u & 0x007FFFFF) != 0)
    rnd = ((u + 0x7FFF + ((u >> 16) & 1)) >> 16).astype(np.uint16)
    nanbf = (((u & 0x80000000) | 0x7FC00000) >> 16).astype(np.uint16)
    return np.where(nan, nanbf, rnd).astype("<u2").tobytes()


def read_header(f):
    rd = lambda fmt: struct.unpack("<" + fmt, f.read(struct.calcsize(fmt)))[0]
    rstr = lambda: f.read(rd("Q")).decode("utf-8", "replace")

    def rval(t):
        if t == 8: return rstr()
        if t == 6: return rd("f")
        if t == 7: return bool(rd("b"))
        if t in (0, 1): return rd("b" if t == 1 else "B")
        if t in (2, 3): return rd("h" if t == 3 else "H")
        if t in (4, 5): return rd("i" if t == 5 else "I")
        if t in (10, 11): return rd("q" if t == 11 else "Q")
        if t == 12: return rd("d")
        if t == 9:
            at = rd("I"); [rval(at) for _ in range(rd("Q"))]; return None
        raise ValueError(t)

    assert f.read(4) == b"GGUF"
    rd("I"); n_tensors = rd("Q"); n_kv = rd("Q")
    align = 32
    for _ in range(n_kv):
        k = rstr(); v = rval(rd("I"))
        if k == "general.alignment": align = v
    tens = {}
    for _ in range(n_tensors):
        name = rstr(); nd = rd("I"); dims = [rd("Q") for _ in range(nd)]
        tens[name] = (dims, rd("I"), rd("Q"))
    ds = f.tell()
    if ds % align: ds += align - (ds % align)
    return tens, ds


def load_f32(mm, ds, dims, qt, off):
    n = int(np.prod(dims))
    base = ds + off
    if qt == 0:
        arr = np.frombuffer(mm, dtype="<f4", count=n, offset=base).astype(np.float32)
    elif qt == 8:
        nb = n // QK8
        raw = np.frombuffer(mm, dtype=np.uint8, count=nb * (2 + QK8), offset=base).reshape(nb, 2 + QK8)
        d = raw[:, 0:2].copy().view(np.float16).reshape(nb, 1).astype(np.float32)
        q = raw[:, 2:].view(np.int8).astype(np.float32)
        arr = (d * q).reshape(-1)
    else:
        raise ValueError(f"quant {qt}")
    return arr.reshape(list(reversed(dims)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gguf", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--layers", type=int, default=27)
    args = ap.parse_args()
    gguf, out = os.path.expanduser(args.gguf), os.path.expanduser(args.out)

    with open(gguf, "rb") as f:
        tens, ds = read_header(f)
    mm = np.memmap(gguf, dtype=np.uint8, mode="r")

    order = [g for g in ("token_embd.weight", "output.weight", "output_norm.weight") if g in tens]
    for n in range(args.layers):
        order += sorted(t for t in tens if t.startswith(f"blk.{n}."))

    DT = {"bf16": ("BF16", 2), "f32": ("F32", 4)}
    header, cursor, plan = {}, 0, {}
    for g in order:
        dims, qt, off = tens[g]
        shape = list(reversed(dims))
        plan[g] = []
        for hf, dt, osh, ei in entries(g, shape):
            st, w = DT[dt]
            nb = int(np.prod(osh)) * w
            header[hf] = {"dtype": st, "shape": list(osh), "data_offsets": [cursor, cursor + nb]}
            plan[g].append((hf, dt, ei))
            cursor += nb
    hj = json.dumps(header, separators=(",", ":")).encode()
    hj += b" " * ((8 - len(hj) % 8) % 8)
    print(f"tensors={len(header)} data={cursor/1e9:.2f}GB header={len(hj)}B out={out}")

    with open(out, "wb") as w:
        w.write(struct.pack("<Q", len(hj))); w.write(hj)
        done = 0
        for g in order:
            arr = load_f32(mm, ds, *tens[g])
            for hf, dt, ei in plan[g]:
                a = arr[ei] if ei is not None else arr
                w.write(a.astype("<f4").tobytes() if dt == "f32" else bf16_bytes(a))
            done += 1
            if done % 30 == 0:
                print(f"  {done}/{len(order)} gguf tensors")
    print("done:", out, f"{os.path.getsize(out)/1e9:.2f}GB")


if __name__ == "__main__":
    main()
