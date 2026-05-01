#!/usr/bin/env wolframscript
(* gab_tak.wls -- thvm port of bench/gab_tak (Gabriel TAK / Takeuchi).

   Source HVM4:
       @pred = lam { 0n: 0n; 1n+: lam &p. p }
       @lte  = lam { 0n: lam &b. #T{};
                     1n+: lam &p. lam { 0n: #F{}; 1n+: lam &q. lte(p, q) } }
       @tak_go = lam { #T: lam &x. lam &y. lam &z. y;
                       #F: lam &x. lam &y. lam &z.
                             ! a = tak(pred(x), y, z);
                             ! b = tak(pred(y), z, x);
                             ! c = tak(pred(z), x, y);
                             tak(a, b, c) }
       @tak    = lam &x. lam &y. lam &z. tak_go(lte(x, y), x, y, z)
       @u32_to = lam { 0n: 0; 1n+: lam &p. u32_to(p) + 1 }
       @main   = u32_to(tak(56n, 50n, 44n))    -- expected: 56

   Constructor labels: ZER=0, SUC=1, T=2, F=3.  Same dup label as
   fib_nat.

   Usage:
       wolframscript -f wl/Examples/_hvmbench/gab_tak.wls
       wolframscript -f wl/Examples/_hvmbench/gab_tak.wls --x=56 --y=50 --z=44
       wolframscript -f wl/Examples/_hvmbench/gab_tak.wls --x=12 --y=8 --z=4 --aot
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

$X    = parseArg["x", 56];
$Y    = parseArg["y", 50];
$Z    = parseArg["z", 44];
$Runs = parseArg["runs", 3];
$WantAOT = MemberQ[$args, "--aot"] || MemberQ[$args, "--both"];
$WantBoth = MemberQ[$args, "--both"];

ZER := TCtr[0];
SUC[x_] := TCtr[1, x];
TT  := TCtr[2];   (* #T{} *)
FF  := TCtr[3];   (* #F{} *)
nat[n_Integer] := If[ n <= 0, ZER, SUC[nat[n - 1]]];

setupGabTakDefs[] := (
    TDef["pred", TMatChain[
        <|0 -> ZER,
          1 -> TLam[p, p]
         |>,
        TLam[ignored, TEra[]]
    ]];
    TDef["lte", TMatChain[
        <|0 -> TLam[ignoredb, TT],
          1 -> TLam[p, TMatChain[
                 <|0 -> FF,
                   1 -> TLam[q, TApp[TApp[TRef["lte"], p], q]]
                  |>,
                 TLam[ignored, TEra[]]
               ]]
         |>,
        TLam[ignored, TEra[]]
    ]];
    TDef["tak_go", TMatChain[
        <|2 -> TLam[x, TLam[y, TLam[z, y]]],
          3 -> TLam[x, TLam[y, TLam[z,
                 TApp[TApp[TApp[TRef["tak"],
                          TApp[TApp[TApp[TRef["tak"], TApp[TRef["pred"], x]], y], z]],
                          TApp[TApp[TApp[TRef["tak"], TApp[TRef["pred"], y]], z], x]],
                          TApp[TApp[TApp[TRef["tak"], TApp[TRef["pred"], z]], x], y]
                 ]]]]
         |>,
        TLam[ignored, TEra[]]
    ]];
    TDef["tak", TLam[x, TLam[y, TLam[z,
        TApp[TApp[TApp[TApp[TRef["tak_go"],
            TApp[TApp[TRef["lte"], x], y]], x], y], z]
    ]]]];
    TDef["u32_to", TMatChain[
        <|0 -> TNum[0],
          1 -> TLam[p, TOp2["+", TApp[TRef["u32_to"], p], TNum[1]]]
         |>,
        TLam[ignored, TEra[]]
    ]];
);

makeMain[x_, y_, z_] := TApp[TRef["u32_to"],
    TApp[TApp[TApp[TRef["tak"], nat[x]], nat[y]], nat[z]]];

runOnce[x_, y_, z_, useAOT_:False] := Module[{t0, tBuild, tWnf, term, itrs0, itrs, result, prog},
    TInit[];
    setupGabTakDefs[];
    prog = If[ useAOT, TAOT["gab_tak", {"pred", "lte", "tak_go", "tak", "u32_to"}], None];
    t0 = AbsoluteTime[];
    term = makeMain[x, y, z];
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

print["[gab_tak] x=", $X, "  y=", $Y, "  z=", $Z, "  runs=", $Runs];

(* Warm-up. *)
runOnce[$X, $Y, $Z, False];

interpRuns = Table[runOnce[$X, $Y, $Z, False], {$Runs}];
interpBest = Min[interpRuns[[All, "wnf_ms"]]];
print["  interpreted: best ", NumberForm[interpBest, 4], " ms,  itrs=",
      interpRuns[[1, "itrs"]], ",  value=", interpRuns[[1, "value"]]];

If[ $WantAOT,
    aotRuns = Table[runOnce[$X, $Y, $Z, True], {$Runs}];
    aotBest = Min[aotRuns[[All, "wnf_ms"]]];
    print["  AOT:         best ", NumberForm[aotBest, 4], " ms,  itrs=",
          aotRuns[[1, "itrs"]], ",  aot_calls=", aotRuns[[1, "aot_calls"]],
          ",  value=", aotRuns[[1, "value"]]];
    print["  speedup:     ", NumberForm[interpBest / aotBest, 3], "x"];
];
