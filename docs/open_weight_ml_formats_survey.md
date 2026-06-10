# Open-weight ML formats: a survey for thvm model import

*Engineer-facing survey, 2026-06-11. Orients toward thvm importing standard open-weight files to run models like FLUX with minimal manual encoding.*

---

## 1. TL;DR / the central insight

**A weights file is not a runnable model.** The single fact that organizes this entire document: every popular open-weight container ships *tensors* (and sometimes *hyperparameters* and a *tokenizer*), but almost none of them ship the *forward pass*. The graph (which matmul feeds which norm, where RoPE is applied, how attention is masked, how MoE routes) lives in *library code* (transformers, diffusers, llama.cpp, tinygrad) keyed off an architecture string, not in the file.

### The spine taxonomy

Every format sits in one of three tiers. The tier dictates how much you must reconstruct yourself, and therefore the importer's effort:

| Tier | What the file carries | What *you* must supply | Formats |
|---|---|---|---|
| **T1 - Weights-only** | named tensors + dtype + shape, nothing else | the *entire* architecture + forward, in your own code | safetensors, GGUF *tensor section in isolation*, MLX `.safetensors`, PyTorch `state_dict` (`.bin`/`.pt`), `.npz`/`.h5`-as-weights |
| **T2 - Weights + arch hyperparameters** | tensors **plus** the architecture's shape knobs (n_layer, n_head, n_embd, rope theta, vocab, eps, activation) - but **not** the op graph | the *family* forward (one Llama, one GPT, one DiT), parameterized by the knobs | **GGUF** (KV metadata + tensors), **HF `config.json` + safetensors**, MLX `config.json` + weights |
| **T3 - Full graph + weights** | the actual op-by-op DAG (matmuls, attention, conv, control flow) serialized alongside weights | only a runtime that *executes the serialized ops* (op coverage is the whole game) | **ONNX**, **Core ML** (`.mlpackage`/`.mlmodelc`), **TF SavedModel**, **TorchScript**, **torch.export `ExportedProgram`**, **TFLite/LiteRT**, **TensorRT engine** |

Two subtleties worth internalizing up front:

- **GGUF spans T1 to T2.** Its tensor blocks alone are T1; its KV-metadata block (`general.architecture` + `llama.*` keys) lifts it to T2. That is the design intent: self-contained enough to stand a model up *without a sidecar config*, but still requiring the loader to know the family's forward.
- **T2 is where open LLMs and diffusion models actually live.** HF `config.json` + safetensors and GGUF dominate. A thvm importer that implements a handful of *families* covers the bulk of real open weights with the least per-model work.

### What "minimal manual encoding" means

It does **not** mean "zero code." Unless you adopt a T3 graph format (and pay for an op-coverage interpreter plus dynamic-shape/control-flow brittleness), running a T1/T2 file requires **one compact per-architecture forward** that reads named tensors out of the loaded dictionary and wires them into ops. Concretely: tinygrad's `examples/llama.py` Transformer class is ~200 lines; its `gguf_load`/`safe_load` handle the tedious dequant and parsing. The forward is domain-specific and hand-written *once per family*, not once per checkpoint.

So the realistic target for "load FLUX and run it" is:

> a standard weights file (safetensors shards or a community GGUF) + a few-hundred-line per-architecture forward in thvm that reads named tensors. **Not** 4,500 lines of hand-exported per-block WL.

The rest of this document surveys the formats, then makes a concrete recommendation for FLUX-on-thvm.

---

## 2. GGUF

GGUF ("GPT-Generated Unified Format" / GGML Universal File) is the on-disk container for `llama.cpp`, `ollama`, `LM Studio`, `GPT4All`, and (for image models) `ComfyUI-GGUF`. Successor to the older GGML/GGJT formats. **It stores weights + hyperparameters + tokenizer, but NOT an executable graph** - the runtime supplies the graph (see 2.4).

### 2.1 Binary structure

Four contiguous sections, little-endian by default (v3 adds big-endian):

```
+-------------------+
| Header            |  magic, version, tensor_count, metadata_kv_count
+-------------------+
| Metadata KV       |  N typed key-value pairs (hyperparams, tokenizer, ...)
+-------------------+
| Tensor Info table |  per-tensor: name, dims, ggml_type, offset
+-------------------+
| (padding to align)|
+-------------------+
| Tensor Data blob  |  raw quantized/float bytes, each at aligned offset
+-------------------+
```

**Header:**

| Field | Type | Notes |
|---|---|---|
| `magic` | `uint32` (4 bytes) | `0x47 0x47 0x55 0x46` = ASCII `"GGUF"` |
| `version` | `uint32` | `3` is current |
| `tensor_count` | `uint64` | entries in the tensor-info table |
| `metadata_kv_count` | `uint64` | metadata key-value pairs |

**Version differences a parser must handle:**
- **v1**: counts and string/array lengths were `uint32`.
- **v2**: widened those count fields to `uint64` (the layout above). Breaking change: a v1 parser misreads a v2 file.
- **v3**: identical physical layout to v2, but formally adds big-endian support (byte order is a property of the producing machine; tooling tags it). Field widths are identical between v2 and v3.

In practice: one code path for v2/v3, branch/reject on v1.

**The `gguf_string_t` primitive** (keys, string values, tensor names):

```c
struct gguf_string_t {
    uint64_t len;         // byte length, NO terminator (uint32 in v1)
    char     string[len]; // UTF-8, NOT null-terminated
};
```

Always read exactly `len` bytes; there is no trailing `\0`.

### 2.2 Metadata key-value section (the self-describing part)

Each entry:

```c
struct gguf_metadata_kv_t {
    gguf_string_t          key;        // e.g. "llama.attention.head_count"
    uint32_t               value_type; // enum below
    <value bytes per type> value;
};
```

Value-type enum (`uint32`):

| Code | Type | Code | Type |
|---|---|---|---|
| 0 | `UINT8` | 7 | `BOOL` (1 byte) |
| 1 | `INT8` | 8 | `STRING` (`gguf_string_t`) |
| 2 | `UINT16` | 9 | `ARRAY` |
| 3 | `INT16` | 10 | `UINT64` |
| 4 | `UINT32` | 11 | `INT64` |
| 5 | `INT32` | 12 | `FLOAT64` |
| 6 | `FLOAT32` | | |

An **ARRAY** is: `uint32 element_type`, `uint64 element_count`, then `count` packed elements. Arrays nest and can hold strings (this is how the tokenizer vocab is stored). Keys are hierarchical snake_case dotted ASCII.

**Key namespaces a runner reads:**

*General (architecture-independent):*
- `general.architecture` - **the central routing key** (lowercase `[a-z0-9_]`: `"llama"`, `"qwen3"`, `"gemma3"`, `"phi3"`, ...).
- `general.quantization_version`, `general.alignment` (uint32, default 32), `general.name`, `general.file_type`, plus optional `author`/`license`/`size_label`/`tags`/`languages`.

*Per-architecture hyperparameters* (namespaced by the arch string itself; a llama file literally uses `llama.context_length`):
- `[arch].context_length`, `[arch].embedding_length`, `[arch].block_count` (= n_layers), `[arch].feed_forward_length`
- `[arch].attention.head_count`, `[arch].attention.head_count_kv` (GQA), `[arch].attention.layer_norm_rms_epsilon`, `.key_length`, `.value_length`, `.clamp_kqv`, `.max_alibi_bias`
- `[arch].rope.dimension_count`, `[arch].rope.freq_base` (theta), `[arch].rope.scaling.type`, `.scaling.factor`, `.scaling.original_context_length`
- MoE: `[arch].expert_count`, `[arch].expert_used_count`
- SSM/Mamba: `[arch].ssm.conv_kernel`, `.ssm.inner_size`, `.ssm.state_size`, `.ssm.time_step_rank`

*Tokenizer (embedded entirely as KVs - no separate file):*
- `tokenizer.ggml.model` (`"gpt2"`/`"llama"`/`"bert"`/`"t5"`), `tokenizer.ggml.tokens` (the vocab array), `tokenizer.ggml.scores`, `tokenizer.ggml.token_type`, `tokenizer.ggml.merges` (BPE merges), special-token IDs `tokenizer.ggml.{bos,eos,unknown,padding}_token_id`, and `tokenizer.chat_template`.

This is the structural difference from safetensors: safetensors is tensors + a thin JSON header; GGUF additionally standardizes *all* the metadata an inference engine needs to stand the model up.

### 2.3 Tensor-info table, alignment, data blob

`tensor_count` entries, each:

```c
struct gguf_tensor_info_t {
    gguf_string_t name;             // <= 64 bytes, e.g. "blk.0.ffn_gate.weight"
    uint32_t      n_dimensions;
    uint64_t      dimensions[n_dimensions];
    uint32_t      type;             // ggml_type enum (see 2.5)
    uint64_t      offset;           // relative to start of tensor-data blob; multiple of alignment
};
```

**Dims are stored in GGML order (fastest-varying axis first) - the *reverse* of PyTorch/NumPy row-major shape order. An importer must reverse them.** Tensor names follow a fixed scheme:

```
token_embd.weight
output_norm.weight
output.weight
blk.{i}.attn_q.weight   blk.{i}.attn_k.weight   blk.{i}.attn_v.weight   blk.{i}.attn_output.weight
blk.{i}.ffn_gate.weight blk.{i}.ffn_up.weight   blk.{i}.ffn_down.weight
blk.{i}.attn_norm.weight ...
```

After the tensor-info table, the file is padded to a multiple of `general.alignment` (default **32**, must be a multiple of 8). Each tensor's `offset` is also a multiple of that alignment. This is deliberate: the whole file is designed to be **`mmap`-ed**, so tensor data is read through pointers with zero-copy. A tensor's byte size is *derived* from its dims and `ggml_type` block size (2.5), not stored explicitly. The blob is concatenated, aligned, type-specific block bytes.

### 2.4 Weights, NOT a graph (the central theme)

**A GGUF file contains, and only contains:** (1) tensor weights, (2) architecture hyperparameters (`general.architecture` + the `[arch].*` KVs), (3) the tokenizer (all as KVs).

**It does NOT contain:** the forward-pass graph, the order of operations, which norm goes where, how RoPE is applied, attention masking, MoE routing, or any serialized IR.

So how does it run? **`llama.cpp` has one hardcoded graph builder per architecture, in C++:**

1. **Load** - `llama_model_loader` reads the KVs, reads `general.architecture`, looks it up in `LLM_ARCH_NAMES` (`src/llama-arch.cpp`) to get an `llm_arch` enum value (`"llama"` to `LLM_ARCH_LLAMA`). 100+ entries.
2. **Instantiate** - a factory in `src/llama-model.cpp` constructs the matching arch class (`src/models/llama.cpp`, `qwen3.cpp`, `mamba-base.cpp`, `rwkv7-base.cpp`). Each implements three methods:
   - `load_arch_hparams()` - pull `[arch].*` hyperparameters from metadata.
   - `load_arch_tensors()` - bind named GGUF tensors to `ggml_tensor*` (and `mmap` the blob).
   - `build_arch_graph()` - **construct the ggml compute graph from scratch using ggml ops**, parameterized by the hparams (loop `block_count` times, build attention with `head_count`/`head_count_kv`, apply RoPE with `rope.freq_base`).
3. **Execute** - run the freshly-built ggml graph over the mmap'd weights.

**The GGUF metadata only *selects which* hardcoded builder runs and *parameterizes* it.** If a model's computation isn't expressible by any existing builder, the GGUF is useless until someone *writes a new C++ builder* and adds the arch to `LLM_ARCH_NAMES`/`LLM_TENSOR_NAMES`/`LLM_TENSOR_INFOS`. This is exactly why a brand-new architecture (FLUX.2, 2.7) can be *converted to a GGUF container* yet still not *run* in llama.cpp.

**Implication for thvm:** importing a GGUF yields tensors + a hyperparameter dictionary + a tokenizer. To *run* it, thvm must contain its own per-architecture graph builder keyed off `general.architecture` (the thvm analogue of `build_llama_graph`). There is nothing in the file to "execute"; the importer reconstructs the graph from the arch string + hparams, exactly as llama.cpp does.

### 2.5 GGML tensor / quantization types

`ggml_type` (the `uint32` in tensor-info) enumerates ~40 types. Codes (stable, from `ggml-common.h`):

```
F32=0  F16=1  Q4_0=2  Q4_1=3  Q5_0=6  Q5_1=7  Q8_0=8  Q8_1=9
Q2_K=10 Q3_K=11 Q4_K=12 Q5_K=13 Q6_K=14 Q8_K=15
IQ2_XXS=16 IQ2_XS=17 IQ3_XXS=18 IQ1_S=19 IQ4_NL=20 IQ3_S=21 IQ2_S=22 IQ4_XS=23
I8=24 I16=25 I32=26 I64=27 F64=28 IQ1_M=29 BF16=30
TQ1_0=34 TQ2_0=35 MXFP4=39   (COUNT=40)
```

**Three families:**

- **Float / passthrough:** `F32`, `F16`, `BF16`, `F64`, integer `I8/I16/I32/I64`. No blocking. `BF16` is the top 16 bits of an f32 (truncated mantissa), *distinct* from IEEE `F16`.
- **Legacy linear quants** (`Q4_0`, `Q4_1`, `Q5_0`, `Q5_1`, `Q8_0`, `Q8_1`): one flat block of **32 weights** with a single `f16` scale `d` (plus a `min`/offset `m` for the `_1` variants). Affine dequant: `w = d*q` (type-0) or `w = d*q + m` (type-1).
- **K-quants** (`Q2_K`..`Q6_K`, ikawrakow PR #1684): a **super-block of `QK_K = 256` weights**, subdivided, with a *two-level* scale hierarchy - each small block has its own low-bit scale/min, themselves scaled by a per-super-block `f16` `d`/`dmin`. Piecewise-affine; why `Q4_K_M` beats legacy `Q4_0` at similar size. The `_S`/`_M`/`_L` suffixes are **mixes**, not distinct ggml types: the quantizer raises sensitive tensors (attention `V`, `ffn_down`, output) to a higher type, leaving the rest at the base K-quant.
- **I-quants** (`IQ1_S`..`IQ4_XS`, `IQ1_M`, `IQ2_XXS`...): sub-4-bit, also `QK_K=256` super-blocks, **non-uniform codebook quantization** (weights index a learned lattice) reconstructed with a super-block scale plus an **importance matrix (imatrix)** from calibration data. Lowest bpw at a given quality, but need the imatrix at quantize time and are slower to dequant. `MXFP4` (PR #15091) is 4-bit Microscaling block-float; `TQ1_0`/`TQ2_0` are ternary (BitNet-style).

**Common types with bits-per-weight:**

| Type | Family | Block / super-block | Scale encoding | **bpw** |
|---|---|---|---|---|
| F32 | float | - | - | 32 |
| F16 / BF16 | float | - | - | 16 |
| Q8_0 | legacy | 32 w | f16 `d` | 8.5 |
| Q4_0 | legacy | 32 w | f16 `d` | 4.5 |
| Q4_1 | legacy | 32 w | f16 `d`,`m` | 5.0 |
| Q5_0 | legacy | 32 w | f16 `d` + qh | 5.5 |
| Q5_1 | legacy | 32 w | f16 `d`,`m` + qh | 6.0 |
| Q2_K | K | 16 blk x 16 w | 4-bit scale+min, f16 `d`,`dmin` | **2.5625** |
| Q3_K | K | 16 blk x 16 w | 6-bit scale, f16 `d` | **3.4375** |
| Q4_K | K | 8 blk x 32 w | 6-bit scale+min, f16 `d`,`dmin` | **4.5** |
| Q5_K | K | 8 blk x 32 w | 6-bit scale+min, f16 `d`,`dmin` + qh | **5.5** |
| Q6_K | K | 16 blk x 16 w | 8-bit scale, f16 `d` | **6.5625** |
| Q8_K | K | 256 w | f32 `d` (intermediate-only) | ~8 |
| IQ4_XS | I | 256 w | super-block scale + imatrix | **~4.25** |
| IQ4_NL | I | 256 w | non-linear codebook + imatrix | ~4.5 |
| IQ3_S | I | 256 w | scale + imatrix | ~3.44 |
| IQ3_XXS | I | 256 w | scale + imatrix | ~3.06 |
| IQ2_S / IQ2_XS / IQ2_XXS | I | 256 w | scale + imatrix | 2.5 / 2.31 / 2.06 |
| IQ1_S / IQ1_M | I | 256 w | scale + imatrix | 1.56 / 1.75 |
| MXFP4 | block-float | 32 w | shared 8-bit exp | ~4.25 |

(`Q4_K_M`'s *effective* file bpw is ~4.8-4.9 due to the higher-precision mixed tensors; `IQ4_XS` ~4.46 effective - why an 8B is ~4.2 GiB at IQ4_XS vs ~4.58 GiB at Q4_K_M.)

**Exact block byte-layouts** (literal `ggml-common.h` structs; `ggml_half` = 2 bytes; `QK_K=256`, `QK4_0=QK5_0=QK8_0=32`, `K_SCALE_SIZE=12`):

```c
// ---- legacy (32 weights/block) ----
block_q4_0 { ggml_half d;              uint8_t qs[16]; }              // 18 B  (16 nibbles = 32 vals)
block_q4_1 { ggml_half d, m;           uint8_t qs[16]; }              // 20 B
block_q5_0 { ggml_half d;  uint8_t qh[4]; uint8_t qs[16]; }           // 22 B  (qh = the 5th bits)
block_q5_1 { ggml_half d, m; uint8_t qh[4]; uint8_t qs[16]; }         // 24 B
block_q8_0 { ggml_half d;              int8_t  qs[32]; }              // 34 B
block_q8_1 { ggml_half d, s;           int8_t  qs[32]; }              // 36 B

// ---- K-quants (256 weights / super-block) ----
block_q2_K { uint8_t scales[16]; uint8_t qs[64];  ggml_half d, dmin; }            //  84 B
block_q3_K { uint8_t hmask[32];  uint8_t qs[64]; uint8_t scales[12]; ggml_half d;}// 110 B
block_q4_K { ggml_half d, dmin;  uint8_t scales[12]; uint8_t qs[128]; }           // 144 B
block_q5_K { ggml_half d, dmin;  uint8_t scales[12]; uint8_t qh[32]; uint8_t qs[128]; } // 176 B
block_q6_K { uint8_t ql[128]; uint8_t qh[64]; int8_t scales[16]; ggml_half d; }   // 210 B
block_q8_K { float d; int8_t qs[256]; int16_t bsums[16]; }                        // 292 B (intermediate)

// ---- example I-quants ----
block_iq4_xs { ggml_half d; uint16_t scales_h; uint8_t scales_l[4]; uint8_t qs[128]; } // 136 B
block_iq2_xxs{ ggml_half d; uint16_t qs[32]; }                                          //  66 B
```

The `scales[12]` field in Q4_K/Q5_K bit-packs **eight 6-bit block scales + eight 6-bit block mins** into 12 bytes (`K_SCALE_SIZE`). A dequantizer unpacks the 6-bit fields, then for weight `q` in sub-block `j`: `w = d * scale_j * q - dmin * min_j`. Q5_K additionally pulls a 5th bit per weight from `qh[32]`. Q6_K combines a low nibble (`ql`) and 2 high bits (`qh`) into a signed 6-bit quant times an `int8` per-block scale times `d`.

### 2.6 Tooling

- **`gguf-py`** (PyPI: `gguf`) - canonical read/write. `GGUFWriter` (builds files in section order, sharded output via `split_max_size`), `GGUFReader` (mmap inspector exposing `.fields`/`.tensors`; basis for the HF GGUF viewer and JS `@huggingface/gguf`), and `TensorNameMap` (`gguf/tensor_mapping.py`) mapping framework-native names (HF `model.layers.0.self_attn.q_proj.weight`) to canonical GGUF names (`blk.0.attn_q.weight`) via a `MODEL_TENSOR` enum.
- **`convert_hf_to_gguf.py`** (in llama.cpp) - reads an HF checkpoint dir, parses `config.json` to pick the arch, indexes `.safetensors`/`.bin`, renames every tensor through `TensorNameMap.map_tensor_name()`, writes hyperparameters as `[arch].*` KVs, embeds the tokenizer, emits via `GGUFWriter`. `--outtype` only supports `f32, f16, bf16, q8_0, tq1_0, tq2_0, auto` - it deliberately does *not* do the K-/I-quants (separate pass). Adding a new arch here means writing a converter subclass *and* a matching C++ graph builder.
- **`llama-quantize`** - `f16`/`f32`/`bf16` GGUF to a K-/I-quant GGUF: `llama-quantize in-f16.gguf out-Q4_K_M.gguf Q4_K_M`. For I-quants pass `--imatrix imatrix.gguf` (from `llama-imatrix` over calibration text). Typical flow: `convert_hf_to_gguf.py --outtype f16` -> (`llama-imatrix`) -> `llama-quantize ... Q4_K_M`.

### 2.7 GGUF for diffusion (FLUX GGUF status)

GGUF the *container* is architecture-agnostic - "typed metadata + a named, quantized tensor blob." So it has been adopted for diffusion / image models, primarily via **`city96/ComfyUI-GGUF`**.

**How non-LLM GGUF works:** the GGUF still carries weights + (sparse) metadata, but `general.architecture` is set to something llama.cpp doesn't know, so **llama.cpp cannot run it**. A *different runner* - ComfyUI's `UnetLoaderGGUF` node - knows the architecture. ComfyUI already has native PyTorch graph definitions for FLUX/SD3/SDXL; the GGUF node **dequantizes the GGUF tensors on the fly back into those existing PyTorch modules** (custom ops, optional GGUF-LoRA). Same 2.4 principle restated for images: **the GGUF is a weight container; the runner owns the graph.** ComfyUI-GGUF targets transformer/DiT backbones (FLUX, SD3.5, plus T5/CLIP text encoders) and notes quantization is *not* applied to conv2d UNet layers. Flagship: **`city96/FLUX.1-dev-gguf`** (Q4_0...Q8_0, Q4_K/Q5_K/Q6_K mixes), widely used to run FLUX on 8 GB VRAM.

**Is FLUX.2 available as GGUF?** Yes - and the files work *inside ComfyUI* - but the upstream ComfyUI-GGUF *quantizer* still errors on the FLUX.2 architecture:

- **`city96/FLUX.2-dev-gguf` is a real, active, populated HF repo** (~86.7k downloads/month, mid-2026). It ships **16 variants**: `Q2_K` (12.9 GB), `Q3_K_S/M`, `Q4_0`, `Q4_1`, `Q4_K_S/M`, `Q5_0`, `Q5_1`, `Q5_K_S/M`, `Q6_K`, `Q8_0` (35 GB), `BF16` (64.4 GB). They run via ComfyUI-GGUF + a Mistral-Small-3.2-24B text encoder + the FLUX.2 VAE. The README candidly flags the block-precision choices for the `_K_M`/`_K_S`/`Q2_K` mixes as "partially guesswork, trial & error."
- **But the public ComfyUI-GGUF quantization *tooling* does not yet recognize FLUX.2's arch**: open issue **#418 (filed 2026-02-13, unresolved)** reports `convert`/quantize failing with `Unknown Architecture: Flux` / `Unknown Architecture: Lumina2`. End-users cannot self-quantize FLUX.2 yet; only city96's pre-made repo (private/patched tooling) provides the files.

Net: FLUX.2 GGUFs are downloadable and runnable in ComfyUI today, but FLUX.2 is not yet a first-class, self-serviceable GGUF arch - and a FLUX GGUF will never run in `llama.cpp` because llama.cpp has no FLUX graph builder.

---

## 3. safetensors + the HuggingFace ecosystem

### 3.1 The format and why it exists

PyTorch's native `.bin`/`.pt`/`.ckpt` are Python **pickle** streams; unpickling executes arbitrary opcodes (`__reduce__` can invoke any callable), so loading from an untrusted source is remote code execution. safetensors fixes three things at once:

| Property | pickle (`.bin`/`.ckpt`) | safetensors |
|---|---|---|
| **Security** | arbitrary code execution on load | pure data; parser reads a JSON header + raw bytes, no code path |
| **Zero-copy** | deserializes into fresh objects | `mmap`s the file; tensors are views into the mapped blob |
| **Lazy / partial** | must load the whole object graph | read one tensor (or a slice) via header offsets |
| **Speed** | CPU-bound deserialize | ~memcpy / page-cache speed; near-instant warm loads |
| **Transparency** | opaque | names/dtypes/shapes inspectable from the header alone |

De-facto standard across transformers, diffusers, ComfyUI, candle, mlx, vLLM. Current release **v0.8.0 (2026-06-09)**.

### 3.2 Byte-level layout

Exactly three concatenated regions:

```
+--------------------+-----------------------------+--------------------------------+
| 8 bytes            | N bytes                     | rest of file                   |
| header length (N)  | JSON header (UTF-8)         | raw tensor data buffer         |
| u64 little-endian  | starts with '{' (0x7B)      | contiguous little-endian blob  |
+--------------------+-----------------------------+--------------------------------+
 offset 0             offset 8                      offset 8 + N
```

1. **Header length** - first 8 bytes, unsigned 64-bit LE integer `N` (`struct.unpack('<Q', first8)`).
2. **JSON header** - next `N` bytes, UTF-8 JSON, begins with `{` (`0x7B`), may be right-padded with spaces (`0x20`) for alignment. Max header size **100 MB** (DoS guard).
3. **Data buffer** - everything after byte `8 + N`. A single contiguous, packed, row-major (C-order), little-endian blob.

**JSON header schema:**

```json
{
  "__metadata__": { "format": "pt" },
  "model.embed_tokens.weight": {
    "dtype": "BF16", "shape": [151936, 2560], "data_offsets": [0, 777912320]
  },
  "model.layers.0.self_attn.q_proj.weight": {
    "dtype": "BF16", "shape": [4096, 2560], "data_offsets": [777912320, 798883840]
  }
}
```

Per-tensor record has exactly three fields: `dtype` (string enum), `shape` (int array; `[]` is a legal 0-rank scalar, size-0 dims allowed), and `data_offsets` `[BEGIN, END)` - a half-open byte range **relative to the start of the data buffer, not the file**. `nbytes = END - BEGIN` must equal `prod(shape) * itemsize(dtype)`.

**Absolute file offset of a tensor = `8 + N + BEGIN`.** This is the single most important formula for an importer.

`__metadata__` is the only special key: an optional **flat string->string map** (non-string values forbidden). No duplicate names; ranges must not overlap or exceed file size. Tensors are commonly 8-byte aligned (header space-padded) for true zero-copy mmap.

**dtypes.** Portable set: `F64 F32 F16 BF16 I64 I32 I16 I8 U8 BOOL`. Newer/quant additions in the Rust core: `F8_E4M3 F8_E5M2`, `U16 U32 U64`, and sub-byte `I4`/`F4`-style packed formats. Caveats: `BF16` is the top 16 bits of an `F32` (not IEEE F16); `FP8` carries no scale (scales live in companion tensors or `__metadata__`); sub-byte FP4 (NVFP4, MXFP4) is stored as packed `U8`/`I8` plus separate block-scale tensors - safetensors stores the *bytes*, the *interpretation* is a convention of the producing library.

**What safetensors does NOT carry:** no graph, no layer wiring, no forward, no architecture identity beyond inferred tensor names, no hyperparameters, no training metadata. Only `{name -> (dtype, shape, raw bytes)}` + an optional flat string map. The graph and hyperparameters live in `config.json` + library code.

### 3.3 Sharding

```
model-00001-of-00002.safetensors
model-00002-of-00002.safetensors
model.safetensors.index.json
```

```json
{
  "metadata": { "total_size": 8044936192 },
  "weight_map": {
    "model.embed_tokens.weight":             "model-00001-of-00002.safetensors",
    "model.layers.0.self_attn.q_proj.weight":"model-00001-of-00002.safetensors",
    "model.norm.weight":                     "model-00002-of-00002.safetensors"
  }
}
```

`metadata.total_size` (sum of all tensor byte sizes - preflight RAM) and `weight_map` (`tensor_name -> shard_filename`). Each shard is a complete, independently-parseable safetensors file with its own header and *its own local* `data_offsets`. For diffusers components the index is `diffusion_pytorch_model.safetensors.index.json`.

**Reader APIs to mirror:** `safetensors.torch.load_file(path)` (eager), `safe_open(path, framework, device)` + `f.get_tensor(k)`/`f.get_slice(k)[...]` (lazy/mmap, partial loads), and a **metadata-only HTTP path** - fetch bytes `0-7` then `8..8+N-1`, parse, and learn every tensor's dtype/shape *without downloading the blob* (`huggingface_hub.get_safetensors_metadata`). Useful for an importer's dry-run/validation pass.

### 3.4 The Hub repo as the distribution unit

A model on the Hub is a **git repo** (LFS/Xet-backed). The repo is the packaging boundary; the *runtime code is not in it* - it lives in transformers/diffusers. A typical single-model (transformers) repo:

| File | Role |
|---|---|
| `config.json` | architecture + hyperparameters. `architectures` (e.g. `["Qwen3ForCausalLM"]`) names the Python class; `model_type` (`"qwen3"`) selects config/model mapping; holds `hidden_size`, `num_hidden_layers`, `vocab_size`, `torch_dtype`/`dtype` |
| `model.safetensors` *or* shards + `model.safetensors.index.json` | the weights |
| `tokenizer.json` | fast-tokenizer (vocab + merges + normalizer + pre-tokenizer in one JSON) |
| `tokenizer_config.json` | tokenizer class, special tokens, chat template |
| `vocab.json` / `merges.txt` | legacy BPE pair (slow tokenizer) |
| `special_tokens_map.json` | BOS/EOS/PAD/UNK mapping |
| `generation_config.json` | default decoding params |
| `README.md` | model card (YAML front-matter) |

**`AutoModelForCausalLM` reconstruction:** download `config.json`, read `architectures[0]` (or map via `model_type`), look up that class **in the transformers library** (not the repo), instantiate the empty module from hyperparameters, then `load_state_dict` from safetensors by **matching tensor names** to module parameter names. The takeaway: **`config.json` + tensor-name conventions are the contract**; the graph builder you implement yourself.

### 3.5 Diffusers multi-folder packaging (the FLUX-relevant layout)

A diffusion *pipeline* is several models orchestrated together. diffusers packages it as a multi-folder repo, one subfolder per component:

```
repo-root/
├── model_index.json              <- pipeline manifest
├── scheduler/      scheduler_config.json
├── text_encoder/   config.json + (sharded) model.safetensors + index
├── tokenizer/      tokenizer.json + tokenizer_config.json + vocab/merges + special_tokens_map
├── transformer/    config.json + diffusion_pytorch_model.safetensors (or shards + index)
└── vae/            config.json + diffusion_pytorch_model.safetensors
```

`model_index.json` maps each component to a `[library, class]` pair:

```json
{
  "_class_name": "Flux2KleinPipeline",
  "_diffusers_version": "0.37.0.dev0",
  "scheduler":    ["diffusers",    "FlowMatchEulerDiscreteScheduler"],
  "text_encoder": ["transformers", "Qwen3ForCausalLM"],
  "tokenizer":    ["transformers", "Qwen2TokenizerFast"],
  "transformer":  ["diffusers",    "Flux2Transformer2DModel"],
  "vae":          ["diffusers",    "AutoencoderKLFlux2"]
}
```

`DiffusionPipeline.from_pretrained(repo)`: read `model_index.json`, dispatch each component subfolder to that library's loader (each reading *its own* `config.json` + safetensors), wire them together. Naming conventions: diffusers weight files are `diffusion_pytorch_model.safetensors`; transformers ones are `model.safetensors`; sharded variants append `-0000k-of-0000N` plus a matching `*.index.json`.

### 3.6 Single-file checkpoints (the alternative packaging)

ComfyUI / Automatic1111 / SwarmUI ship a **single `.safetensors`** containing *all* components' weights in one flat dict, distinguished by key prefixes (`model.diffusion_model.*`, `text_encoders.*`, `vae.*`, or BFL's native naming). No `model_index.json`, no per-folder configs - the loader fingerprints the architecture from keys and applies a **rename map** to the canonical diffusers layout. diffusers supports these via `from_single_file(...)`.

### 3.7 How FLUX.2-klein-4B is actually laid out

*(Live `black-forest-labs/FLUX.2-klein-4B` listing, June 2026. Repo ~23.7 GB, Apache-2.0, Xet-backed LFS.)* It ships **both** packagings: the diffusers multi-folder tree *and* a root single-file.

**Root:**
```
model_index.json              446 B    (pipeline manifest: Flux2KleinPipeline)
flux-2-klein-4b.safetensors   7.75 GB  (single-file/ComfyUI-style packaging of the transformer)
README.md / LICENSE.md / *.jpg
scheduler/ text_encoder/ tokenizer/ transformer/ vae/
```

**`transformer/` - the denoiser (`Flux2Transformer2DModel`):** `diffusion_pytorch_model.safetensors` (7.75 GB, single file, no shards), `config.json`:

```json
{
  "_class_name": "Flux2Transformer2DModel",
  "attention_head_dim": 128, "axes_dims_rope": [32, 32, 32, 32],
  "eps": 1e-06, "guidance_embeds": false, "in_channels": 128,
  "joint_attention_dim": 7680, "mlp_ratio": 3.0,
  "num_attention_heads": 24, "num_layers": 5, "num_single_layers": 20,
  "patch_size": 1, "rope_theta": 2000, "timestep_guidance_channels": 256
}
```

An **MMDiT** (multimodal diffusion transformer): 5 joint/double-stream blocks + 20 single-stream blocks, 24 heads x 128 = **3072 hidden**, 4-axis RoPE (32x4), latent `in_channels=128`. "klein" (small) and **distilled** (`is_distilled: true`), hence `guidance_embeds: false`.

**`text_encoder/` - Qwen3 (`Qwen3ForCausalLM`):** 2 shards + index (`model-00001-of-00002.safetensors` 4.97 GB, `model-00002-of-00002.safetensors` 3.08 GB, index `total_size = 8 044 936 192` ~8.04 GB). `config.json`: `model_type: "qwen3"`, `dtype: "bfloat16"`, `hidden_size: 2560`, `num_hidden_layers: 36`, `vocab_size: 151936`. FLUX.2 abandons the FLUX.1-era T5+CLIP dual encoder for a single **Qwen3 4B-class causal LM** (decoder-LM hidden states as conditioning) - the largest component. Standard HF LM names: `model.embed_tokens.weight`, `model.layers.{i}.self_attn.{q,k,v,o}_proj.weight`, `model.norm.weight`.

**`vae/` - `AutoencoderKLFlux2`:** `diffusion_pytorch_model.safetensors` (168 MB). `latent_channels 32`, patch `[2,2]`, down/up blocks `128->256->512->512`. Note **32 latent channels** (vs FLUX.1's 16) and a patchified `[2,2]` front end, matching the transformer's `in_channels: 128` (= 32 latent x 4 from 2x2 packing).

**`tokenizer/` - `Qwen2TokenizerFast`:** Qwen2 BPE fast tokenizer, vocab 151,936. **`scheduler/` - `FlowMatchEulerDiscreteScheduler`:** config only - FLUX.2 is a rectified-flow / flow-matching model, not DDPM.

**dtype:** all weights bf16; your importer should read `dtype` from each tensor's header, not trust config (transformer/VAE configs omit an explicit `torch_dtype`).

| Component | Class | Files | Size | dtype |
|---|---|---|---|---|
| transformer | `Flux2Transformer2DModel` | 1 | 7.75 GB | bf16 |
| text_encoder | `Qwen3ForCausalLM` | 2 shards + index | 8.04 GB | bf16 |
| vae | `AutoencoderKLFlux2` | 1 | 168 MB | bf16 |
| tokenizer | `Qwen2TokenizerFast` | json | small | - |
| scheduler | `FlowMatchEulerDiscreteScheduler` | config only | tiny | - |
| *(root single-file)* | transformer, ComfyUI-style | `flux-2-klein-4b.safetensors` | 7.75 GB | bf16 |

**Quantized FLUX.2 variants ship as separate repos** (scheme in repo/filename): `FLUX.2-klein-4b-fp8` -> `flux-2-klein-4b-fp8.safetensors` (FP8 E4M3, weights + scales); `FLUX.2-dev-NVFP4` -> `flux2-dev-nvfp4{,-mixed}.safetensors` (NVIDIA Blackwell FP4, an FP8-E4M3 scale per 16-value block). FP4/FP8 weights are packed `U8`/`I8`/`F8_E4M3` tensors plus sibling scale tensors; the layout is defined by the quant toolkit, not by safetensors.

---

## 4. The rest of the landscape

### 4.1 PyTorch pickle (`.pt`/`.pth`/`.bin`) - Tier 1, but a pickle envelope

`torch.save(obj, path)` serializes with Python `pickle`. Since PyTorch 1.6 the container is a **ZIP archive**: `data.pkl` (pickled structure) + `data/` raw tensor storages + `version`.

```
PK\x03\x04 ...                      # ZIP local file header
  archive/data.pkl                  # pickled module/state_dict structure (refs storages by key)
  archive/data/0, archive/data/1..  # raw little-endian tensor storage bytes (untyped blobs)
  archive/version                   # serialization protocol version
```

Idiomatic PyTorch saves `model.state_dict()` (an `OrderedDict` of `"layer.weight" -> Tensor`), *not* the model object, so a `state_dict` `.bin`/`.pt` is **Tier 1** despite the pickle envelope; restoring requires reconstructing the `nn.Module` in code then `load_state_dict()`. The legacy HF multi-shard convention was `pytorch_model-00001-of-0000N.bin` + an index JSON.

**Security:** pickle can execute arbitrary code at deserialize time. HF runs PickleScan, but JFrog disclosed 3 zero-day bypasses in 2025. **PyTorch 2.6** flipped the default to `torch.load(..., weights_only=True)` (a constrained unpickler allowing only tensor-rebuilding globals); loading optimizer state / custom classes now needs an explicit `add_safe_globals()` allowlist. An importer that reads these without Python must implement a minimal pickle VM (or restrict to the `weights_only` opcode subset) and parse the ZIP - exactly why thvm should prefer safetensors/GGUF and treat raw pickles as a fallback.

### 4.2 ONNX - the one mainstream **full-graph** option (Tier 3)

A **protobuf** (`.onnx`) carrying the *entire computation graph* (a DAG of typed `NodeProto` ops from versioned **opsets**) **plus** initializers (weights, as `TensorProto`). One file, load and run, no forward code. Executed by **ONNX Runtime** across Execution Providers (CPU, CUDA, TensorRT, CoreML, QNN, WebGPU, DirectML).

```
ModelProto { ir_version; opset_import[{domain, version}];
  GraphProto { node[NodeProto{op_type, input[], output[], attribute[]}];
               initializer[TensorProto{name, dims[], data_type, raw_data}];
               input[]; output[]; value_info[] } }
# >2GB: initializers' raw_data offloaded to an external file via TensorProto.external_data
```

The *canonical* Tier-3 interchange: import once, run anywhere. ORT's transformer op surface is broad (`MatMul, Gemm, Softmax, LayerNormalization`, plus contrib ops `Attention, MultiHeadAttention, GroupQueryAttention, RotaryEmbedding, SkipLayerNormalization`). But heavy/brittle for big LLMs and diffusion:

- **Op coverage gaps.** The legacy TorchScript-based exporter can't lower fused ops; exporting FLUX/SD3 backbones fails with `UnsupportedOperatorError: aten::rms_norm to ONNX opset 20 is not supported`; bf16 needs a higher opset than T5's default 12. Fix: the newer **dynamo exporter** (`torch.onnx.export(..., dynamo=True)`) or manual RMSNorm decomposition.
- **Dynamic shapes & control flow.** Variable seq-len / KV-cache / batch are expressible (`dynamic_axes`) but fragile on some EPs (QNN-HTP historically rejected dynamic shapes).
- **>2 GB.** Protobuf's hard 2 GB cap forces external-data sidecars.

**SD/FLUX status (2025-2026):** `ORTDiffusionPipeline` (in `optimum-onnx`) added SD3.x and FLUX.1, but you generally need the dynamo path to get past RMSNorm/bf16. Workable, not turnkey for the newest DiT backbones. **thvm's own import path is conceptually a T3 *lowering* target** - the "import anything, no forward code" prize, paid for in an op-coverage interpreter and dynamic-shape brittleness.

### 4.3 MLX (Apple) - Tier 2, safetensors-based

Relevant because the *current* FLUX WL implementation runs on MLX. Not a single-file format - a **HF-repo directory**: one or more `*.safetensors` (Tier 1) + `config.json` (hyperparams -> Tier 2) + tokenizer files. `mlx-lm`/`mlx` load directly from the Hub; the community hub is `mlx-community`. Quantization is **affine group quantization** (`mlx.core.quantize(w, group_size, bits)`): each group (commonly 32 or 64) shares a `scale` + `bias` (bf16), stored on disk as the packed integer weight tensor plus sibling `*.scales`/`*.biases` tensors, with `bits`/`group_size`/`mode` in `config.json`'s `quantization` block. **An importer that already reads safetensors only needs the affine-dequant rule (`w = q * scale + bias` per group) + the `config.json` schema** to consume MLX repos - far cheaper than GGUF's per-quant block decoders.

### 4.4 Apple Core ML - `.mlpackage` / `.mlmodelc` (Tier 3)

Apple's on-device graph+weights format. Two backends: modern **ML Program** (`mlprogram`, typed SSA-like op graph, default since coremltools 5) and legacy Neural Network. `.mlpackage` is the authoring/distribution bundle (not cached-compiled, recompiles per compute unit on load); `.mlmodelc` is the compiled, cached form that ships in apps. Produced by `coremltools.convert(traced_or_exported_pytorch, ...)` directly (no ONNX hop since coremltools 4.0); Apple's `ml-stable-diffusion` provides the SD->CoreML pipeline + Swift runtime. Compression: palettization (1/2/4/6/8-bit LUT), linear quantization, pruning, and **Mixed-Bit Palettization** (per-layer recipes to a target average bit-width). SD ships 6-bit palettized to fit on iPhone.

### 4.5 TensorFlow / Keras - mostly historical for open LLMs

| Format | Tier | Status |
|---|---|---|
| **SavedModel** (`saved_model.pb` + `variables/` + `assets/`) | **T3** | canonical TF export; deprecated for Keras in favor of `.keras` |
| **`.keras`** (zip: config JSON + weights + metadata) | T2/T3 | current Keras 3 format |
| **`.h5` / HDF5** | T1/T2 | legacy; superseded |
| **`.tflite` / LiteRT** (FlatBuffer) | **T3** | active, rebranded **LiteRT** under Google AI Edge; LiteRT-LM + MediaPipe LLM Inference is the on-device-Gemma path |

The open-weights LLM community (Llama, Mistral, Qwen, DeepSeek, GPT-OSS) ships safetensors/GGUF, not TF formats. TF/Keras matter for thvm mainly for older vision/CV checkpoints and Gemma-on-edge. The TFLite converter now also ingests JAX (`from_jax`) and PyTorch (via **AI Edge Torch**, using `torch.export`).

### 4.6 Other runtime / packaging formats (brief)

- **TorchScript** (`torch.jit.script`/`.trace`, `.pt`) - Tier 3 self-contained graph; superseded by **`torch.export` -> `ExportedProgram`** (the modern AOT graph capture feeding ExecuTorch / AI Edge Torch).
- **TensorRT engine** (`.engine`/`.plan`) - Tier 3 but **hardware-frozen**: an offline-compiled, GPU-arch-specific, kernel-fused plan; not portable (tied to GPU SM + TRT version), not really a distribution format (you ship the source model and *build* the engine). **TensorRT-LLM** builds these.
- **llamafile** (Mozilla.ai) - **packaging, not a new format**: GGUF weights + the llama.cpp runtime fused into ONE cross-OS/arch executable via Cosmopolitan Libc. The model inside is still GGUF (Tier 2).
- **Modular MAX** - Mojo inference platform; MAX Pipelines natively *load GGUF* (since 24.4, also on macOS). Consumes existing formats rather than minting one.
- **ExecuTorch** (`.pte`) - PyTorch's on-device runtime, v1.0 late 2025, ~50 KB base footprint, ~80% of popular HF edge LLMs have working exports; built from `ExportedProgram`. Tier 3 for edge.

### 4.7 Quantization as a distribution axis

Quantization is a *second axis* orthogonal to the container: it dictates the **byte layout of tensor blocks** and **which runtime can consume them**. An importer implements the **dequant rule** per scheme. GGML quants ship in GGUF; everything else mostly ships in safetensors + a quant config.

| Scheme | Bits | Block / grouping | Scale repr | Ships in | Consumed by | Notes |
|---|---|---|---|---|---|---|
| **GGML K-quants** (`Q4_K_M`, `Q5_K`, `Q6_K`, `Q8_0`) | 2-8 | super-block (256), per-sub-block scale+min | fp16 | **GGUF** | llama.cpp, Ollama, llamafile, LM Studio, MAX | `_K` = k-quant; `_M`/`_S`/`_L` = size mix |
| **GGML I-quants** (`IQ2_XXS`, `IQ3_S`, `IQ4_NL`, `IQ1_S`) | 1-4 | codebook / non-linear | fp16 | GGUF | llama.cpp | better quality at very low bits; slower decode |
| **GPTQ** | 3/4/8 | per-group (128), col-wise | fp16 | safetensors + config | vLLM, Transformers, TGI, **GPTQModel** | calibration error-compensation; AutoGPTQ archived Apr 2025, GPTQModel is the successor |
| **AWQ** | 4 | per-group, activation-aware salient-channel scaling | fp16 | safetensors + config | **TensorRT-LLM**, vLLM, Transformers | often best 4-bit accuracy; needs calibration stats |
| **bitsandbytes NF4** | 4 | block (64) + double-quant | fp16/fp8 | applied at load over fp16 safetensors | Transformers/PEFT (QLoRA) | info-theoretic-optimal 4-bit for ~Gaussian weights; trainable |
| **bitsandbytes int8** (LLM.int8) | 8 | block-wise + outlier fp16 path | per-block | load-time | Transformers | mixed int8 + fp16-outlier decomposition |
| **FP8 E4M3** | 8 | per-tensor/row/block scale | fp32 | safetensors (`F8_E4M3`) / TRT-LLM | Hopper/Blackwell, vLLM, TRT-LLM | 4-exp/3-mant; default FP8 weight/act type |
| **FP8 E5M2** | 8 | per-tensor scale | fp32 | safetensors (`F8_E5M2`) | same | 5-exp/2-mant; wider range, less precision |
| **MXFP4** (microscaling) | 4.25 eff. | block 32, shared **E8M0** (8-bit exp) scale | E8M0 | safetensors / native | **Blackwell/MI300 native**, GPT-OSS | OCP microscaling std; GPT-OSS 120B ships MoE in MXFP4 |
| **NVFP4** | ~4 | block 16, **E4M3** scale (finer) | E4M3 | NVIDIA checkpoints, TRT-LLM | Blackwell | finer grouping than MXFP4 -> better accuracy; FLUX.2 NVFP4, DeepSeek-V4 native FP4+FP8 |
| **EXL3 / EXL2** | 2-8 (variable) | mixed per-layer | - | ExLlama format | ExLlamaV2/V3 | enthusiast GPU runtime; not a cross-ecosystem standard |

**Ecosystem mapping:** llama.cpp/Ollama/llamafile/LM Studio/MAX -> GGUF k-/i-quants (+ MXFP4 for GPT-OSS); vLLM/Transformers/TGI -> GPTQ/AWQ/bitsandbytes/FP8 + native MXFP4; TensorRT-LLM -> AWQ/FP8/NVFP4; MLX -> affine group-quant in safetensors; Core ML -> palettization/MBP. The emerging frontier is FP8 (train+infer parity) and microscaling FP4 (MXFP4/NVFP4), the first production-viable 4-bit FP because Blackwell executes it natively.

---

## 5. Decision table

| Format | Tier | Tokenizer? | Arch hparams? | Quant support | mmap / zero-copy? | thvm-fit | Effort to import |
|---|---|---|---|---|---|---|---|
| **safetensors** | T1 | no | no (need `config.json`) | stores raw bytes; dequant per scheme | **yes** (8B len + JSON + mmap blob) | **best**: thvm already reads it | very low to parse; per-arch forward to use |
| **HF repo (config.json + safetensors)** | T2 | yes (tokenizer.json sidecar) | **yes** (config.json) | GPTQ/AWQ/bnb/FP8 via config | yes (per-shard) | **best**: the LLM/diffusion sweet spot | low parse; one family forward |
| **GGUF** | T1->T2 | **yes** (embedded KVs) | **yes** (`general.architecture` + `[arch].*`) | **native** GGML k-/i-quants | **yes** (whole file mmap-designed) | good: self-contained, but needs a reader + dequant kernels | medium: header parser + per-quant block decoders + per-arch forward |
| **MLX repo** | T2 | yes | yes (config.json) | affine group-quant (scales/biases siblings) | yes (it's safetensors) | good: nearly free given safetensors + affine dequant | low |
| **PyTorch `.pt`/`.bin`** | T1 | no | no | stored as-is | no (pickle deserialize) | **avoid as primary** (security + pickle VM) | high (pickle VM + ZIP) |
| **ONNX** | T3 | no | n/a (graph encodes it) | QDQ / int8 / fp8 nodes | partial (external-data) | "import anything" prize, later project | high (op-coverage interpreter; dynamic-shape brittleness) |
| **Core ML** | T3 | no | n/a | palettization / MBP / linear | compiled `.mlmodelc` | low priority (Apple-specific) | high (protobuf graph + op coverage) |
| **TF SavedModel** | T3 | no | n/a | per-op | no | low (legacy for LLMs) | high |
| **TorchScript / ExportedProgram** | T3 | no | n/a | per-op | no | low | high |
| **TensorRT engine** | T3 (frozen) | no | n/a | AWQ/FP8/NVFP4 baked | n/a | not a distribution format | n/a (build per target) |

---

## 6. thvm today

### 6.1 safetensors: YES (`SafeTensors.wl`)

**File:** `wl/THVMLink/Kernel/SafeTensors.wl`. Public API:
- `TSafeTensorLoad[path]` -> lazy mmap-backed disk tensor (Association `name -> TTerm`)
- `TSafeTensorSave[assoc, path]` -> writes a safetensors file
- `TTensorMMap[path, byteOffset, nbytes, dtype, shape]` -> zero-copy mmap tensor view

**dtype coverage (round-trip safe), via the `$safeToThvm` map - byte-aligned only:**
- ints: bool, i8, u8, i16, u16, i32, u32, i64, u64
- floats: f32, f64

**Critical gaps:**
- **No f16, bf16, fp8 (e4m3/e5m2)** - explicitly excluded from the round-trip surface (SafeTensors.wl:50-53 comment: "nibble + narrow-float dtypes are not part of the round-trip surface").
- **No int4/uint4 (nibble)** packed dtypes.
- **No GGML quantization types** (Q4_0, Q4_K, Q5_K, Q6_K, IQ2_S, IQ3_XXS, ...).

**Tests:** `wl/THVMLink/Tests/safetensors.wlt` covers f32 round-trip (bit-identical anchor), i64 round-trip, lazy mmap-backed disk tensor + CPU ops, and spec compliance (8-byte LE header, JSON parsing, offset accuracy).

### 6.2 The dtype mismatch (richer runtime than the safetensors surface)

`src/thvm.h` (lines 262-278) defines a *richer* dtype set than the WL safetensors surface exposes:

```
DT_BOOL(0)     DT_INT8(1)    DT_UINT8(2)   DT_INT16(3)  DT_UINT16(4)
DT_INT32(5)    DT_UINT32(6)  DT_INT64(7)   DT_UINT64(8)
DT_FP8E4M3(9)  DT_FP8E5M2(10) DT_FP16(11)  DT_BF16(12)  DT_FP32(13) DT_FP64(14)
DT_INT4(15)    DT_UINT4(16)  [packed nibble]
```

So the runtime *supports* f16/bf16/fp8 (DT_FP16/DT_BF16/DT_FP8E4M3/DT_FP8E5M2) and nibble (DT_INT4/DT_UINT4), but `TSafeTensorLoad` **cannot load** them - they are not in `$safeToThvm`. **This is the immediate blocker for FLUX.2**, whose weights are bf16: a bf16 safetensors tensor is silently dropped at load (unknown dtype -> no entry). Closing the gap is mostly mapping the header dtype string to the existing runtime dtype at load (plus bitcast handling), not new runtime capability.

### 6.3 GGUF: NONE in thvm proper

`grep gguf wl/...` -> 0 matches in `.wl`. The MLX submodule has a full C++ implementation - `external/mlx/mlx/io/gguf.cpp` (metadata parser + tensor header reader + GGML dequant kernels for Q4_0/Q4_1/Q5_0/Q5_1/Q8_0/Q4_K/Q5_K/Q6_K/IQ3_XXS/IQ3_S/IQ2_S/IQ4_XS/MXFP4/Q1_0, plus native F16/BF16/FP8) - **but it is not wired into thvm's WL layer.** A GGUF importer would need: (1) a header/metadata parser (magic check, KV dict, tensor-info array), (2) GGML dequant kernels in thvm (block unpacking + scale reconstruction), (3) a name->tensor mapping layer (`load("model.gguf")["..."]` -> `TTerm`).

### 6.4 "Import" today is `TFromNet`, not a weights file

`TFromNet[net, x]` (`wl/THVMLink/Kernel/NN.wl`) **only accepts Wolfram NeuralNetworks objects** (NetChain, LinearLayer, ConvolutionLayer, ElementwiseLayer, BatchNormalizationLayer, PoolingLayer). It walks the layer tree via the `fromLayer` pattern-match (lines 1100+), extracts weights via `NetExtract[layer, "Weights"]` -> NumericArray, and lifts each layer's forward into a UOp graph by hand (TLinear, TConv2D, TMatMul, TReLU, TSoftmax). Examples: LinearLayer -> TMatVec/TLinear (1100-1119); ElementwiseLayer -> dispatch by function symbol (1208-1216); ConvolutionLayer -> TConv2DIm2Col; PoolingLayer -> TMaxPool2d (1169-1199).

**The gap:** there is **NO generic weights-file -> forward bridge** and **no `load_state_dict` equivalent**. To run a FLUX/Llama from a weights file you must (1) parse the file (safetensors: done modulo dtype; gguf: not done) and (2) **hand-write the forward** in ~150-300 lines of TTerm/UOp combinators, then map `{name -> TTensor}` into it by hand.

| Aspect | Status | Notes |
|---|---|---|
| safetensors load | YES | `TSafeTensorLoad`; mmap lazy tensors; zero-copy via `TTensorMMap` |
| safetensors save | YES | `TSafeTensorSave`; spec-compliant; tested |
| safetensors dtype coverage | PARTIAL | f32/f64/ints only; **no f16/bf16/fp8/nibble** in the WL surface |
| GGUF load | NO | MLX C++ exists but not wired into WL |
| GGUF dequant | NO | Q4_0-Q6_K, IQ* not accessible from thvm |
| weights -> runnable | MANUAL | `TFromNet` only lifts Wolfram NetChain; no weights-file -> forward bridge |
| state-dict load | NO GENERIC | must map `{name -> TTensor}` to weights by hand |
| quantized inference | NO | Q-formats need dequant; not in the safetensors surface |

---

## 7. FLUX-on-thvm recommendation

### 7.1 The current heavy manual WL/MLX path

FLUX.2-klein-4B runs today entirely on MLX (Apple Silicon) as a monolithic `.wlnet` (`exported_FLUX2_klein_4B/flux2_q8.wlnet`, 7.3 GB, Q8 = INT8 weights + BFloat16 scales), generated by a heavy export pipeline. The manual-encoding burden is **~4,524 lines of WL across 9 files**, hand-writing every block, attention pattern, and projection:

| File | Lines | Purpose |
|---|---|---|
| Export.wl | 1,853 | weight loading, arch config, per-component export, block building |
| TextEncoders.wl | 1,142 | Qwen3 decoder blocks (36 x ~32 lines), BPE tokenizer, text-encoder nets |
| Generate.wl | 626 | MLX runtime: embedding, attention, RoPE, linear/quantized_matmul, sampler loop |
| Transformer.wl | 221 | single/double FLUX blocks, RMS/layer norms, SiLU gating, attention |
| Layers.wl | 310 | RMSNorm, timestep embeddings, SiLU, modulation, time MLP |
| VAE.wl | 138 | VAE decoder blocks (ResBlock, UpBlock, MidAttention) |
| RoPE.wl | 94 | multi-axis RoPE: freq tables, Cos/Sin, interleaved rotation |
| Qwen3Tokenizer.wl | 109 | BPE tokenizer (legacy) |
| FLUX.wl | 31 | paclet loader / imports |

Every layer is an explicit Wolfram `NetGraph` with `LinearLayer`/`ConvolutionLayer`/`AttentionLayer`/`ReshapeLayer`/`PartLayer`. The 20 single blocks alone are ~100 lines each of near-identical copy-paste (~2,000 lines); the 5 double blocks repeat the pattern; RoPE is 94 hand-coded lines; architecture constants (`$Dim=3072`, `$NumHeads=24`, `$HeadDim=128`, `$MLPDim=9216`, `$NumDoubleBlocks=5`, `$NumSingleBlocks=20`, `RoPETheta=2000`, ...) are scattered as globals and changing the arch requires file edits + re-export. Export is a ~200-line orchestration script (load HF weights via `MLXLink`ImportSafetensors`, build the 3 stages, Q8-quantize via `MLXLink`Quantization`, emit one monolithic `.wlnet`); generation re-imports the whole 7.3 GB file. End-to-end ~16s, 3 hand-exported `.wlnet`, ~8 files of hand-encoded layers.

### 7.2 The desired path

> Load a *standard* file (safetensors shards from the HF diffusers repo, or a community GGUF) + a **compact per-architecture forward in thvm that reads named tensors**, running on Metal in **fp16/bf16** with a **<3s warm** target.

This collapses the ~4,524 lines to a loop over the architecture config: build N blocks from one factory parameterized by hyperparameters read from `config.json`, instead of copy-pasting 25 near-identical blocks.

### 7.3 Concrete recommended path

**Which file to load.** Prefer the **HF diffusers repo `black-forest-labs/FLUX.2-klein-4B`** (3.7): the `transformer/` `diffusion_pytorch_model.safetensors` (7.75 GB bf16, single file, no shards), the `text_encoder/` Qwen3 (2 shards + index, ~8.04 GB bf16), the `vae/` (168 MB), each with its own `config.json` giving the hyperparameters. This is the lowest-effort path because **thvm already has a safetensors loader** (6.1). The community **`city96/FLUX.2-dev-gguf`** is the alternative if you want a smaller quantized file in one container, but it requires a GGUF reader + GGML dequant kernels and FLUX.2 GGUF is not yet a first-class self-serviceable arch (2.7).

**What thvm needs to add (two routes):**

1. **safetensors route (recommended, least new code):**
   - **Extend the safetensors dtype map** to load **bf16/f16** (map the header dtype string to the existing `DT_BF16`/`DT_FP16` runtime dtypes at load, with bitcast handling). This is the *one hard blocker* (6.2) - without it the bf16 FLUX weights are silently dropped. No new runtime capability needed; the dtypes already exist in `thvm.h`.
   - Add **fp8 e4m3/e5m2** + the FLUX.2-fp8/NVFP4 sibling-scale dequant rule only if you target the quantized repos; for a first cut, load the bf16 transformer/VAE directly.
   - **Write a compact FLUX forward in thvm** (~few-hundred lines of `TTerm`/UOp combinators) that reads named tensors: a block factory looped `num_layers`/`num_single_layers` times, MMDiT double-stream + single-stream attention, multi-axis RoPE (`axes_dims_rope [32,32,32,32]`, `rope_theta 2000`), modulation/SiLU gating, RMSNorm; plus the Qwen3 text-encoder forward (36 layers, GQA) and the VAE decoder. The forward is the *value-add* and is hand-written **once per family**, then any same-family checkpoint loads by reading its hparams - exactly thvm's existing `TFromNet` family approach, but keyed off a weights file instead of a NetChain.
   - Add a `load_state_dict`-style helper to map `{name -> TTerm}` into the forward (the missing generic bridge, 6.4).

2. **GGUF route (only if you want the single-file quantized container):** add (a) a GGUF header/KV/tensor-info parser, (b) GGML dequant kernels in thvm (you can port the layouts from `external/mlx/mlx/io/gguf.cpp` (6.3), but the *spec* to follow is tinygrad's, below), and (c) the same per-architecture forward as route 1. More code than route 1 for the same forward, justified only by the smaller download and self-contained metadata.

**tinygrad is the spec to port.** Per the project rule that tinygrad is the spec, port the *loaders* and *compact forwards* directly:
- `tinygrad/tinygrad/llm/gguf.py` - `gguf_load(Tensor) -> (metadata, state_dict {name: Tensor})`: parses magic/version/KV/tensor headers, routes native types directly, routes quant types (Q4_0, Q4_K, Q5_K, Q6_K, IQ2_S, IQ3_XXS, ...) through inline dequant kernels (shift/pack/bitmask unpacking, `scale * quantized-values`). This is the reference for thvm's GGUF reader + dequant.
- tinygrad's `safe_load` / safetensors path - the reference for the dtype-complete loader (bf16/fp8 included).
- `tinygrad/examples/stable_diffusion.py` (and `examples/llama.py`, ~200 lines; `examples/gpt2.py`, ~150 lines) - the reference for the **compact, hand-written per-architecture forward** that reads a state dict and wires ops. tinygrad does NOT auto-generate the forward; each architecture is a ~200-line class. `gguf_load`/`safe_load` handle the tedious dequant/parse; the forward is domain-specific. thvm's FLUX forward should mirror `stable_diffusion.py`'s structure (DiT/UNet block factory + scheduler loop), adapted to FLUX.2's MMDiT.

**The <3s warm target.** Metal, fp16/bf16. The cost is dominated by the **4B-parameter transformer** (the Qwen3 text encoder at ~8 GB bf16 plus the 7.75 GB MMDiT denoiser; the 168 MB VAE is negligible). FLUX.2-klein is the *distilled, 4-step* model (`is_distilled: true`, 4-step flow-matching Euler schedule), so the denoiser runs only 4 forward passes - that 4x (not 28-50x) is what makes a few-second warm latency realistic. "Warm" = weights already mmap'd and the thvm UOp graph already lifted+compiled (the analogue of the GPT-2 `TJit` capture/slot-replay path, where re-lift was the bottleneck, not compute); the first call pays the lift/compile, subsequent calls are kernel dispatch only. Hitting <3s requires: bf16 weights resident (no fp32 round-trip), the MMDiT and Qwen3 forwards lifted once and JIT-replayed, and Metal dispatch that doesn't over-fuse (prescreen kernel count on `DEV=cpu` first; the known TRAIN-mode over-fusion that orphans the Metal GPU is an inference non-issue but worth a count check). The win vs. the current ~16s monolithic-`.wlnet` path comes from (a) dropping the per-component re-export/re-import, (b) loading standard bf16 weights mmap'd zero-copy instead of a 7.3 GB Q8 blob, and (c) a single lifted forward replayed across the 4 sampler steps.

---

## 8. References

**GGUF**
- ggml-org/ggml - docs/gguf.md (canonical spec): https://github.com/ggml-org/ggml/blob/master/docs/gguf.md
- HuggingFace Hub - GGUF docs (quant-type table, JS parser): https://huggingface.co/docs/hub/en/gguf
- ggml-org/llama.cpp - ggml/src/ggml-common.h (block structs): https://github.com/ggml-org/llama.cpp/blob/master/ggml/src/ggml-common.h
- DeepWiki - llama.cpp GGUF File Format: https://deepwiki.com/ggml-org/llama.cpp/7.1-gguf-file-format
- DeepWiki - llama.cpp Supported Model Architectures (arch->builder): https://deepwiki.com/ggml-org/llama.cpp/3.11-supported-model-architectures
- llama.cpp PR #1684 - k-quants (ikawrakow): https://github.com/ggml-org/llama.cpp/pull/1684
- Artefact2 gist - GGUF quantizations overview (bpw table): https://gist.github.com/Artefact2/b5f810600771265fc1e39442288e8ec9
- DeepWiki - Model Conversion from HuggingFace (gguf-py / TensorNameMap): https://deepwiki.com/ggml-org/llama.cpp/7.2-model-conversion-from-huggingface
- gguf on PyPI: https://pypi.org/project/gguf/
- llama.cpp - tools/quantize/README.md: https://github.com/ggml-org/llama.cpp/blob/master/tools/quantize/README.md
- city96/ComfyUI-GGUF: https://github.com/city96/ComfyUI-GGUF
- city96/FLUX.1-dev-gguf: https://huggingface.co/city96/FLUX.1-dev-gguf
- city96/FLUX.2-dev-gguf: https://huggingface.co/city96/FLUX.2-dev-gguf
- ComfyUI-GGUF issue #418 ("Unknown Architecture: Flux / Lumina2"): https://github.com/city96/ComfyUI-GGUF/issues/418
- Unsloth docs - Running diffusion GGUFs in ComfyUI: https://unsloth.ai/docs/blog/comfyui

**safetensors + HuggingFace**
- safetensors GitHub (format spec, dtypes, v0.8.0): https://github.com/huggingface/safetensors
- HF docs - safetensors index: https://huggingface.co/docs/safetensors/index
- HF docs - safetensors metadata parsing (byte layout, offsets, Range requests): https://huggingface.co/docs/safetensors/metadata_parsing
- DeepWiki - safetensors file format: https://deepwiki.com/huggingface/safetensors/2.1-file-format
- FLUX.2-klein-4B repo tree: https://huggingface.co/black-forest-labs/FLUX.2-klein-4B/tree/main
- FLUX.2-klein-4B model_index.json: https://huggingface.co/black-forest-labs/FLUX.2-klein-4B/blob/main/model_index.json
- FLUX.2-klein-4B transformer/config.json: https://huggingface.co/black-forest-labs/FLUX.2-klein-4B/blob/main/transformer/config.json
- FLUX.2-klein-4B text_encoder folder: https://huggingface.co/black-forest-labs/FLUX.2-klein-4B/tree/main/text_encoder
- FLUX.2-klein-4B vae/config.json: https://huggingface.co/black-forest-labs/FLUX.2-klein-4B/blob/main/vae/config.json
- FLUX.2-dev-NVFP4 repo: https://huggingface.co/black-forest-labs/FLUX.2-dev-NVFP4/blob/main/flux2-dev-nvfp4.safetensors
- NVIDIA - Scaling NVFP4 Inference for FLUX.2 on Blackwell: https://developer.nvidia.com/blog/scaling-nvfp4-inference-for-flux-2-on-nvidia-blackwell-data-center-gpus/
- SGLang diffusion quantization docs (NVFP4/MXFP4/FP8): https://sgl-project.github.io/diffusion/quantization.html
- black-forest-labs/flux2 official inference repo: https://github.com/black-forest-labs/flux2

**Other formats + quantization**
- coremltools convert-pytorch: https://apple.github.io/coremltools/docs-guides/source/convert-pytorch.html
- PyTorch 2.6 blog (weights_only default flip): https://pytorch.org/blog/pytorch2-6/
- dev-discuss - weights_only BC-break: https://dev-discuss.pytorch.org/t/bc-breaking-change-torch-load-is-being-flipped-to-use-weights-only-true-by-default-in-the-nightlies-after-137602/2573
- HF pickle security: https://huggingface.co/docs/hub/en/security-pickle
- JFrog - PickleScan zero-days: https://jfrog.com/blog/unveiling-3-zero-day-vulnerabilities-in-picklescan/
- optimum SD3 ONNX export #2093: https://github.com/huggingface/optimum/issues/2093
- NVIDIA Model-Optimizer RMSNorm #262: https://github.com/NVIDIA/Model-Optimizer/issues/262
- diffusers ONNX optimization docs: https://huggingface.co/docs/diffusers/optimization/onnx
- HF MLX docs: https://huggingface.co/docs/hub/en/mlx
- mlx.core.quantize: https://ml-explore.github.io/mlx/build/html/python/_autosummary/mlx.core.quantize.html
- ml-explore/mlx-lm: https://github.com/ml-explore/mlx-lm
- An Examination of MLX Quantization: https://n8programs.substack.com/p/an-examination-of-mlx-quantization
- apple/ml-stable-diffusion: https://github.com/apple/ml-stable-diffusion
- HF blog - fast diffusers on Core ML: https://huggingface.co/blog/fast-diffusers-coreml
- Stable Diffusion with Core ML (Apple ML Research): https://machinelearning.apple.com/research/stable-diffusion-coreml-apple-silicon
- TF SavedModel guide: https://www.tensorflow.org/guide/saved_model
- LiteRT convert: https://ai.google.dev/edge/litert/models/convert_tf
- mozilla-ai/llamafile: https://github.com/mozilla-ai/llamafile
- Modular MAX 24.4 (GGUF support): https://www.modular.com/blog/whats-new-in-max-24-4-max-on-macos-fast-local-llama3-native-quantization-and-gguf-support
- NVIDIA/TensorRT-LLM: https://github.com/NVIDIA/TensorRT-LLM
- edgeaistack - edge LLM runtime stack 2026: https://edgeaistack.ai/blog/edge-llm-runtime-stack-2026/
- MXFP4 study (arXiv:2509.23202): https://arxiv.org/abs/2509.23202
- NVFP4 pretraining (arXiv:2509.25149): https://arxiv.org/pdf/2509.25149
- openai/gpt-oss: https://github.com/openai/gpt-oss
- nvidia/DeepSeek-R1-NVFP4: https://huggingface.co/nvidia/DeepSeek-R1-NVFP4
- GPTQ vs AWQ vs GGUF vs bitsandbytes tradeoffs: https://www.bestaiweb.ai/gptq-vs-awq-vs-gguf-vs-bitsandbytes-quantization-formats-and-their-tradeoffs-explained/
- HF quantization concept guide: https://huggingface.co/docs/transformers/en/quantization/concept_guide
- Understanding safetensors (DEV): https://dev.to/lukehinds/understanding-safetensors-a-secure-alternative-to-pickle-for-ml-models-o71

**tinygrad (the spec to port)**
- tinygrad/tinygrad/llm/gguf.py (gguf_load + dequant)
- tinygrad/examples/stable_diffusion.py (compact diffusion forward)
- tinygrad/examples/llama.py, gpt2.py, llama3.py (compact LLM forwards + load dispatch)

**thvm (current state)**
- wl/THVMLink/Kernel/SafeTensors.wl (TSafeTensorLoad/Save, TTensorMMap)
- wl/THVMLink/Tests/safetensors.wlt (round-trip + spec-compliance tests)
- wl/THVMLink/Kernel/NN.wl (TFromNet, fromLayer lifting)
- src/thvm.h:262-278 (DT_* dtype enum)
- external/mlx/mlx/io/gguf.cpp (C++ GGUF load + GGML dequant, not wired to WL)
