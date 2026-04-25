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
TMatNum::usage = "TMatNum[matchVal, handler, fallback] returns a TAG_MAT atom that, when applied to a NUM, reduces to `handler` on match or to APP[fallback, num] on miss.";
TIfZero::usage  = "TIfZero[counter, thenTerm, elseTerm] is sugar for APP[ TMatNum[0, thenTerm, lam _ . elseTerm], counter ].  The else branch ignores the bound argument so the user-side reads like a plain conditional.";

Begin["`Private`"];

$op2Codes = <|
    "+"  -> 0,  "ADD" -> 0,
    "-"  -> 1,  "SUB" -> 1,
    "*"  -> 2,  "MUL" -> 2,
    "==" -> 3,  "EQ"  -> 3,
    "<"  -> 4,  "LT"  -> 4
|>;

$termNewOp2Fn := $termNewOp2Fn = load["thvm_wl_term_new_op2",
    {Integer, Integer, Integer}, Integer]
$termNewMatFn := $termNewMatFn = load["thvm_wl_term_new_mat",
    {Integer, Integer, Integer}, Integer]

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

TMatNum[matchVal_Integer, handler_, fallback_] := (
    ensureInit[];
    TTerm[$termNewMatFn[matchVal, ttermRaw[handler], ttermRaw[fallback]]]
)

(* Sugar: TIfZero[counter, then, else] -- elseTerm doesn't take the
   counter; we wrap it in a discarding lambda (the bound name is
   never referenced) so MAT-MIS lands correctly. *)
TIfZero[counter_, thenTerm_, elseTerm_] :=
    TApp[TMatNum[0, thenTerm, TLam[ignored, elseTerm]], counter]

End[];

EndPackage[];
