(* ::Package:: *)
(* Switch.wl - numeric-switch (TAG_MAT) + binary-op (TAG_OP2)
   surface, plus a tiny `TNum` constructor for inline integer
   literals.

   These are the termination + counter primitives the recursive
   training loop needs on top of TDef / TRef -- a `train_step`
   def can MAT on the iteration counter, decrement it via OP2
   SUB, and recurse via TRef. *)

BeginPackage["THVMLink`"];

TNum::usage = "TNum[i] returns a TTerm wrapping a TAG_NUM atom holding the integer `i` as DT_I32.  TNum[i, dtype] picks the dtype (\"i32\" or \"f32\"); for f32 the value is bit-reinterpreted (use TUOpConst for arithmetic floats).";
TOp2::usage = "TOp2[opcode, x, y] returns a TAG_OP2 term computing `opcode(x, y)` once both operands reduce to TAG_NUM.  Opcodes: \"+\", \"-\", \"*\", \"==\", \"<\".";
TMatNum::usage = "TMatNum[matchVal, handler, fallback] returns a TAG_MAT atom that dispatches by tag of its applied arg: TAG_NUM with value matchVal -> `handler`; TAG_CTR with ext matchVal -> destructure (handler applied to each CTR child via APP-chain, mirroring HVM4's APP-MAT-CTR-MAT); anything else -> APP[fallback, arg].  TMatCtr is a sugar alias for the CTR-destructuring use case (same primitive).";

TMatCtr::usage = "TMatCtr[ctorName, handler, fallback] -- sugar for TMatNum[ctorName, handler, fallback] when the intended use is destructuring a CTR (constructor name `ctorName`, anonymous CTR uses 0).  When applied to a matching CTR, applies `handler` positionally to each CTR child.  Used by TGradMany to bind multi-target gradient results into a body lambda without an indexed projection primitive.";
TIfZero::usage  = "TIfZero[counter, thenTerm, elseTerm] is sugar for APP[ TMatNum[0, thenTerm, lam _ . elseTerm], counter ].  The else branch ignores the bound argument so the user-side reads like a plain conditional.";

TEql::usage = "TEql[a, b] returns a TAG_EQL term that reduces to NUM(1) iff `a` and `b` are structurally equal, NUM(0) otherwise.  Strict on both args; SUP-on-either-side commutes (clones the other side via DUP, distributes); ERA/ANY short-circuit.  CTR-CTR and LAM-LAM rules land with the upcoming HVM4 port (currently fall through to stuck-rebuild).  Use this instead of TOp2[\"==\", ...] for theorem-proving where the args may be SUPs / structures.";

TCtr::usage = "TCtr[label, c1, c2, ...] constructs a TAG_CTR with the given integer label and child terms.  Mirrors HVM4's `#K{a, b, ...}`.  Arity capped at 16 (matches HVM4's CTR limit).  An IC-level dup of the result fires DUP-CTR via interact_dup_ctr.";

TBookCtr::usage = "TBookCtr[label, c1, c2, ...] constructs a TAG_CTR in BOOK_HEAP rather than the dynamic HEAP.  Use when constructing CTR inputs destined for the Metal AOT path -- the kernel's heap MTLBuffer is bound to BOOK_HEAP, so destructure derefs only resolve for vals in that range.  Phase 7 iter T.";

TMatChain::usage = "TMatChain[<|label1 -> handler1, label2 -> handler2, ...|>, fallback] builds nested TMatNum atoms so a single matcher dispatches multiple constructor labels, mirroring HVM4's `lambda { #L1: h1; #L2: h2; ... }` syntax.  Each handler receives the destructured CTR fields positionally via APP-MAT-CTR-MAT.";

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

(* TAG_NUM is just a packed term -- no library call needed. *)
TNum[i_Integer]                      := TNum[i, "i32"]
TNum[i_Integer, dtype_String]        := (
    ensureInit[];
    TTerm[$termNewFn[0, $TagNUM, dtypeCode[dtype], i]]
)

TOp2[op_String, x_, y_] := (
    ensureInit[];
    TTerm[$termNewOp2Fn[
        Lookup[$op2Codes, op,
            (Message[TOp2::badop, op]; 0)],
        ttermRaw[x],
        ttermRaw[y]
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
    TTerm[$termNewEqlFn[ttermRaw[a], ttermRaw[b]]]
)

TMatNum[matchVal_Integer, handler_, fallback_] := (
    ensureInit[];
    TTerm[$termNewMatFn[matchVal, ttermRaw[handler], ttermRaw[fallback]]]
)
TMatCtr[ctorName_Integer, handler_, fallback_] :=
    TMatNum[ctorName, handler, fallback]

(* Sugar: TIfZero[counter, then, else] -- elseTerm doesn't take the
   counter; we wrap it in a discarding lambda (the bound name is
   never referenced) so MAT-MIS lands correctly. *)
TIfZero[counter_, thenTerm_, elseTerm_] :=
    TApp[TMatNum[0, thenTerm, TLam[ignored, elseTerm]], counter]

TCtr[label_Integer, children___] := (
    ensureInit[];
    TTerm[$termNewCtrFn[label, ttermRaw /@ {children}]]
)

(* TBookCtr -- like TCtr but allocates the cell sequence in BOOK_HEAP
   (rather than the dynamic HEAP).  Use when constructing CTR inputs
   destined for the Metal AOT path: the kernel's `device Term *heap`
   buffer is zero-copy bound to BOOK_HEAP, so destructure derefs only
   work for vals in book_heap range.  Phase 7 iter T. *)
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
    TMatNum[#2[[1]], #2[[2]], #1] &,
    fallback,
    Reverse @ Normal @ arms
]

(* === numeric arithmetic UpValues ====================================
   Lift `k - 1`, `k + 1`, `2 * k` etc. against integer-context TTerms
   (TAG_NUM / TAG_VAR / TAG_OP2 / TAG_MAT) into TOp2 trees, mirroring
   the tensor-arith UpValues in Tensor.wl.  Lets recursive-loop
   construction read like normal WL arithmetic instead of a forest of
   manual TOp2 calls. *)

numericTermQ[t_TTerm] := With[{tag = $termTagFn[ttermRaw[t]]},
    tag === $TagNUM || tag === $TagVAR ||
    tag === $TagOP2 || tag === $TagMAT
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
TTerm /: Less[t_TTerm ? numericTermQ, n_Integer]  := TOp2["<",  t, TNum[n]]
TTerm /: Less[n_Integer, t_TTerm ? numericTermQ]  := TOp2["<",  TNum[n], t]

End[];

EndPackage[];
