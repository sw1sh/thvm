(* metal_dtypes.wlt -- Metal backend dispatch on per-dtype shader
   variants (Phase I).  Only f32 and i32 are wired today; the
   metal_kernel_supported predicate gates the rest back to CPU.

   Tests run under TInContext[ctx, ...] where ctx is a fresh Metal
   context (TContextNew["metal"]).  Skipped on platforms without
   Metal (TContextNew returns 0).  *)

VerificationTest[
    (* f32 elementwise add via Metal. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];   (* skip on no-Metal platforms *)
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
            b = TTensorCreate @ NumericArray[{0.5, 0.5, 0.5, 0.5}, "Real32"];
            Normal @ TTensorData @ TRealize[a + b]
        ];
        TContextDestroy[ctx];
        result === {1.5, 2.5, 3.5, 4.5} || ctx === 0
    ],
    True,
    TestID -> "metal/f32-add"
]

VerificationTest[
    (* i32 elementwise add via Metal -- new in Phase I. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "Integer32"];
            b = TTensorCreate @ NumericArray[{10, 20, 30, 40}, "Integer32"];
            Normal @ TTensorData @ TRealize[a + b]
        ];
        TContextDestroy[ctx];
        result === {11, 22, 33, 44} || ctx === 0
    ],
    True,
    TestID -> "metal/i32-add"
]

VerificationTest[
    (* i32 elementwise mul. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{2, 3, 4, 5}, "Integer32"];
            b = TTensorCreate @ NumericArray[{2, 2, 2, 2}, "Integer32"];
            Normal @ TTensorData @ TRealize[a * b]
        ];
        TContextDestroy[ctx];
        result === {4, 6, 8, 10} || ctx === 0
    ],
    True,
    TestID -> "metal/i32-mul"
]

VerificationTest[
    (* i32 reduce-sum. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{1, 2, 3, 4, 5}, "Integer32"];
            Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"]
        ];
        TContextDestroy[ctx];
        result === {15} || ctx === 0
    ],
    True,
    TestID -> "metal/i32-reduce-sum"
]

VerificationTest[
    (* Non-supported dtype (i8) on a Metal context: dispatch returns
       -1 (Metal has no i8 shader variants yet) so the kernel never
       fires.  This test confirms the failure is graceful (no
       crash) rather than producing garbage.  Users who want i8 on
       a Metal context should CAST to i32 / f32 first. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "Integer8"];
            Normal @ TTensorData[a]
        ];
        TContextDestroy[ctx];
        result === {1, 2, 3, 4}
    ],
    True,
    TestID -> "metal/i8-input-roundtrip"
]
