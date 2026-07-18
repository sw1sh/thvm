(* Gguf.wl -- native import of an ollama / llama.cpp DeepSeek-V2 GGUF into the
   thvm decode weights, with no intermediate file.  GGUF stores deepseek2 tensors
   as reversed-ne (= the HF [out, in] layout dsvHfWeights consumes) with Q8_0
   quantization (blocks of 32: one f16 scale + 32 int8).  Each tensor is mmapped
   in place (TTensorMMap, the TSafeTensorLoad primitive); Q8_0 weights are
   dequantized to bf16 by a custom kernel (dsvDequantQ8), F32 norms map straight
   through.  dsvGgufInit then reuses dsvHfWeights + dsvState, so the decode path
   is identical to the safetensors route. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

dsvGgufHeader::usage = "dsvGgufHeader[path] parses a GGUF file header and returns <|\"DataStart\" -> byte offset of the tensor data section, \"Align\" -> alignment, \"Tensors\" -> <|name -> <|\"Dims\" -> ne list, \"Quant\" -> ggml type code, \"Offset\" -> data-relative byte offset|>|>|>.  Only the tensor table is retained; metadata values are parsed only to advance the stream.";
dsvGgufWeights::usage = "dsvGgufWeights[cfg, path] mmaps a DeepSeek-V2 GGUF and returns the decode weight association (name -> TTerm in HF layout), dequantizing Q8_0 tensors to bf16 on cfg's device.  Feeds dsvHfWeights' HF layout, so dsvGgufInit reuses the safetensors decode path.";
dsvGgufInit::usage = "dsvGgufInit[cfg, path] returns the decode state association over the GGUF checkpoint at path (native import; the GGUF analog of dsvHfInit).";

Begin["`Private`"];

(* === GGUF header parse ============================================ *)

(* ggml quant type codes we handle: 0 = F32, 8 = Q8_0 (1 f16 scale + 32 i8). *)
$ggmlF32 = 0;
$ggmlQ8 = 8;

(* GGUF metadata value types (enough to SKIP any value to reach the tensor
   table).  Widths in bytes for the fixed-size scalars. *)
$ggufScalarBytes = <|0 -> 1, 1 -> 1, 2 -> 2, 3 -> 2, 4 -> 4, 5 -> 4, 6 -> 4, 7 -> 1, 10 -> 8, 11 -> 8, 12 -> 8|>;

ggufStr[strm_] := Module[{n = BinaryRead[strm, "UnsignedInteger64", ByteOrdering -> -1]},
    FromCharacterCode[BinaryReadList[strm, "Byte", n], "UTF8"]]

(* Skip one metadata value of type t (advancing strm); returns the value only
   for the alignment key, which the caller reads by name. *)
ggufSkipVal[strm_, t_] := Which[
    t === 8, ggufStr[strm],
    t === 9,
        Module[{at = BinaryRead[strm, "UnsignedInteger32", ByteOrdering -> -1],
                 n = BinaryRead[strm, "UnsignedInteger64", ByteOrdering -> -1]},
            Do[ggufSkipVal[strm, at], {n}]],
    KeyExistsQ[$ggufScalarBytes, t], BinaryReadList[strm, "Byte", $ggufScalarBytes[t]],
    True, $Failed
]

dsvGgufHeader[path_String] := Module[{strm, ver, nTensors, nKv, align = 32, tensors, k, t, v, name, nd, dims, qt, off, ds},
    strm = OpenRead[path, BinaryFormat -> True];
    If[ FromCharacterCode @ BinaryReadList[strm, "Byte", 4] =!= "GGUF", Close[strm]; Return[$Failed]];
    ver = BinaryRead[strm, "UnsignedInteger32", ByteOrdering -> -1];
    nTensors = BinaryRead[strm, "UnsignedInteger64", ByteOrdering -> -1];
    nKv = BinaryRead[strm, "UnsignedInteger64", ByteOrdering -> -1];
    Do[
        k = ggufStr[strm];
        t = BinaryRead[strm, "UnsignedInteger32", ByteOrdering -> -1];
        v = ggufSkipVal[strm, t];
        If[k === "general.alignment", align = FromDigits[Reverse[v], 256]],
        {nKv}
    ];
    tensors = Association @ Table[
        name = ggufStr[strm];
        nd = BinaryRead[strm, "UnsignedInteger32", ByteOrdering -> -1];
        dims = BinaryReadList[strm, "UnsignedInteger64", nd, ByteOrdering -> -1];
        qt = BinaryRead[strm, "UnsignedInteger32", ByteOrdering -> -1];
        off = BinaryRead[strm, "UnsignedInteger64", ByteOrdering -> -1];
        name -> <|"Dims" -> dims, "Quant" -> qt, "Offset" -> off|>,
        {nTensors}
    ];
    ds = StreamPosition[strm];
    Close[strm];
    ds = ds + Mod[-ds, align];
    <|"DataStart" -> ds, "Align" -> align, "Tensors" -> tensors|>
]

(* === Q8_0 dequant kernel ========================================= *)

(* out[i] = f16_to_f32(scale[i/32]) * (int8)q[i], where the raw Q8_0 stream is
   contiguous 34-byte blocks (2-byte f16 scale then 32 int8).  `N` = element
   count.  Both dialects carry a half->float converter and the f2bf writer
   (bit-identical to src/dtype/fp_convert.c and the DSV2 kernels). *)
$dsvDeqCpu = "
#include <stdint.h>
typedef unsigned short bf16t;
static inline float h2f(unsigned short h) {
  unsigned int s = (unsigned int)(h & 0x8000u) << 16;
  unsigned int e = (h >> 10) & 0x1f, m = h & 0x3ff, o;
  if (e == 0) { if (m == 0) o = s; else { e = 127 - 15 + 1; while (!(m & 0x400)) { m <<= 1; e--; } m &= 0x3ff; o = s | (e << 23) | (m << 13); } }
  else if (e == 0x1f) o = s | 0x7f800000u | (m << 13);
  else o = s | ((e - 15 + 127) << 23) | (m << 13);
  union { unsigned int u; float f; } v; v.u = o; return v.f;
}
static inline bf16t f2bf(float f) { union { unsigned int u; float f; } v; v.f = f; unsigned int b = v.u;
  if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0u) return (bf16t)(((b & 0x80000000u) | 0x7f800000u | 0x400000u) >> 16);
  return (bf16t)((b + 0x7fffu + ((b >> 16) & 1u)) >> 16); }
void dequant_q8(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  bf16t *out = (bf16t *)bufs[0];
  const unsigned char *raw = (const unsigned char *)bufs[1];
  for (long i = 0; i < `N`; i++) {
    long blk = i / 32, within = i % 32;
    const unsigned char *bp = raw + blk * 34;
    unsigned short h = (unsigned short)(bp[0] | (bp[1] << 8));
    signed char q = (signed char)bp[2 + within];
    out[i] = f2bf(h2f(h) * (float)q);
  }
}
"

$dsvDeqMetal = "
#include <metal_stdlib>
using namespace metal;
typedef ushort bf16t;
static inline bf16t f2bf(float f) { uint b = as_type<uint>(f);
  if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0u) return (bf16t)(((b & 0x80000000u) | 0x7f800000u | 0x400000u) >> 16);
  return (bf16t)((b + 0x7fffu + ((b >> 16) & 1u)) >> 16); }
kernel void dequant_q8(device bf16t* out [[buffer(0)]], device const uchar* raw [[buffer(1)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  long i = (long)(tgid.x * ntg.x + tid3.x);
  if (i >= `N`) return;
  long blk = i / 32, within = i % 32;
  device const uchar* bp = raw + blk * 34;
  half h = as_type<half>((ushort)(bp[0] | (bp[1] << 8)));
  int q = (int)(char)bp[2 + within];
  out[i] = f2bf((float)h * (float)q);
}
"

$dsvDeqCuda = "
typedef unsigned short bf16t;
static __device__ __forceinline__ bf16t f2bf(float f) { unsigned int b = __float_as_uint(f);
  if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0u) return (bf16t)(((b & 0x80000000u) | 0x7f800000u | 0x400000u) >> 16);
  return (bf16t)((b + 0x7fffu + ((b >> 16) & 1u)) >> 16); }
extern \"C\" __global__ void dequant_q8(bf16t* out, const unsigned char* raw) {
  long i = (long)blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= `N`) return;
  long blk = i / 32, within = i % 32;
  const unsigned char* bp = raw + blk * 34;
  unsigned short h = (unsigned short)(bp[0] | (bp[1] << 8));
  signed char q = (signed char)bp[2 + within];
  out[i] = f2bf(__half2float(__ushort_as_half(h)) * (float)q);
}
"

dsvDeqKernel[dev_, n_] := TKernel[<|
    "Program" -> StringTemplate[Switch[dev, "metal", $dsvDeqMetal, "cpu", $dsvDeqCpu, _, $dsvDeqCuda]][<|"N" -> ToString[n]|>],
    "Name" -> "dequant_q8",
    "Grid" -> {Ceiling[n / 256]},
    "Block" -> {256},
    "Outs" -> {1}
|>]

(* === tensor import =============================================== *)

(* Number of elements in a GGUF tensor (product of ne). *)
ggufNumel[spec_] := Times @@ spec["Dims"]

(* Raw Q8_0 byte count: 34 bytes per 32-element block. *)
ggufQ8Bytes[n_] := (n / 32) * 34

(* Import one GGUF tensor as a TTerm in HF (reversed-ne) shape on device dev.
   F32 -> direct mmap view; Q8_0 -> mmap the raw block stream as bytes, dequant
   to a bf16 tensor via dsvDequantQ8. *)
dsvGgufTensor[path_, ds_, spec_, dev_] := Block[{n, shape, raw, out},
    n = ggufNumel[spec];
    shape = Reverse[spec["Dims"]];
    Which[
        spec["Quant"] === $ggmlF32,
            dsvUp[TUOpReshape[TTensorMMap[path, ds + spec["Offset"], n * 4, "f32", {n}], shape], dev],
        spec["Quant"] === $ggmlQ8,
            raw = TTensorMMap[path, ds + spec["Offset"], ggufQ8Bytes[n], "u8", {ggufQ8Bytes[n]}];
            out = TRealize @ If[dev === "cpu", TTensor[{n}, "bf16"], TToDevice[TTensor[{n}, "bf16"], dev]];
            dsvDeqKernel[dev, n][out, If[dev === "cpu", raw, TToDevice[raw, dev]]];
            TDiskDropWeight[raw];   (* the raw block stream is transient; release its mmap pages *)
            TRealize @ TUOpReshape[out, shape],
        True, Message[dsvGgufWeights::quant, spec["Quant"]]; $Failed
    ]
]

dsvGgufWeights::quant = "Unsupported GGUF quant type `1` (only F32 and Q8_0).";

(* GGUF per-layer role -> HF weight name (the layout dsvHfLayer consumes).
   Experts stay pre-stacked (dsvExpertStack flattens them). *)
$dsvGgufRole = <|
    "attn_norm.weight" -> "input_layernorm.weight",
    "attn_q.weight" -> "self_attn.q_proj.weight",
    "attn_kv_a_mqa.weight" -> "self_attn.kv_a_proj_with_mqa.weight",
    "attn_kv_a_norm.weight" -> "self_attn.kv_a_layernorm.weight",
    "attn_kv_b.weight" -> "self_attn.kv_b_proj.weight",
    "attn_output.weight" -> "self_attn.o_proj.weight",
    "ffn_norm.weight" -> "post_attention_layernorm.weight",
    "ffn_gate.weight" -> "mlp.gate_proj.weight",
    "ffn_up.weight" -> "mlp.up_proj.weight",
    "ffn_down.weight" -> "mlp.down_proj.weight",
    "ffn_gate_inp.weight" -> "mlp.gate.weight",
    "ffn_gate_exps.weight" -> "mlp.experts.gate_proj.weight",
    "ffn_up_exps.weight" -> "mlp.experts.up_proj.weight",
    "ffn_down_exps.weight" -> "mlp.experts.down_proj.weight",
    "ffn_gate_shexp.weight" -> "mlp.shared_experts.gate_proj.weight",
    "ffn_up_shexp.weight" -> "mlp.shared_experts.up_proj.weight",
    "ffn_down_shexp.weight" -> "mlp.shared_experts.down_proj.weight"
|>;

(* GGUF roles present in one layer (dense-MLP first block vs the MoE blocks). *)
dsvGgufLayerRoles[cfg_, l_] := Join[
    {"attn_norm.weight", "attn_q.weight", "attn_kv_a_mqa.weight", "attn_kv_a_norm.weight",
     "attn_kv_b.weight", "attn_output.weight", "ffn_norm.weight"},
    If[ l <= cfg["FirstDense"],
        {"ffn_gate.weight", "ffn_up.weight", "ffn_down.weight"},
        {"ffn_gate_inp.weight", "ffn_gate_exps.weight", "ffn_up_exps.weight", "ffn_down_exps.weight",
         "ffn_gate_shexp.weight", "ffn_up_shexp.weight", "ffn_down_shexp.weight"}
    ]
]

(* Build the decode weights layer-by-layer.  Each layer's GGUF tensors are
   dequantized into a small HF-named sd, transformed by dsvHfLayer, and then the
   intermediates drop -- so only ONE layer's dequant is resident at a time.  The
   earlier version realized the WHOLE dequantized model into an sd and then let
   dsvHfWeights transpose it into a second copy (~2x the model in RAM, the OOM);
   this interleaves so peak RAM is one model + one layer, matching the lazy-mmap
   safetensors path. *)
dsvGgufWeights[cfg_Association, path_String] := Block[{hdr = dsvGgufHeader[path], dev = cfg["Device"], gt, layerSd},
    gt[gname_] := dsvGgufTensor[path, hdr["DataStart"], hdr["Tensors"][gname], dev];
    layerSd[l_] := With[{n = ToString[l - 1]},
        Association[("model.layers." <> n <> "." <> $dsvGgufRole[#] -> gt["blk." <> n <> "." <> #]) & /@ dsvGgufLayerRoles[cfg, l]]
    ];
    <|
        "Embed" -> gt["token_embd.weight"],
        "Layers" -> Table[dsvHfLayer[cfg, layerSd[l], l], {l, cfg["NLayers"]}],
        "Norm" -> gt["output_norm.weight"],
        "LmHead" -> dsvUp[Transpose @ gt["output.weight"], dev]
    |>
]

dsvGgufInit[cfg_Association, path_String] := dsvState[cfg, dsvGgufWeights[cfg, path]]

End[];

EndPackage[];
