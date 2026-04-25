(* uop_load.wlt -- UOP_LOAD opcode plumbing.

   Sub-item (a) of the UOP_LOAD arc: just verifies the opcode slot
   is reserved + reachable from WL.  Constructor / materializer /
   linearizer behaviour lands in (b)/(c)/(d). *)

VerificationTest[
    IntegerQ[$UopLoad],
    True,
    TestID -> "uop-load/constant-is-integer"
]

(* $UopLoad must not collide with any other reserved opcode. *)
VerificationTest[
    With[{
        others = {$UopMaterialize, $UopKernel, $UopConst,
                  $UopReshape,    $UopPermute, $UopExpand,
                  $UopPad,        $UopShrink,  $UopFlip,
                  $UopAdd,        $UopMul,     $UopNeg,
                  $UopRecip,      $UopExp2,    $UopLog2,
                  $UopSqrt,       $UopCmplt,   $UopReduce,
                  $UopGrad,       $UopCmpeq}
    },
        FreeQ[others, $UopLoad]
    ],
    True,
    TestID -> "uop-load/distinct-from-other-opcodes"
]

(* Round-trips through the $uopNames pretty-printer table.  The
   table itself is private; reach in by full context. *)
VerificationTest[
    Lookup[THVMLink`Private`$uopNames, $UopLoad, Missing["NotFound"]],
    "LOAD",
    TestID -> "uop-load/named-load"
]

(* === sub-item (b): TUOpLoad constructor + identity materializer ===
   TUOpLoad[t] |> TRealize must produce a tensor element-equal to t. *)

VerificationTest[
    TInit[];
    src = TTensorCreate @ NumericArray[
        ConstantArray[1.0, {2, 3}], "Real32"];
    out = TRealize @ TUOpLoad[src];
    Normal @ TTensorData[out],
    {{1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}},
    TestID -> "uop-load/identity-on-ones"
]

VerificationTest[
    TInit[];
    src = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    out = TRealize @ TUOpLoad[src];
    {TTensorShape[out], Normal @ TTensorData[out]},
    {{2, 3}, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}},
    TestID -> "uop-load/identity-preserves-shape-and-values"
]

(* Composition: TUOpLoad inside an ADD chain should be transparent. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    Normal @ TTensorData @ TRealize @ TUOpAdd[TUOpLoad[a], TUOpLoad[b]],
    {5.0, 7.0, 9.0},
    TestID -> "uop-load/composes-under-add"
]

(* === sub-item (c): linearizer prepends LOAD per input slot ===
   A 2-input ADD kernel program now contains [LOAD, LOAD, ADD]. *)

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, 2.0, 3.0}];
    b = TTensor[{3}, {4.0, 5.0, 6.0}];
    k    = TMaterialize[TUOpAdd[a, b]];
    kid  = TTermVal @ THeapRead[TTermVal[k] + 1];
    info = TKernelInfo[kid];
    {info["n_inputs"], info["n_ops"],
     info["program"][[1, "opcode"]],
     info["program"][[2, "opcode"]],
     info["program"][[3, "opcode"]]},
    {2, 3, "LOAD", "LOAD", "ADD"},
    TestID -> "uop-load/linearizer-prepends-load-per-input"
]
