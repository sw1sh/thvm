#!/usr/bin/env wolframscript
(* u32_fib.wls -- thvm port of bench/u32_fib (naive Fibonacci on
   native U32).  Compares the lazy-ALO interpreter to the hand-coded
   AOT path.

   Source HVM4:
       @u32_fib = lam {
         0: 0;
         1: 1;
         lam &k. fib(k - 1) + fib(k - 2)
       }
       @main = u32_fib(38)

   Single def, single arg, dispatch on a NUM head.  Recursion works
   on FRESH NUMs -- AOT-friendly access pattern (see gab_tak.c for
   the contrast).

   Usage:
       wolframscript -f wl/Examples/_hvmbench/u32_fib.wls 25
       wolframscript -f wl/Examples/_hvmbench/u32_fib.wls 30 --runs=5
       wolframscript -f wl/Examples/_hvmbench/u32_fib.wls 30 --aot
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"]

$args = Drop[$ScriptCommandLine, 1];
parseArg[name_String, default_] := With[{
    a = First[Select[$args, StringStartsQ[#, "--" <> name <> "="] &], None]},
    If[ a === None, default,
        ToExpression @ StringDrop[a, StringLength["--" <> name <> "="]]]
];

$N    = If[ Length[$args] >= 1 && ! StringStartsQ[First @ $args, "--"],
            ToExpression @ First @ $args,
            25];
$Runs = parseArg["runs", 3];
$WantAOT = MemberQ[$args, "--aot"] || MemberQ[$args, "--both"];

setupU32FibDef[] := (
    TDef["u32_fib", TMatChain[
        <|0 -> TNum[0],
          1 -> TNum[1]
         |>,
        TLam[k, TOp2["+",
            TApp[TRef["u32_fib"], TOp2["-", k, TNum[1]]],
            TApp[TRef["u32_fib"], TOp2["-", k, TNum[2]]]
        ]]
    ]];
);

runOnce[n_Integer, useAOT_:False] := Module[{t0, tBuild, tWnf, term, itrs0, itrs, result, prog},
    TInit[];
    setupU32FibDef[];
    prog = If[ useAOT, TAOT["u32_fib", {"u32_fib"}], None];
    t0 = AbsoluteTime[];
    term = TApp[TRef["u32_fib"], TNum[n]];
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
       "value"      -> If[ result["tagName"] === "NUM", result["val"], None]
     |>
];

print["[u32_fib] N=", $N, "  runs=", $Runs];

(* Warm-up. *)
runOnce[$N, False];

interpRuns = Table[runOnce[$N, False], {$Runs}];
interpBest = Min[interpRuns[[All, "wnf_ms"]]];
print["  interpreted: best ", NumberForm[interpBest, 4], " ms,  itrs=",
      interpRuns[[1, "itrs"]], ",  value=", interpRuns[[1, "value"]]];

If[ $WantAOT,
    aotRuns = Table[runOnce[$N, True], {$Runs}];
    aotBest = Min[aotRuns[[All, "wnf_ms"]]];
    print["  AOT:         best ", NumberForm[aotBest, 4], " ms,  itrs=",
          aotRuns[[1, "itrs"]], ",  aot_calls=", aotRuns[[1, "aot_calls"]],
          ",  value=", aotRuns[[1, "value"]]];
    print["  speedup:     ", NumberForm[interpBest / aotBest, 3], "x"];
];
print["  expected Fibonacci[", $N, "] = ", Fibonacci[$N]];
