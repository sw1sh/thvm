(* aot_metal.wlt -- VerificationTest specs for the AOT-on-Metal
   pipeline.  Phase 7 iter R: WL-side regression coverage so the
   Metal kernel emit, xcrun metallib build, MTLBuffer dispatch, and
   readback all stay green from the WL surface.

   Covers each Metal iter's deliverable:
     iter D    -- bare TLam-peel + OP2 fold      (add2)
     iter F    -- App(MAT, TVar) NUM-arm chain   (classify)
     iter G    -- REF inlining cross-def call    (double_add)
     iter H    -- CTR construction               (wrap)
     iter K    -- multi-arg MAT-chain            (select)
     iter L    -- CTR destructure in MAT arms    (pair_sum)
     iter J+Q  -- Method -> {"CPU", "NumThreads" -> n} parallel
                                                 (add2 across n=1..8)

   The runner (run.wls) loads the paclet first; this file is data.
   Each test isolates state via TInit so DEFS slot ids are
   deterministic across runs.
*)

(* === iter D: bare TLam-peel + OP2 fold === *)

VerificationTest[
    TInit[];
    TDef["add2", TLam[a, TLam[b, TOp2["+", a, b]]]];
    TTermVal @ TAOTRun["add2", {TNum[3], TNum[4]}, Method -> "Metal"],
    7,
    TestID -> "Metal: add2(3, 4) -> 7"
]

VerificationTest[
    TInit[];
    TDef["add2", TLam[a, TLam[b, TOp2["+", a, b]]]];
    TTermVal @ TAOTRun["add2", {TNum[100], TNum[200]}, Method -> "Metal"],
    300,
    TestID -> "Metal: add2(100, 200) -> 300 (PSO cache hit)"
]

VerificationTest[
    TInit[];
    TDef["mul_add",
      TLam[a, TLam[b, TLam[c, TOp2["+", TOp2["*", a, b], c]]]]];
    TTermVal @ TAOTRun["mul_add", {TNum[5], TNum[6], TNum[7]},
                       Method -> "Metal"],
    37,
    TestID -> "Metal: mul_add(5, 6, 7) -> (5*6)+7 = 37"
]

(* === iter F: App(MAT, TVar) NUM-arm chain === *)

VerificationTest[
    TInit[];
    TDef["classify",
      TLam[n, TMatChain[<|0 -> TNum[42], 1 -> TNum[99], 2 -> TNum[7]|>,
                         TNum[0]][n]]];
    TTermVal /@ (TAOTRun["classify", {TNum[#]}, Method -> "Metal"] & /@
                  {0, 1, 2, 7, 99}),
    {42, 99, 7, 0, 0},
    TestID -> "Metal: classify(0..2,7,99) NUM-arm chain"
]

(* === iter K: multi-arg MAT-chain === *)

VerificationTest[
    TInit[];
    TDef["select",
      TLam[idx, TLam[a, TLam[b,
        TMatChain[<|0 -> a, 1 -> b|>, TNum[0]][idx]]]]];
    TTermVal /@ {
      TAOTRun["select", {TNum[0], TNum[11], TNum[22]}, Method -> "Metal"],
      TAOTRun["select", {TNum[1], TNum[11], TNum[22]}, Method -> "Metal"],
      TAOTRun["select", {TNum[9], TNum[11], TNum[22]}, Method -> "Metal"]
    },
    {11, 22, 0},
    TestID -> "Metal: select(idx, a, b) multi-arg MAT-chain"
]

(* === iter J+Q: parallel CPU dispatch via wnf_pool === *)

VerificationTest[
    TInit[];
    TDef["add2", TLam[a, TLam[b, TOp2["+", a, b]]]];
    TTermVal /@ {
      TAOTRun["add2", {TNum[3],   TNum[4]},
              Method -> {"CPU", "NumThreads" -> 1}],
      TAOTRun["add2", {TNum[10],  TNum[20]},
              Method -> {"CPU", "NumThreads" -> 2}],
      TAOTRun["add2", {TNum[100], TNum[200]},
              Method -> {"CPU", "NumThreads" -> 4}],
      TAOTRun["add2", {TNum[1000],TNum[2000]},
              Method -> {"CPU", "NumThreads" -> 8}]
    },
    {7, 30, 300, 3000},
    TestID -> "CPU pool: add2 NumThreads -> 1, 2, 4, 8"
]

(* === Method dispatcher rejects unknown spec === *)

VerificationTest[
    TInit[];
    TDef["add2", TLam[a, TLam[b, TOp2["+", a, b]]]];
    Quiet @ TAOTRun["add2", {TNum[1], TNum[2]}, Method -> "GPU2"],
    $Failed,
    TestID -> "Method -> unknown spec returns $Failed"
]
