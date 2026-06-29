(* ::Package:: *)
(* Uop.wl - per-opcode metadata for UOPs.

   Lives in WolframInstitute`THVMLink`Private` (shared via BeginPackage["WolframInstitute`THVMLink`"]).
   Three things in one place so other files don't duplicate the
   per-opcode tables:
     uopArity[op]        compute-input arity (NUM args excluded)
     uopName[op]         opcode -> short label string
     uopShapeOf[base]    inferred output shape, mirrors
                         src/schedule/materialize.c

   Shape inference depends on Shape.wl's broadcastShape / dropAxes /
   tenShapeOf -- both files share WolframInstitute`THVMLink`Private` so the calls
   resolve directly. *)

BeginPackage["WolframInstitute`THVMLink`"];

Begin["`Private`"];

(* === arity ============================================================ *)

uopArity[$UopMaterialize] = 1
uopArity[$UopKernel] = 2 (* output_buf, NUM(kid) -- kid skipped *)
uopArity[$UopConst] = 0
uopArity[$UopAdd] = 2
uopArity[$UopMul] = 2
uopArity[$UopCmplt] = 2
uopArity[$UopCmpeq] = 2
uopArity[$UopNeg] = 1
uopArity[$UopRecip] = 1
uopArity[$UopExp2] = 1
uopArity[$UopLog2] = 1
uopArity[$UopSqrt] = 1
uopArity[$UopReshape] = 1
uopArity[$UopPermute] = 1
uopArity[$UopExpand] = 1
uopArity[$UopPad] = 1
uopArity[$UopShrink] = 1
uopArity[$UopFlip] = 1
uopArity[$UopReduce] = 1
uopArity[$UopGrad] = 1
uopArity[$UopFwd] = 1
uopArity[_] = 0

(* === names =========================================================== *)
(* (Same as $uopNames in THVMLink.wl, kept here as a function so
   downstream files needn't go through Lookup.) *)

uopName[$UopMaterialize] = "MATERIALIZE"
uopName[$UopKernel] = "KERNEL"
uopName[$UopConst] = "CONST"
uopName[$UopReshape] = "RESHAPE"
uopName[$UopPermute] = "PERMUTE"
uopName[$UopExpand] = "EXPAND"
uopName[$UopPad] = "PAD"
uopName[$UopShrink] = "SHRINK"
uopName[$UopFlip] = "FLIP"
uopName[$UopAdd] = "ADD"
uopName[$UopMul] = "MUL"
uopName[$UopNeg] = "NEG"
uopName[$UopRecip] = "RECIP"
uopName[$UopExp2] = "EXP2"
uopName[$UopLog2] = "LOG2"
uopName[$UopSqrt] = "SQRT"
uopName[$UopCmplt] = "CMPLT"
uopName[$UopReduce] = "REDUCE"
uopName[$UopGrad] = "GRAD"
uopName[$UopFwd] = "FWD"
uopName[$UopCmpeq] = "CMPEQ"
uopName[op_] := "UOP?" <> ToString[op]

(* === heap fallback ===================================================
   Find a cell holding TAG_UOP val=base; used to recover an opcode
   when the caller didn't pre-build an Association (e.g. CONSTs that
   the IC-string-diagram path renders per reference instead of as a
   shared agent). *)

opcodeFromHeap[base_Integer] := Block[{lo = THeapBase[], n = THeapPos[], cell, found = $UopConst},
    Do[
        cell = THeapRead[loc];
        If[ TTermTag[cell] === $TagUOP && TTermVal[cell] === base,
            found = TTermExt[cell]; Break[]],
        {loc, lo, n - 1}
    ];
    found
]

(* === inferred output shape ===========================================
   Mirror of op_output_shape + the EXPAND special case in
   src/schedule/materialize.c.  Pure heap walk; no side effects.

   Recursion uses opcodeFromHeap when an opcode isn't already in
   $uopOpcodeContext (a Block-scoped cache the renderer sets to
   the result of discoverUopOpcodesHere). *)

$uopOpcodeContext = <||>

uopShapeOf[base_Integer] := With[{op = Lookup[$uopOpcodeContext, base, opcodeFromHeap[base]]},
    uopShapeOfFor[op, base]
]

cellShape[cell_] := Switch[TTermTag[cell],
    $TagTEN, tenShapeOf[TTermVal[cell]],
    $TagUOP, uopShapeOf[TTermVal[cell]],
    _, {1}
]

uopSrcShape[base_Integer, off_Integer] := cellShape[THeapRead[base + off]]

uopShapeOfFor[$UopConst, _] := {1}

uopShapeOfFor[op_, base_] /; MemberQ[{$UopAdd, $UopMul, $UopCmplt, $UopCmpeq}, op] := broadcastShape[uopSrcShape[base, 0], uopSrcShape[base, 1]]

uopShapeOfFor[op_, base_] /; MemberQ[{$UopNeg, $UopRecip, $UopExp2, $UopLog2, $UopSqrt}, op] := uopSrcShape[base, 0]

(* REDUCE heap layout (src/uop/reduce.c):
     heap[base + 0]     = src
     heap[base + 1]     = NUM(kind)
     heap[base + 2]     = NUM(n_axes)
     heap[base + 3 + i] = NUM(axis_i)   for i in 0..n_axes-1
   A multi-axis REDUCE folds n_axes axes in one shot, so drop them all. *)
uopShapeOfFor[$UopReduce, base_] := With[{nAxes = TTermVal[THeapRead[base + 2]]},
    dropAxes[uopSrcShape[base, 0], Table[TTermVal[THeapRead[base + 3 + i]], {i, 0, nAxes - 1}]]
]

(* EXPAND heap layout (src/uop/expand.c):
     heap[base + 0]         = src
     heap[base + 1]         = NUM(ndim)
     heap[base + 2 + i]     = NUM(d_i)   for i in 0..ndim-1
   ndim is stored explicitly so EXPAND can change rank (e.g. lift a
   scalar to a non-singleton target shape during backprop). *)
uopShapeOfFor[$UopExpand, base_] := Block[{ndim = TTermVal[THeapRead[base + 1]]},
    Table[TTermVal[THeapRead[base + 2 + i]], {i, 0, ndim - 1}]
]

(* GRAD/FWD share cell [y]; both have y's shape (the bw is shape-
   lifted at the leaf via expand_to_target, then propagated up). *)
uopShapeOfFor[$UopGrad, base_] := uopSrcShape[base, 0]
uopShapeOfFor[$UopFwd, base_] := uopSrcShape[base, 0]

uopShapeOfFor[_, base_] := uopSrcShape[base, 0]

End[];

EndPackage[];
