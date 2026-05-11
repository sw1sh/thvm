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

VerificationTest[
    (* TFree leaves the runtime uninitialized; the next heap op should
       silently re-init via ensureInit[]. *)
    (TFree[]; THeapPos[]),
    0,
    TestID -> "ensureInit auto-runs TInit on first heap op"
]

(* === TTerm atomic wrapper === *)

VerificationTest[
    (TReset[]; Module[{t = TEra[]},
        {Head[t], t["tagName"], t["sub"], IntegerQ[t["raw"]]}]),
    {TTerm, "ERA", 0, True},
    TestID -> "TEra returns a TTerm with ERA tag"
]

VerificationTest[
    (* Inspectors accept either the wrapper or the raw integer. *)
    (TReset[]; Module[{t = TEra[]},
        {TTermTag[t], TTermTag[t["raw"]]}]),
    {$TagERA, $TagERA},
    TestID -> "TTermTag accepts TTerm or raw Integer"
]

VerificationTest[
    (* TTerm[id] indexing covers all the per-field accessors. *)
    (TReset[]; Module[{t = TLam[var, var]},
        {t["tag"], t["tagName"], t["val"], t["sub"]}]),
    {$TagLAM, "LAM", 0, 0},
    TestID -> "TTerm[id][\"tag/tagName/val/sub\"] forwards to bridge"
]

(* === heap === *)

VerificationTest[
    (TReset[]; Module[{a = THeapAlloc[2], era = TEra[]},
        THeapSet[a, era];
        {a, THeapPos[], THeapRead[a] == era}
    ]),
    {0, 2, True},
    TestID -> "heap alloc + set + read"
]

(* === TLam / TVarFor === *)

VerificationTest[
    (TReset[]; Module[{id = TLam[var, var], loc, cell},
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
        f   = TLam[var, var];
        x   = TEra[];
        app = TApp[f, x];
        loc = TTermVal[app];
        {TTermTag[app], THeapRead[loc] == f, THeapRead[loc + 1] == x}
    ]),
    {$TagAPP, True, True},
    TestID -> "TApp lays f and x at consecutive heap cells"
]

(* === TSup / TDup explicit-label === *)

VerificationTest[
    (TReset[]; Module[{sup = TSup[7, TEra[], TEra[]]},
        {TTermTag[sup], TTermExt[sup]}
    ]),
    {$TagSUP, 7},
    TestID -> "TSup tag + explicit label preserved"
]

VerificationTest[
    (TReset[]; Module[{pair = TDup[5, TEra[], {a, b} |-> {a, b}]},
        {TTermTag[pair[[1]]], TTermTag[pair[[2]]],
         TTermExt[pair[[1]]],
         TTermVal[pair[[1]]] === TTermVal[pair[[2]]]}
    ]),
    {$TagDP0, $TagDP1, 5, True},
    TestID -> "TDup yields DP0/DP1 sharing explicit label and heap loc"
]

VerificationTest[
    (TReset[]; Module[{pair = TDup[5, TEra[]]},
        {Head[pair], Length[pair],
         TTermTag[pair[[1]]], TTermTag[pair[[2]]],
         TTermExt[pair[[1]]],
         TTermVal[pair[[1]]] === TTermVal[pair[[2]]]}
    ]),
    {List, 2, $TagDP0, $TagDP1, 5, True},
    TestID -> "TDup[label, body] returns the {dp0, dp1} pair directly"
]

VerificationTest[
    (TReset[];
     Block[{p1 = TDup[TEra[]], p2 = TDup[TEra[]]},
        {Head[p1], TTermExt[p1[[1]]], TTermExt[p2[[1]]],
         TTermExt[p1[[1]]] =!= TTermExt[p2[[1]]]}]),
    {List, 1, 2, True},
    TestID -> "TDup[body] auto-labels and returns a fresh {dp0, dp1} pair"
]

(* === TFreshLabel + auto-label overloads === *)

VerificationTest[
    (TReset[]; {TFreshLabel[], TFreshLabel[], TFreshLabel[]}),
    {1, 2, 3},
    TestID -> "TFreshLabel returns a monotonic counter starting at 1"
]

VerificationTest[
    (TReset[]; TFreshLabel[]; TFreshLabel[];
     TReset[]; TFreshLabel[]),
    1,
    TestID -> "TReset rewinds the fresh-label counter"
]

VerificationTest[
    (TReset[];
     Block[{s1 = TSup[TEra[], TEra[]], s2 = TSup[TEra[], TEra[]]},
        {TTermExt[s1], TTermExt[s2], TTermExt[s1] =!= TTermExt[s2]}]),
    {1, 2, True},
    TestID -> "TSup[a,b] auto-labels with fresh distinct integers"
]

VerificationTest[
    (TReset[];
     Block[{p1 = TDup[TEra[], {a, b} |-> {a, b}],
            p2 = TDup[TEra[], {a, b} |-> {a, b}]},
        {TTermExt[p1[[1]]], TTermExt[p2[[1]]],
         TTermExt[p1[[1]]] =!= TTermExt[p2[[1]]]}]),
    {1, 2, True},
    TestID -> "TDup[body,k] auto-labels with fresh distinct integers"
]

(* === THeap snapshot === *)

VerificationTest[
    (TReset[]; TLam[var, TApp[var, TEra[]]];
     Module[{snap = THeap[]},
        {KeyExistsQ[snap, "nextLoc"],
         KeyExistsQ[snap, "cells"],
         KeyExistsQ[snap, "Graph"],
         snap["nextLoc"] === THeapPos[]}
     ]),
    {True, True, True, True},
    TestID -> "THeap snapshot has nextLoc / cells / Graph"
]

VerificationTest[
    (TReset[]; TLam[var, var];
     GraphQ[THeap[]["Graph"]]),
    True,
    TestID -> "THeap[][\"Graph\"] is a Graph"
]

(* === THeapGraph examples === *)

VerificationTest[
    (* Identity lambda: 1 LAM agent (inferred from VAR back-ref),
       1 self-loop "body var" edge.  No seed needed since the
       VAR back-reference is enough. *)
    (TReset[]; TLam[var, var];
     Module[{g = THeapGraph[]},
        {VertexCount[g], EdgeCount[g]}]),
    {1, 1},
    TestID -> "THeapGraph on identity lambda has 1 agent + 1 self-loop"
]

VerificationTest[
    (* TApp[id, ERA]: heap holds VAR(0), LAM(0), ERA.  THeapGraph[]
       sees the LAM (via VAR back-ref) and the orphan ERA at cell 2,
       but not the heapless APP.  Pass the APP explicitly to
       THeapGraph[term] so the APP triangle joins the picture. *)
    (TReset[]; Block[{app = TApp[TLam[var, var], TEra[]]},
        Module[{g = THeapGraph[app]},
            {VertexCount[g], EdgeCount[g]}]]),
    {3, 3},
    TestID -> "THeapGraph on (id ERA) seeded with APP has 3 vertices + 3 edges"
]

VerificationTest[
    (* TDup[TSup[ERA, ERA], k]: heap holds ERA, ERA, SUP(val=0).
       Seed with one of DP0/DP1 so the DUP agent is discovered too. *)
    (TReset[]; TDup[TSup[TEra[], TEra[]],
        {dp0, dp1} |-> Module[{g = THeapGraph[dp0]},
            {VertexCount[g], EdgeCount[g]}]]),
    {4, 3},
    TestID -> "THeapGraph seeded with DP0 sees DUP + SUP + 2 ERAs"
]

VerificationTest[
    (* No seed: THeapGraph[] only sees the SUP and the two ERAs;
       the DUP is invisible because no heap cell references it. *)
    (TReset[]; TDup[TSup[TEra[], TEra[]], {a, b} |-> {a, b}];
     Module[{g = THeapGraph[]},
        {VertexCount[g], EdgeCount[g]}]),
    {3, 2},
    TestID -> "THeapGraph without seed misses the heapless DUP"
]

(* === TWnf === *)

VerificationTest[
    (TReset[]; Module[{e = TEra[]}, TWnf[e] == e]),
    True,
    TestID -> "TWnf on a WHNF (ERA) returns the same term"
]

VerificationTest[
    (* (lam x.x) ERA  ->  ERA, taking exactly one APP-LAM interaction. *)
    (TReset[]; Block[{id = TLam[var, var], era = TEra[], app, out, before},
        app    = TApp[id, era];
        before = TItrs[];
        out    = TWnf[app];
        {TTagName[TTermTag[out]], TItrs[] - before}
    ]),
    {"ERA", 1},
    TestID -> "TWnf on (id ERA) fires APP-LAM and returns ERA"
]

VerificationTest[
    (* APP-ERA: applying the eraser to anything yields the eraser. *)
    (TReset[]; Block[{era = TEra[], lam = TLam[var, var], app, out},
        app = TApp[era, lam];
        out = TWnf[app];
        TTagName[TTermTag[out]]
    ]),
    "ERA",
    TestID -> "TWnf on (ERA lam) fires APP-ERA"
]

VerificationTest[
    (* DUP-SUP same label: !&7{x0,x1} = &7{ERA,LAM}; dp0 -> ERA.
       Plain DPs are Levy-opaque under TWnf since the Phase 1+2
       readback split: TCnf is the user-facing path that fires
       the dup interaction. *)
    (TReset[]; TDup[7, TSup[7, TEra[], TLam[var, var]],
        {dp0, dp1} |-> TTagName[TTermTag[TCnf[dp0]]]
    ]),
    "ERA",
    TestID -> "TCnf on dp0 of same-label DUP-SUP picks the left branch"
]

VerificationTest[
    (* DUP-LAM: cloning the identity lambda then applying one copy to
       ERA should produce ERA.  TCnf drives the dup interaction; TWnf
       alone would leave the DP at the root unfired (Levy-opaque). *)
    (TReset[];
     TDup[TLam[var, var],
        {f0, f1} |-> TTagName[TTermTag[TCnf[TApp[f0, TEra[]]]]]]),
    "ERA",
    TestID -> "DUP-LAM clones a lambda end-to-end"
]

(* === TReduce + TTermExpr (snapshotting before / after) === *)

VerificationTest[
    (* TTermExpr returns a nested expression with tag-name string
       heads.  Walking the original APP term seed before vs after
       TWnf shows the substituted body cell. *)
    (TReset[]; Block[{app, before, after},
        app    = TApp[TLam[var, var], TEra[]];
        before = TTermExpr[app];
        TWnf[app];
        after  = TTermExpr[app];
        {before, after}]),
    {"APP"["LAM"["VAR"[0]], "ERA"], "APP"["LAM"["ERA"], "ERA"]},
    TestID -> "TTermExpr before vs after TWnf shows substituted body"
]

VerificationTest[
    (* TWnf returns the WHNF; TTermExpr of that is just "ERA". *)
    (TReset[]; TTermExpr[TWnf[TApp[TLam[var, var], TEra[]]]]),
    "ERA",
    TestID -> "TTermExpr of TWnf-result is the WHNF tree"
]

VerificationTest[
    (* Sugar: TTerm[id][arg] is shorthand for TApp[TTerm[id], arg]. *)
    (TReset[]; Module[{id = TLam[var, var], app},
        app = id[TEra[]];
        {Head[app], TTagName[TTermTag[app]]}]),
    {TTerm, "APP"},
    TestID -> "TTerm[id][arg] sugar -> TApp"
]

VerificationTest[
    (* Church 2 applied to identity and ERA: church2 f x = f (f x).
       Exercises APP-LAM + DUP-LAM together.  Use TCnf to drive the
       Levy-opaque DP through its readback. *)
    (TReset[]; Block[{
        church2 = TLam[s,
            TDup[s, {s0, s1} |->
                TLam[z, TApp[s0, TApp[s1, z]]]]],
        f       = TLam[var, var],
        x       = TEra[],
        out
     },
        out = TCnf[TApp[TApp[church2, f], x]];
        TTagName[TTermTag[out]]]),
    "ERA",
    TestID -> "Church 2 applied to id and ERA reduces to ERA"
]

(* TRealize on a TEN-tagged Term short-circuits the wnf+materialize
   loop entirely -- no kernel emitted, no wnf or materialize calls.
   Catches regressions where an idempotent realize would burn
   per-call kernel slots / hot-path cycles. *)
VerificationTest[
    TInit[];
    xT        = TTensorCreate @ N @ Range[12];
    sumResult = TRealize @ TUOpReduce[xT, 0, "SUM"];   (* now a TEN *)
    n0 = TKernelCount[];
    THotCountersReset[];
    Do[ TRealize @ sumResult, {10}];
    {TKernelCount[] - n0,
     THotCounters[]["WnfCalls"],
     THotCounters[]["MaterializeCalls"]},
    {0, 0, 0},
    TestID -> "TRealize on TEN short-circuits"
]
