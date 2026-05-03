(* dyn_lab.wlt -- VerificationTest specs for TDsu / TDdu (HVM4
   dynamic-label SUP / DUP).

   Strict-on-label semantics: wnf reduces the label term first,
   then dispatches into DSU-{NUM, ERA, SUP} or DDU-{NUM, ERA, SUP}.
   See src/interact/dsu_*.c, src/interact/ddu_*.c for the rules. *)

(* === DSU-NUM: &(#n){a, b} -> SUP^n{a, b} =========================== *)

VerificationTest[
    TTermTag @ TWnf @ TDsu[TNum[7], TNum[11], TNum[22]],
    $TagSUP,
    TestID -> "DynLab/dsu-num/produces-SUP"
]

VerificationTest[
    TTermExt @ TWnf @ TDsu[TNum[7], TNum[11], TNum[22]],
    7,
    TestID -> "DynLab/dsu-num/label-becomes-ext"
]

VerificationTest[
    Block[{r = TWnf @ TDsu[TNum[5], TNum[100], TNum[200]]},
        {TTermVal @ THeapRead[TTermVal[r] + 0],
         TTermVal @ THeapRead[TTermVal[r] + 1]}],
    {100, 200},
    TestID -> "DynLab/dsu-num/branches-preserved"
]

(* === DSU-ERA: &(ERA){a, b} -> ERA ================================== *)

VerificationTest[
    TTermTag @ TWnf @ TDsu[TEra[], TNum[1], TNum[2]],
    $TagERA,
    TestID -> "DynLab/dsu-era/era-label-collapses"
]

(* === DSU-SUP: &(&L{x,y}){a,b} -> SUP^L{DSU(x,A0,B0), DSU(y,A1,B1)}
   The outer wnf produces SUP^L; inner DSUs hold NUM labels (the
   SUP's children) so a follow-up wnf resolves each into plain SUP. *)

VerificationTest[
    Block[{lab = TSup[5, TNum[8], TNum[9]],
           r},
        r = TWnf @ TDsu[lab, TNum[100], TNum[200]];
        {TTermTag[r], TTermExt[r]}],
    {$TagSUP, 5},
    TestID -> "DynLab/dsu-sup/outer-becomes-SUP-with-sup-label"
]

VerificationTest[
    Block[{lab = TSup[5, TNum[8], TNum[9]],
           r, left, right, lw, rw},
        r     = TWnf @ TDsu[lab, TNum[100], TNum[200]];
        (* THeapRead already returns TTerm; don't double-wrap. *)
        left  = THeapRead[TTermVal[r] + 0];
        right = THeapRead[TTermVal[r] + 1];
        lw    = TWnf[left];
        rw    = TWnf[right];
        {TTermTag[lw], TTermExt[lw], TTermTag[rw], TTermExt[rw]}],
    {$TagSUP, 8, $TagSUP, 9},
    TestID -> "DynLab/dsu-sup/inner-DSUs-resolve-to-children-labels"
]

(* === DDU-NUM: ! X &(#n) = v; (\x.\y. x op y) -> v op v ============= *)

VerificationTest[
    Block[{body = With[{x = Module[{xs}, xs], y = Module[{ys}, ys]},
                       TLam[x, TLam[y, TOp2["+", x, y]]]],
           r},
        r = TWnf @ TDdu[TNum[3], TNum[42], body];
        {TTermTag[r], TTermVal[r]}],
    {$TagNUM, 84},
    TestID -> "DynLab/ddu-num/dup-fires-then-body-runs"
]

VerificationTest[
    Block[{body = With[{x = Module[{xs}, xs], y = Module[{ys}, ys]},
                       TLam[x, TLam[y, TOp2["*", x, y]]]],
           r},
        r = TWnf @ TDdu[TNum[7], TNum[5], body];
        TTermVal[r]],
    25,
    TestID -> "DynLab/ddu-num/multiplication-25"
]

(* === DDU-ERA: ! X &(ERA) = v; b -> ERA ============================= *)

VerificationTest[
    TTermTag @ TWnf @ TDdu[TEra[], TNum[99],
        With[{x = Module[{xs}, xs], y = Module[{ys}, ys]},
            TLam[x, TLam[y, TEra[]]]]],
    $TagERA,
    TestID -> "DynLab/ddu-era/era-label-collapses"
]

(* === DSU label = OP2(2+3) -> NUM(5) -> SUP^5{a,b}
   Confirms strict-on-label drives full reduction of a compound
   label, not just one step. *)

VerificationTest[
    Block[{r = TWnf @ TDsu[TOp2["+", TNum[2], TNum[3]],
                            TNum[10], TNum[20]]},
        {TTermTag[r], TTermExt[r]}],
    {$TagSUP, 5},
    TestID -> "DynLab/dsu-num/label-reduces-from-op2"
]

(* === Visualization round-trip: TTermExpr decodes DSU/DDU shape === *)

VerificationTest[
    TTermExpr @ TDsu[TNum[7], TNum[1], TNum[2]],
    "DSU"["NUM"[7], "NUM"[1], "NUM"[2]],
    TestID -> "DynLab/dsu/TermExpr-shape"
]

VerificationTest[
    TTagName @ $TagDSU,
    "DSU",
    TestID -> "DynLab/tag-names/DSU"
]

VerificationTest[
    TTagName @ $TagDDU,
    "DDU",
    TestID -> "DynLab/tag-names/DDU"
]
