(* ::Package:: *)

(* Rewrite.wl - WL spec layer for KOpt rewriting over TTermExpr.
 *
 * The 11 KOpts the C-side autotuner (apply_opt_dag.c) emits become 11
 * one-line WL rules over the TTermExpr `"UOP"[name, ...]` form produced
 * by the existing heap walker (THVMLink.wl:710).  Each rule is
 * declared in operator form via ReplaceAll[rules] / ReplaceRepeated[rules],
 * making it a callable function that takes a TTermExpr snapshot and
 * returns the rewritten snapshot.
 *
 * Cross-validation: applying a KOpt via the WL rule must produce the
 * same TTermExpr as applying it via the C-side uop_dag_apply_kopt then
 * snapshotting.  See wl/THVMLink/Tests/rewrite.wlt for the
 * VerificationTest corpus driving this comparison.
 *
 * Constants ( $Kax*, $Opt*, $Kop* ) mirror the matching #defines in
 * src/thvm.h.  Any drift between this file and thvm.h is a real bug;
 * the rewrite.wlt suite catches drift via xvalid against the C side.
 *)

BeginPackage["THVMLink`"];

(* === KAX_* axis types =========================================== *)
$KaxLoop::usage = $KaxReduce::usage = $KaxUpcast::usage =
    $KaxUnroll::usage = $KaxLocal::usage = $KaxGlobal::usage =
    $KaxGroupReduce::usage =
        "axis_type id; mirrors KAX_* in src/thvm.h.";

(* === UOP_OPT_* annotation kinds ================================== *)
$OptUnroll::usage = $OptUpcast::usage = $OptTC::usage =
    $OptLocal::usage = $OptGroupReduce::usage = $OptConv::usage =
    $OptFastMath::usage = $OptSimdReduce::usage = $OptVecLoad::usage =
        "UOp_OPT.kind value; mirrors UOP_OPT_* in src/thvm.h.";

(* === KOP_* opcodes ============================================== *)
$KopNone::usage = $KopUpcast::usage = $KopUnroll::usage =
    $KopLocal::usage = $KopGroup::usage = $KopGroupTop::usage =
    $KopSwap::usage = $KopPadTo::usage = $KopNoLocals::usage =
    $KopTC::usage = $KopGlobal::usage = $KopFastMath::usage =
    $KopSimdReduce::usage = $KopVecLoad::usage =
        "KOpt.op value; mirrors KOP_* in src/thvm.h.";

(* === C-side bridge ============================================== *)
TUOpDagApplyKOpt::usage =
    "TUOpDagApplyKOpt[t_TTerm, op_Integer, axis_Integer, arg_Integer] " <>
    "applies a KOpt to the kernel's UOp DAG via the C-side " <>
    "uop_dag_apply_kopt and returns a new TTerm wrapping the rewritten " <>
    "root.  Returns TTerm[0] on bail / unsupported KOpt.  Mirror of " <>
    "py_uop_dag_apply_kopt in py/csource/thvm_py.c.";

(* === KOpt rule operator-forms ===================================
 * Each KOptXyz[args...] returns an operator (ReplaceAll[rules] or
 * ReplaceRepeated[rules]) that takes a TTermExpr snapshot and
 * returns the rewritten snapshot.  Composition via RightComposition.
 *
 * These rewrite the snapshot SHAPE; they do not touch C-side terms
 * directly.  Pair with TUOpDagApplyKOpt + TTermExpr for
 * cross-validation.
 *)
KOptTC::usage =
    "KOptTC[factor_Integer] returns an operator that wraps the inner " <>
    "REDUCE inside STORE.value with OPT(_, TC, factor), or replaces an " <>
    "existing TC factor.  Idempotent at the same factor.";

KOptGlobal::usage =
    "KOptGlobal[axis_Integer] returns an operator that swaps every " <>
    "RANGE leaf at the given axis_id from KAX_LOOP to KAX_GLOBAL.";

KOptUpcast::usage = KOptUnroll::usage = KOptLocal::usage =
    KOptGroup::usage = KOptGroupTop::usage =
        "KOpt<X>[axis_Integer, k_Integer] returns an operator that splits " <>
        "RANGE(axis, LOOP, N) into outer RANGE(axis, LOOP, N/k) and inner " <>
        "RANGE(axis+1, KAX_<X>, k), substituting refs with " <>
        "IADD(IMUL(outer, k), inner).";

KOptSwap::usage =
    "KOptSwap[a_Integer, b_Integer] returns an operator that swaps the " <>
    "axis_ids of the two RANGE leaves.  Bidirectional; staged through " <>
    "a fresh placeholder symbol to avoid the rewrite loop.";

KOptFastMath::usage =
    "KOptFastMath is an operator that wraps every unary FP op " <>
    "(EXP2/LOG2/SQRT/NEG/RECIP) with OPT(_, FastMath, 0).  Idempotent.";

KOptSimdReduce::usage =
    "KOptSimdReduce is an operator that wraps every REDUCE with " <>
    "OPT(_, SimdReduce, 0).  Idempotent.";

KOptVecLoad::usage =
    "KOptVecLoad[width_Integer] returns an operator that wraps every " <>
    "INDEX_E whose addr is contiguous-shaped " <>
    "(IADD(IMUL(_, _), RANGE)) with OPT(_, VecLoad, width).";

Begin["`Private`"];

(* === KAX_* axis types ============================================ *)
$KaxLoop = 0
$KaxReduce = 1
$KaxUpcast = 2
$KaxUnroll = 3
$KaxLocal = 4
$KaxGlobal = 5
$KaxGroupReduce = 6

(* === UOP_OPT_* annotation kinds ================================== *)
$OptUnroll = 0
$OptUpcast = 1
$OptTC = 2
$OptLocal = 3
$OptGroupReduce = 4
$OptConv = 5
$OptFastMath = 6
$OptSimdReduce = 7
$OptVecLoad = 8

(* === KOP_* opcodes =============================================== *)
$KopNone = 0
$KopUpcast = 1
$KopUnroll = 2
$KopLocal = 3
$KopGroup = 4
$KopGroupTop = 5
$KopSwap = 6
$KopPadTo = 7
$KopNoLocals = 8
$KopTC = 9
$KopGlobal = 10
$KopFastMath = 11
$KopSimdReduce = 12
$KopVecLoad = 13

(* === C-side bridge =============================================== *)
$applyKOptFn := $applyKOptFn = LibraryFunctionLoad[
    $lib, "thvm_wl_uop_dag_apply_kopt",
    {Integer, Integer, Integer, Integer}, Integer
]

TUOpDagApplyKOpt[t_TTerm, op_Integer, axis_Integer, arg_Integer] := (
    ensureInit[];
    TTerm[$applyKOptFn[ttermRaw[t], op, axis, arg]]
)

(* === KOpt rule operators =========================================
 * All rules pattern-match against TTermExpr's `"UOP"[name, child, ...]`
 * form with `"NUM"[value]` atoms for opcode parameters.  See
 * docs/plans/wl_kopt_rewriting.md Part 3 for the design.
 *)

(* ---- TC: wrap inner REDUCE with OPT(_, TC, factor) ---------------- *)
KOptTC[factor_Integer] := ReplaceAll[{
    (* bare REDUCE inside STORE.value: wrap *)
    "UOP"["STORE", b_, a_, r:"UOP"["REDUCE", __]] :>
        "UOP"["STORE", b, a,
            "UOP"["OPT", r, "NUM"[$OptTC], "NUM"[factor]]],
    (* already TC-wrapped: replace factor *)
    "UOP"["STORE", b_, a_,
        "UOP"["OPT", r:"UOP"["REDUCE", __], "NUM"[$OptTC], _]] :>
        "UOP"["STORE", b, a,
            "UOP"["OPT", r, "NUM"[$OptTC], "NUM"[factor]]]
}]

(* ---- GLOBAL: axis_type swap LOOP -> GLOBAL on the named axis ----- *)
KOptGlobal[axis_Integer] := ReplaceAll[
    "UOP"["RANGE", "NUM"[axis], "NUM"[$KaxLoop], extN_] :>
    "UOP"["RANGE", "NUM"[axis], "NUM"[$KaxGlobal], extN]
]

(* ---- Splits: UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP =================
 * Each splits RANGE(axis, type, N) into IADD(IMUL(outer, k), inner)
 * where:
 *   - outer = RANGE(axis, <original type>, N/k)
 *   - inner = OPT(RANGE(axis+1, innerKax, k), optKind, k)
 *   - LOCAL is special: optKind = None so inner is the bare RANGE.
 * Other RANGE leaves with axis_id > target are renumbered +1 (the
 * new inner steals id `axis+1`).  Mirrors uop_dag_apply_split in
 * src/uop/apply_opt_dag.c. *)
splitRule[innerKax_, optKind_, axis_, k_] := With[{
    innerExpr = If[ optKind === None,
        "UOP"["RANGE", "NUM"[axis + 1], "NUM"[innerKax], "NUM"[k]],
        "UOP"["OPT",
            "UOP"["RANGE", "NUM"[axis + 1], "NUM"[innerKax], "NUM"[k]],
            "NUM"[optKind], "NUM"[k]]]
},
    {
        "UOP"["RANGE", "NUM"[axis], "NUM"[outerType_], "NUM"[n_]] :>
            "UOP"["IADD",
                "UOP"["IMUL",
                    "UOP"["RANGE", "NUM"[axis], "NUM"[outerType],
                        "NUM"[Quotient[n, k]]],
                    "UOP"["CONST", "NUM"[k]]],
                innerExpr],
        "UOP"["RANGE", "NUM"[other_Integer /; other > axis], t_, e_] :>
            "UOP"["RANGE", "NUM"[other + 1], t, e]
    }
]

(* REDUCE's own axis arg is a bare NUM (not a RANGE leaf), so the RANGE
   rules in splitRule[] don't reach it.  Mirror C-side apply_opt_dag.c
   reduce_shift_above: any red_axis > the target axis shifts by +1.
   Applied as a SEPARATE second pass because if it lived inside
   splitRule[]'s rule list, ReplaceAll would match the REDUCE
   first and stop descending into the body, leaving the body's RANGE
   leaves un-rewritten. *)
reduceAxisShift[axis_] := {
    "UOP"["REDUCE", body_, kindN_, "NUM"[redAxis_Integer /; redAxis > axis]] :>
        "UOP"["REDUCE", body, kindN, "NUM"[redAxis + 1]]
}

doSplit[innerKax_, optKind_, axis_, k_][expr_] :=
    (expr /. splitRule[innerKax, optKind, axis, k]) /. reduceAxisShift[axis]

KOptUpcast[axis_Integer, k_Integer] := doSplit[$KaxUpcast, $OptUpcast, axis, k]
KOptUnroll[axis_Integer, k_Integer] := doSplit[$KaxUnroll, $OptUnroll, axis, k]
KOptLocal[axis_Integer, k_Integer] := doSplit[$KaxLocal, None, axis, k]
(* KOP_GROUP / KOP_GROUPTOP are retired opcodes but their KOpt entry
   points stay for back-compat.  C-side apply_opt_dag falls through to
   KAX_LOOP + no-OPT default for these; WL mirrors that. *)
KOptGroup[axis_Integer, k_Integer] := doSplit[$KaxLoop, None, axis, k]
KOptGroupTop[axis_Integer, k_Integer] := doSplit[$KaxLoop, None, axis, k]

(* ---- SWAP: bidirectional axis swap.  ReplaceAll's single-pass
   semantics give us idempotent swap for free: rule 1 fires on each
   axis_a leaf (replacing with axis_b) and rule 2 on each axis_b leaf,
   without re-visiting outputs. *)
KOptSwap[a_Integer, b_Integer] := ReplaceAll[{
    "UOP"["RANGE", "NUM"[a], t_, e_] :> "UOP"["RANGE", "NUM"[b], t, e],
    "UOP"["RANGE", "NUM"[b], t_, e_] :> "UOP"["RANGE", "NUM"[a], t, e]
}]

(* ---- FastMath / SimdReduce: bottom-up walkers ===================
 * ReplaceAll is outside-in: a rule matching the outer node skips the
 * subtree, so a nested REDUCE inside a REDUCE body (softmax: the row
 * max-reduce lives inside the sum-of-exp body) would never get
 * wrapped.  Both C walkers (apply_opt_dag_{fm,sr}_walk) recurse into
 * children first, then wrap on the way back up.  Mirror that with an
 * explicit post-order traversal of the "UOP"[name, args...] tree. *)

fastMathUnaryQ = MemberQ[{"EXP2", "LOG2", "SQRT"}, #] &;

(* already FastMath-wrapped unary: descend into the unary's body,
   re-wrap in the same shape (idempotent); mirrors the OPT/FAST_MATH
   idempotency arm in apply_opt_dag_fm_walk_uncached. *)
fmWalk["UOP"["OPT", "UOP"[in_String /; fastMathUnaryQ[in], inArgs___],
             "NUM"[$OptFastMath], _]] :=
    "UOP"["OPT", "UOP"[in, Sequence @@ (fmWalk /@ {inArgs})],
          "NUM"[$OptFastMath], "NUM"[0]]
fmWalk[u : "UOP"[name_String /; fastMathUnaryQ[name], args___]] :=
    "UOP"["OPT", "UOP"[name, Sequence @@ (fmWalk /@ {args})],
          "NUM"[$OptFastMath], "NUM"[0]]
fmWalk["UOP"[name_, args___]] := "UOP"[name, Sequence @@ (fmWalk /@ {args})]
fmWalk[x_] := x

KOptFastMath[expr_] := fmWalk[expr]

srWalk["UOP"["OPT", "UOP"["REDUCE", inArgs___], "NUM"[$OptSimdReduce], _]] :=
    "UOP"["OPT", "UOP"["REDUCE", Sequence @@ (srWalk /@ {inArgs})],
          "NUM"[$OptSimdReduce], "NUM"[0]]
srWalk[r : "UOP"["REDUCE", args___]] :=
    "UOP"["OPT", "UOP"["REDUCE", Sequence @@ (srWalk /@ {args})],
          "NUM"[$OptSimdReduce], "NUM"[0]]
srWalk["UOP"[name_, args___]] := "UOP"[name, Sequence @@ (srWalk /@ {args})]
srWalk[x_] := x

KOptSimdReduce[expr_] := srWalk[expr]

(* ---- VecLoad: wrap contiguous INDEX_E with OPT(_, VecLoad, w) --- *)
KOptVecLoad[width_Integer] := ReplaceRepeated[{
    "UOP"["OPT", e:"UOP"["INDEX_E", __], "NUM"[$OptVecLoad], _] :>
        "UOP"["OPT", e, "NUM"[$OptVecLoad], "NUM"[width]],
    e:"UOP"["INDEX_E", _,
        "UOP"["IADD", "UOP"["IMUL", _, _], "UOP"["RANGE", __]]] :>
        "UOP"["OPT", e, "NUM"[$OptVecLoad], "NUM"[width]]
}]

(* Composition: callers use `RightComposition[k1, k2, ...][expr]`
   directly; no helper needed. *)

End[];   (* `Private` *)

EndPackage[];
