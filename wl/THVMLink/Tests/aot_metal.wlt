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

(* === Method dispatcher rejects unknown spec === *)

VerificationTest[
    TInit[];
    TDef["add2", TLam[a, TLam[b, TOp2["+", a, b]]]];
    Quiet @ TAOTRun["add2", {TNum[1], TNum[2]}, Method -> "GPU2"],
    $Failed,
    TestID -> "Method -> unknown spec returns $Failed"
]
