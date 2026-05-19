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

(* === 9.2: TATP[File[path]] file-driven runner === *)

VerificationTest[
    (* File-form runs wald_parse_file + saturation directly.
       monoid_right_id.pr is a basic universal-goal fixture
       (status=PROVED per its .expect file). *)
    TATP[File["tests/data/atp/monoid_right_id.pr"]]["Status"],
    "PROVED",
    TestID -> "ATP/TATP/file/monoid-right-id-proves"
]

VerificationTest[
    (* exists_inverse.pr declares an EXISTS section; the C-side
       runner picks that up via spec->n_existential and switches
       to set_goal_existential.  Witness bindings are not surfaced
       in v0; the file form returns just stats. *)
    TATP[File["tests/data/atp/exists_inverse.pr"]]["Status"],
    "PROVED",
    TestID -> "ATP/TATP/file/exists-inverse-proves"
]

VerificationTest[
    (* The result Association has the same shape as the expression
       form (Status/Rules/Steps/QueueSize) but no Witness key. *)
    Sort @ Keys @ TATP[File["tests/data/atp/monoid_right_id.pr"]],
    Sort[{"Status", "Rules", "Steps", "QueueSize"}],
    TestID -> "ATP/TATP/file/keys-shape-matches-expression-form"
]

VerificationTest[
    (* A non-existent file path causes wald_parse_file to fail;
       the C-side runner returns ATP_RUNNING (=0) as a parse-fail
       sentinel, which decodes to "RUNNING". *)
    TATP[File["tests/data/atp/_does_not_exist_.pr"]]["Status"],
    "RUNNING",
    TestID -> "ATP/TATP/file/missing-path-yields-running-sentinel"
]

(* === TFindEquationalProof: C ATP engine ============================ *)

(* TFindEquationalProof runs thvm's C ATP completion engine and
   returns a real WL ProofObject for provable conjectures, $Failed
   otherwise.  The proof chain is extracted over the oriented input
   axioms, so the dataset cites axioms and the ProofFunction
   verifier accepts it. *)

VerificationTest[
    Head @ TFindEquationalProof[a == c, {a == b, b == c}],
    ProofObject,
    TestID -> "ATP/TFEP/transitivity-3-proves"
]

VerificationTest[
    TFindEquationalProof[a == d, {a == b, b == c}],
    $Failed,
    TestID -> "ATP/TFEP/unprovable-yields-failed"
]

VerificationTest[
    Head @ TFindEquationalProof[a == e,
        {a == b, b == c, c == d, d == e}],
    ProofObject,
    TestID -> "ATP/TFEP/4-step-chain-proves"
]

VerificationTest[
    Head @ TFindEquationalProof[f[a] == f[b], {a == b}],
    ProofObject,
    TestID -> "ATP/TFEP/subst-1pos-proves"
]

VerificationTest[
    Head @ TFindEquationalProof[f[a, b] == f[c, d],
        {a == c, b == d}],
    ProofObject,
    TestID -> "ATP/TFEP/subst-2pos-proves"
]

VerificationTest[
    Head @ TFindEquationalProof[f[a] == f[c], {a == b, b == c}],
    ProofObject,
    TestID -> "ATP/TFEP/subst-via-trans-proves"
]

VerificationTest[
    TFindEquationalProof[f[a] == g[a], {a == b}],
    $Failed,
    TestID -> "ATP/TFEP/head-mismatch-yields-failed"
]

VerificationTest[
    Head @ TFindEquationalProof[c == a, {a == b, b == c}],
    ProofObject,
    TestID -> "ATP/TFEP/backward-needed-proves"
]

VerificationTest[
    Head @ TFindEquationalProof[b == a, {a == b}],
    ProofObject,
    TestID -> "ATP/TFEP/symmetry-1step-proves"
]

(* End-to-end: the ProofObject's ProofFunction verifier returns
   Success when applied to the conjecture statement. *)
VerificationTest[
    Module[{p},
        p = TFindEquationalProof[a == c, {a == b, b == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/proof-function-verifies-transitivity"
]

VerificationTest[
    Module[{p},
        p = TFindEquationalProof[f[a] == f[c], {a == b, b == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/proof-function-verifies-subst-via-trans"
]

(* === TFindEquationalProof: pattern axioms (milestone 5) ============= *)

(* Axioms with `Pattern[x, Blank[]]` (= x_) variables.  WL's normal
   surface uses the underscore shorthand, which TFEP's HoldAll
   front-end accepts as-is. *)

VerificationTest[
    (* Direct pattern axiom: f[x_, e] == x_ as a right-identity
       rewrite, applied once. *)
    Head @ TFindEquationalProof[f[a, e] == a,
        {f[Pattern[x, Blank[]], e] == Pattern[x, Blank[]]}],
    ProofObject,
    TestID -> "ATP/TFEP/pattern-rightId-1use"
]

VerificationTest[
    (* Same axiom, two applications nested. *)
    Head @ TFindEquationalProof[f[f[b, e], e] == b,
        {f[Pattern[x, Blank[]], e] == Pattern[x, Blank[]]}],
    ProofObject,
    TestID -> "ATP/TFEP/pattern-rightId-2uses"
]

VerificationTest[
    (* Pattern-axiom proof verifier round-trip. *)
    Module[{p},
        p = TFindEquationalProof[f[a, e] == a,
            {f[Pattern[x, Blank[]], e] == Pattern[x, Blank[]]}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/pattern-rightId-verifies"
]

(* === TFindEquationalProof: ForAll-wrapped axioms (m5 doc shape) ===== *)

(* The standard FindEquationalProof surface uses
   ForAll[var, lhs == rhs] or ForAll[{vars}, lhs == rhs] for
   universally-quantified axioms.  forAllToPattern strips the
   ForAll and rewrites the bound bare symbols as Pattern[v, Blank[]],
   so downstream BFS gets the same shape as a Pattern-form axiom. *)

VerificationTest[
    (* Doc example: prove ForAll[x, f[g[x]] == g[f[x]]] from
       ForAll[x, f[x] == g[x]]. *)
    Head @ TFindEquationalProof[
        ForAll[x, f[g[x]] == g[f[x]]],
        {ForAll[x, f[x] == g[x]]}],
    ProofObject,
    TestID -> "ATP/TFEP/forall-fg-gf-from-fx-gx"
]

VerificationTest[
    (* Multi-var ForAll axiom: f[g[a], g[b]] == g[f[a, b]] from
       ForAll[{x, y}, f[g[x], g[y]] == g[f[x, y]]]. *)
    Head @ TFindEquationalProof[
        f[g[a], g[b]] == g[f[a, b]],
        {ForAll[{x, y}, f[g[x], g[y]] == g[f[x, y]]]}],
    ProofObject,
    TestID -> "ATP/TFEP/forall-multi-axiom-instantiated"
]

VerificationTest[
    (* WL doc Properties&Relations example: associativity rewrite
       on a ground-instantiated 4-LHS / 4-RHS form. *)
    Head @ TFindEquationalProof[
        f[f[u, f[v, w]], u] == f[u, f[f[v, w], u]],
        {ForAll[{a, b, c}, f[a, f[b, c]] == f[f[a, b], c]]}],
    ProofObject,
    TestID -> "ATP/TFEP/assoc-rewrite-doc-example"
]

VerificationTest[
    (* Single-step ForAll axiom matching ForAll conjecture: trivial
       direct rewrite via the axiom itself. *)
    Head @ TFindEquationalProof[
        ForAll[x, f[x] == g[x]],
        {ForAll[x, f[x] == g[x]]}],
    ProofObject,
    TestID -> "ATP/TFEP/forall-single-1step"
]

VerificationTest[
    (* Multi-var ForAll on both conjecture and axiom (commutativity-
       style; here it's identity since axiom IS the conjecture). *)
    Head @ TFindEquationalProof[
        ForAll[{x, y}, f[x, y] == f[y, x]],
        {ForAll[{a, b}, f[a, b] == f[b, a]]}],
    ProofObject,
    TestID -> "ATP/TFEP/forall-multi-symmetric"
]

(* ProofFunction verifier round-trips.  Backward-axiom steps carry
   Orientation -> -1 so WL's verifier reads the axiom Statement in
   reverse for that step; a single axiom can therefore be used in
   both directions within one chain (the fg-gf case below does
   exactly that). *)

VerificationTest[
    Module[{p},
        p = TFindEquationalProof[
            ForAll[x, f[g[x]] == g[f[x]]],
            {ForAll[x, f[x] == g[x]]}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/forall-fg-gf-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindEquationalProof[
            f[g[a], g[b]] == g[f[a, b]],
            {ForAll[{x, y}, f[g[x], g[y]] == g[f[x, y]]]}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/forall-multi-axiom-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindEquationalProof[
            f[f[u, f[v, w]], u] == f[u, f[f[v, w], u]],
            {ForAll[{a, b, c}, f[a, f[b, c]] == f[f[a, b], c]]}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/assoc-rewrite-verifies"
]

VerificationTest[
    (* Backward-axiom usage: c == a from {a == b, b == c} forces
       both axioms to be applied right-to-left.  Orientation -> -1
       on each step keeps the verifier in sync. *)
    Module[{p},
        p = TFindEquationalProof[c == a, {a == b, b == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/backward-axiom-verifies"
]

(* === TFindEquationalProof: doc-shape negative cases ================= *)

VerificationTest[
    (* WL doc Scope example: prove a == d from {a == b, b == c, c == d}. *)
    Head @ TFindEquationalProof[a == d, {a == b, b == c, c == d}],
    ProofObject,
    TestID -> "ATP/TFEP/doc-scope-3step-chain"
]

VerificationTest[
    (* WL doc Scope example: a == c from a == b alone -> unprovable. *)
    TFindEquationalProof[a == c, {a == b}],
    $Failed,
    TestID -> "ATP/TFEP/doc-scope-insufficient-axioms"
]

(* === TFindEquationalProof: C-engine verifier round-trips =========== *)

(* The proof chain thvm's C ATP engine extracts assembles into a
   4-arg ProofObject whose ProofFunction verifier returns Success. *)

VerificationTest[
    (* backward-needed: c == a from {a == b, b == c} uses both axioms
       right-to-left; Orientation -> -1 keeps the verifier in sync. *)
    Module[{p},
        p = TFindEquationalProof[c == a, {a == b, b == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/backward-needed-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindEquationalProof[a == e,
            {a == b, b == c, c == d, d == e}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/depth-4-verifies"
]

(* === TFindEquationalProof: AxiomaticTheory string form ============= *)

(* TFindEquationalProof["Theorem", "Theory"] resolves both names
   through AxiomaticTheory.  An unknown theorem name surfaces a
   parse Failure rather than $Failed. *)

VerificationTest[
    Head @ TFindEquationalProof["NoSuchTheorem", "WolframAxioms"],
    Failure,
    TestID -> "ATP/TFEP/string-unknown-theorem"
]

VerificationTest[
    (* The string form resolves a NotableTheorem of WolframAxioms
       and runs the C engine.  A small step budget keeps the test
       fast: completion does not close DoubleNegation here, so the
       result is $Failed -- the point is that the string-form
       plumbing (AxiomaticTheory resolution, quantifier elimination,
       encoding) runs end to end without error. *)
    MatchQ[
        TFindEquationalProof["DoubleNegation", "WolframAxioms",
            MaxSteps -> 64],
        _ProofObject | $Failed
    ],
    True,
    TestID -> "ATP/TFEP/string-doublenegation-resolves"
]
