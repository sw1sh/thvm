(* core.wlt -- VerificationTest specs for the THVMLink paclet.

   Run via:  bash wl/THVMLink/Tests/run.sh

   The runner loads the paclet (PacletDirectoryLoad + Needs) before
   invoking TestReport on this file, so this file is purely test data.
*)

(* === lifecycle === *)

VerificationTest[
    TInit[],
    True,
    TestID -> "TInit"
]

VerificationTest[
    TItrs[],
    0,
    TestID -> "TItrs starts at 0"
]

VerificationTest[
    THeapPos[],
    0,
    TestID -> "THeapPos starts at 0"
]

(* === term packing === *)

VerificationTest[
    Module[{t = TTermNew[1, $TagDUP, 12345, 999]},
        {TTermSub[t], TTermTag[t], TTermExt[t], TTermVal[t], TTagName[TTermTag[t]]}
    ],
    {1, $TagDUP, 12345, 999, "DUP"},
    TestID -> "term packing roundtrip"
]

(* === heap === *)

VerificationTest[
    (TReset[]; Module[{a = THeapAlloc[2], era = TEra[]},
        THeapSet[a, era];
        {a, THeapPos[], THeapRead[a] === era}
    ]),
    {0, 2, True},
    TestID -> "heap alloc + set + read"
]

(* === TLam / TVarFor === *)

VerificationTest[
    (TReset[]; Module[{id = TLam[var |-> var], loc, cell},
        loc  = TTermVal[id];
        cell = THeapRead[loc];
        {TTermTag[id], TTermTag[cell], TTermVal[cell] === loc}
    ]),
    {$TagLAM, $TagVAR, True},
    TestID -> "TLam identity body cell points back at lam loc"
]

(* === TApp === *)

VerificationTest[
    (TReset[]; Block[{f, x, app, loc},
        f   = TLam[var |-> var];
        x   = TEra[];
        app = TApp[f, x];
        loc = TTermVal[app];
        {TTermTag[app], THeapRead[loc] === f, THeapRead[loc + 1] === x}
    ]),
    {$TagAPP, True, True},
    TestID -> "TApp lays f and x at consecutive heap cells"
]

(* === TSup / TDup === *)

VerificationTest[
    (TReset[]; Module[{sup = TSup[7, TEra[], TEra[]]},
        {TTermTag[sup], TTermExt[sup]}
    ]),
    {$TagSUP, 7},
    TestID -> "TSup tag + label preserved"
]

VerificationTest[
    (TReset[]; Module[{pair = TDup[5, TEra[], Function[{a, b}, {a, b}]]},
        {TTermTag[pair[[1]]], TTermTag[pair[[2]]],
         TTermExt[pair[[1]]],
         TTermVal[pair[[1]]] === TTermVal[pair[[2]]]}
    ]),
    {$TagDP0, $TagDP1, 5, True},
    TestID -> "TDup yields DP0/DP1 sharing label and heap loc"
]

(* === THeap snapshot === *)

VerificationTest[
    (TReset[]; TLam[var |-> TApp[var, TEra[]]];
     Module[{snap = THeap[]},
        {KeyExistsQ[snap, "nextLoc"],
         KeyExistsQ[snap, "cells"],
         snap["nextLoc"] === THeapPos[]}
     ]),
    {True, True, True},
    TestID -> "THeap snapshot has expected keys"
]

(* === TWnf stub passthrough (becomes a real test in step 6) === *)

VerificationTest[
    (TReset[]; Module[{e = TEra[]}, TWnf[e] === e]),
    True,
    TestID -> "TWnf stub returns its input unchanged"
]
