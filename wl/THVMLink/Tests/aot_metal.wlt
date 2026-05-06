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

(* === iter G: REF inlining (cross-def static call) === *)

VerificationTest[
    TInit[];
    TDef["add2",       TLam[a, TLam[b, TOp2["+", a, b]]]];
    TDef["double_add", TLam[x, TLam[y,
      TOp2["*", TApp[TApp[TRef["add2"], x], y], TNum[2]]]]];
    TTermVal @ TAOTRun["double_add", {TNum[3], TNum[4]},
                       Method -> "Metal"],
    14,   (* (3 + 4) * 2 -- add2 inlined at the call site *)
    TestID -> "Metal: REF inlining double_add(3,4) = (a+b)*2 = 14"
]

(* === iter L: CTR destructure in MAT arm ===

   The matched-arm case (CTR{2, [NUM(7), NUM(35)]} -> 42) needs the
   input CTR's children to be reachable by the kernel via the `heap`
   MTLBuffer.  Today that buffer is zero-copy bound to BOOK_HEAP, but
   WL's TCtr[label, ...] allocates in the DYNAMIC HEAP (via
   term_new_ctr -> heap_alloc).  When the kernel reads
   `heap[scrutinee_val + 1]` it actually indexes into book_heap at the
   wrong offset, returning zeros.

   Fix candidates (deferred -- separate iter):
     * pass dyn_heap to the kernel as a second buffer; choose by val
       range or via a new WL helper that allocates in book_heap
     * marshal call-time CTRs into book_heap before dispatch
     * teach WL to allocate certain inputs in book_heap directly

   The C test in tests/test_aot_metal_run.c covers the matched-arm
   path correctly because it builds the input CTR via book_alloc /
   book_set.  Only the default-arm path round-trips cleanly through
   WL (no destructure -> no heap deref).
*)

VerificationTest[
    TInit[];
    TDef["pair_sum",
      TLam[p, TMatChain[
        <|2 -> TLam[x, TLam[y, TOp2["+", x, y]]]|>,
        TNum[0]][p]]];
    TTermVal @ TAOTRun["pair_sum", {TCtr[99, TNum[1]]},
                       Method -> "Metal"],
    0,   (* label 99 doesn't match -- default arm fires *)
    TestID -> "Metal: pair_sum(CTR{99, [NUM(1)]}) -> default 0 (no destructure)"
]

(* iter T: TBookCtr allocates in BOOK_HEAP so the matched-arm
   destructure deref into the kernel's `heap` MTLBuffer resolves
   to the right cells. *)

VerificationTest[
    TInit[];
    TDef["pair_sum",
      TLam[p, TMatChain[
        <|2 -> TLam[x, TLam[y, TOp2["+", x, y]]]|>,
        TNum[0]][p]]];
    TTermVal @ TAOTRun["pair_sum", {TBookCtr[2, TNum[7], TNum[35]]},
                       Method -> "Metal"],
    42,
    TestID -> "Metal: pair_sum(TBookCtr{2, [NUM(7), NUM(35)]}) = 7+35 = 42"
]

VerificationTest[
    TInit[];
    TDef["fst_pair",
      TLam[p, TMatChain[
        <|2 -> TLam[x, TLam[y, x]]|>,
        TNum[0]][p]]];
    TTermVal @ TAOTRun["fst_pair", {TBookCtr[2, TNum[111], TNum[222]]},
                       Method -> "Metal"],
    111,
    TestID -> "Metal: fst_pair(TBookCtr{2, ...}) returns first child"
]

(* === iter V: TAG_DUP support (multi-use binder) === *)

(* TLam[x, x*x]: WL's auto_dup rewrites the multi-use TVar(x) into a
   chain of DP0/DP1 cells; the Metal emit memos dup_loc -> uint var
   so both projections share one computation. *)
VerificationTest[
    TInit[];
    TDef["square", TLam[x, TOp2["*", x, x]]];
    TTermVal /@ (TAOTRun["square", {TNum[#]}, Method -> "Metal"] & /@
                  {3, 5, 12, 100}),
    {9, 25, 144, 10000},
    TestID -> "Metal: square(x) = x*x via auto-dup'd binder"
]

(* === iter Y: variable-arity dispatch (>4 args) === *)

VerificationTest[
    TInit[];
    TDef["sum5", TLam[a, TLam[b, TLam[c, TLam[d, TLam[e,
      TOp2["+", a, TOp2["+", b, TOp2["+", c, TOp2["+", d, e]]]]]]]]]];
    TTermVal /@ {
      TAOTRun["sum5", {TNum[1], TNum[2], TNum[3], TNum[4], TNum[5]},
              Method -> "Metal"],
      TAOTRun["sum5", {TNum[10], TNum[20], TNum[30], TNum[40], TNum[50]},
              Method -> "Metal"]
    },
    {15, 150},
    TestID -> "Metal: sum5(a,b,c,d,e) -- 5 args via run_n bridge"
]

(* === iter H: kernel-built CTR result === *)

(* TLam[x, TCtr[1, x]] -- kernel allocates a single-child CTR cell on
   book_heap via aot_book_alloc, returns a TAG_CTR Term.  Verify tag
   and label round-trip through the WL surface. *)
VerificationTest[
    TInit[];
    TDef["wrap1", TLam[x, TCtr[1, x]]];
    With[{r = TAOTRun["wrap1", {TNum[42]}, Method -> "Metal"]},
      {TTermTag[r], TTermExt[r]}],
    {$TagCTR, 1},
    TestID -> "Metal: wrap1(x) returns TAG_CTR with label=1"
]

(* iter FF: TBookRead lets WL inspect kernel-built CTR cells.
   Verify the children are what we passed in. *)
VerificationTest[
    TInit[];
    TDef["wrap1", TLam[x, TCtr[1, x]]];
    Module[{r, loc},
      r   = TAOTRun["wrap1", {TNum[42]}, Method -> "Metal"];
      loc = TTermVal[r];
      {TTermVal @ TBookRead[loc],         (* n_cell: NUM(1) *)
       TTermVal @ TBookRead[loc + 1]}     (* child[0]: NUM(42) *)
    ],
    {1, 42},
    TestID -> "Metal: wrap1(42) child round-trip via TBookRead"
]

VerificationTest[
    TInit[];
    TDef["pair", TLam[a, TLam[b, TCtr[2, a, b]]]];
    With[{r = TAOTRun["pair", {TNum[7], TNum[35]}, Method -> "Metal"]},
      {TTermTag[r], TTermExt[r]}],
    {$TagCTR, 2},
    TestID -> "Metal: pair(a, b) returns TAG_CTR with label=2"
]

VerificationTest[
    TInit[];
    TDef["pair", TLam[a, TLam[b, TCtr[2, a, b]]]];
    Module[{r, loc},
      r   = TAOTRun["pair", {TNum[7], TNum[35]}, Method -> "Metal"];
      loc = TTermVal[r];
      {TTermVal @ TBookRead[loc],
       TTermVal @ TBookRead[loc + 1],
       TTermVal @ TBookRead[loc + 2]}
    ],
    {2, 7, 35},
    TestID -> "Metal: pair(7, 35) full content round-trip via TBookRead"
]

(* === iter MM: cross-backend equivalence === *)

(* Same def, same input, two backends -> same result.  Catches
   regressions where one path drifts from the other. *)
VerificationTest[
    TInit[];
    TDef["add2", TLam[a, TLam[b, TOp2["+", a, b]]]];
    Module[{metalR, cpuR},
      metalR = TTermVal @ TAOTRun["add2", {TNum[7], TNum[11]},
                                  Method -> "Metal"];
      cpuR   = TTermVal @ TAOTRun["add2", {TNum[7], TNum[11]},
                                  Method -> "CPU"];
      {metalR, cpuR, metalR === cpuR}
    ],
    {18, 18, True},
    TestID -> "Equivalence: add2(7,11) Metal == CPU"
]

VerificationTest[
    TInit[];
    TDef["mul_then_add",
      TLam[a, TLam[b, TLam[c, TOp2["+", TOp2["*", a, b], c]]]]];
    Module[{metalR, cpuR},
      metalR = TTermVal @ TAOTRun["mul_then_add",
                  {TNum[5], TNum[6], TNum[7]}, Method -> "Metal"];
      cpuR   = TTermVal @ TAOTRun["mul_then_add",
                  {TNum[5], TNum[6], TNum[7]}, Method -> "CPU"];
      {metalR, cpuR, metalR === cpuR}
    ],
    {37, 37, True},
    TestID -> "Equivalence: mul_then_add(5,6,7) Metal == CPU"
]

VerificationTest[
    TInit[];
    TDef["classify",
      TLam[n, TMatChain[<|0 -> TNum[42], 1 -> TNum[99], 2 -> TNum[7]|>,
                         TNum[0]][n]]];
    Module[{metals, cpus},
      metals = TTermVal /@ (TAOTRun["classify", {TNum[#]},
                              Method -> "Metal"] & /@ {0, 1, 2, 99});
      cpus   = TTermVal /@ (TAOTRun["classify", {TNum[#]},
                              Method -> "CPU"]   & /@ {0, 1, 2, 99});
      {metals, cpus, metals === cpus}
    ],
    {{42, 99, 7, 0}, {42, 99, 7, 0}, True},
    TestID -> "Equivalence: classify(0,1,2,99) Metal == CPU"
]

(* iter PP: CTR-returning equivalence -- same def, both backends should
   produce a CTR Term with the same {tag, ext} (the val differs because
   Metal allocates in BOOK_HEAP and CPU allocates in HEAP, but the
   structural identity matches). *)
VerificationTest[
    TInit[];
    TDef["wrap1", TLam[x, TCtr[1, x]]];
    Module[{metalR, cpuR},
      metalR = TAOTRun["wrap1", {TNum[42]}, Method -> "Metal"];
      cpuR   = TAOTRun["wrap1", {TNum[42]}, Method -> "CPU"];
      {{TTermTag[metalR], TTermExt[metalR]},
       {TTermTag[cpuR],   TTermExt[cpuR]}}
    ],
    {{$TagCTR, 1}, {$TagCTR, 1}},
    TestID -> "Equivalence: wrap1(42) tag/label Metal == CPU"
]

(* === iter QQ: WL surface for the batch OP2 fold dispatcher ===

   The kernel side (aot_eval_op2_fold_batch) launches one thread per
   OP2 redex, all in a single dispatch.  WL helper TAOTBatchOp2Fold
   takes a list of book_heap locs (each pointing at an OP2(NUM,NUM)
   cell) and returns the folded NUM TTerms.

   Build the OP2 cells directly in book_heap via the private bridges
   so the kernel's `heap` MTLBuffer can deref them. *)
VerificationTest[
    TInit[];
    Module[{bookAlloc, bookSet, termNewRaw, ttermRawFn, opCodes,
            locs, results},
      bookAlloc  = Symbol["THVMLink`Private`$bookAllocFn"];
      bookSet    = Symbol["THVMLink`Private`$bookSetFn"];
      termNewRaw = Symbol["THVMLink`Private`$termNewFn"];
      (* ttermRaw lives in THVMLink`Private`; the wlt runs at global
         scope so we have to fetch it by qualified name. *)
      ttermRawFn = Symbol["THVMLink`Private`ttermRaw"];
      opCodes    = {0, 1, 2, 3, 4};   (* ADD, SUB, MUL, EQ, LT *)
      locs = Table[
        Module[{argLoc, rootLoc, opTerm},
          argLoc  = bookAlloc[2];
          rootLoc = bookAlloc[1];
          bookSet[argLoc,     ttermRawFn @ TNum[i]];
          bookSet[argLoc + 1, ttermRawFn @ TNum[i + 1]];
          (* OP2 Term: tag=13 (OP2), ext=opcode, val=argLoc *)
          opTerm = termNewRaw[0, $TagOP2, opCodes[[i + 1]], argLoc];
          bookSet[rootLoc, opTerm];
          rootLoc
        ],
        {i, 0, 4}];
      results = TAOTBatchOp2Fold[locs];
      TTermVal /@ results
    ],
    (* 0+1=1, 1-2 wraps as u32 to 4294967295 (i=1: SUB takes args[i]
       and args[i+1] = NUM(1), NUM(2)), 2*3=6, 3==4 -> 0, 4<5 -> 1.
       Note: i loop is 0..4, so for op i=1 (SUB), inputs are NUM(1)
       and NUM(2), and so on. *)
    {1, 4294967295, 6, 0, 1},
    TestID -> "Metal: batch OP2 fold across 5 redexes (ADD,SUB,MUL,EQ,LT)"
]

(* === Method dispatcher rejects unknown spec === *)

VerificationTest[
    TInit[];
    TDef["add2", TLam[a, TLam[b, TOp2["+", a, b]]]];
    Quiet @ TAOTRun["add2", {TNum[1], TNum[2]}, Method -> "GPU2"],
    $Failed,
    TestID -> "Method -> unknown spec returns $Failed"
]

(* === iter Z: Church-encoded IC bodies (SUP / nested LAM) ===

   The IC-construction emit_term cases + GPU-side wnf state machine
   handle TDef bodies that contain TAG_SUP / TAG_LAM / TAG_DP / TAG_BJ
   beyond the existing scalar-fold path.  Kernel allocates compound
   cells in BOOK_HEAP via aot_book_alloc, performs IC interactions
   (app_lam, app_sup, dup_sup, dup_lam, dup_num, dup_era), returns a
   root Term whose subtree the host migrates book->dyn before
   TCollapse walks the SUP-tree of leaves.  Mirrors the v1_church.wls
   CPU path. *)

(* Test: closed body containing a SUP at the head -- kernel builds
   the SUP cell, returns its root.  No IC reduction needed. *)
VerificationTest[
    TInit[];
    TDef["sup_const", TLam[ig, TSup[1, TNum[7], TNum[42]]]];
    Sort[FromTTerm /@ TCollapse[
        TAOTRun["sup_const", {TNum[0]}, Method -> "Metal"]]],
    {7, 42},
    TestID -> "Metal Z: SUP^1{NUM(7), NUM(42)} root + CPU collapse"
]

(* Test: Church-encoded boolToNum applied to SUP^1{T, F} reduces on
   GPU to a SUP^1{NUM(1)-tree, NUM(0)-tree}; CPU collapse yields {1,0}. *)
VerificationTest[
    TInit[];
    With[{
        T  = TLam[t1, TLam[f1, t1]],
        F  = TLam[t2, TLam[f2, f2]]
    },
        TDef["bool_to_num_sup",
            TLam[ig,
                TApp[TApp[TSup[1, T, F], TNum[1]], TNum[0]]
            ]
        ]
    ];
    Sort[FromTTerm /@ TCollapse[
        TAOTRun["bool_to_num_sup", {TNum[0]}, Method -> "Metal"]]],
    {0, 1},
    TestID -> "Metal Z: boolToNum[SUP^1{T, F}] -> {0, 1}"
]

(* Test: Church AND of two distinct-label SUP-typed variables.  After
   APP-SUP commutes the result has a SUP-tree of NUM leaves; the
   short-circuit means FALSE branches don't expand the second var. *)
VerificationTest[
    TInit[];
    With[{
        Tx = TLam[t3, TLam[f3, t3]], Fx = TLam[t4, TLam[f4, f4]],
        Ty = TLam[t5, TLam[f5, t5]], Fy = TLam[t6, TLam[f6, f6]]
    },
        TDef["sat_and_2",
            TLam[ig,
                TApp[TApp[
                    (* Church AND: (a (\x.x) (\y.F) b) *)
                    TApp[TApp[TApp[
                        TSup[1, Tx, Fx],
                        TLam[u1, u1]],
                        TLam[u2, TLam[t7, TLam[f7, f7]]]],
                        TSup[2, Ty, Fy]
                    ],
                    TNum[1]
                ], TNum[0]]
            ]
        ]
    ];
    MemberQ[FromTTerm /@ TCollapse[
        TAOTRun["sat_and_2", {TNum[0]}, Method -> "Metal"]], 1],
    True,
    TestID -> "Metal Z: SUP^1{T,F} AND SUP^2{T,F} has a 1 leaf (sat)"
]

(* === iter Z+1: parallel cnf+collapse shader =================================
   TAOTIcCollapse dispatches the static aot_ic_collapse PSO with grid =
   2^depth over a BOOK_HEAP-rooted SUP-tree (the kernel-1 result of an
   iter-Z TDef, captured pre-migration via THVM_AOT_METAL_KEEP_BOOK=1).
   Each thread decodes its tid into a binary path through the SUP-tree
   and drives its leaf to WHNF on-thread via per-thread IC inlines. *)

VerificationTest[
    SetEnvironment["THVM_AOT_METAL_KEEP_BOOK" -> "1"];
    TInit[];
    With[{
        T  = TLam[t1, TLam[f1, t1]],
        F  = TLam[t2, TLam[f2, f2]]
    },
        TDef["bool_to_num_collapse",
            TLam[ig,
                TApp[TApp[TSup[1, T, F], TNum[1]], TNum[0]]
            ]
        ]
    ];
    Sort[FromTTerm /@ TAOTIcCollapse[
        TAOTRun["bool_to_num_collapse", {TNum[0]}, Method -> "Metal"],
        1]],
    {0, 1},
    TestID -> "Metal Z+1: boolToNum[SUP^1{T,F}] via parallel collapse"
]

VerificationTest[
    SetEnvironment["THVM_AOT_METAL_KEEP_BOOK" -> "1"];
    TInit[];
    With[{
        Tx = TLam[t3, TLam[f3, t3]], Fx = TLam[t4, TLam[f4, f4]],
        Ty = TLam[t5, TLam[f5, t5]], Fy = TLam[t6, TLam[f6, f6]]
    },
        TDef["sat_and_2_collapse",
            TLam[ig,
                TApp[TApp[
                    TApp[TApp[TApp[
                        TSup[1, Tx, Fx],
                        TLam[u1, u1]],
                        TLam[u2, TLam[t7, TLam[f7, f7]]]],
                        TSup[2, Ty, Fy]
                    ],
                    TNum[1]
                ], TNum[0]]
            ]
        ]
    ];
    MemberQ[FromTTerm /@ TAOTIcCollapse[
        TAOTRun["sat_and_2_collapse", {TNum[0]}, Method -> "Metal"], 2], 1],
    True,
    TestID -> "Metal Z+1: SUP^1{T,F} AND SUP^2{T,F} parallel sat-check"
]
