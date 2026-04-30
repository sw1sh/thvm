(* lowering.wlt -- Phase A scaffolding tests for the scalar-UOp
   introspection surface (TKernelScalarUops).  The rangeify
   lowering pass itself lands in Phase B; this file currently
   only exercises the introspection plumbing on the legacy emit
   path (which never fills `ke->scalar_uops`, so every kernel
   reads back as Missing["NotLowered"]).  As Phases B/C/D land,
   add assertions here that lock in the expected post-lowering
   shape (op counts, src wiring, axis types). *)

(* When rangeify is off (default), every kernel emits via the
   legacy visit() path and TKernelScalarUops should report
   Missing["NotLowered"]. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    TRealize[a + b];
    n = TKernelCount[] - 1;
    AllTrue[Range[n], TKernelScalarUops[#] === Missing["NotLowered"] &],
    True,
    TestID -> "lowering/legacy-path-reports-missing"
]

(* TKernelScalarUops[0] (the reserved sentinel slot) should also
   read back as Missing -- kid 0 has no program. *)
VerificationTest[
    TInit[];
    TKernelScalarUops[0],
    Missing["NotLowered"],
    TestID -> "lowering/sentinel-slot-missing"
]

(* Querying past KERNELS_NEXT should be a Missing too (the C-side
   returns LIBRARY_FUNCTION_ERROR; the WL wrapper turns that into
   Missing).  Use a kid we KNOW doesn't exist (TKernelCount[]+100). *)
VerificationTest[
    TInit[];
    Quiet @ Check[TKernelScalarUops[TKernelCount[] + 100],
                  Missing["OutOfRange"]],
    Missing["OutOfRange"] | _Missing,
    SameTest -> MatchQ,
    TestID -> "lowering/out-of-range-graceful"
]
