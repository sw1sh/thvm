#!/usr/bin/env wolframscript
(* term.wls -- pool / profiling sweep across canonical workloads.

   Run via:  wolframscript -f wl/Examples/_pool_profile/term.wls

   Workloads (each at T=1, 2, 4, 8 -- WNF and NF baselines for T=1):

     chain    n=1000, 5000, 10000     ADDs in a single dependency chain.
                                       Serial; T>1 spawns workers idly.
     tree-id  d=8, 10, 12, 14         Balanced ADD tree, leaves all =NUM(1).
                                       Parallel by level; identical leaves
                                       collapse via heap_replace use-list.
     tree-d   d=8, 10, 12, 14         Balanced ADD tree, distinct leaves
                                       (regression for the use-list
                                       saturation bug).
     id-app   n=100, 500, 1000        APP-LAM beta chain (id ((id (id ERA)))).
     mixed    n=100, 500, 1000        Alternating APP-LAM + OP2.

   Print one line per (workload, threads).  No correctness assertions
   here -- the runtime tests cover that. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

println[]      := WriteString["stdout", "\n"];

formatMs[t_] := If[ t < 1.*^-3, ToString[N[t*1000000, 4]] <> "us",
                    ToString[N[t*1000, 4]] <> "ms"];

peek[r_] := Module[{tag = TTermTag[r]},
  Which[
    tag == 10, "NUM(" <> ToString[TTermVal[r]] <> ")",
    tag ==  3, "ERA",
    tag ==  1, "LAM",
    tag ==  0, "APP",
    tag == 13, "OP2",
    True,      TTagName[tag]
  ]];

bench[label_, builder_] := (
  TInit[]; TThreads[1];
  Module[ {tWnf, iWnf, tCnf, iCnf, t, rWnf, rCnf},
    t = builder; iWnf = TItrs[]; tWnf = First @ AbsoluteTiming[rWnf = TWnf[t]]; iWnf = TItrs[] - iWnf;
    TInit[];
    t = builder; iCnf = TItrs[]; tCnf = First @ AbsoluteTiming[rCnf = TCnf[t]]; iCnf = TItrs[] - iCnf;
    Print[StringPadRight[label, 22], "  T=1/wnf  ", formatMs[tWnf], "  ", iWnf, "i ", peek[rWnf]];
    Print[StringPadRight[label, 22], "  T=1/cnf  ", formatMs[tCnf], "  ", iCnf, "i ", peek[rCnf]];
  ];
  Do[
    TInit[]; TThreads[T];
    Module[ {tNf, iNf, t, rNf},
      t = builder;
      iNf = TItrs[];
      tNf = First @ AbsoluteTiming[rNf = TNf[t]];
      iNf = TItrs[] - iNf;
      Print[StringPadRight[label, 22], "  T=", T, "/nf   ", formatMs[tNf],
            "  ", iNf, "i ", peek[rNf]]
    ],
    {T, {1, 2, 4, 8}}
  ];
);

SetAttributes[bench, HoldRest];

(* ---- Workload builders ---- *)

chain[n_] := Module[{t = TNum[0]}, Do[t = TOp2["+", t, TNum[i]], {i, 1, n}]; t];

treeIter[depth_, leafFn_] := Module[
  {level = Table[leafFn[i], {i, 1, 2^depth}], n},
  Do[ n = Length[level] / 2;
      level = Table[TOp2["+", level[[2 j - 1]], level[[2 j]]], {j, 1, n}],
      {depth}];
  First[level]
];

idApp[depth_] := Module[{t = TEra[]}, Do[t = TApp[TLam[x, x], t], {depth}]; t];

mixed[n_] := Module[{t = TNum[0]},
  Do[ t = TApp[TLam[x, TOp2["+", x, TNum[i]]], t], {i, 1, n}];
  t
];

(* ---- Sweep ---- *)

Print[StringRepeat["=", 70]];
Print["pool profile sweep -- THVMLink ", $Version];
Print[StringRepeat["=", 70]];

println[];
Print["----- chain ADD (serial) -----"];
Do[ bench["chain n=" <> ToString[n], chain[n]], {n, {1000, 5000, 10000}}];

println[];
Print["----- tree ADD identical leaves (parallel by level) -----"];
Do[ bench["tree-id d=" <> ToString[d], treeIter[d, TNum[1]&]], {d, {8, 10, 12, 14}}];

println[];
Print["----- tree ADD distinct leaves -----"];
Do[ bench["tree-d  d=" <> ToString[d], treeIter[d, TNum[#]&]], {d, {8, 10, 12, 14}}];

println[];
Print["----- id-app chain (APP-LAM) -----"];
Do[ bench["id-app n=" <> ToString[n], idApp[n]], {n, {100, 500, 1000}}];

println[];
Print["----- mixed (APP-LAM + OP2) -----"];
Do[ bench["mixed  n=" <> ToString[n], mixed[n]], {n, {100, 500, 1000}}];
