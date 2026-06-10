(* ::Package:: *)
(* Switch.wl - numeric-switch (TAG_MAT) + binary-op (TAG_OP2)
   surface, plus a tiny `TNum` constructor for inline integer
   literals.

   These are the termination + counter primitives the recursive
   training loop needs on top of TDef / TRef - a `train_step`
   def can MAT on the iteration counter, decrement it via OP2
   SUB, and recurse via TRef. *)

BeginPackage["THVMLink`"];

GeneralUtilities`SetUsage[TNum, "TNum[i$] returns a TTerm wrapping a TAG_NUM atom holding the integer i$ as DT_I32.
TNum[i$, dtype$] picks the dtype (\"i32\" or \"f32\"); for f32 the value is bit-reinterpreted, so use TUOpConst for arithmetic floats."];
GeneralUtilities`SetUsage[TOp2, "TOp2[opcode$, x$, y$] returns a TAG_OP2 term computing opcode$(x$, y$) once both operands reduce to TAG_NUM.
Opcodes include \"+\", \"-\", \"*\", \"==\", \"<\"."];
GeneralUtilities`SetUsage[TMatNum, "TMatNum[matchVal$, handler$, fallback$] returns a TAG_MAT atom that dispatches by the tag of its applied argument: a TAG_NUM equal to matchVal$ runs handler$, a TAG_CTR with ext matchVal$ destructures (handler$ applied positionally to each child), and anything else applies fallback$.
TMatCtr is a sugar alias for the CTR-destructuring case."];

GeneralUtilities`SetUsage[TMatCtr, "TMatCtr[ctorName$, handler$, fallback$] is sugar for TMatNum[ctorName$, handler$, fallback$] used to destructure a CTR (anonymous CTR uses 0).
On a matching CTR it applies handler$ positionally to each child; used by TGradMany to bind multi-target gradient results into a body lambda."];
GeneralUtilities`SetUsage[TIfZero, "TIfZero[counter$, thenTerm$, elseTerm$] is sugar for a TMatNum on 0 applied to counter$, with elseTerm$ wrapped in a discarding lambda so it reads like a plain conditional."];

GeneralUtilities`SetUsage[TEql, "TEql[a$, b$] returns a TAG_EQL term that reduces to NUM(1) when a$ and b$ are structurally equal and NUM(0) otherwise.
Strict on both arguments; a SUP on either side commutes (clones the other side via DUP), and ERA/ANY short-circuit.
Prefer this over TOp2[\"==\", $$] for theorem-proving where the arguments may be SUPs or structures."];

GeneralUtilities`SetUsage[TCtr, "TCtr[label$, c$1, c$2, $$] constructs a TAG_CTR with the given integer label$ and child terms (arity capped at 16, matching HVM4's CTR limit).
An IC-level dup of the result fires DUP-CTR."];

GeneralUtilities`SetUsage[TBookCtr, "TBookCtr[label$, c$1, c$2, $$] constructs a TAG_CTR in BOOK_HEAP rather than the dynamic HEAP.
Use when building CTR inputs destined for the Metal AOT path, whose heap MTLBuffer is bound to BOOK_HEAP, so destructure derefs only resolve for values in that range."];

GeneralUtilities`SetUsage[TMatChain, "TMatChain[arms$, fallback$] takes an association of label to handler and builds nested TMatNum atoms so one matcher dispatches multiple constructor labels.
Each handler receives the destructured CTR fields positionally; fallback$ handles labels that do not match."];

Begin["`Private`"];

$op2Codes = <|
    "+"  -> 0,   "ADD" -> 0,
    "-"  -> 1,   "SUB" -> 1,
    "*"  -> 2,   "MUL" -> 2,
    "==" -> 3,   "EQ"  -> 3,
    "<"  -> 4,   "LT"  -> 4,
    "/"  -> 5,   "DIV" -> 5,
    "%"  -> 6,   "MOD" -> 6,
    "^"  -> 7,   "XOR" -> 7,
    "&"  -> 8,   "AND" -> 8,
    "|"  -> 9,   "OR"  -> 9,
    "<<" -> 10,  "SHL" -> 10,
    ">>" -> 11,  "SHR" -> 11,
    ">"  -> 12,  "GT"  -> 12,
    "<=" -> 13,  "LE"  -> 13,
    ">=" -> 14,  "GE"  -> 14,
    "!=" -> 15,  "NE"  -> 15
|>;

$termNewOp2Fn := $termNewOp2Fn = load["thvm_wl_term_new_op2",
    {Integer, Integer, Integer}, Integer]
$termNewMatFn := $termNewMatFn = load["thvm_wl_term_new_mat",
    {Integer, Integer, Integer}, Integer]
$termNewEqlFn := $termNewEqlFn = load["thvm_wl_term_new_eql",
    {Integer, Integer}, Integer]
$termNewCtrFn := $termNewCtrFn = load["thvm_wl_term_new_ctr",
    {Integer, {Integer, 1}}, Integer]
$termNewBookCtrFn := $termNewBookCtrFn = load["thvm_wl_term_new_book_ctr",
    {Integer, {Integer, 1}}, Integer]

(* TAG_NUM is just a packed term - no library call needed. *)
TNum[i_Integer] := TNum[i, "i32"]
TNum[i_Integer, dtype_String] := (
    ensureInit[];
    TTerm[$termNewFn[0, $TagNUM, dtypeCode[dtype], i]]
)

(* numCoerce - term-constructor sugar.  A bare Integer in a value
   position means "the i32 NUM with that value", so TSup[1, 2] is
   TSup[TNum[1], TNum[2]], TOp2["+", x, 3] is TOp2["+", x, TNum[3]],
   etc.  Without it, ttermRaw[1] === 1 gets spliced into a heap cell
   as a raw packed Term word - which the runtime decodes as
   APP(loc=1), a malformed term that crashes on reduction.  Applied
   at the heapWith funnel (THVMLink.wl) for every heapTerm-based
   constructor, and explicitly in TOp2 / TEql below (those bypass
   heapWith - they call the C term_new_op2 / term_new_eql directly).

   Integer only, on purpose: a bare Integer maps unambiguously to a
   DT_I32 NUM with that value (literally TNum[i]).  A bare Real would
   need to be bit-reinterpreted to f32 (TNum stores the 32-bit *bits*,
   not the value - see TNum::usage), and the surface convention for
   arithmetic float scalars is TUOpConst.  If you want a float NUM,
   write TNum[bits, "f32"] explicitly.  Everything that isn't a bare
   Integer (already a TTerm, a Real, a List, ...) passes through
   untouched. *)
numCoerce[i_Integer] := TNum[i]
numCoerce[x_] := x

TOp2[op_String, x_, y_] := (
    ensureInit[];
    TTerm[$termNewOp2Fn[
        Lookup[$op2Codes, op,
            (Message[TOp2::badop, op]; 0)],
        ttermRaw[numCoerce[x]],
        ttermRaw[numCoerce[y]]
    ]]
)
TOp2::badop = "Unknown OP2 opcode `1`; expected one of \"+\", \"-\", \"*\", \"==\", \"<\".";

(* TAG_EQL: structural equality, strict on both sides.  Reducer
   handles NUM-NUM, ERA/ANY shorts, and EQL-SUP commutes (both
   sides).  Prefer this over TOp2["==", ...] for theorem-proving
   contexts where the args may be SUPs, CTRs, LAMs (HVM4-style
   higher-order equality).  Future EQL-CTR / EQL-LAM rules let
   structural equality recurse natively. *)
TEql[a_, b_] := (
    ensureInit[];
    TTerm[$termNewEqlFn[ttermRaw[numCoerce[a]], ttermRaw[numCoerce[b]]]]
)

TMatNum[matchVal_Integer, handler_, fallback_] := (
    ensureInit[];
    TTerm[$termNewMatFn[matchVal, ttermRaw[handler], ttermRaw[fallback]]]
)
TMatCtr[ctorName_Integer, handler_, fallback_] :=
    TMatNum[ctorName, handler, fallback]

(* Sugar: TIfZero[counter, then, else] - elseTerm doesn't take the
   counter; we wrap it in a discarding lambda (the bound name is
   never referenced) so MAT-MIS lands correctly. *)
TIfZero[counter_, thenTerm_, elseTerm_] :=
    TApp[TMatNum[0, thenTerm, TLam[ignored, elseTerm]], counter]

TCtr[label_Integer, children___] := (
    ensureInit[];
    TTerm[$termNewCtrFn[label, ttermRaw /@ {children}]]
)

(* TBookCtr - like TCtr but allocates the cell sequence in BOOK_HEAP
   (rather than the dynamic HEAP).  Use when constructing CTR inputs
   destined for the Metal AOT path: the kernel's `device Term *heap`
   buffer is zero-copy bound to BOOK_HEAP, so destructure derefs only
   work for vals in book_heap range. *)
TBookCtr[label_Integer, children___] := (
    ensureInit[];
    TTerm[$termNewBookCtrFn[label, ttermRaw /@ {children}]]
)

(* TMatChain[<|0 -> h0, 1 -> h1, ...|>, fb] expands to
     TMatNum[0, h0, TMatNum[1, h1, ... TMatNum[k, hk, fb] ...]].
   The chain dispatches by matching the applied arg's CTR label
   against each TMatNum in turn; if none match, fallback gets the
   arg via APP-MAT-MIS.  Mirrors HVM4's `lam { #K1: h1; #K2: h2 }`. *)
TMatChain[arms_Association, fallback_] := Fold[
    {acc, arm} |-> TMatNum[arm[[1]], arm[[2]], acc],
    fallback,
    Reverse @ Normal @ arms
]

(* === numeric arithmetic UpValues ====================================
   Lift `k - 1`, `k + 1`, `2 * k` etc. against integer-context TTerms
   (TAG_NUM / TAG_VAR / TAG_OP2 / TAG_MAT) into TOp2 trees, mirroring
   the tensor-arith UpValues in Tensor.wl.  Lets recursive-loop
   construction read like normal WL arithmetic instead of a forest of
   manual TOp2 calls. *)

(* "Could reduce to a NUM in numeric context": NUM literals, bound
   vars, OP2 / MAT trees, plus SUP and DP0/DP1 projections (so
   `TSup[1,2] + 3` and `dp0 * 2` route through TOp2 - OP2-SUP
   commutes the op into each branch; the OP2 frame drives DP heads
   through cnf before folding).  REF / ALO are deliberately excluded
   - a named recursive def isn't necessarily numeric.

   DP0/DP1 cells with DupGradFlag set are tensor projections from
   TGradMany's chain-rule walk - NOT numeric scalars.  Without this
   guard, `grad * grad` (e.g. Adam's v-update) would match the
   numeric `Times[a ? numericTermQ, b ? numericTermQ]` UpValue below
   and emit a tensor-shaped TAG_OP2 the IC reducer has no
   `OP2(DP1_x, DP1_x)`-self rule for - the OP2 stays symbolic, no
   weight update fires.  Let those fall through to Tensor.wl's
   tensor-arithmetic Times UpValue, which routes through TUOpMul and
   produces a clean TAG_UOP.

   A TAG_VAR carrying a TLamShape annotation is the bound variable of
   a TENSOR lambda, also not a numeric scalar: the same fall-through
   lets `x + 1` / `2 x` over such a binder build a UOP graph (via
   Tensor.wl's tensorTermQ UpValues) that APP-LAM can JIT-materialize,
   instead of a numeric TAG_OP2 that strands at realize.  A plain
   binder (no annotation) stays numeric so ordinary IC lambdas reduce
   as before. *)
numericTermQ[t_TTerm] := With[{raw = ttermRaw[t], tag = $termTagFn[ttermRaw[t]]},
    tag === $TagNUM ||
    tag === $TagOP2 || tag === $TagMAT ||
    tag === $TagSUP ||
    And[ tag === $TagVAR, TTermShape[t] === {} ] ||
    And[ Or[tag === $TagDP0, tag === $TagDP1],
         BitAnd[$termExtFn[raw], $DupGradFlag] === 0 ]
]
numericTermQ[_] := False

(* Plus[k, n] / Plus[n, k]: build TOp2["+"] (or "-" if n<0 to keep
   NUMs unsigned).  Multi-arg Plus folds pairwise. *)
TTerm /: Plus[t_TTerm ? numericTermQ, n_Integer] :=
    If[ n < 0, TOp2["-", t, TNum[-n]], TOp2["+", t, TNum[n]]]
TTerm /: Plus[n_Integer, t_TTerm ? numericTermQ] :=
    If[ n < 0, TOp2["-", t, TNum[-n]], TOp2["+", t, TNum[n]]]
TTerm /: Plus[a_TTerm ? numericTermQ, b_TTerm ? numericTermQ] :=
    TOp2["+", a, b]

(* Subtract: WL flattens `a - b` to `Plus[a, Times[-1, b]]`, so
   matching just the binary form here covers the explicit case. *)
TTerm /: Subtract[a_TTerm ? numericTermQ, b_TTerm ? numericTermQ] :=
    TOp2["-", a, b]
TTerm /: Subtract[t_TTerm ? numericTermQ, n_Integer] :=
    TOp2["-", t, TNum[n]]

(* Times[k, n] / Times[n, k] / Times[k1, k2]. *)
TTerm /: Times[t_TTerm ? numericTermQ, n_Integer] := TOp2["*", t, TNum[n]]
TTerm /: Times[n_Integer, t_TTerm ? numericTermQ] := TOp2["*", TNum[n], t]
TTerm /: Times[a_TTerm ? numericTermQ, b_TTerm ? numericTermQ] :=
    TOp2["*", a, b]

(* Equal / Less for control-flow predicates. *)
TTerm /: Equal[t_TTerm ? numericTermQ, n_Integer] := TOp2["==", t, TNum[n]]
TTerm /: Equal[n_Integer, t_TTerm ? numericTermQ] := TOp2["==", TNum[n], t]
TTerm /: Less[t_TTerm ? numericTermQ, n_Integer] := TOp2["<", t, TNum[n]]
TTerm /: Less[n_Integer, t_TTerm ? numericTermQ] := TOp2["<", TNum[n], t]

End[];

EndPackage[];
