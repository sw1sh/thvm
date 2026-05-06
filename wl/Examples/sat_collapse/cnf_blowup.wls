#!/usr/bin/env wolframscript
(* Repro for the nested-different-label SUP cnf blowup.

   `cnf` succeeds in lifting one SUP through OP2:
     OP2[*, SUP^1{0,1}, NUM(5)]  ->  SUP^1{0, 5}

   But fails when both args are SUPs with DIFFERENT labels:
     OP2[*, SUP^1{0,1}, SUP^2{0,1}]
   should collapse to {0, 0, 0, 1} (cross product).  Instead the
   second cnf step (on the cnf'd child) exhausts the entire 16M-cell
   GC space.

   Run this with `wolframscript -f cnf_blowup.wls`. *)

PacletDirectoryAdd[FileNameJoin[{ParentDirectory[ParentDirectory[ParentDirectory[$InputFileName]]], "wl"}]];
Needs["THVMLink`"];
TInitContext[];
print[a___] := WriteString["stdout", StringRiffle[ToString /@ {a}, " "], "\n"];

(* ---- Single SUP arg -- works. ---- *)
TReset[];
x = TSup[1, TNum[0], TNum[1]];
form = TOp2["*", x, TNum[5]];
print["OP2[*, SUP^1{0,1}, NUM(5)] -> ", FromTTerm /@ TCollapse[form]];
(* expected: {0, 5} *)

(* ---- Same-label SUPs -- pairwise annihilate. ---- *)
TReset[];
x = TSup[1, TNum[0], TNum[1]];
y = TSup[1, TNum[0], TNum[1]];
form = TOp2["*", x, y];
print["OP2[*, SUP^1, SUP^1]    -> ", FromTTerm /@ TCollapse[form]];
(* expected (annihilate): {0, 1} *)

(* ---- DIFFERENT-label SUPs -- BLOWS UP. ---- *)
TReset[];
x = TSup[1, TNum[0], TNum[1]];
y = TSup[2, TNum[0], TNum[1]];
form = TOp2["*", x, y];
print["OP2[*, SUP^1, SUP^2]    -> ", FromTTerm /@ TCollapse[form, 4]];
(* expected (cross product): {0, 0, 0, 1} *)
(* actual: heap_alloc: from-space exhausted (need 2 cells, HEAP_NEXT=134217728, ...) *)
