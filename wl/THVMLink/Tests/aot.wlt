(* aot.wlt -- VerificationTest specs for the AOT WL surface.

   Covers TAOTEmit / TAOTCompile / TAOTRun / TAOTPath end-to-end.
   The runner (run.wls) loads the paclet first; this file is data.

   Each compile + run test isolates state via TInit so DEFS slot
   assignment is deterministic across runs.
*)

(* === Test program 1: trivial MAT-on-NUM dispatch ===
       TLam[x, MAT[<|0->NUM(42), 1->NUM(43)|>, default 99][x]]
       Verifies all three arms (NUM-arm 0, NUM-arm 1, default).
*)

VerificationTest[
    TInit[];
    TDef["aotTrivial",
      TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
                        TLam[TNum[99]]][x]]];
    Head[TAOTEmit["aotTrivial"]],
    String,
    TestID -> "TAOTEmit returns a String"
]

VerificationTest[
    TInit[];
    TDef["aotTrivial",
      TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
                        TLam[TNum[99]]][x]]];
    StringContainsQ[TAOTEmit["aotTrivial"], "FN_aotTrivial"],
    True,
    TestID -> "TAOTEmit output contains FN_<name>"
]

VerificationTest[
    TInit[];
    TDef["aotTrivial",
      TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
                        TLam[TNum[99]]][x]]];
    Head[TAOTCompile["aotTrivial"]],
    String,
    TestID -> "TAOTCompile returns a String dylib path"
]

VerificationTest[
    TInit[];
    TDef["aotTrivial",
      TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
                        TLam[TNum[99]]][x]]];
    TAOTCompile["aotTrivial"];
    With[{p = TAOTPath["aotTrivial"]},
        StringContainsQ[p, ".dylib"] || StringContainsQ[p, ".so"]],
    True,
    TestID -> "TAOTPath returns the stashed compiled path"
]

VerificationTest[
    MissingQ[TAOTPath["never_compiled_aot_def"]],
    True,
    TestID -> "TAOTPath returns Missing for an uncompiled name"
]

(* === Run end-to-end and inspect each NUM arm result === *)

VerificationTest[
    TInit[];
    TDef["aotTrivial",
      TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
                        TLam[TNum[99]]][x]]];
    TAOTCompile["aotTrivial"];
    TTermVal[TAOTRun["aotTrivial", 0]],
    42,
    TestID -> "TAOTRun trivial(0) -> NUM(42)"
]

VerificationTest[
    TInit[];
    TDef["aotTrivial",
      TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
                        TLam[TNum[99]]][x]]];
    TAOTCompile["aotTrivial"];
    TTermVal[TAOTRun["aotTrivial", 1]],
    43,
    TestID -> "TAOTRun trivial(1) -> NUM(43)"
]

VerificationTest[
    TInit[];
    TDef["aotTrivial",
      TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
                        TLam[TNum[99]]][x]]];
    TAOTCompile["aotTrivial"];
    TTermVal[TAOTRun["aotTrivial", 99]],
    99,
    TestID -> "TAOTRun trivial(99) -> NUM(99) (default arm)"
]

(* === Cache check: same source -> same path === *)

VerificationTest[
    TInit[];
    TDef["aotTrivial",
      TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
                        TLam[TNum[99]]][x]]];
    Module[{p1 = TAOTCompile["aotTrivial"],
            p2},
      TInit[];
      TDef["aotTrivial",
        TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
                          TLam[TNum[99]]][x]]];
      p2 = TAOTCompile["aotTrivial"];
      p1 == p2
    ],
    True,
    TestID -> "TAOTCompile cache hit: same source -> same path"
]

(* === TAOTRun without prior TAOTCompile -> $Failed === *)

VerificationTest[
    TInit[];
    TAOTRun["never_compiled_aot_def", 0],
    $Failed,
    TestID -> "TAOTRun on never-compiled name returns $Failed"
]

(* === fib(N) end-to-end: exercises value-position TOp2 emit + recursion ===
   Iter C-D landed TAG_OP2/TAG_APP in value position; the recursive
   args here (k - 1, k - 2) hit that path.  NUM in / NUM out -- fits
   the dylib model (no cross-heap CTR marshalling needed).
*)

VerificationTest[
    TInit[];
    TDef["fib",
      TLam[n, TMatChain[
        <|0 -> TNum[0], 1 -> TNum[1]|>,
        TLam[k, TOp2["+",
          TApp[TRef["fib"], TOp2["-", k, TNum[1]]],
          TApp[TRef["fib"], TOp2["-", k, TNum[2]]]]]
      ][n]]];
    TAOTCompile["fib"];
    TTermVal[TAOTRun["fib", 8]],
    21,
    TestID -> "TAOTRun fib(8) -> 21 (value-pos OP2 + recursion)"
]

VerificationTest[
    TInit[];
    TDef["fib",
      TLam[n, TMatChain[
        <|0 -> TNum[0], 1 -> TNum[1]|>,
        TLam[k, TOp2["+",
          TApp[TRef["fib"], TOp2["-", k, TNum[1]]],
          TApp[TRef["fib"], TOp2["-", k, TNum[2]]]]]
      ][n]]];
    TAOTCompile["fib"];
    TTermVal[TAOTRun["fib", 15]],
    610,
    TestID -> "TAOTRun fib(15) -> 610 (deeper recursion)"
]

(* === CTR-input round-trip: shared-heap dylib reads host's CTRs ===
   Phase 6 iter A made CURRENT_CTX shared between host and dylib so
   Term locs into the host heap resolve correctly inside the AOT'd
   code.  tree_sum is the canonical leaf{}/node{} workload.  Each
   test feeds a host-built CTR tree, sums the leaves via AOT, and
   checks the NUM result.
*)

VerificationTest[
    TInit[];
    TDef["tsum",
      TLam[t, TMatChain[
        <|0 -> TLam[v, v],
          1 -> TLam[l, TLam[r, TOp2["+",
            TApp[TRef["tsum"], l],
            TApp[TRef["tsum"], r]]]]
        |>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["tsum"];
    TTermVal[TAOTRun["tsum", TCtr[0, TNum[42]]]],
    42,
    TestID -> "TAOTRun tsum leaf{42} -> 42 (CTR input, NUM output)"
]

VerificationTest[
    TInit[];
    TDef["tsum",
      TLam[t, TMatChain[
        <|0 -> TLam[v, v],
          1 -> TLam[l, TLam[r, TOp2["+",
            TApp[TRef["tsum"], l],
            TApp[TRef["tsum"], r]]]]
        |>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["tsum"];
    TTermVal[TAOTRun["tsum",
      TCtr[1, TCtr[0, TNum[10]], TCtr[0, TNum[20]]]]],
    30,
    TestID -> "TAOTRun tsum node{leaf{10}, leaf{20}} -> 30"
]

VerificationTest[
    TInit[];
    TDef["tsum",
      TLam[t, TMatChain[
        <|0 -> TLam[v, v],
          1 -> TLam[l, TLam[r, TOp2["+",
            TApp[TRef["tsum"], l],
            TApp[TRef["tsum"], r]]]]
        |>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["tsum"];
    Module[{leaf, node, big},
      leaf[v_] := TCtr[0, TNum[v]];
      node[a_, b_] := TCtr[1, a, b];
      big = node[
        node[node[leaf[1], leaf[2]], node[leaf[3], leaf[4]]],
        node[node[leaf[5], leaf[6]], node[leaf[7], leaf[8]]]];
      TTermVal[TAOTRun["tsum", big]]],
    36,
    TestID -> "TAOTRun tsum sum(1..8) over depth-3 tree -> 36"
]

(* === CTR-output round-trip: AOT'd def builds a CTR tree, host
   feeds it to a SECOND AOT'd def to sum.  Both share the host's
   heap, so the tree built by build1 is directly readable by
   tsum without any marshalling.  Exercises the output side of
   shared-heap AOT.

   build1(d) returns a balanced binary tree of depth d with all
   leaves carrying NUM(5).  tsum sums leaves -> 5 * 2^d.
*)

VerificationTest[
    TInit[];
    TDef["build1",
      TLam[d, TMatChain[
        <|0 -> TCtr[0, TNum[5]]|>,
        TLam[dd, TCtr[1,
          TApp[TRef["build1"], TOp2["-", dd, TNum[1]]],
          TApp[TRef["build1"], TOp2["-", dd, TNum[1]]]]]
      ][d]]];
    TDef["tsum",
      TLam[t, TMatChain[
        <|0 -> TLam[v, v],
          1 -> TLam[l, TLam[r, TOp2["+",
            TApp[TRef["tsum"], l],
            TApp[TRef["tsum"], r]]]]
        |>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["build1"];
    TAOTCompile["tsum"];
    TTermVal[TAOTRun["tsum", TAOTRun["build1", 3]]],
    40,
    TestID -> "TAOTRun build1(3) -> tsum -> 40 (5 * 2^3)"
]

VerificationTest[
    TInit[];
    TDef["build1",
      TLam[d, TMatChain[
        <|0 -> TCtr[0, TNum[5]]|>,
        TLam[dd, TCtr[1,
          TApp[TRef["build1"], TOp2["-", dd, TNum[1]]],
          TApp[TRef["build1"], TOp2["-", dd, TNum[1]]]]]
      ][d]]];
    TDef["tsum",
      TLam[t, TMatChain[
        <|0 -> TLam[v, v],
          1 -> TLam[l, TLam[r, TOp2["+",
            TApp[TRef["tsum"], l],
            TApp[TRef["tsum"], r]]]]
        |>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["build1"];
    TAOTCompile["tsum"];
    TTermVal[TAOTRun["tsum", TAOTRun["build1", 8]]],
    1280,
    TestID -> "TAOTRun build1(8) -> tsum -> 1280 (5 * 2^8)"
]

(* === PRI callback round-trip: AOT'd def fires a host-registered
   WL callback via TPri and returns a continuation.  Phase 6 iter A
   shares CURRENT_CTX so the dylib's prim_pri queues into the host's
   queue / dispatches via host's foreign-fn binding; iter D adds
   TAG_PRI value-position emit + -DTHVM_HAS_WL_BRIDGE
   -undefined dynamic_lookup to the dylib compile so the dylib's
   thvm_pri_wl_invoke_returning extern resolves to the host's
   strong override (rather than the local weak no-op stub).
*)

VerificationTest[
    TInit[];
    Module[{captured = {}, cb},
      cb[v_] := AppendTo[captured, TTermVal @ v];
      TDef["pri_lit", TLam[x, TPri[cb, TNum[42], x]]];
      TAOTCompile["pri_lit"];
      (* TWnf needed: pri_lit's body IS a TPri redex (App-of-PRI),
         so TAOTRun returns the unreduced redex.  TWnf reduces it,
         which fires the PRI primitive that enqueues the (slot, val)
         pair for the WL callback.  This is the EXPLICIT side-effect
         pattern -- TWnf isn't a no-op for value-expression bodies. *)
      TWnf[TAOTRun["pri_lit", 7]];
      TPriDrain[];
      captured],
    {42},
    TestID -> "TAOTRun TPri[cb, NUM(42), x] -> cb fires with 42"
]

VerificationTest[
    TInit[];
    Module[{captured = {}, cb, results = {}},
      cb[v_] := AppendTo[captured, TTermVal @ v];
      TDef["pri_lit", TLam[x, TPri[cb, TNum[42], x]]];
      TAOTCompile["pri_lit"];
      Do[
        AppendTo[results, TTermVal @ TWnf[TAOTRun["pri_lit", k]]];
        TPriDrain[],
        {k, {3, 7, 11}}];
      {captured, results}],
    {{42, 42, 42}, {3, 7, 11}},
    TestID -> "TAOTRun TPri repeated calls -- cb captures 42 each time, cont returns x"
]

(* === Multi-arg TAOTRun: list form passes up to 4 args ===
   Phase 6 iter E: TAOTRun[name, {arg0, arg1, ...}] dispatches to
   thvm_wl_aot_run4 which passes 4 input slots to the dylib's
   aot_program_<name>_run.  Trailing slots default to 0; defs
   ignore unused args.  Unlocks 2/3-arg defs (build, ack, etc.).
*)

VerificationTest[
    TInit[];
    TDef["add2", TLam[a, TLam[b, TOp2["+", a, b]]]];
    TAOTCompile["add2"];
    TTermVal[TAOTRun["add2", {3, 4}]],
    7,
    TestID -> "TAOTRun add2(3, 4) -> 7 (multi-arg via list)"
]

VerificationTest[
    TInit[];
    TDef["fst2", TLam[a, TLam[b, a]]];
    TAOTCompile["fst2"];
    TTermVal[TAOTRun["fst2", {7, 99}]],
    7,
    TestID -> "TAOTRun fst2(7, 99) -> 7 (returns first arg)"
]

VerificationTest[
    TInit[];
    TDef["snd2", TLam[a, TLam[b, b]]];
    TAOTCompile["snd2"];
    TTermVal[TAOTRun["snd2", {7, 99}]],
    99,
    TestID -> "TAOTRun snd2(7, 99) -> 99 (returns second arg)"
]

(* 2-arg build that takes a depth + leaf-value, returns a CTR tree.
   Then sums via tsum -- exercises the multi-arg + CTR-output round
   trip in one go.  Result has DP-wrapped leaves (auto-dup of x);
   TCnf forces them through DUP-NUM before tsum reads. *)

VerificationTest[
    TInit[];
    TDef["build",
      TLam[d, TLam[x, TMatChain[
        <|0 -> TCtr[0, x]|>,
        TLam[dd, TCtr[1,
          TApp[TApp[TRef["build"], TOp2["-", dd, TNum[1]]], x],
          TApp[TApp[TRef["build"], TOp2["-", dd, TNum[1]]], x]]]
      ][d]]]];
    TDef["tsum",
      TLam[t, TMatChain[
        <|0 -> TLam[v, v],
          1 -> TLam[l, TLam[r, TOp2["+",
            TApp[TRef["tsum"], l],
            TApp[TRef["tsum"], r]]]]
        |>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["build"];
    TAOTCompile["tsum"];
    TTermVal[TAOTRun["tsum", TCnf[TAOTRun["build", {3, 7}]]]],
    56,
    TestID -> "TAOTRun build(3, 7) -> tsum -> 56 (7*2^3, 2-arg + DP*)"
]

VerificationTest[
    TInit[];
    TDef["build",
      TLam[d, TLam[x, TMatChain[
        <|0 -> TCtr[0, x]|>,
        TLam[dd, TCtr[1,
          TApp[TApp[TRef["build"], TOp2["-", dd, TNum[1]]], x],
          TApp[TApp[TRef["build"], TOp2["-", dd, TNum[1]]], x]]]
      ][d]]]];
    TDef["tsum",
      TLam[t, TMatChain[
        <|0 -> TLam[v, v],
          1 -> TLam[l, TLam[r, TOp2["+",
            TApp[TRef["tsum"], l],
            TApp[TRef["tsum"], r]]]]
        |>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["build"];
    TAOTCompile["tsum"];
    TTermVal[TAOTRun["tsum", TCnf[TAOTRun["build", {6, 4}]]]],
    256,
    TestID -> "TAOTRun build(6, 4) -> tsum -> 256 (4*2^6)"
]

(* === CTR arity 3-4 emit ===
   Phase 6 iter F: aot_emit_value_expr now emits aot_make_ctr3 /
   aot_make_ctr4 / aot_make_ctrn (generic up to HVM4's 16-cap)
   instead of stubbing.  Exercises both reading (arm-handler peels
   N LAMs binding term_ctr_at(dv, i) for i in 0..N-1) and writing
   (def body returns TCtr[label, c0, c1, c2, ...]).
*)

VerificationTest[
    TInit[];
    TDef["sum_triple",
      TLam[t, TMatChain[
        <|0 -> TLam[a, TLam[b, TLam[c,
            TOp2["+", a, TOp2["+", b, c]]]]]|>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["sum_triple"];
    TTermVal[TAOTRun["sum_triple",
      TCtr[0, TNum[10], TNum[20], TNum[30]]]],
    60,
    TestID -> "TAOTRun sum_triple{10,20,30} -> 60 (CTR arity 3 read)"
]

VerificationTest[
    TInit[];
    TDef["sum_quad",
      TLam[t, TMatChain[
        <|0 -> TLam[a, TLam[b, TLam[c, TLam[d,
            TOp2["+", a, TOp2["+", b, TOp2["+", c, d]]]]]]]|>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["sum_quad"];
    TTermVal[TAOTRun["sum_quad",
      TCtr[0, TNum[1], TNum[2], TNum[3], TNum[4]]]],
    10,
    TestID -> "TAOTRun sum_quad{1,2,3,4} -> 10 (CTR arity 4 read)"
]

VerificationTest[
    TInit[];
    TDef["mk_triple", TLam[a, TLam[b, TLam[c, TCtr[0, a, b, c]]]]];
    TDef["sum_triple",
      TLam[t, TMatChain[
        <|0 -> TLam[a, TLam[b, TLam[c,
            TOp2["+", a, TOp2["+", b, c]]]]]|>,
        TLam[TEra[]]
      ][t]]];
    TAOTCompile["mk_triple"];
    TAOTCompile["sum_triple"];
    TTermVal[TAOTRun["sum_triple",
      TCnf[TAOTRun["mk_triple", {7, 8, 9}]]]],
    24,
    TestID -> "TAOTRun mk_triple(7,8,9) -> sum_triple -> 24 (CTR arity 3 round-trip)"
]

(* === HOFs: TAG_LAM in value position ===
   Phase 6 iter G adds TAG_LAM emit so a def can return a closed
   lambda.  The arm-handler kind logic now distinguishes:
     - CTR arms (kind=1):   peel LAMs binding to ctr_at(i)
     - NUM arms (kind=0):   no peel, handler IS the value (HVM4
                            MAT-NUM rule)
     - default arm (kind=2): peel ONE LAM binding to dv (MAT-MIS
                            applies the fallback to the matched
                            value)
   Dead-arm pruning was tightened to require 2+ nested LAMs in a
   handler before assuming CTR-only dispatch -- single TLam can
   be either a CTR destructure or a HOF return.

   mkfn dispatches on a NUM and returns DIFFERENT lambdas per arm,
   then the host applies the result via TApp.
*)

VerificationTest[
    TInit[];
    TDef["mkfn", TLam[k, TMatChain[
      <|0 -> TLam[x, x],
        1 -> TLam[x, TNum[5]]|>,
      TLam[TLam[x, x]]
    ][k]]];
    TAOTCompile["mkfn"];
    {TTermVal[TWnf[TApp[TAOTRun["mkfn", 0], TNum[7]]]],
     TTermVal[TWnf[TApp[TAOTRun["mkfn", 1], TNum[7]]]],
     TTermVal[TWnf[TApp[TAOTRun["mkfn", 99], TNum[11]]]]},
    {7, 5, 11},
    TestID -> "TAOTRun mkfn -- HOF returning identity / const / default-id per NUM key"
]

(* === TAG_SUP value-position emit ===
   Phase 6 iter H adds TAG_SUP construction to the AOT'd code.
   IC's superposition `&L{a, b}` is a 2-arity heap-backed term;
   used by non-linear duplication (DUP-SUP commute) and ATP-style
   enumeration.  TAG_DUP (the explicit binder form) is rare in
   typical defs since multi-use bound vars produce TAG_DP0/DP1
   directly via the heap-time TLam machinery -- deferred.

   Test: a def that returns &7{ x, NUM(42) } given NUM x.
*)

VerificationTest[
    TInit[];
    TDef["mksup", TLam[x, TSup[7, x, TNum[42]]]];
    TAOTCompile["mksup"];
    Module[{r = TAOTRun["mksup", 99]},
      {TTermTag[r], TTermExt[r]}],
    {6, 7},  (* TAG_SUP = 6, label = 7 *)
    TestID -> "TAOTRun mksup(99) -> SUP with label 7 (TAG_SUP value-pos emit)"
]

(* === Classical recursive programs ===
   Phase 6 iter J onward: cover the full HVM combinator surface
   on actual programs (factorial, Ackermann, NAT-CTR Fibonacci,
   list ops via cons/nil).  The iter-J cnf-fast-path lets DP*-
   wrapped recursive args reach a NUM/CTR head before tag-check
   (DUP-NUM and DUP-CTR fire under cnf, not wnf).  The arm-handler
   recursion lets nested matches dispatch correctly (ack's outer m
   match defaults into an inner n match).
*)

VerificationTest[
    TInit[];
    TDef["fact", TLam[n, TMatChain[
      <|0 -> TNum[1]|>,
      TLam[k, TOp2["*", k, TApp[TRef["fact"], TOp2["-", k, TNum[1]]]]]
    ][n]]];
    TAOTCompile["fact"];
    {TTermVal[TAOTRun["fact", 0]],
     TTermVal[TAOTRun["fact", 5]],
     TTermVal[TAOTRun["fact", 8]],
     TTermVal[TAOTRun["fact", 10]]},
    {1, 120, 40320, 3628800},
    TestID -> "TAOTRun fact -- 0/5/8/10 = 1/120/40320/3628800"
]

VerificationTest[
    TInit[];
    TDef["ack", TLam[m, TLam[n, TMatChain[
      <|0 -> TOp2["+", n, TNum[1]]|>,
      TLam[mp, TMatChain[
        <|0 -> TApp[TApp[TRef["ack"], TOp2["-", mp, TNum[1]]], TNum[1]]|>,
        TLam[np, TApp[TApp[TRef["ack"], TOp2["-", mp, TNum[1]]],
                      TApp[TApp[TRef["ack"], mp], TOp2["-", np, TNum[1]]]]]
      ][n]]
    ][m]]]];
    TAOTCompile["ack"];
    {TTermVal[TAOTRun["ack", {0, 5}]],
     TTermVal[TAOTRun["ack", {1, 1}]],
     TTermVal[TAOTRun["ack", {2, 2}]],
     TTermVal[TAOTRun["ack", {3, 3}]],
     TTermVal[TAOTRun["ack", {3, 5}]]},
    {6, 3, 7, 61, 253},
    TestID -> "TAOTRun ack -- (0,5)/(1,1)/(2,2)/(3,3)/(3,5) = 6/3/7/61/253 (nested matches)"
]

VerificationTest[
    TInit[];
    (* fib_nat: NAT input, NUM output. *)
    TDef["fib_nat", TLam[n, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[k, TMatChain[
          <|0 -> TNum[1],
            1 -> TLam[kp, TOp2["+",
              TApp[TRef["fib_nat"], TCtr[1, kp]],
              TApp[TRef["fib_nat"], kp]]]|>,
          TLam[TEra[]]
        ][k]]|>,
      TLam[TEra[]]
    ][n]]];
    TAOTCompile["fib_nat"];
    Module[{nat},
      nat[n_] := Nest[Function[k, TCtr[1, k]], TCtr[0], n];
      Table[TTermVal[TAOTRun["fib_nat", nat[k]]], {k, 0, 10}]],
    {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55},
    TestID -> "TAOTRun fib_nat -- NAT input -> NUM Fibonacci sequence (0..10)"
]

(* nat_len: convert NAT input -> NUM count of SUCs. *)
VerificationTest[
    TInit[];
    TDef["nat_len", TLam[n, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[p, TOp2["+", TApp[TRef["nat_len"], p], TNum[1]]]|>,
      TLam[TEra[]]
    ][n]]];
    TAOTCompile["nat_len"];
    Module[{nat},
      nat[n_] := Nest[Function[k, TCtr[1, k]], TCtr[0], n];
      {TTermVal[TAOTRun["nat_len", nat[0]]],
       TTermVal[TAOTRun["nat_len", nat[5]]],
       TTermVal[TAOTRun["nat_len", nat[20]]]}],
    {0, 5, 20},
    TestID -> "TAOTRun nat_len -- SUC^N ZER -> N (CTR-counting recursion)"
]

(* === List ops via cons/nil CTRs ===
   Standard ADT shape: nil = TCtr[0], cons(h, t) = TCtr[1, h, t].
   Single-arg defs that walk the spine and accumulate.  Exercises
   CTR-arm peel binding 2 LAMs (TLam[h, TLam[t, ...]]) -- the
   classical List destructure pattern.
*)

VerificationTest[
    TInit[];
    TDef["llen", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", TNum[1], TApp[TRef["llen"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["llen"];
    Module[{nil = TCtr[0], cons, fromList},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[lst_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ lst]];
      {TTermVal[TAOTRun["llen", nil]],
       TTermVal[TAOTRun["llen", fromList[{1, 2, 3}]]],
       TTermVal[TAOTRun["llen", fromList[Range[10]]]],
       TTermVal[TAOTRun["llen", fromList[Range[50]]]]}],
    {0, 3, 10, 50},
    TestID -> "TAOTRun llen -- list length via cons/nil destructure"
]

VerificationTest[
    TInit[];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["lsum"];
    Module[{nil = TCtr[0], cons, fromList},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[lst_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ lst]];
      {TTermVal[TAOTRun["lsum", nil]],
       TTermVal[TAOTRun["lsum", fromList[{1, 2, 3}]]],
       TTermVal[TAOTRun["lsum", fromList[Range[10]]]],
       TTermVal[TAOTRun["lsum", fromList[Range[100]]]]}],
    {0, 6, 55, 5050},
    TestID -> "TAOTRun lsum -- list sum via cons/nil destructure"
]

(* lmap: HOF that takes a function (via TRef) and a list, applies f
   to each head.  Exercises TApp of a passed-in arg ref combined
   with cons-cell destructure and self-recursion in the spine. *)
VerificationTest[
    TInit[];
    TDef["doubler",  TLam[xx, TOp2["+", xx, xx]]];
    TDef["plus_one", TLam[yy, TOp2["+", yy, TNum[1]]]];
    TDef["lmap", TLam[f, TLam[xs, TMatChain[
      <|0 -> TCtr[0],
        1 -> TLam[h, TLam[t,
          TCtr[1, TApp[f, h], TApp[TApp[TRef["lmap"], f], t]]]]|>,
      TLam[TEra[]]
    ][xs]]]];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["doubler"];
    TAOTCompile["plus_one"];
    TAOTCompile["lmap"];
    TAOTCompile["lsum"];
    Module[{nil = TCtr[0], cons, fromList, lst},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[xs_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ xs]];
      lst = fromList[Range[5]];
      {TTermVal[TAOTRun["lsum",
        TCnf[TAOTRun["lmap", {TRef["doubler"], lst}]]]],
       TTermVal[TAOTRun["lsum",
        TCnf[TAOTRun["lmap", {TRef["plus_one"], lst}]]]]}],
    {30, 20},  (* 2+4+6+8+10 = 30; 2+3+4+5+6 = 20 *)
    TestID -> "TAOTRun lmap doubler/plus_one over [1..5] -> sums 30/20 (HOF)"
]

(* power: 2-arg recursion with self-call inside an OP2 multiply. *)
VerificationTest[
    TInit[];
    TDef["power", TLam[a, TLam[n, TMatChain[
      <|0 -> TNum[1]|>,
      TLam[k, TOp2["*", a,
        TApp[TApp[TRef["power"], a], TOp2["-", k, TNum[1]]]]]
    ][n]]]];
    TAOTCompile["power"];
    {TTermVal[TAOTRun["power", {2, 0}]],
     TTermVal[TAOTRun["power", {2, 8}]],
     TTermVal[TAOTRun["power", {3, 5}]],
     TTermVal[TAOTRun["power", {7, 4}]]},
    {1, 256, 243, 2401},
    TestID -> "TAOTRun power -- a^n: 2^0/2^8/3^5/7^4 = 1/256/243/2401"
]

(* Mutual recursion: even / odd defined in terms of each other.
   Cross-def TRef dispatch falls back to wnf for the other def --
   correct, but slower than self-recursion.  Tests that the AOT'd
   even can call into odd (the other AOT'd def) via TRef without
   collapsing. *)
(* repeat(n, x): n copies of x.  Builds list via cons + recursion. *)
VerificationTest[
    TInit[];
    TDef["repeat", TLam[n, TLam[xv, TMatChain[
      <|0 -> TCtr[0]|>,
      TLam[k, TCtr[1, xv,
        TApp[TApp[TRef["repeat"], TOp2["-", k, TNum[1]]], xv]]]
    ][n]]]];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["repeat"];
    TAOTCompile["lsum"];
    {TTermVal[TAOTRun["lsum", TCnf[TAOTRun["repeat", {0, 7}]]]],
     TTermVal[TAOTRun["lsum", TCnf[TAOTRun["repeat", {7, 3}]]]],
     TTermVal[TAOTRun["lsum", TCnf[TAOTRun["repeat", {10, 5}]]]]},
    {0, 21, 50},
    TestID -> "TAOTRun repeat -- (0,7)/(7,3)/(10,5) summed = 0/21/50"
]

(* range(n): [0, 1, ..., n-1] -- accumulator builder. *)
VerificationTest[
    TInit[];
    TDef["range_aux", TLam[n, TLam[acc, TMatChain[
      <|0 -> acc|>,
      TLam[k, TApp[TApp[TRef["range_aux"],
        TOp2["-", k, TNum[1]]],
        TCtr[1, TOp2["-", k, TNum[1]], acc]]]
    ][n]]]];
    TDef["range", TLam[n, TApp[TApp[TRef["range_aux"], n], TCtr[0]]]];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["range_aux"];
    TAOTCompile["range"];
    TAOTCompile["lsum"];
    {TTermVal[TAOTRun["lsum", TCnf[TAOTRun["range", 0]]]],
     TTermVal[TAOTRun["lsum", TCnf[TAOTRun["range", 5]]]],
     TTermVal[TAOTRun["lsum", TCnf[TAOTRun["range", 10]]]],
     TTermVal[TAOTRun["lsum", TCnf[TAOTRun["range", 20]]]]},
    {0, 10, 45, 190},  (* sum(0..n-1) = n*(n-1)/2 *)
    TestID -> "TAOTRun range -- range(0/5/10/20) summed = 0/10/45/190"
]

(* reverse via accumulator -- exercises tail-recursive list spine. *)
VerificationTest[
    TInit[];
    TDef["revAux", TLam[xs, TLam[acc, TMatChain[
      <|0 -> acc,
        1 -> TLam[h, TLam[t,
          TApp[TApp[TRef["revAux"], t], TCtr[1, h, acc]]]]|>,
      TLam[TEra[]]
    ][xs]]]];
    TDef["reverse", TLam[xs, TApp[TApp[TRef["revAux"], xs], TCtr[0]]]];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["revAux"];
    TAOTCompile["reverse"];
    TAOTCompile["lsum"];
    Module[{nil = TCtr[0], cons, fromList},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[xs_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ xs]];
      {TTermVal[TAOTRun["lsum", TCnf[TAOTRun["reverse", nil]]]],
       TTermVal[TAOTRun["lsum", TCnf[TAOTRun["reverse", fromList[Range[5]]]]]],
       TTermVal[TAOTRun["lsum", TCnf[TAOTRun["reverse", fromList[Range[20]]]]]]}],
    {0, 15, 210},
    TestID -> "TAOTRun reverse -- nil/[1..5]/[1..20] reversed; sum-invariant"
]

(* === Victor Taelin's Bend2 par_* benchmark series ===
   Implementing the integer-only / non-floating subset that fits
   our current OP_ set (ADD / SUB / MUL / EQ / LT).  Programs
   needing OP_XOR / OP_DIV / OP_MOD / floats / random are
   deferred until those op codes land.
*)

(* par_pow2_u32: pow2(0) = 1; pow2(n) = pow2(n-1) + pow2(n-1).
   Sum-tree shape; total work is 2^n.  Same workload as Bend2's
   reference, just expressed via NUM dispatch instead of NAT. *)
VerificationTest[
    TInit[];
    TDef["pow2", TLam[n, TMatChain[
      <|0 -> TNum[1]|>,
      TLam[k, TOp2["+",
        TApp[TRef["pow2"], TOp2["-", k, TNum[1]]],
        TApp[TRef["pow2"], TOp2["-", k, TNum[1]]]]]
    ][n]]];
    TAOTCompile["pow2"];
    {TTermVal[TAOTRun["pow2", 0]],
     TTermVal[TAOTRun["pow2", 5]],
     TTermVal[TAOTRun["pow2", 10]],
     TTermVal[TAOTRun["pow2", 15]]},
    {1, 32, 1024, 32768},
    TestID -> "TAOTRun par_pow2_u32 -- 2^0/5/10/15 = 1/32/1024/32768"
]

(* par_quadtree: 4-ary tree.  build_quad(d) returns a depth-d
   quadtree of NUM(1) leaves.  qsum sums leaves -> 4^d.
   Exercises CTR arity 4 + 4-LAM destructure + 4-way recursive
   sum (via two layers of OP2 ADD). *)
VerificationTest[
    TInit[];
    TDef["build_quad", TLam[d, TMatChain[
      <|0 -> TCtr[0, TNum[1]]|>,
      TLam[k, Module[{rec = TApp[TRef["build_quad"], TOp2["-", k, TNum[1]]]},
        TCtr[1, rec, rec, rec, rec]]]
    ][d]]];
    TDef["qsum", TLam[t, TMatChain[
      <|0 -> TLam[v, v],
        1 -> TLam[a, TLam[b, TLam[c, TLam[dd,
          TOp2["+", TOp2["+", TApp[TRef["qsum"], a], TApp[TRef["qsum"], b]],
                    TOp2["+", TApp[TRef["qsum"], c], TApp[TRef["qsum"], dd]]]]]]]|>,
      TLam[TEra[]]
    ][t]]];
    TAOTCompile["build_quad"];
    TAOTCompile["qsum"];
    {TTermVal[TAOTRun["qsum", TCnf[TAOTRun["build_quad", 1]]]],
     TTermVal[TAOTRun["qsum", TCnf[TAOTRun["build_quad", 2]]]],
     TTermVal[TAOTRun["qsum", TCnf[TAOTRun["build_quad", 3]]]]},
    {4, 16, 64},
    TestID -> "TAOTRun par_quadtree -- depth 1/2/3 -> 4/16/64 (4^d leaves of 1)"
]

(* par_merkle: build a binary tree with leaves carrying derived ints
   (build_inc(d, base) gives leaves base*2^d + 0..2^d-1 across d
   levels), then hash by XOR-combining children with a 7-bit rotate
   on the left.  Exercises new OP codes: OP_XOR, OP_OR, OP_SHL,
   OP_SHR.  Produces deterministic NUM hashes. *)

VerificationTest[
    TInit[];
    TDef["build_inc", TLam[d, TLam[base, TMatChain[
      <|0 -> TCtr[0, base]|>,
      TLam[k, TCtr[1,
        TApp[TApp[TRef["build_inc"], TOp2["-", k, TNum[1]]],
             TOp2["*", base, TNum[2]]],
        TApp[TApp[TRef["build_inc"], TOp2["-", k, TNum[1]]],
             TOp2["+", TOp2["*", base, TNum[2]], TNum[1]]]]]
    ][d]]]];
    TDef["mhash", TLam[t, TMatChain[
      <|0 -> TLam[v, v],
        1 -> TLam[l, TLam[r, TOp2["^",
          TOp2["|", TOp2["<<", TApp[TRef["mhash"], l], TNum[7]],
                    TOp2[">>", TApp[TRef["mhash"], l], TNum[25]]],
          TApp[TRef["mhash"], r]]]]|>,
      TLam[TEra[]]
    ][t]]];
    TAOTCompile["build_inc"];
    TAOTCompile["mhash"];
    {TTermVal[TAOTRun["mhash", TCnf[TAOTRun["build_inc", {1, 0}]]]],
     TTermVal[TAOTRun["mhash", TCnf[TAOTRun["build_inc", {3, 0}]]]],
     TTermVal[TAOTRun["mhash", TCnf[TAOTRun["build_inc", {5, 0}]]]]},
    {1, 114695, 4026531870},
    TestID -> "TAOTRun par_merkle -- XOR-rotate hash on inc-leaved trees (deterministic)"
]

(* par_sort_merge (merge half): merge two sorted cons-lists.
   Not full mergesort -- the split + recurse + merge composition
   blows the heap on auto-dup of the spine.  But the merge primitive
   itself works: 3-level nested match (a / b / cmp) exercising the
   App-of-Mat recursion in arm-handler + the new OP_LT comparison. *)
VerificationTest[
    TInit[];
    TDef["lmerge", TLam[a, TLam[b, TMatChain[
       <|
         0 -> b,
         1 -> TLam[ah, TLam[at, TApp[TMatChain[
           <|
             0 -> TCtr[1, ah, at],
             1 -> TLam[bh, TLam[bt, TApp[TMatChain[
               <|0 -> TCtr[1, bh,
                   TApp[TApp[TRef["lmerge"], TCtr[1, ah, at]], bt]]|>,
               TLam[ig, TCtr[1, ah,
                 TApp[TApp[TRef["lmerge"], at], TCtr[1, bh, bt]]]]
             ], TOp2["<", ah, bh]]]]
           |>,
           TLam[TEra[]]
         ], b]]]
       |>,
       TLam[TEra[]]
    ][a]]]];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0], 1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["lmerge"];
    TAOTCompile["lsum"];
    Module[{nil = TCtr[0], cons, fromList, lmerge2sum},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[xs_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ xs]];
      lmerge2sum[xs_, ys_] := TTermVal[TAOTRun["lsum",
          TCnf[TAOTRun["lmerge", {fromList[xs], fromList[ys]}]]]];
      {lmerge2sum[{1}, {2}],
       lmerge2sum[{2}, {1}],
       lmerge2sum[{1, 3}, {2, 4}],
       lmerge2sum[{1, 3, 5}, {2, 4, 6}],
       lmerge2sum[{1, 4, 7}, {2, 3, 9}]}],
    {3, 3, 10, 21, 26},
    TestID -> "TAOTRun par_sort_merge (merge primitive) -- 3-level nested match + OP_LT"
]

(* === More numeric programs exercising the new OP codes === *)

(* GCD via Euclidean: gcd(a, b) -- uses OP_MOD. *)
VerificationTest[
    TInit[];
    TDef["gcd", TLam[a, TLam[b, TMatChain[
      <|0 -> a|>,
      TLam[ig, TApp[TApp[TRef["gcd"], b], TOp2["%", a, b]]]
    ][b]]]];
    TAOTCompile["gcd"];
    {TTermVal[TCnf[TAOTRun["gcd", {48, 18}]]],
     TTermVal[TCnf[TAOTRun["gcd", {100, 75}]]],
     TTermVal[TCnf[TAOTRun["gcd", {17, 13}]]],
     TTermVal[TCnf[TAOTRun["gcd", {123, 456}]]]},
    {6, 25, 1, 3},
    TestID -> "TAOTRun gcd Euclidean -- (48,18)/(100,75)/(17,13)/(123,456)"
]

(* Collatz step count: collatz(n, c) iterates 3n+1 / n/2 until n=1. *)
VerificationTest[
    TInit[];
    TDef["collatz", TLam[n, TLam[c, TMatChain[
      <|1 -> c|>,
      TLam[ig, TApp[TApp[TRef["collatz_step"], n], c]]
    ][n]]]];
    TDef["collatz_step", TLam[n, TLam[c, TApp[TMatChain[
      <|0 -> TApp[TApp[TRef["collatz"], TOp2["/", n, TNum[2]]],
                  TOp2["+", c, TNum[1]]]|>,
      TLam[ig, TApp[TApp[TRef["collatz"],
                          TOp2["+", TOp2["*", n, TNum[3]], TNum[1]]],
                          TOp2["+", c, TNum[1]]]]
    ], TOp2["%", n, TNum[2]]]]]];
    TAOTCompile["collatz"];
    TAOTCompile["collatz_step"];
    {TTermVal[TCnf[TAOTRun["collatz", {1, 0}]]],
     TTermVal[TCnf[TAOTRun["collatz", {2, 0}]]],
     TTermVal[TCnf[TAOTRun["collatz", {7, 0}]]],
     TTermVal[TCnf[TAOTRun["collatz", {27, 0}]]]},
    {0, 1, 16, 111},
    TestID -> "TAOTRun collatz step-count -- 1/2/7/27 = 0/1/16/111"
]

(* Sum of decimal digits via OP_MOD + OP_DIV. *)
VerificationTest[
    TInit[];
    TDef["digsum", TLam[n, TMatChain[
      <|0 -> TNum[0]|>,
      TLam[TOp2["+", TOp2["%", n, TNum[10]],
                         TApp[TRef["digsum"], TOp2["/", n, TNum[10]]]]]
    ][n]]];
    TAOTCompile["digsum"];
    {TTermVal[TCnf[TAOTRun["digsum", 0]]],
     TTermVal[TCnf[TAOTRun["digsum", 123]]],
     TTermVal[TCnf[TAOTRun["digsum", 99999]]],
     TTermVal[TCnf[TAOTRun["digsum", 1000000]]]},
    {0, 6, 45, 1},
    TestID -> "TAOTRun digsum -- 0/123/99999/1000000 = 0/6/45/1"
]

(* Triangular sum: tri(n) = sum 1..n.  Linear recursion. *)
VerificationTest[
    TInit[];
    TDef["tri", TLam[n, TMatChain[
      <|0 -> TNum[0]|>,
      TLam[k, TOp2["+", k, TApp[TRef["tri"], TOp2["-", k, TNum[1]]]]]
    ][n]]];
    TAOTCompile["tri"];
    {TTermVal[TCnf[TAOTRun["tri", 0]]],
     TTermVal[TCnf[TAOTRun["tri", 10]]],
     TTermVal[TCnf[TAOTRun["tri", 100]]],
     TTermVal[TCnf[TAOTRun["tri", 1000]]]},
    {0, 55, 5050, 500500},
    TestID -> "TAOTRun tri -- triangular sums 0/10/100/1000 = 0/55/5050/500500"
]

(* === Tree analytics: min, max, depth, count === *)

(* tmin: leaf v -> v; node l r -> if tmin(l) < tmin(r) then tmin(l) else tmin(r).
   Uses inner App-of-Mat on the comparison result. *)
VerificationTest[
    TInit[];
    TDef["tmin", TLam[t, TMatChain[
      <|0 -> TLam[v, v],
        1 -> TLam[l, TLam[r, TApp[TMatChain[
          <|0 -> TApp[TRef["tmin"], r]|>,
          TLam[ig, TApp[TRef["tmin"], l]]
        ], TOp2["<", TApp[TRef["tmin"], l], TApp[TRef["tmin"], r]]]]]|>,
      TLam[TEra[]]
    ][t]]];
    TAOTCompile["tmin"];
    Module[{leaf, node, big},
      leaf[v_] := TCtr[0, TNum[v]];
      node[a_, b_] := TCtr[1, a, b];
      big = node[node[leaf[7], leaf[2]], node[leaf[5], leaf[1]]];
      {TTermVal[TAOTRun["tmin", leaf[7]]],
       TTermVal[TAOTRun["tmin", node[leaf[3], leaf[5]]]],
       TTermVal[TAOTRun["tmin", big]]}],
    {7, 3, 1},
    TestID -> "TAOTRun tmin -- min over leaf-NUM tree (inner comparison dispatch)"
]

(* tmax: same structural pattern as tmin with > instead of <. *)
VerificationTest[
    TInit[];
    TDef["tmax", TLam[t, TMatChain[
      <|0 -> TLam[v, v],
        1 -> TLam[l, TLam[r, TApp[TMatChain[
          <|0 -> TApp[TRef["tmax"], r]|>,
          TLam[ig, TApp[TRef["tmax"], l]]
        ], TOp2[">", TApp[TRef["tmax"], l], TApp[TRef["tmax"], r]]]]]|>,
      TLam[TEra[]]
    ][t]]];
    TAOTCompile["tmax"];
    Module[{leaf, node, big},
      leaf[v_] := TCtr[0, TNum[v]];
      node[a_, b_] := TCtr[1, a, b];
      big = node[node[node[leaf[7], leaf[2]], node[leaf[5], leaf[1]]], leaf[42]];
      {TTermVal[TAOTRun["tmax", leaf[7]]],
       TTermVal[TAOTRun["tmax", node[leaf[3], leaf[5]]]],
       TTermVal[TAOTRun["tmax", big]]}],
    {7, 5, 42},
    TestID -> "TAOTRun tmax -- max over leaf-NUM tree"
]

(* tcount: count leaves.  Pure linear sum over the spine, no
   auto-dup gotchas (l and r each used once). *)
VerificationTest[
    TInit[];
    TDef["tcount", TLam[t, TMatChain[
      <|0 -> TLam[TNum[1]],
        1 -> TLam[l, TLam[r, TOp2["+",
          TApp[TRef["tcount"], l],
          TApp[TRef["tcount"], r]]]]|>,
      TLam[TEra[]]
    ][t]]];
    TAOTCompile["tcount"];
    Module[{leaf, node, big},
      leaf[v_] := TCtr[0, TNum[v]];
      node[a_, b_] := TCtr[1, a, b];
      big = node[node[node[leaf[7], leaf[2]], node[leaf[5], leaf[1]]], leaf[42]];
      {TTermVal[TAOTRun["tcount", leaf[9]]],
       TTermVal[TAOTRun["tcount", node[leaf[1], leaf[2]]]],
       TTermVal[TAOTRun["tcount", big]]}],
    {1, 2, 5},
    TestID -> "TAOTRun tcount -- count tree leaves"
]

(* tdepth: tree depth.  Naive `1 + max(tdepth l, tdepth r)` recomputes
   tdepth twice per child + auto-dup blows the heap.  Solution: route
   through an imax helper that takes both depths as args -- so
   tdepth(l) / tdepth(r) are computed ONCE each and the comparison
   is on plain NUMs (atomic dups).  Demonstrates a pattern for
   avoiding HOF-style heap explosion via specialised helpers. *)
VerificationTest[
    TInit[];
    TDef["imax", TLam[a, TLam[b, TApp[TMatChain[
      <|0 -> b|>,
      TLam[ig, a]
    ], TOp2[">=", a, b]]]]];
    TDef["tdepth", TLam[t, TMatChain[
      <|0 -> TLam[TNum[0]],
        1 -> TLam[l, TLam[r, TOp2["+", TNum[1],
          TApp[TApp[TRef["imax"], TApp[TRef["tdepth"], l]],
                                   TApp[TRef["tdepth"], r]]]]]|>,
      TLam[TEra[]]
    ][t]]];
    TAOTCompile["imax"];
    TAOTCompile["tdepth"];
    Module[{leaf, node, big},
      leaf[v_] := TCtr[0, TNum[v]];
      node[a_, b_] := TCtr[1, a, b];
      big = node[node[node[leaf[7], leaf[2]], node[leaf[5], leaf[1]]], leaf[42]];
      {TTermVal[TAOTRun["tdepth", leaf[9]]],
       TTermVal[TAOTRun["tdepth", node[leaf[1], leaf[2]]]],
       TTermVal[TAOTRun["tdepth", big]]}],
    {0, 1, 3},
    TestID -> "TAOTRun tdepth via imax helper -- avoids dup-explosion of dual self-call result"
]

(* === Bit ops + numeric algorithms === *)

(* popcount: count 1-bits via SHR + AND.  Linear recursion that
   exits when n reaches 0.  Three uses of n per level (dispatch,
   n & 1, n >> 1) -- works because each recursion's n is fresh. *)
VerificationTest[
    TInit[];
    TDef["popcount", TLam[n, TMatChain[
      <|0 -> TNum[0]|>,
      TLam[ig, TOp2["+",
        TOp2["&", n, TNum[1]],
        TApp[TRef["popcount"], TOp2[">>", n, TNum[1]]]]]
    ][n]]];
    TAOTCompile["popcount"];
    {TTermVal[TAOTRun["popcount", 0]],
     TTermVal[TAOTRun["popcount", 1]],
     TTermVal[TAOTRun["popcount", 255]],
     TTermVal[TAOTRun["popcount", FromDigits["AAAAAAAA", 16]]],
     TTermVal[TAOTRun["popcount", FromDigits["FFFFFFFF", 16]]]},
    {0, 1, 8, 16, 32},
    TestID -> "TAOTRun popcount -- 0/1/255/0xAAAAAAAA/0xFFFFFFFF -> 0/1/8/16/32"
]

(* sum_squares: sum i^2 for i=1..n.  Linear recursion. *)
VerificationTest[
    TInit[];
    TDef["sumsq", TLam[n, TMatChain[
      <|0 -> TNum[0]|>,
      TLam[k, TOp2["+", TOp2["*", k, k],
                        TApp[TRef["sumsq"], TOp2["-", k, TNum[1]]]]]
    ][n]]];
    TAOTCompile["sumsq"];
    {TTermVal[TAOTRun["sumsq", 0]],
     TTermVal[TAOTRun["sumsq", 1]],
     TTermVal[TAOTRun["sumsq", 10]],
     TTermVal[TAOTRun["sumsq", 100]]},
    {0, 1, 385, 338350},
    TestID -> "TAOTRun sumsq -- sum i^2 for i in 1..n; 0/1/10/100 -> 0/1/385/338350"
]

(* tsumsq: sum of squares over a tree of NUM leaves.  The leaf arm
   returns sq(v) -- a cross-def TApp on TRef -- which produces an
   unreduced TAG_APP that the parent SPLIT cont's OP_ADD would read
   as a heap loc.  Iter U fix: arm-handler value-fallback detects
   compound result tags (APP, OP2, REF, MAT, DP) and emits a
   cnf-fast-path force on the result before returning. *)
VerificationTest[
    TInit[];
    TDef["sq", TLam[v, TOp2["*", v, v]]];
    TDef["tsumsq", TLam[t, TMatChain[
      <|0 -> TLam[v, TApp[TRef["sq"], v]],
        1 -> TLam[l, TLam[r, TOp2["+",
          TApp[TRef["tsumsq"], l],
          TApp[TRef["tsumsq"], r]]]]|>,
      TLam[TEra[]]
    ][t]]];
    TAOTCompile["sq"];
    TAOTCompile["tsumsq"];
    Module[{leaf, node, t4, t8},
      leaf[v_] := TCtr[0, TNum[v]];
      node[a_, b_] := TCtr[1, a, b];
      t4 = node[node[leaf[1], leaf[2]], node[leaf[3], leaf[4]]];
      t8 = node[t4, node[node[leaf[5], leaf[6]], node[leaf[7], leaf[8]]]];
      {TTermVal[TAOTRun["tsumsq", leaf[3]]],
       TTermVal[TAOTRun["tsumsq", node[leaf[1], leaf[2]]]],
       TTermVal[TAOTRun["tsumsq", t4]],
       TTermVal[TAOTRun["tsumsq", t8]]}],
    {9, 5, 30, 204},
    TestID -> "TAOTRun tsumsq -- sum of squares over tree (cross-def TRef in leaf arm)"
]

(* === Programs using the new TLam[body] 1-arg sugar === *)

(* prefix sum via accumulator: psum[i] = sum(xs[0..i]). *)
VerificationTest[
    TInit[];
    TDef["psum_aux", TLam[xs, TLam[acc, TMatChain[
      <|0 -> TCtr[0],
        1 -> TLam[h, TLam[t, TCtr[1, TOp2["+", acc, h],
                                      TApp[TApp[TRef["psum_aux"], t],
                                           TOp2["+", acc, h]]]]]|>,
      TLam[TEra[]]
    ][xs]]]];
    TDef["psum", TLam[xs, TApp[TApp[TRef["psum_aux"], xs], TNum[0]]]];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TDef["llen", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", TNum[1], TApp[TRef["llen"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["psum_aux"];
    TAOTCompile["psum"];
    TAOTCompile["lsum"];
    TAOTCompile["llen"];
    Module[{nil = TCtr[0], cons, fromList},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[xs_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ xs]];
      {TTermVal[TAOTRun["lsum", TCnf[TAOTRun["psum", fromList[{1, 2, 3, 4}]]]]],
       TTermVal[TAOTRun["llen", TCnf[TAOTRun["psum", fromList[Range[6]]]]]],
       TTermVal[TAOTRun["lsum", TCnf[TAOTRun["psum", fromList[Range[10]]]]]]}],
    {20, 6, 220},
    TestID -> "TAOTRun psum -- prefix sum [1,2,3,4]/[1..6]/[1..10] -- sum 20, len 6, sum 220"
]

(* Modular exponentiation via repeated squaring.  Six uses of
   modpow(b, e/2, m) per even/odd path, but each level halves e
   so the total fires are O(log e), not O(e). *)
VerificationTest[
    TInit[];
    TDef["modpow", TLam[b, TLam[e, TLam[m, TMatChain[
      <|0 -> TNum[1]|>,
      TLam[k, TApp[TMatChain[
        <|0 -> TOp2["%", TOp2["*",
                              TApp[TApp[TApp[TRef["modpow"], b],
                                        TOp2["/", k, TNum[2]]], m],
                              TApp[TApp[TApp[TRef["modpow"], b],
                                        TOp2["/", k, TNum[2]]], m]],
                         m]|>,
        TLam[TOp2["%", TOp2["*", b,
                      TOp2["%", TOp2["*",
                                      TApp[TApp[TApp[TRef["modpow"], b],
                                                TOp2["/", k, TNum[2]]], m],
                                      TApp[TApp[TApp[TRef["modpow"], b],
                                                TOp2["/", k, TNum[2]]], m]],
                                m]],
                      m]]
      ], TOp2["%", k, TNum[2]]]]
    ][e]]]]];
    TAOTCompile["modpow"];
    {TTermVal[TCnf[TAOTRun["modpow", {2, 10, 1000}]]],
     TTermVal[TCnf[TAOTRun["modpow", {3, 7, 100}]]],
     TTermVal[TCnf[TAOTRun["modpow", {5, 6, 1000}]]],
     TTermVal[TCnf[TAOTRun["modpow", {7, 13, 10000}]]]},
    {24, 87, 625, 407},  (* 2^10%1000=24, 3^7%100=87, 5^6%1000=625, 7^13%10000=407 *)
    TestID -> "TAOTRun modpow -- 2^10/3^7/5^6/7^13 mod 1000/100/1000/10000"
]

(* zipSum: 2 sorted/equal-length lists, sum corresponding elements
   into a new list.  Nested match: outer on first list, then on
   second list per cons cell.  Two CTR-arm 2-LAM peels.  *)
VerificationTest[
    TInit[];
    TDef["zipSum", TLam[xs, TLam[ys, TMatChain[
      <|0 -> TCtr[0],
        1 -> TLam[a, TLam[ats, TApp[TMatChain[
          <|0 -> TCtr[0],
            1 -> TLam[bb, TLam[bts, TCtr[1,
              TOp2["+", a, bb],
              TApp[TApp[TRef["zipSum"], ats], bts]]]]|>,
          TLam[TEra[]]
        ], ys]]]|>,
      TLam[TEra[]]
    ][xs]]]];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["zipSum"];
    TAOTCompile["lsum"];
    Module[{nil = TCtr[0], cons, fromList},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[xs_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ xs]];
      {TTermVal[TAOTRun["lsum",
        TCnf[TAOTRun["zipSum", {fromList[{1, 2, 3}], fromList[{10, 20, 30}]}]]]],
       TTermVal[TAOTRun["lsum",
        TCnf[TAOTRun["zipSum", {fromList[Range[5]], fromList[Range[10, 14]]}]]]]}],
    {66, 75},  (* (1+10)+(2+20)+(3+30) = 66; (1+10)+(2+11)+(3+12)+(4+13)+(5+14) = 75 *)
    TestID -> "TAOTRun zipSum -- elementwise sum of two cons-lists"
]

(* lmax: max element of a cons-list.  Routes the spine via imax
   helper so each recursive lmax(t) is computed only once. *)
VerificationTest[
    TInit[];
    TDef["imax", TLam[a, TLam[b, TApp[TMatChain[<|0 -> b|>, TLam[a]],
                                       TOp2[">=", a, b]]]]];
    TDef["lmax", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TApp[TApp[TRef["imax"], h],
                                       TApp[TRef["lmax"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["imax"];
    TAOTCompile["lmax"];
    Module[{nil = TCtr[0], cons, fromList},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[xs_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ xs]];
      {TTermVal[TAOTRun["lmax", fromList[{3, 1, 4, 1, 5, 9, 2, 6, 5}]]],
       TTermVal[TAOTRun["lmax", fromList[{100, 50, 200}]]],
       TTermVal[TAOTRun["lmax", fromList[{42}]]]}],
    {9, 200, 42},
    TestID -> "TAOTRun lmax -- max over cons-list (uses imax helper)"
]

(* matsum: sum NUM entries in a list-of-lists matrix. *)
VerificationTest[
    TInit[];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TDef["matsum", TLam[m, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[row, TLam[rest, TOp2["+",
          TApp[TRef["lsum"], row],
          TApp[TRef["matsum"], rest]]]]|>,
      TLam[TEra[]]
    ][m]]];
    TAOTCompile["lsum"];
    TAOTCompile["matsum"];
    Module[{nil = TCtr[0], cons, fromList, fromMatrix},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[xs_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ xs]];
      fromMatrix[m_] := Fold[cons[#2, #1] &, nil, Reverse[fromList /@ m]];
      {TTermVal[TAOTRun["matsum",
        fromMatrix[{{1, 2}, {3, 4}, {5, 6}}]]],
       TTermVal[TAOTRun["matsum",
        fromMatrix[ConstantArray[1, {5, 5}]]]],
       TTermVal[TAOTRun["matsum",
        fromMatrix[{{10, 20, 30}, {40, 50, 60}}]]]}],
    {21, 25, 210},
    TestID -> "TAOTRun matsum -- sum NUM entries of list-of-lists matrix"
]

(* take(n, xs): first n elements.  Two binders n & xs both
   dispatched at different levels.  Tests 2-arg defs with
   list-spine recursion. *)
VerificationTest[
    TInit[];
    TDef["take", TLam[n, TLam[xs, TMatChain[
      <|0 -> TCtr[0]|>,
      TLam[k, TMatChain[
        <|0 -> TCtr[0],
          1 -> TLam[h, TLam[t, TCtr[1, h,
            TApp[TApp[TRef["take"], TOp2["-", k, TNum[1]]], t]]]]|>,
        TLam[TEra[]]
      ][xs]]
    ][n]]]];
    TDef["lsum", TLam[xs, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[h, TLam[t, TOp2["+", h, TApp[TRef["lsum"], t]]]]|>,
      TLam[TEra[]]
    ][xs]]];
    TAOTCompile["take"];
    TAOTCompile["lsum"];
    Module[{nil = TCtr[0], cons, fromList, takeSum},
      cons[h_, t_] := TCtr[1, h, t];
      fromList[xs_] := Fold[cons[#2, #1] &, nil, Reverse[TNum /@ xs]];
      takeSum[n_, xs_] := TTermVal[TAOTRun["lsum",
          TCnf[TAOTRun["take", {n, fromList[xs]}]]]];
      {takeSum[0, Range[5]],
       takeSum[3, Range[10]],
       takeSum[7, Range[10]],
       takeSum[10, Range[5]]}],  (* take past end -- depends on impl *)
    {0, 6, 28, 15},  (* take(10, [1..5]) returns whole list -> sum 15 *)
    TestID -> "TAOTRun take -- first n of list (n=0/3/7/over-len)"
]

VerificationTest[
    TInit[];
    TDef["evn", TLam[n, TMatChain[
      <|0 -> TNum[1],
        1 -> TLam[k, TApp[TRef["odd"], k]]|>,
      TLam[TEra[]]
    ][n]]];
    TDef["odd", TLam[n, TMatChain[
      <|0 -> TNum[0],
        1 -> TLam[k, TApp[TRef["evn"], k]]|>,
      TLam[TEra[]]
    ][n]]];
    TAOTCompile["evn"];
    TAOTCompile["odd"];
    Module[{nat},
      nat[m_] := Nest[Function[k, TCtr[1, k]], TCtr[0], m];
      {TTermVal[TAOTRun["evn", nat[0]]],
       TTermVal[TAOTRun["evn", nat[5]]],
       TTermVal[TAOTRun["evn", nat[8]]],
       TTermVal[TAOTRun["odd", nat[7]]],
       TTermVal[TAOTRun["odd", nat[12]]]}],
    {1, 0, 1, 1, 0},
    TestID -> "TAOTRun even/odd -- mutual recursion via cross-def TRef"
]
