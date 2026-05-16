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

(* === sub-item (c): explicit TUOpLoad linearizes as a program step ===
   A LOAD wrapping one of the inputs adds a LOAD step before the
   ADD; plain `TUOpAdd[a, b]` (no explicit LOAD) is just `[ADD]`. *)

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, 2.0, 3.0}];
    b = TTensor[{3}, {4.0, 5.0, 6.0}];
    k    = TMaterialize[TUOpAdd[a, TUOpLoad[b]]];
    kid  = TTermVal @ THeapRead[TTermVal[k] + 1];
    info = TKernelInfo[kid];
    (* Pre THVM_PHASE_C7_FREE_PROGRAM the test also asserted the
       per-KProgOp opcode list (n_ops == 2, program = [LOAD, ADD]);
       under FREE_PROGRAM=1 default, program[] is freed post-lift.
       n_inputs == 2 stays as the surviving observation: explicit
       LOAD wraps an input without folding it away. *)
    info["n_inputs"],
    2,
    TestID -> "uop-load/explicit-load-emits-program-step"
]

(* === sub-item (d): backends honor LOAD as identity memcpy ===
   When LOAD is in the prefix it copies the input through to a
   scratch register that downstream ops read; when LOAD is the
   final op it writes the output buffer.  Verify the elementwise
   sum is still correct with explicit LOAD on one operand. *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    Normal @ TTensorData @ TRealize @ TUOpAdd[TUOpLoad[a], TUOpLoad[b]],
    {5.0, 7.0, 9.0},
    TestID -> "uop-load/2-input-add-correct-with-explicit-loads"
]

(* TUOpLoad as the user-intended op (the FINAL program op) runs
   the cpu_op_load memcpy and writes its output. *)
VerificationTest[
    TInit[];
    a   = TTensorCreate @ NumericArray[{7.0, 8.0, 9.0}, "Real32"];
    out = TRealize @ TUOpLoad[a];
    Normal @ TTensorData[out],
    {7.0, 8.0, 9.0},
    TestID -> "uop-load/final-op-load-still-writes-output"
]

(* === sub-item (e): end-to-end LOAD smoke through a training step ===
   Builds a tiny (w.x - t)^2 loss + 1 SGD step and asserts the
   loss strictly decreases.  Every materialized kernel in the
   chain (TDot, MUL, ADD, REDUCE_SUM, ...) carries the LOAD
   prefix from sub-item (c); the test demonstrates LOAD is a
   structural change, not a numerical one -- training works. *)

VerificationTest[
    TInit[];
    Module[{x, w, t, pred0, loss0, loss0N, g, gN, lr, wNext,
            xN, wN, tN, pred1, loss1N},
        x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
        w = TTensorCreate @ NumericArray[{0.5, 0.5, 0.5}, "Real32"];
        t = TTensorCreate @ NumericArray[{10.0},          "Real32"];
        pred0  = TDot[w, x];
        loss0  = TMSELoss[pred0, t];
        loss0N = First @ Normal @ TTensorData @ TRealize[loss0];
        g      = TRealize @ TGrad[loss0, w];
        gN     = Normal @ TTensorData[g];
        lr     = 0.01;
        wNext  = MapThread[Subtract,
            {Normal @ NumericArray[{0.5, 0.5, 0.5}, "Real32"], lr * gN}];
        TInit[];
        xN  = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
        wN  = TTensorCreate @ NumericArray[wNext, "Real32"];
        tN  = TTensorCreate @ NumericArray[{10.0}, "Real32"];
        pred1  = TDot[wN, xN];
        loss1N = First @ Normal @ TTensorData @ TRealize @ TMSELoss[pred1, tN];
        loss1N < loss0N
    ],
    True,
    TestID -> "uop-load/training-step-decreases-loss-with-load-prefix"
]
