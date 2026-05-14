# Dtypes

THVM mirrors tinygrad's full dtype set (15 concrete dtypes) plus
packed `int4` / `uint4` for modern quantization.  Every dtype is
end-to-end through the CPU interpreter and the WL bridge; the
codegen / JIT / BLAS / Metal paths still gate to F32 only and fall
back to interpret.c on anything else.

Reference:
- enum + DTypeInfo:    [src/dtype/info.c](../src/dtype/info.c)
- dtype primitives:    [src/dtype/lane.c](../src/dtype/lane.c) · [src/dtype/fp_convert.c](../src/dtype/fp_convert.c) · [src/dtype/fp8.c](../src/dtype/fp8.c) · [src/dtype/nibble.c](../src/dtype/nibble.c)
- enum:                [src/thvm.h:167](../src/thvm.h#L167)
- WL surface:          [wl/THVMLink/Kernel/Tensor.wl](../wl/THVMLink/Kernel/Tensor.wl) (`naTypeFor`, `TTensorCreate[..., dtype_String]`, `TUOpCast`, `TUOpBitcast`)

## Table

| ID | Name        | itemsize | kind     | signed | NumericArray carrier | ALU             |
|----|-------------|----------|----------|--------|----------------------|-----------------|
|  0 | `bool`      | 1        | bool     | n      | UnsignedInteger8     | logical OR/AND  |
|  1 | `i8`        | 1        | sint     | y      | Integer8             | native          |
|  2 | `u8`        | 1        | uint     | n      | UnsignedInteger8     | native          |
|  3 | `i16`       | 2        | sint     | y      | Integer16            | native          |
|  4 | `u16`       | 2        | uint     | n      | UnsignedInteger16    | native          |
|  5 | `i32`       | 4        | sint     | y      | Integer32            | native          |
|  6 | `u32`       | 4        | uint     | n      | UnsignedInteger32    | native          |
|  7 | `i64`       | 8        | sint     | y      | Integer64            | native          |
|  8 | `u64`       | 8        | uint     | n      | UnsignedInteger64    | native          |
|  9 | `fp8e4m3`   | 1        | fp8      | y      | UnsignedInteger8 *   | promote -> f32  |
| 10 | `fp8e5m2`   | 1        | fp8      | y      | UnsignedInteger8 *   | promote -> f32  |
| 11 | `f16`       | 2        | fp16     | y      | UnsignedInteger16 *  | promote -> f32  |
| 12 | `bf16`      | 2        | bf16     | y      | UnsignedInteger16 *  | promote -> f32  |
| 13 | `f32`       | 4        | float    | y      | Real32               | native          |
| 14 | `f64`       | 8        | float    | y      | Real64               | native          |
| 15 | `i4`        | packed   | int4     | y      | UnsignedInteger8 *   | unpack -> i8    |
| 16 | `u4`        | packed   | uint4    | n      | UnsignedInteger8 *   | unpack -> i8    |

`*` -- raw-bytes carrier.  The NumericArray bytes do not represent
their numeric value directly; the WL surface decodes via the dtype-
specific helper (`TFP16ToReal`, `TFP8E4M3ToReal`, `TUnpackInt4`,
etc.), and TTensorCreate accepts these dtypes by packing the input
through the inverse helper before allocating the tensor.

For packed nibble dtypes (`i4`, `u4`) the storage byte count is
`ceil(numel / 2)`; logical numel is carried on `TenDesc.view.numel`.
The WL surface receives the raw byte buffer and reconstructs the
logical shape via `TTensorShape[]`.

## ALU strategy

Native lanes -- f32, f64, the integer family.  The kernel reads /
writes the dtype's storage directly and runs typed arithmetic.

Promote-to-f32 lanes -- f16, bf16, fp8e4m3, fp8e5m2.  No native ALU
on any backend; arithmetic always promotes inputs to f32 via
`to_fp32_lane`, runs the f32 kernel, then demotes via
`from_fp32_lane`.  Driver: `cpu_op_run_via_f32` in
[src/backend/cpu/op/_promote.c](../src/backend/cpu/op/_promote.c).
This path is bit-for-bit precision-bound by f32 internally; bf16 and
fp8 round-trip exact for their representable values, f16 / fp8e5m2
overflow saturate to inf, fp8e4m3 saturates at MAXNORM = 448.

Unpack-to-i8 lanes -- i4, u4.  Storage is 2 nibbles per byte, low
nibble first.  Movement ops route through `cpu_op_run_via_i8` (in
the same `_promote.c`): unpack the source bytes to a temp i8
buffer, run the i8 movement kernel, repack to nibbles.  Elementwise
ops route through `cpu_op_run_via_f32` like the float-narrow types
(int4 / uint4 unpacking is handled inside `to_fp32_lane`).

## CAST and BITCAST

CAST is value-preserving across any dtype pair.  Heap layout
`[src, NUM(dst_dtype)]`.  Implementation: `to_fp32_lane(src)` ->
`from_fp32_lane(dst)`.

BITCAST is bit-level reinterpret; source and destination must share
itemsize.  Implementation is a memcpy.  Width mismatch is caught at
the constructor (`uop_bitcast` returns the source unchanged with a
warning).  Backward gradient is undefined (returns CONST(0) of
src.dtype), matching tinygrad/gradient.py:42.

CAST backward is `cast(gy, src.dtype)`, matching
tinygrad/gradient.py:17.

Folding rules:
- `CAST(x, x.dtype) -> x`
- `CAST(CONST(bits, src), dst) -> CONST(converted, dst)` -- folded
  at the constructor level
- `BITCAST(x, x.dtype) -> x`
- `BITCAST(BITCAST(x, mid), dst) -> BITCAST(x, dst)`
- `BITCAST(CONST(bits, src), dst) -> CONST(bits, dst)` -- same bits
  reinterpreted

## Quantization

The dequantization pattern composes through CAST + arithmetic.  No
dedicated UOP_DEQUANT / UOP_QUANT primitives -- matches tinygrad's
choice (see TinyHVM/tinygrad/tinygrad/codegen/quantize.py for
tinygrad's pattern-rewrite-based approach).

```mathematica
(* Dequantize an int4 tensor: (raw - zp) * scale *)
weights_int4 = TTensorCreate[{1, -2, 3, -4}, "i4"];
zp           = TTensorCreate @ NumericArray[{-3.0}, "Real32"];
sc           = TTensorCreate @ NumericArray[{0.5}, "Real32"];
out = TRealize[(TUOpCast[weights_int4, "f32"] - zp) * sc];
(* {2.0, 2.5, 3.0, 3.5} *)
```

For symmetric quantization (zp = 0), the subtract drops out:
`out = TUOpCast[weights, "f32"] * sc`.

For per-channel quantization, broadcast scale via reshape (which is
a view-only no-op when contiguous-compatible).

## Backend coverage

| Path        | Wired dtypes |
|-------------|--------------|
| CPU interpret  | all 17    |
| CPU JIT (clang) | f32 only |
| CPU BLAS (Accelerate) | f32 only (matmul / matvec / dot) |
| Metal shaders | f32 only |

Non-f32 paths fall through to interpret.

## WL surface examples

```mathematica
(* Native NumericArray carriers: just create with the matching type *)
a = TTensorCreate @ NumericArray[{1.5, 2.5, 3.5}, "Real64"];   (* f64 *)
b = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "Integer8"];    (* i8 *)
c = TTensorCreate @ NumericArray[{1, -1, 0, 1}, "UnsignedInteger8"]; (* u8 *)

(* Raw-bytes carrier dtypes -- pack from a Real / Integer list *)
d = TTensorCreate[{1.5, 2.5, 3.5}, "f16"];          (* fp16: packed bf16 raw bytes inside *)
e = TTensorCreate[{1.0, 2.0, 4.0, 8.0}, "fp8e4m3"]; (* fp8 *)
f = TTensorCreate[{1, -2, 3, -4}, "i4"];            (* packed nibbles *)

(* Read back: native carriers go through Normal directly; raw-bytes
   carriers need the dtype helper *)
Normal @ TTensorData[a]                 (* {1.5, 2.5, 3.5} *)
Normal @ TTensorData[b]                 (* {1, 2, 3, 4} *)
TFP16ToReal @ TTensorData[d]            (* {1.5, 2.5, 3.5} *)
TFP8E4M3ToReal @ TTensorData[e]         (* {1.0, 2.0, 4.0, 8.0} *)
TUnpackInt4[TTensorData[f], 4]          (* {1, -2, 3, -4} -- numel passed since
                                           bytes / 2 is ambiguous on odd-numel *)

(* Cast across dtypes -- value-preserving *)
TRealize @ TUOpCast[a, "f32"]
TRealize @ TUOpCast[b, "i32"]

(* Bitcast same-itemsize -- bit-level reinterpret *)
TRealize @ TUOpBitcast[
    TTensorCreate @ NumericArray[{1.0, 2.0}, "Real32"],
    "i32"
]   (* {1065353216, 1073741824} = {0x3F800000, 0x40000000} *)
```

## Test coverage

| File                                         | Tests | Focus                                          |
|----------------------------------------------|-------|------------------------------------------------|
| `wl/THVMLink/Tests/dtype_int8.wlt`           | 12    | i8 add/mul/neg/cmpeq/cmplt/reduce/wrap/move    |
| `wl/THVMLink/Tests/dtype_int_family.wlt`     | 14    | bool/u8/i16/u16/u32/i64/u64 wrap + 8B movement |
| `wl/THVMLink/Tests/dtype_f64.wlt`            | 9     | f64 native ALU + 8B movement                    |
| `wl/THVMLink/Tests/dtype_f16.wlt`            | 11    | f16 IEEE half conversion + arithmetic          |
| `wl/THVMLink/Tests/dtype_bf16.wlt`           | 8     | bfloat16 round-trip + arithmetic               |
| `wl/THVMLink/Tests/dtype_fp_convert.wlt`     | 8     | direct unit tests for f16 / bf16 helpers       |
| `wl/THVMLink/Tests/dtype_fp8.wlt`            | 19    | fp8e4m3 + fp8e5m2 round-trip + arithmetic      |
| `wl/THVMLink/Tests/cast.wlt`                 | 15    | CAST across int / float / bool / fp8           |
| `wl/THVMLink/Tests/bitcast.wlt`              | 11    | BITCAST 1B / 2B / 4B / 8B + folding            |
| `wl/THVMLink/Tests/dtype_int4.wlt`           | 17    | packed int4 / uint4 + dequant via CAST         |

Total: 124 dtype-specific VerificationTests.
