#!/usr/bin/env wolframscript
(* fib_nat.wls -- thvm port of bench/fib_nat from
   https://github.com/HigherOrderCO/bench

   Source HVM4:
       @add = λ{
         #ZER: λb. b;
         #SUC: λa. λb. @add(a, #SUC{b})
       }
       @fib = λ{
         #ZER: #ZER;
         #SUC: λ{
           #ZER: #SUC{#ZER}
           #SUC: λp.
             ! P &L = p;
             @add(@fib(#SUC{P0}), @fib(P1))
         }
       }
       @u32 = λ{
         #ZER: λn. n
         #SUC: λp. λn. @u32(p, 1+n)
       }
       @main = @u32(@fib(34n), 0)

   Constructor labels: ZER = 0, SUC = 1.  TDup label arbitrary (7).
   The whole pipeline -- DUP-CTR commute, MAT chain dispatch,
   TRef recursion, OP2 arithmetic -- must work end-to-end. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

$args = Drop[$ScriptCommandLine, 1];
$N    = If[ Length[$args] >= 1 && ! StringStartsQ[First @ $args, "--"],
            ToExpression @ First @ $args,
            10];

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

runOnce[n_Integer] := Module[{t0, tBuild, tWnf, term, itrs0, itrs, result},
    TInit[];
    setupDefs[];
    t0 = AbsoluteTime[];
    term = TApp[TApp[TRef["u32"], TApp[TRef["fib"], nat[n]]], TNum[0]];
    tBuild = (AbsoluteTime[] - t0) * 1000.;
    itrs0 = TItrs[];
    t0 = AbsoluteTime[];
    result = TWnf[term];
    tWnf = (AbsoluteTime[] - t0) * 1000.;
    itrs = TItrs[] - itrs0;
    <| "build_ms" -> tBuild,
       "wnf_ms"   -> tWnf,
       "itrs"     -> itrs,
       "head"     -> result["tagName"],
       "value"    -> If[ result["tagName"] === "NUM", result["val"], None]
     |>
];

Print["[thvm] fib_nat n=", $N];
r0 = runOnce[$N];
Print["[thvm] head=", r0["head"], "  value=", r0["value"],
      "  itrs=", r0["itrs"],
      "  build_ms=", NumberForm[r0["build_ms"], 4],
      "  wnf_ms=",   NumberForm[r0["wnf_ms"],   4]];

(* Print expected fib(n) for the n-values we care about. *)
fibRef[k_Integer] := With[{a = Fibonacci[k]}, a];
Print["[thvm] expected Fibonacci[", $N, "] = ", fibRef[$N]];
