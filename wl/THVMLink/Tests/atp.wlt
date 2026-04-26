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
