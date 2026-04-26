(* atp.wlt -- VerificationTest specs for the 8.7b LibraryLink ATP
   helper.  Builds tiny saturation problems by passing pre-encoded
   Term values to thvm_wl_atp_run, sidestepping the higher-level
   WL-expression encoder (deferred to 8.7c-d).
*)

(* === ATP runner roundtrip === *)

(* The ATP runner expects a packed Int64 NumericArray:
     [n_axioms, lhs_0, rhs_0, ..., lhs_{n-1}, rhs_{n-1},
      goal_lhs, goal_rhs]
   Returns [status, n_rules, n_trace, n_cps] (also Int64 NA).
*)

VerificationTest[
    TInit[],
    True,
    TestID -> "ATP/init"
]

(* Build an FVR(0) Term via the raw thvm_wl_term_new entry. *)
fvr0 = THVMLink`Private`$termNewFn[0, 22 (* TAG_FVR *), 0, 0];

VerificationTest[
    (* Self-equation x = x with goal x = x.  goal_check fires
       immediately on structural equality; saturation is a no-op. *)
    Module[{packed, stats},
      packed = NumericArray[
        {1, fvr0, fvr0, fvr0, fvr0},
        "Integer64"
      ];
      stats = THVMLink`Private`$atpRunFn[packed, 8, 4];
      Normal @ stats
    ],
    (* Adding the axiom pushes a TRACE_AXIOM entry; goal_check fires
       on structural equality before any saturation step runs. *)
    {1 (* ATP_PROVED *), 0, 1, 1},
    TestID -> "ATP/runner/trivial-self-equation-proves"
]

VerificationTest[
    (* Distinct FVRs as goal (x = y).  No axioms; goal_check returns
       RUNNING (not equal); saturation has no CPs to pop and returns
       QUEUE_EMPTY (status code 4). *)
    Module[{fvr1, packed, stats},
      fvr1 = THVMLink`Private`$termNewFn[0, 22, 1, 0];
      packed = NumericArray[
        {0, fvr0, fvr1},
        "Integer64"
      ];
      stats = THVMLink`Private`$atpRunFn[packed, 8, 4];
      Normal @ stats
    ],
    {4 (* ATP_QUEUE_EMPTY *), 0, 0, 0},
    TestID -> "ATP/runner/no-axioms-distinct-goals-empties-queue"
]

VerificationTest[
    (* Returned NumericArray has fixed shape [4]. *)
    Module[{packed, stats},
      packed = NumericArray[{1, fvr0, fvr0, fvr0, fvr0}, "Integer64"];
      stats = THVMLink`Private`$atpRunFn[packed, 8, 4];
      Dimensions @ Normal @ stats
    ],
    {4},
    TestID -> "ATP/runner/return-shape"
]

(* === 8.7c: WL-expression-to-Term encoder === *)

VerificationTest[
    (* A bare symbol becomes a nullary CTR.  Verify the resulting
       Term has TAG_CTR (20). *)
    Module[{result, t, state},
      state = THVMLink`Private`encodeAtpTermInit[];
      result = THVMLink`Private`encodeAtpTerm[zero, state];
      t = result[[1]];
      THVMLink`Private`$termTagFn[t]
    ],
    20 (* TAG_CTR *),
    TestID -> "ATP/encoder/symbol-becomes-nullary-ctr"
]

VerificationTest[
    (* Distinct symbols get distinct labels.  After encoding zero
       and nil, the symbol map should have 2 entries. *)
    Module[{state, r1, r2},
      state = THVMLink`Private`encodeAtpTermInit[];
      r1 = THVMLink`Private`encodeAtpTerm[zero, state];
      r2 = THVMLink`Private`encodeAtpTerm[nil, r1[[2]]];
      Length[r2[[2]]["sym"]]
    ],
    2,
    TestID -> "ATP/encoder/distinct-symbols-get-distinct-labels"
]

VerificationTest[
    (* head[args...] becomes a CTR with the right number of
       children.  Verify the resulting Term's tag and the symbol
       map size. *)
    Module[{state, result, t},
      state = THVMLink`Private`encodeAtpTermInit[];
      result = THVMLink`Private`encodeAtpTerm[
        f[zero, succ[zero]], state
      ];
      t = result[[1]];
      {THVMLink`Private`$termTagFn[t], Length[result[[2]]["sym"]]}
    ],
    {20 (* TAG_CTR *), 3 (* f, zero, succ *)},
    TestID -> "ATP/encoder/compound-head-becomes-ctr-with-children"
]

VerificationTest[
    (* Pattern[x, Blank[]] becomes a TAG_FVR. *)
    Module[{state, result, t},
      state = THVMLink`Private`encodeAtpTermInit[];
      result = THVMLink`Private`encodeAtpTerm[Pattern[x, Blank[]], state];
      t = result[[1]];
      {THVMLink`Private`$termTagFn[t], Length[result[[2]]["var"]]}
    ],
    {22 (* TAG_FVR *), 1},
    TestID -> "ATP/encoder/pattern-becomes-fvr"
]

VerificationTest[
    (* Same pattern variable name reused gets the same FVR id. *)
    Module[{state, r1, r2},
      state = THVMLink`Private`encodeAtpTermInit[];
      r1 = THVMLink`Private`encodeAtpTerm[Pattern[x, Blank[]], state];
      r2 = THVMLink`Private`encodeAtpTerm[Pattern[x, Blank[]], r1[[2]]];
      (* Var map should still have 1 entry; both encodes return the
         same Term value. *)
      {Length[r2[[2]]["var"]], r1[[1]] == r2[[1]]}
    ],
    {1, True},
    TestID -> "ATP/encoder/pattern-var-stable-across-occurrences"
]

(* === 8.7d: TATP[] WL surface === *)

VerificationTest[
    (* Trivial reflexive-axiom proof: a == a derives a == a. *)
    Lookup[
      TATP[{a == a}, a == a],
      "Status"
    ],
    "PROVED",
    TestID -> "ATP/TATP/trivial-reflexive-proves"
]

VerificationTest[
    (* Direct rewrite: rule f[x_, e] == x; goal f[a, e] == a.
       Goal-check fires on the first normalize pass; saturator
       converges immediately. *)
    Lookup[
      TATP[{f[Pattern[x, Blank[]], e] == Pattern[x, Blank[]]},
           f[a, e] == a],
      "Status"
    ],
    "PROVED",
    TestID -> "ATP/TATP/direct-rewrite-proves"
]

VerificationTest[
    (* TATP returns an Association with the expected keys. *)
    Sort @ Keys @ TATP[{a == a}, a == a],
    {"QueueSize", "Rules", "Status", "Steps"},
    TestID -> "ATP/TATP/return-keys"
]

VerificationTest[
    (* Bad-shape axiom: not Equal[lhs, rhs] -> Failure. *)
    Head @ TATP[{a (* missing == *)}, a == a],
    Failure,
    TestID -> "ATP/TATP/bad-axiom-yields-failure"
]

VerificationTest[
    (* Bad-shape conjecture. *)
    Head @ TATP[{a == a}, a (* missing == *)],
    Failure,
    TestID -> "ATP/TATP/bad-conjecture-yields-failure"
]

(* === 8.9e: TATP[..., Witness -> {x_}] surface === *)

VerificationTest[
    (* Existential goal: rule f(a, e) == a; goal f(x_, e) == a;
       witness x.  Should bind x = a. *)
    Module[{r},
      r = TATP[{f[a, e] == a}, f[Pattern[x, Blank[]], e] == a,
               Witness -> {Pattern[x, Blank[]]}];
      r["Status"]
    ],
    "PROVED",
    TestID -> "ATP/TATP/witness/proves-with-narrow"
]

VerificationTest[
    (* Same setup; verify the Witness key is present in the
       result Association. *)
    Module[{r},
      r = TATP[{f[a, e] == a}, f[Pattern[x, Blank[]], e] == a,
               Witness -> {Pattern[x, Blank[]]}];
      KeyExistsQ[r, "Witness"]
    ],
    True,
    TestID -> "ATP/TATP/witness/witness-key-present"
]

VerificationTest[
    (* The Witness association should have one entry keyed by `x`. *)
    Module[{r},
      r = TATP[{f[a, e] == a}, f[Pattern[x, Blank[]], e] == a,
               Witness -> {Pattern[x, Blank[]]}];
      Keys[r["Witness"]]
    ],
    {x},
    TestID -> "ATP/TATP/witness/key-is-x"
]

VerificationTest[
    (* Witness with a name not in the conjecture -> Failure. *)
    Head @ TATP[{a == a}, a == a, Witness -> {Pattern[z, Blank[]]}],
    Failure,
    TestID -> "ATP/TATP/witness/missing-name-yields-failure"
]

VerificationTest[
    (* Empty Witness option behaves like the universal-goal path. *)
    TATP[{a == a}, a == a, Witness -> {}]["Status"],
    "PROVED",
    TestID -> "ATP/TATP/witness/empty-runs-universal-path"
]

(* === 9.1c: TATP[..., AllWitnesses -> True] surface === *)

VerificationTest[
    (* AllWitnesses -> True returns "Witnesses" (plural) key, not
       "Witness".  Single-witness setup so the list has length 1. *)
    Module[{r},
      r = TATP[{f[a, e] == a}, f[Pattern[x, Blank[]], e] == a,
               Witness -> {Pattern[x, Blank[]]},
               AllWitnesses -> True];
      KeyExistsQ[r, "Witnesses"]
    ],
    True,
    TestID -> "ATP/TATP/all-witnesses/witnesses-key-present"
]

VerificationTest[
    (* AllWitnesses -> True returns a List of Associations. *)
    Module[{r},
      r = TATP[{f[a, e] == a}, f[Pattern[x, Blank[]], e] == a,
               Witness -> {Pattern[x, Blank[]]},
               AllWitnesses -> True];
      Head[r["Witnesses"]]
    ],
    List,
    TestID -> "ATP/TATP/all-witnesses/witnesses-is-list"
]

VerificationTest[
    (* Two distinct axioms unifying with the goal at top yield two
       witnesses.  f(a, e) -> a binds x=a; f(e, e) -> a binds x=e. *)
    Module[{r},
      r = TATP[{f[a, e] == a, f[e, e] == a},
               f[Pattern[x, Blank[]], e] == a,
               Witness -> {Pattern[x, Blank[]]},
               AllWitnesses -> True];
      Length[r["Witnesses"]] >= 2
    ],
    True,
    TestID -> "ATP/TATP/all-witnesses/two-rules-yield-two-witnesses"
]

VerificationTest[
    (* MaxWitnesses -> 1 caps the list at one entry. *)
    Module[{r},
      r = TATP[{f[a, e] == a, f[e, e] == a},
               f[Pattern[x, Blank[]], e] == a,
               Witness -> {Pattern[x, Blank[]]},
               AllWitnesses -> True,
               MaxWitnesses -> 1];
      Length[r["Witnesses"]]
    ],
    1,
    TestID -> "ATP/TATP/all-witnesses/max-witnesses-caps-list"
]

VerificationTest[
    (* AllWitnesses -> False (default) keeps the singular "Witness"
       Association key for backwards compatibility. *)
    Module[{r},
      r = TATP[{f[a, e] == a}, f[Pattern[x, Blank[]], e] == a,
               Witness -> {Pattern[x, Blank[]]}];
      KeyExistsQ[r, "Witness"] && ! KeyExistsQ[r, "Witnesses"]
    ],
    True,
    TestID -> "ATP/TATP/all-witnesses/default-stays-singular"
]
