#!/usr/bin/env python3
# tools/dsv2/dsv2_pack_weights.py - DeepSeek-V2-Lite HF checkpoint -> flat weight pack
# for bench/dsv2_decode_bench.c (DSV2_WEIGHTS=<pack>).  No Wolfram / torch /
# safetensors-lib dependency: the sharded .safetensors are parsed directly
# (8-byte LE header length + JSON header + raw little-endian payloads) and bf16
# tensors are handled as uint16 words.
#
# The mapping mirrors tinygrad examples/deepseek_v2.py load_weights
# (deepseek_v2.py:1257-1272) + MLA.absorb (deepseek_v2.py:993-997):
#   * "model." prefix stripped, "lm_head.weight" kept as-is (deepseek_v2.py:1261-1264)
#   * per-layer experts STACKED into 3 tensors w_gate/w_up/w_down of shape
#     {E,1408,2048}/{E,1408,2048}/{E,2048,1408} from mlp.experts.{e}.{gate,up,down}_proj
#     (deepseek_v2.py:1266-1271, MoE.__init__ deepseek_v2.py:1133-1135)
#   * kv_b_proj ABSORB split: reshape {H*(dn+dv),kv} -> {H,dn+dv,kv};
#     w_uk = [:, :dn] contiguous, w_uv = [:, dn:] contiguous (deepseek_v2.py:993-997)
# plus the layout conventions of the C bench (bench/dsv2_decode_bench.c
# model_init): mm() consumes {in,out} matrices, so every nn.Linear weight
# ({out,in} in the checkpoint) is TRANSPOSED, and the norm/gate weights are
# widened bf16 -> f32 (exact; tinygrad keeps them bf16 and upcasts in-op).
#
# Pack format (little-endian):
#   char magic[8] = "DSV2PK01"
#   u64  n_tensors
#   n_tensors x { char name[48]; u32 dtype (0=bf16, 1=f32); u32 pad;
#                 u64 offset; u64 nbytes }          # offset from file start
#   payloads, each 64-byte aligned
#
# usage:
#   python3 tools/dsv2/dsv2_pack_weights.py --snap <hf snapshot dir> --out dsv2.pack
#   python3 tools/dsv2/dsv2_pack_weights.py --fake --out test.pack \
#       --dims dim=128,n_layers=4,n_heads=8,kv_lora=64,d_nope=32,d_rope=16,d_v=32,\
#              n_exp=8,top_k=3,moe_inter=64,dense_inter=256,vocab=1024
#     (--fake generates seeded random HF-LAYOUT weights and runs them through
#      the same mapping: a local smoke of the C loader, no checkpoint needed)
import argparse, json, struct, zlib
from pathlib import Path
import numpy as np

MAGIC = b"DSV2PK01"
DT_BF16, DT_F32 = 0, 1

# === sharded safetensors reader (raw; bf16 as uint16) ========================
def st_index(snap):
  """name -> (path, abs_offset, nbytes, dtype, shape) over all *.safetensors."""
  idx = {}
  for p in sorted(Path(snap).glob("*.safetensors")):
    with open(p, "rb") as f:
      hlen = struct.unpack("<Q", f.read(8))[0]
      hdr = json.loads(f.read(hlen))
    for name, spec in hdr.items():
      if name == "__metadata__": continue
      b, e = spec["data_offsets"]
      idx[name] = (p, 8 + hlen + b, e - b, spec["dtype"], spec["shape"])
  return idx

def st_read(idx, name):
  """Read one tensor: bf16 -> uint16 ndarray, f32 -> float32 ndarray."""
  path, off, nbytes, dtype, shape = idx[name]
  with open(path, "rb") as f:
    f.seek(off)
    raw = f.read(nbytes)
  if dtype == "BF16": return np.frombuffer(raw, dtype=np.uint16).reshape(shape)
  if dtype == "F32":  return np.frombuffer(raw, dtype=np.float32).reshape(shape)
  raise SystemExit(f"unsupported dtype {dtype} for {name}")

def bf16_to_f32(u16):
  assert u16.dtype == np.uint16, u16.dtype   # widening a non-bf16 tensor would corrupt silently
  return (u16.astype(np.uint32) << 16).view(np.float32)

def t(u16_2d):
  """Transpose a checkpoint {out,in} matrix to the mm() {in,out} layout."""
  return np.ascontiguousarray(u16_2d.T)

# === the pack tensor list ====================================================
def cfg_from_snapshot(snap):
  c = json.loads((Path(snap) / "config.json").read_text())
  return dict(dim=c["hidden_size"], n_layers=c["num_hidden_layers"],
              n_heads=c["num_attention_heads"], kv_lora=c["kv_lora_rank"],
              d_nope=c["qk_nope_head_dim"], d_rope=c["qk_rope_head_dim"],
              d_v=c["v_head_dim"], n_exp=c["n_routed_experts"],
              n_shared=c["n_shared_experts"], top_k=c["num_experts_per_tok"],
              moe_inter=c["moe_intermediate_size"], dense_inter=c["intermediate_size"],
              first_dense=c["first_k_dense_replace"], vocab=c["vocab_size"])

def pack_tensors(cfg, rd):
  """Yield (pack_name, dtype, ndarray) in a fixed order.  `rd(hf_name)` returns
  the checkpoint tensor (bf16 as uint16).  Names match the C loader
  (bench/dsv2_decode_bench.c pack_ld calls)."""
  d, hn = cfg["dim"], cfg["n_heads"]
  dn, dr, dv, kv = cfg["d_nope"], cfg["d_rope"], cfg["d_v"], cfg["kv_lora"]
  E, mi, si = cfg["n_exp"], cfg["moe_inter"], cfg["moe_inter"] * cfg["n_shared"]
  yield "embed", DT_BF16, rd("model.embed_tokens.weight")                    # {V,d}, row gather
  yield "norm", DT_F32, bf16_to_f32(rd("model.norm.weight"))
  yield "lm_head", DT_BF16, t(rd("lm_head.weight"))                          # {V,d} -> {d,V}
  for l in range(cfg["n_layers"]):
    p = f"model.layers.{l}."
    yield f"l{l}.win", DT_F32, bf16_to_f32(rd(p + "input_layernorm.weight"))
    yield f"l{l}.wq", DT_BF16, t(rd(p + "self_attn.q_proj.weight"))          # -> {d, H*(dn+dr)}
    yield f"l{l}.wkv", DT_BF16, t(rd(p + "self_attn.kv_a_proj_with_mqa.weight"))  # -> {d, kv+dr}
    yield f"l{l}.wkvn", DT_F32, bf16_to_f32(rd(p + "self_attn.kv_a_layernorm.weight"))
    # absorb split (deepseek_v2.py:993-997): {H*(dn+dv),kv} -> {H,dn+dv,kv},
    # w_uk = [:, :dn] ({H,dn,kv}, kernel index wuk[(h*dn+d)*kv+k]),
    # w_uv = [:, dn:] ({H,dv,kv}, kernel index wuv[(h*dv+m)*kv+k])
    kvb = rd(p + "self_attn.kv_b_proj.weight").reshape(hn, dn + dv, kv)
    yield f"l{l}.wuk", DT_BF16, np.ascontiguousarray(kvb[:, :dn])
    yield f"l{l}.wuv", DT_BF16, np.ascontiguousarray(kvb[:, dn:])
    yield f"l{l}.wo", DT_BF16, t(rd(p + "self_attn.o_proj.weight"))          # -> {H*dv, d}
    yield f"l{l}.wpost", DT_F32, bf16_to_f32(rd(p + "post_attention_layernorm.weight"))
    if l < cfg["first_dense"]:
      yield f"l{l}.wg1", DT_BF16, t(rd(p + "mlp.gate_proj.weight"))          # -> {d, dense_inter}
      yield f"l{l}.wu1", DT_BF16, t(rd(p + "mlp.up_proj.weight"))
      yield f"l{l}.wd1", DT_BF16, t(rd(p + "mlp.down_proj.weight"))          # -> {dense_inter, d}
    else:
      yield f"l{l}.gate", DT_F32, np.ascontiguousarray(bf16_to_f32(rd(p + "mlp.gate.weight")).T)  # -> {d,E} f32
      # expert stacks (deepseek_v2.py:1266-1271): checkpoint {mi,d}/{d,mi} rows
      # already match the kernel index wg[(se*mi+i)*d+k] / wd[(se*d+j)*mi+i]
      for proj, nm in (("gate_proj", "wg"), ("up_proj", "wu"), ("down_proj", "wd")):
        yield f"l{l}.{nm}", DT_BF16, np.stack(
          [rd(p + f"mlp.experts.{e}.{proj}.weight") for e in range(E)])
      yield f"l{l}.swg", DT_BF16, rd(p + "mlp.shared_experts.gate_proj.weight")  # {si,d}
      yield f"l{l}.swu", DT_BF16, rd(p + "mlp.shared_experts.up_proj.weight")
      yield f"l{l}.swd", DT_BF16, rd(p + "mlp.shared_experts.down_proj.weight")  # {d,si}

# === fake checkpoint (local smoke; HF layouts, seeded) =======================
def fake_reader(cfg, seed=7):
  d, hn = cfg["dim"], cfg["n_heads"]
  dn, dr, dv, kv = cfg["d_nope"], cfg["d_rope"], cfg["d_v"], cfg["kv_lora"]
  mi, si = cfg["moe_inter"], cfg["moe_inter"] * cfg["n_shared"]
  shapes = {"model.embed_tokens.weight": (cfg["vocab"], d), "model.norm.weight": (d,),
            "lm_head.weight": (cfg["vocab"], d)}
  for l in range(cfg["n_layers"]):
    p = f"model.layers.{l}."
    shapes[p + "input_layernorm.weight"] = (d,)
    shapes[p + "self_attn.q_proj.weight"] = (hn * (dn + dr), d)
    shapes[p + "self_attn.kv_a_proj_with_mqa.weight"] = (kv + dr, d)
    shapes[p + "self_attn.kv_a_layernorm.weight"] = (kv,)
    shapes[p + "self_attn.kv_b_proj.weight"] = (hn * (dn + dv), kv)
    shapes[p + "self_attn.o_proj.weight"] = (d, hn * dv)
    shapes[p + "post_attention_layernorm.weight"] = (d,)
    if l < cfg["first_dense"]:
      shapes[p + "mlp.gate_proj.weight"] = (cfg["dense_inter"], d)
      shapes[p + "mlp.up_proj.weight"] = (cfg["dense_inter"], d)
      shapes[p + "mlp.down_proj.weight"] = (d, cfg["dense_inter"])
    else:
      shapes[p + "mlp.gate.weight"] = (cfg["n_exp"], d)
      for e in range(cfg["n_exp"]):
        shapes[p + f"mlp.experts.{e}.gate_proj.weight"] = (mi, d)
        shapes[p + f"mlp.experts.{e}.up_proj.weight"] = (mi, d)
        shapes[p + f"mlp.experts.{e}.down_proj.weight"] = (d, mi)
      shapes[p + "mlp.shared_experts.gate_proj.weight"] = (si, d)
      shapes[p + "mlp.shared_experts.up_proj.weight"] = (si, d)
      shapes[p + "mlp.shared_experts.down_proj.weight"] = (d, si)
  def rd(name):
    rng = np.random.default_rng([seed, zlib.crc32(name.encode())])   # stable across runs (hash() is salted)
    f = rng.uniform(-0.05, 0.05, shapes[name]).astype(np.float32)
    if name.endswith("layernorm.weight") or name == "model.norm.weight":
      f = np.ones(shapes[name], dtype=np.float32)
    return (f.view(np.uint32) >> 16).astype(np.uint16)   # truncate-to-bf16 (fake data)
  return rd

# === writer ==================================================================
ENT_SZ, HDR_SZ = 48 + 4 + 4 + 8 + 8, 8 + 8

def n_tensors(cfg):
  """Tensor count is deterministic from the config (per layer: 8 attn/norm
  tensors + 3 dense-MLP or 7 MoE), so the table can be reserved up front and
  payloads STREAM one tensor at a time (peak RAM = one expert stack)."""
  return 3 + sum(8 + (3 if l < cfg["first_dense"] else 7) for l in range(cfg["n_layers"]))

def write_pack(out, cfg, items):
  n = n_tensors(cfg)
  table = []
  with open(out, "wb") as f:
    f.write(MAGIC)
    f.write(struct.pack("<Q", n))
    f.write(b"\0" * (ENT_SZ * n))   # table placeholder, filled after the payloads
    off = HDR_SZ + ENT_SZ * n
    for name, dt, arr in items:
      off = (off + 63) & ~63
      f.seek(off)
      data = np.ascontiguousarray(arr)
      f.write(data.tobytes())
      table.append((name, dt, off, data.nbytes))
      off += data.nbytes
    assert len(table) == n, (len(table), n)
    f.seek(HDR_SZ)
    for name, dt, o, nb in table:
      bname = name.encode()
      assert len(bname) < 48, name
      f.write(bname + b"\0" * (48 - len(bname)))
      f.write(struct.pack("<IIQQ", dt, 0, o, nb))
  print(f"wrote {out}: {n} tensors, {off/1e9:.2f} GB")

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--snap", help="HF snapshot dir (sharded safetensors + config.json)")
  ap.add_argument("--out", required=True)
  ap.add_argument("--fake", action="store_true", help="seeded random HF-layout weights (local smoke)")
  ap.add_argument("--dims", default="", help="fake mode: k=v,... config overrides")
  args = ap.parse_args()
  if args.fake:
    cfg = dict(dim=128, n_layers=4, n_heads=8, kv_lora=64, d_nope=32, d_rope=16, d_v=32,
               n_exp=8, n_shared=2, top_k=3, moe_inter=64, dense_inter=256,
               first_dense=1, vocab=1024)
    for kv in filter(None, args.dims.split(",")):
      k, v = kv.split("="); cfg[k.strip()] = int(v)
    rd = fake_reader(cfg)
  else:
    if not args.snap: raise SystemExit("--snap required without --fake")
    cfg = cfg_from_snapshot(args.snap)
    idx = st_index(args.snap)
    rd = lambda name: st_read(idx, name)
  print("config:", cfg)
  write_pack(args.out, cfg, pack_tensors(cfg, rd))

if __name__ == "__main__":
  main()
