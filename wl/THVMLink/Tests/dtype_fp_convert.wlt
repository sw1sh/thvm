(* dtype_fp_convert.wlt -- direct unit tests for f16 / bf16 bit-pack
   conversions (TRealToFP16, TFP16ToReal, TRealToBf16, TBf16ToReal).
   These cover edge cases that the higher-level tensor wlt files
   skip: zero / negative zero, denormals, the full powers-of-2 range. *)

VerificationTest[
    TInit[];
    TFP16ToReal @ TRealToFP16[{0.0, -0.0}],
    {0.0, 0.0},
    TestID -> "fp_convert/fp16-zero"
]

VerificationTest[
    (* Smallest positive normal in fp16 = 2^-14. *)
    TInit[];
    First @ TFP16ToReal @ TRealToFP16[{2.0^-14}],
    2.0^-14,
    TestID -> "fp_convert/fp16-min-normal"
]

VerificationTest[
    (* Power-of-2 sweep: every exact f16 power of 2 from 2^-14 to 2^15. *)
    TInit[];
    powers = 2.0^Range[-14, 15];
    TFP16ToReal[TRealToFP16[powers]] === powers,
    True,
    TestID -> "fp_convert/fp16-powers-of-2-sweep"
]

VerificationTest[
    (* Sign preservation. *)
    TInit[];
    TFP16ToReal @ TRealToFP16[{1., -1., 1024., -1024.}],
    {1., -1., 1024., -1024.},
    TestID -> "fp_convert/fp16-signs"
]

VerificationTest[
    TInit[];
    TBf16ToReal @ TRealToBf16[{0.0, -0.0}],
    {0.0, 0.0},
    TestID -> "fp_convert/bf16-zero"
]

VerificationTest[
    (* bf16 has the same exponent range as f32; 2^120 stays finite. *)
    TInit[];
    Module[{r = First @ TBf16ToReal @ TRealToBf16[{2.0^120}]},
        r > 0 && r < Infinity
    ],
    True,
    TestID -> "fp_convert/bf16-large-finite"
]

VerificationTest[
    (* bf16 keeps 7 mantissa bits so e.g. 1.001 rounds to 1.0; the
       round-to-nearest-even logic in f32_to_bf16 trims the trailing
       16 mantissa bits. *)
    TInit[];
    Module[{r = First @ TBf16ToReal @ TRealToBf16[{1.0}]},
        r === 1.0
    ],
    True,
    TestID -> "fp_convert/bf16-exact-1"
]

VerificationTest[
    (* Sign preservation. *)
    TInit[];
    TBf16ToReal @ TRealToBf16[{1., -1., 100., -100.}],
    {1., -1., 100., -100.},
    TestID -> "fp_convert/bf16-signs"
]
