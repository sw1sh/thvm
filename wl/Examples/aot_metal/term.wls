#!/usr/bin/env wolframscript
(* term.wls -- AOT-on-Metal walk-through.

   Run via:  wolframscript -f wl/Examples/aot_metal/term.wls

   Demonstrates the three Method backends end-to-end: define a few
   functions, run each via Metal, single-thread CPU, and worker-pool
   CPU.  Verify they all produce the same answer.  Time the hot
   path so users can see where Metal wins (or loses).

   This file is illustrative -- the wlt regression suite at
   wl/THVMLink/Tests/aot_metal.wlt is the source of truth for
   correctness.  See docs/aot_metal.md for the manual.
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

println[]      := WriteString["stdout", "\n"];
formatUs[t_]   := ToString[N[t*1000000, 4]] <> "us";

TInit[];

(* === Define some functions === *)

Print["Defining add2, mul_then_add, classify..."];
TDef["add2",         TLam[a, TLam[b, TOp2["+", a, b]]]];
TDef["mul_then_add", TLam[a, TLam[b, TLam[c,
  TOp2["+", TOp2["*", a, b], c]]]]];
TDef["classify",     TLam[n, TMatChain[
  <|0 -> TNum[42], 1 -> TNum[99], 2 -> TNum[7]|>,
  TNum[0]][n]]];
println[];

(* === Cross-backend evaluation === *)

Print["=== add2(7, 11) ==="];
Print["  Metal:                          ",
  TTermVal @ TAOTRun["add2", {TNum[7], TNum[11]},
                     Method -> "Metal"]];
Print["  CPU (single-thread):            ",
  TTermVal @ TAOTRun["add2", {TNum[7], TNum[11]},
                     Method -> "CPU"]];
Print["  CPU pool (NumThreads -> 4):     ",
  TTermVal @ TAOTRun["add2", {TNum[7], TNum[11]},
                     Method -> {"CPU", "NumThreads" -> 4}]];
println[];

Print["=== mul_then_add(5, 6, 7) === (= 5*6+7 = 37)"];
Print["  Metal:                          ",
  TTermVal @ TAOTRun["mul_then_add", {TNum[5], TNum[6], TNum[7]},
                     Method -> "Metal"]];
Print["  CPU:                            ",
  TTermVal @ TAOTRun["mul_then_add", {TNum[5], TNum[6], TNum[7]},
                     Method -> "CPU"]];
println[];

Print["=== classify(0..2, 99) ==="];
Do[
  Print["  classify(", k, ")  Metal -> ",
        TTermVal @ TAOTRun["classify", {TNum[k]},
                           Method -> "Metal"],
        "  CPU -> ",
        TTermVal @ TAOTRun["classify", {TNum[k]},
                           Method -> "CPU"]],
  {k, {0, 1, 2, 99}}];
println[];

(* === Hot-path timing comparison === *)

Print["=== Hot-path timing (100 runs, caches warm) ==="];
metalArgs = {TNum[7], TNum[11]};

(* Warm caches *)
Do[ TAOTRun["add2", metalArgs, Method -> "Metal"], {3} ];
Do[ TAOTRun["add2", metalArgs, Method -> "CPU"],   {3} ];

tMetal = First @ AbsoluteTiming[
    Do[ TAOTRun["add2", metalArgs, Method -> "Metal"], {100} ];];
tCpu   = First @ AbsoluteTiming[
    Do[ TAOTRun["add2", metalArgs, Method -> "CPU"],   {100} ];];

Print["  100x add2 via Metal:  ", formatUs[tMetal],
      "  (avg ", formatUs[tMetal/100], "/call)"];
Print["  100x add2 via CPU:    ", formatUs[tCpu],
      "  (avg ", formatUs[tCpu/100], "/call)"];
Print["  Metal/CPU ratio:      ~", N[tMetal/tCpu, 3], "x"];
println[];

Print["For tiny defs Metal is dominated by ~190 us kernel-launch"];
Print["overhead.  GPU wins when you batch many independent reductions"];
Print["per launch -- demonstrated below."];
println[];

(* === Batch dispatch (iter QQ) ===

   When you have N independent OP2 redexes queued up, fire them in a
   single Metal dispatch via TAOTBatchOp2Fold instead of N sequential
   TAOTRun calls.  Two orders of magnitude speedup in practice.
*)

Print["=== Batch dispatch: N OP2 ADDs in one Metal launch ==="];

(* Build N=256 OP2(NUM(i), NUM(i+1)) cells in book_heap. *)
N0 = 256;
bookAlloc  = Symbol["WolframInstitute`THVMLink`Private`$bookAllocFn"];
bookSet    = Symbol["WolframInstitute`THVMLink`Private`$bookSetFn"];
termNewRaw = Symbol["WolframInstitute`THVMLink`Private`$termNewFn"];
ttermRawFn = Symbol["WolframInstitute`THVMLink`Private`ttermRaw"];

batchLocs = Table[
  Module[{argLoc, rootLoc},
    argLoc  = bookAlloc[2];
    rootLoc = bookAlloc[1];
    bookSet[argLoc,     ttermRawFn @ TNum[i]];
    bookSet[argLoc + 1, ttermRawFn @ TNum[i + 1]];
    bookSet[rootLoc, termNewRaw[0, $TagOP2, 0 (* ADD *), argLoc]];
    rootLoc],
  {i, 0, N0 - 1}];

(* Warm-up dispatch. *)
TAOTBatchOp2Fold[batchLocs];

tBatch = First @ AbsoluteTiming[ TAOTBatchOp2Fold[batchLocs]; ];

(* Sequential: N separate TAOTRun calls (fresh Metal launches).  Use
   the per-cell layout we already have in book_heap. *)
tSeq = First @ AbsoluteTiming[
    Do[ TAOTRun["add2", {TNum[i], TNum[i + 1]}, Method -> "Metal"],
        {i, 0, N0 - 1} ];];

Print["  ", N0, " OP2 ADDs sequential (N TAOTRun calls): ", formatUs[tSeq],
      "  (avg ", formatUs[tSeq/N0], "/call)"];
Print["  ", N0, " OP2 ADDs batch     (1 dispatch):       ", formatUs[tBatch],
      "  (avg ", formatUs[tBatch/N0], "/call)"];
Print["  Speedup:  ~", N[tSeq/tBatch, 3], "x"];
