#!/usr/bin/env wolframscript
(* fib_nat_aot.wls -- runs fib_nat under the AOT path and prints
   timings vs the same program under the lazy-ALO interpreter.

   Usage:
       wolframscript -f wl/Examples/_hvmbench/fib_nat_aot.wls 25
       wolframscript -f wl/Examples/_hvmbench/fib_nat_aot.wls 25 --runs=5
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"]

$args = Drop[$ScriptCommandLine, 1];
$N    = If[ Length[$args] >= 1 && ! StringStartsQ[First @ $args, "--"],
            ToExpression @ First @ $args,
            10];
$Runs = With[{a = First[Select[$args, StringStartsQ[#, "--runs="] &], None]},
    If[ a === None, 3, ToExpression[StringDrop[a, StringLength["--runs="]]]]];

ZER := TCtr[0];
SUC[x_] := TCtr[1, x];
nat[n_Integer] := If[ n <= 0, ZER, SUC[nat[n - 1]]];

setupDefs[] := (
    TDef["add", TMatChain[
        <|0 -> TLam[b, b],
          1 -> TLam[a, TLam[b, TApp[TApp[TRef["add"], a], SUC[b]]]]
         |>,
        TLam[ignored, TEra[]]
    ]];
    TDef["fib", TMatChain[
        <|0 -> ZER,
          1 -> TMatChain[
                 <|0 -> SUC[ZER],
                   1 -> TLam[p,
                             TDup[7, p, {p0, p1} |->
                                 TApp[TApp[TRef["add"],
                                          TApp[TRef["fib"], SUC[p0]]],
                                      TApp[TRef["fib"], p1]]]]
                  |>,
                 TLam[ignored, TEra[]]
               ]
         |>,
        TLam[ignored, TEra[]]
    ]];
    TDef["u32", TMatChain[
        <|0 -> TLam[n, n],
          1 -> TLam[p, TLam[n, TApp[TApp[TRef["u32"], p], TOp2["+", TNum[1], n]]]]
         |>,
        TLam[ignored, TEra[]]
    ]];
);

runOnce[n_Integer, useAOT_:False] := Module[{t0, tBuild, tWnf, term, itrs0, itrs, result, prog},
    TInit[];
    setupDefs[];
    prog = If[ useAOT, TAOT["fib_nat", {"add", "fib", "u32"}], None];
    t0 = AbsoluteTime[];
    term = TApp[TApp[TRef["u32"], TApp[TRef["fib"], nat[n]]], TNum[0]];
    tBuild = (AbsoluteTime[] - t0) * 1000.;
    itrs0 = TItrs[];
    t0 = AbsoluteTime[];
    result = TWnf[term];
    tWnf = (AbsoluteTime[] - t0) * 1000.;
    itrs = TItrs[] - itrs0;
    <| "build_ms"   -> tBuild,
       "wnf_ms"     -> tWnf,
       "itrs"       -> itrs,
       "aot_calls"  -> If[prog === None, 0, prog["calls"]],
       "head"       -> result["tagName"],
       "value"      -> If[ result["tagName"] === "NUM", result["val"], None],
       "prog"       -> prog
     |>
];

print["[fib_nat_aot] N=", $N, "  runs=", $Runs];

(* Warm + timed runs. *)
runOnce[$N, False];

interpRuns = Table[runOnce[$N, False], {$Runs}];
aotRuns    = Table[runOnce[$N, True],  {$Runs}];

interpBest = Min[interpRuns[[All, "wnf_ms"]]];
aotBest    = Min[aotRuns[[All, "wnf_ms"]]];

print["  interpreted: best ", NumberForm[interpBest, 4], " ms,  itrs=",
      interpRuns[[1, "itrs"]], ",  value=", interpRuns[[1, "value"]]];
print["  AOT:         best ", NumberForm[aotBest, 4], " ms,  itrs=",
      aotRuns[[1, "itrs"]], ",  aot_calls=", aotRuns[[1, "aot_calls"]],
      ",  value=", aotRuns[[1, "value"]]];
print["  speedup:     ", NumberForm[interpBest / aotBest, 3], "x"];
print["  expected Fibonacci[", $N, "] = ", Fibonacci[$N]];

(* Show the TAOTProgram object summary -- via short InputForm-ish dump *)
prog = aotRuns[[1, "prog"]];
print["  TAOTProgram object: name=", prog["name"],
      ", defs=", prog["defs"],
      ", slots=", prog["slots"],
      ", registered=", prog["registered"]];
