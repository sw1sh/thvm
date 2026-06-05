#!/usr/bin/env wolframscript
(* cnot.wls -- thvm side of the HigherOrderCO/bench cnot_NN suite.

   Builds the same lambda-encoded NOT-iterated-2^N program as
   bench/cnot_04, cnot_16, cnot_24 from
   https://github.com/HigherOrderCO/bench, expressed via the WL
   surface (TLam / TApp / TDup / TVarFor).  Reduces it to WNF
   under TWnf and reports wall time + interaction count.

   The Source main is `@P_N @cnot @ctru`.  We append two ERA
   trailers (two `*` arguments) so the resulting Church bool
   reduces past its outer binders to a single ERA cell -- thvm's
   TWnf is weak head NF and HVM4 normalizes under binders by
   default; the trailing apps drive the dup chain to completion at
   WHNF too.  The two extra APP-LAM interactions on the trailer
   match exactly between the two runtimes.

   Usage:
       wolframscript -f wl/Examples/_hvmbench/cnot.wls 4
       wolframscript -f wl/Examples/_hvmbench/cnot.wls 16
       wolframscript -f wl/Examples/_hvmbench/cnot.wls 4 --runs=5
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

$args = Drop[$ScriptCommandLine, 1];
$N    = If[ Length[$args] >= 1 && ! StringStartsQ[First @ $args, "--"],
            ToExpression @ First @ $args,
            4];
$Runs = With[{a = First[Select[$args, StringStartsQ[#, "--runs="] &], None]},
    If[ a === None, 3, ToExpression[StringDrop[a, StringLength["--runs="]]]]];

ctru := TLam[t, TLam[f, t]];
cfal := TLam[t, TLam[f, f]];
cnot := TLam[x, TApp[TApp[x, cfal], ctru]];

dupLabel = 7;
compose[src_] := TDup[dupLabel, src,
    {s0, s1} |-> TLam[k, TApp[s0, TApp[s1, k]]]];
P[n_Integer?Positive] := TLam[f, Nest[compose, f, n]];

runOnce[n_Integer] := Module[{tBuild, tWnf, term, itrs0, itrs, result, t0},
    TInit[];
    t0 = AbsoluteTime[];
    term = TApp[TApp[TApp[TApp[P[n], cnot], ctru], TEra[]], TEra[]];
    tBuild = (AbsoluteTime[] - t0) * 1000.;
    itrs0 = TItrs[];
    t0 = AbsoluteTime[];
    result = TWnf[term];
    tWnf = (AbsoluteTime[] - t0) * 1000.;
    itrs = TItrs[] - itrs0;
    <| "build_ms" -> tBuild,
       "wnf_ms"   -> tWnf,
       "itrs"     -> itrs,
       "head"     -> result["tagName"] |>
];

Print["[thvm] cnot_", IntegerString[$N, 10, 2], " (runs=", $Runs, ")"];

(* Discard first run as warm-up. *)
runOnce[$N];
results = Table[runOnce[$N], {$Runs}];
itrs    = results[[1, "itrs"]];
head    = results[[1, "head"]];
wnfMs   = results[[All, "wnf_ms"]];
buildMs = results[[All, "build_ms"]];

Print["[thvm] N=", $N,
      "  build_ms=[", StringRiffle[NumberForm[#, 4]& /@ buildMs, ", "], "]",
      "  wnf_ms=[", StringRiffle[NumberForm[#, 4]& /@ wnfMs, ", "], "]"];
Print["[thvm] best_wnf_ms=", NumberForm[Min[wnfMs], 4],
      "  itrs=", itrs,
      "  Mitrs/s=", NumberForm[itrs / Min[wnfMs] / 1000., 4],
      "  head=", head];
