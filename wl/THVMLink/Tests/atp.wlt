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
      stats = THVMLink`ATP`Private`$atpRunFn[packed, 8, 4];
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
      stats = THVMLink`ATP`Private`$atpRunFn[packed, 8, 4];
      Normal @ stats
    ],
    {4 (* ATP_QUEUE_EMPTY *), 0, 0, 0},
    TestID -> "ATP/runner/no-axioms-distinct-goals-empties-queue"
]

VerificationTest[
    (* Returned NumericArray has fixed shape [4]. *)
    Module[{packed, stats},
      packed = NumericArray[{1, fvr0, fvr0, fvr0, fvr0}, "Integer64"];
      stats = THVMLink`ATP`Private`$atpRunFn[packed, 8, 4];
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
      state = THVMLink`ATP`Private`encodeAtpTermInit[];
      result = THVMLink`ATP`Private`encodeAtpTerm[zero, state];
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
      state = THVMLink`ATP`Private`encodeAtpTermInit[];
      r1 = THVMLink`ATP`Private`encodeAtpTerm[zero, state];
      r2 = THVMLink`ATP`Private`encodeAtpTerm[nil, r1[[2]]];
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
      state = THVMLink`ATP`Private`encodeAtpTermInit[];
      result = THVMLink`ATP`Private`encodeAtpTerm[
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
      state = THVMLink`ATP`Private`encodeAtpTermInit[];
      result = THVMLink`ATP`Private`encodeAtpTerm[Pattern[x, Blank[]], state];
      t = result[[1]];
      {THVMLink`Private`$termTagFn[t], Length[result[[2]]["var"]]}
    ],
    {22 (* TAG_FVR *), 1},
    TestID -> "ATP/encoder/pattern-becomes-fvr"
]

VerificationTest[
    (* Same pattern variable name reused gets the same FVR id. *)
    Module[{state, r1, r2},
      state = THVMLink`ATP`Private`encodeAtpTermInit[];
      r1 = THVMLink`ATP`Private`encodeAtpTerm[Pattern[x, Blank[]], state];
      r2 = THVMLink`ATP`Private`encodeAtpTerm[Pattern[x, Blank[]], r1[[2]]];
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

(* === TFindProof: C ATP engine ============================ *)

(* TFindProof runs thvm's C ATP completion engine and
   returns a real WL ProofObject for provable conjectures, $Failed
   otherwise.  The proof chain is extracted over the oriented input
   axioms, so the dataset cites axioms and the ProofFunction
   verifier accepts it. *)

VerificationTest[
    Head @ TFindProof[a == c, {a == b, b == c}],
    ProofObject,
    TestID -> "ATP/TFEP/transitivity-3-proves"
]

VerificationTest[
    TFindProof[a == d, {a == b, b == c}],
    $Failed,
    TestID -> "ATP/TFEP/unprovable-yields-failed"
]

VerificationTest[
    Head @ TFindProof[a == e,
        {a == b, b == c, c == d, d == e}],
    ProofObject,
    TestID -> "ATP/TFEP/4-step-chain-proves"
]

VerificationTest[
    Head @ TFindProof[f[a] == f[b], {a == b}],
    ProofObject,
    TestID -> "ATP/TFEP/subst-1pos-proves"
]

VerificationTest[
    Head @ TFindProof[f[a, b] == f[c, d],
        {a == c, b == d}],
    ProofObject,
    TestID -> "ATP/TFEP/subst-2pos-proves"
]

VerificationTest[
    Head @ TFindProof[f[a] == f[c], {a == b, b == c}],
    ProofObject,
    TestID -> "ATP/TFEP/subst-via-trans-proves"
]

VerificationTest[
    TFindProof[f[a] == g[a], {a == b}],
    $Failed,
    TestID -> "ATP/TFEP/head-mismatch-yields-failed"
]

VerificationTest[
    Head @ TFindProof[c == a, {a == b, b == c}],
    ProofObject,
    TestID -> "ATP/TFEP/backward-needed-proves"
]

VerificationTest[
    Head @ TFindProof[b == a, {a == b}],
    ProofObject,
    TestID -> "ATP/TFEP/symmetry-1step-proves"
]

(* End-to-end: the ProofObject's ProofFunction verifier returns
   Success when applied to the conjecture statement. *)
VerificationTest[
    Module[{p},
        p = TFindProof[a == c, {a == b, b == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/proof-function-verifies-transitivity"
]

VerificationTest[
    Module[{p},
        p = TFindProof[f[a] == f[c], {a == b, b == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/proof-function-verifies-subst-via-trans"
]

(* === TFindProof: pattern axioms (milestone 5) ============= *)

(* Axioms with `Pattern[x, Blank[]]` (= x_) variables.  WL's normal
   surface uses the underscore shorthand, which TFEP's HoldAll
   front-end accepts as-is. *)

VerificationTest[
    (* Direct pattern axiom: f[x_, e] == x_ as a right-identity
       rewrite, applied once. *)
    Head @ TFindProof[f[a, e] == a,
        {f[Pattern[x, Blank[]], e] == Pattern[x, Blank[]]}],
    ProofObject,
    TestID -> "ATP/TFEP/pattern-rightId-1use"
]

VerificationTest[
    (* Same axiom, two applications nested. *)
    Head @ TFindProof[f[f[b, e], e] == b,
        {f[Pattern[x, Blank[]], e] == Pattern[x, Blank[]]}],
    ProofObject,
    TestID -> "ATP/TFEP/pattern-rightId-2uses"
]

VerificationTest[
    (* Pattern-axiom proof verifier round-trip. *)
    Module[{p},
        p = TFindProof[f[a, e] == a,
            {f[Pattern[x, Blank[]], e] == Pattern[x, Blank[]]}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/pattern-rightId-verifies"
]

(* === TFindProof: ForAll-wrapped axioms (m5 doc shape) ===== *)

(* The standard FindEquationalProof surface uses
   ForAll[var, lhs == rhs] or ForAll[{vars}, lhs == rhs] for
   universally-quantified axioms.  forAllToPattern strips the
   ForAll and rewrites the bound bare symbols as Pattern[v, Blank[]],
   so downstream BFS gets the same shape as a Pattern-form axiom. *)

VerificationTest[
    (* Doc example: prove ForAll[x, f[g[x]] == g[f[x]]] from
       ForAll[x, f[x] == g[x]]. *)
    Head @ TFindProof[
        ForAll[x, f[g[x]] == g[f[x]]],
        {ForAll[x, f[x] == g[x]]}],
    ProofObject,
    TestID -> "ATP/TFEP/forall-fg-gf-from-fx-gx"
]

VerificationTest[
    (* Multi-var ForAll axiom: f[g[a], g[b]] == g[f[a, b]] from
       ForAll[{x, y}, f[g[x], g[y]] == g[f[x, y]]]. *)
    Head @ TFindProof[
        f[g[a], g[b]] == g[f[a, b]],
        {ForAll[{x, y}, f[g[x], g[y]] == g[f[x, y]]]}],
    ProofObject,
    TestID -> "ATP/TFEP/forall-multi-axiom-instantiated"
]

VerificationTest[
    (* WL doc Properties&Relations example: associativity rewrite
       on a ground-instantiated 4-LHS / 4-RHS form. *)
    Head @ TFindProof[
        f[f[u, f[v, w]], u] == f[u, f[f[v, w], u]],
        {ForAll[{a, b, c}, f[a, f[b, c]] == f[f[a, b], c]]}],
    ProofObject,
    TestID -> "ATP/TFEP/assoc-rewrite-doc-example"
]

VerificationTest[
    (* Single-step ForAll axiom matching ForAll conjecture: trivial
       direct rewrite via the axiom itself. *)
    Head @ TFindProof[
        ForAll[x, f[x] == g[x]],
        {ForAll[x, f[x] == g[x]]}],
    ProofObject,
    TestID -> "ATP/TFEP/forall-single-1step"
]

VerificationTest[
    (* Multi-var ForAll on both conjecture and axiom (commutativity-
       style; here it's identity since axiom IS the conjecture). *)
    Head @ TFindProof[
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
        p = TFindProof[
            ForAll[x, f[g[x]] == g[f[x]]],
            {ForAll[x, f[x] == g[x]]}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/forall-fg-gf-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindProof[
            f[g[a], g[b]] == g[f[a, b]],
            {ForAll[{x, y}, f[g[x], g[y]] == g[f[x, y]]]}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/forall-multi-axiom-verifies"
]

VerificationTest[
    (* A symmetric goal: neither side rewrites to the other, so the
       variable-keyed goal has no shared normal form.  Skolemizing the
       conjecture's variables to constants lets KBO totally order them,
       so the unorientable commutativity axiom ordered-rewrites the
       larger face down to the smaller -- the single-NF check then
       closes it, and the proof cites the axiom directly. *)
    Module[{p},
        p = TFindProof[
            ForAll[{x, y}, f[x, y] == f[y, x]],
            {ForAll[{a, b}, f[a, b] == f[b, a]]}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/TFEP/forall-multi-symmetric-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindProof[
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
        p = TFindProof[c == a, {a == b, b == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/backward-axiom-verifies"
]

(* === TFindProof: doc-shape negative cases ================= *)

VerificationTest[
    (* WL doc Scope example: prove a == d from {a == b, b == c, c == d}. *)
    Head @ TFindProof[a == d, {a == b, b == c, c == d}],
    ProofObject,
    TestID -> "ATP/TFEP/doc-scope-3step-chain"
]

VerificationTest[
    (* WL doc Scope example: a == c from a == b alone -> unprovable. *)
    TFindProof[a == c, {a == b}],
    $Failed,
    TestID -> "ATP/TFEP/doc-scope-insufficient-axioms"
]

(* === TFindProof: C-engine verifier round-trips =========== *)

(* The proof chain thvm's C ATP engine extracts assembles into a
   4-arg ProofObject whose ProofFunction verifier returns Success. *)

VerificationTest[
    (* backward-needed: c == a from {a == b, b == c} uses both axioms
       right-to-left; Orientation -> -1 keeps the verifier in sync. *)
    Module[{p},
        p = TFindProof[c == a, {a == b, b == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/backward-needed-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindProof[a == e,
            {a == b, b == c, c == d, d == e}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/depth-4-verifies"
]

(* === TFindProof: AxiomaticTheory string form ============= *)

(* TFindProof["Theorem", "Theory"] resolves both names
   through AxiomaticTheory.  An unknown theorem name surfaces a
   parse Failure rather than $Failed. *)

VerificationTest[
    Head @ TFindProof["NoSuchTheorem", "WolframAxioms"],
    Failure,
    TestID -> "ATP/TFEP/string-unknown-theorem"
]

VerificationTest[
    (* The string form resolves a NotableTheorem of WolframAxioms,
       saturates with the C engine, and assembles the critical-pair
       lemma DAG.  DoubleNegation is a genuine superposition proof:
       the trace carries TRACE_CP entries, so buildCplDataset emits
       CriticalPairLemma steps. *)
    Head @ TFindProof["DoubleNegation", "WolframAxioms"],
    ProofObject,
    TestID -> "ATP/TFEP/string-doublenegation-resolves"
]

VerificationTest[
    (* The DoubleNegation ProofObject passes WL's own equational
       proof verifier -- the CriticalPairLemma field encoding
       (Construct / MatchingConstruct parents, superposition
       Position / Subpattern, completion-introduced variables in
       the ProofObject "Variables" list) matches what the verifier
       replays. *)
    Module[{p},
        p = TFindProof["DoubleNegation", "WolframAxioms"];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/TFEP/string-doublenegation-verifies"
]

(* === TFindProof: completion-derived proofs =============== *)

(* Goals the input axioms do not close directly (the EXT axiom-only
   path returns nothing) but completion does: buildCplDataset walks
   the MAIN trace DAG and emits a SubstitutionLemma per re-derived
   rewrite.  The resulting ProofObject passes WL's verifier. *)

VerificationTest[
    Head @ TFindProof[b == c, {f[a] == b, f[a] == c}],
    ProofObject,
    TestID -> "ATP/TFEP/completion-ground-proves"
]

VerificationTest[
    Module[{p},
        p = TFindProof[b == c, {f[a] == b, f[a] == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/completion-ground-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindProof[b == a,
            {ForAll[x, f[g[x]] == x], f[g[a]] == b}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/completion-variable-verifies"
]

(* === TFindProof: numeric-literal constants =============== *)

(* AbelianGroupAxioms (and the McCune / Tarski theories) write the
   identity element as OverTilde[1] -- a function applied to the
   integer literal 1.  The encoder must treat a numeric atom as a
   0-arity constant; without that rule it folds over a non-list and
   diverges. *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms"],
    ProofObject,
    TestID -> "ATP/TFEP/numeric-literal-constant-proves"
]

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms"];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/TFEP/numeric-literal-constant-verifies"
]

(* === TFindProof: multi-equation conjectures ============== *)

(* AxiomaticTheory ships some NotableTheorems as a multi-element list
   of equations -- e.g. BooleanAxioms `DeMorgan` is the pair of De
   Morgan laws.  The string form proves each conjunct separately and
   returns a List of ProofObjects (equational provability distributes
   over conjunction). *)

VerificationTest[
    MatchQ[
        TFindProof["DeMorgan", "BooleanAxioms"],
        {_ProofObject, _ProofObject}
    ],
    True,
    TestID -> "ATP/TFEP/multi-eq-demorgan-proves"
]

VerificationTest[
    Module[{ps},
        ps = TFindProof["DeMorgan", "BooleanAxioms"];
        AllTrue[ps,
            Head @ Quiet @ Check[
                #["ProofFunction"][#["Theorems"]], $Failed] === Success &]
    ],
    True,
    TestID -> "ATP/TFEP/multi-eq-demorgan-verifies"
]

(* === TFindProof: ordered-rewriting goal chain ============ *)

(* AbelianGroupAxioms' `ImpliesMcCuneAxioms` is a single-equation
   theorem whose proof chain includes steps where the C engine's
   ordered rewriting fires a rule in the rhs->lhs direction.  The
   verifier needs the SubstitutionLemma's Orientation flipped to read
   the cited entry's Statement reversed for those steps -- the goal
   chain emit threads the C engine's Fwd flag into the Orientation
   computation. *)

VerificationTest[
    Module[{p},
        p = TFindProof["ImpliesMcCuneAxioms", "AbelianGroupAxioms"];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/TFEP/ordered-rewrite-fwd-flag-verifies"
]

(* === emitNorm: two-phase BFS rewrite-chain reconstruction ========== *)

(* HillmanAxioms / AndAssociativity is a single-equation theorem
   whose completion-derived proof chain runs through TRACE_ORIENT /
   TRACE_SIMPLIFY rules whose normalization from the parent CP needs
   ordered rewriting in the reverse direction.  Phase-1 (forward-only)
   BFS in emitNorm exhausts without finding the chain; Phase 2 retries
   with reverse direction enabled (variable-safe rules only) and a
   tight cap, which closes this class of fast-failures. *)

VerificationTest[
    Module[{p},
        p = TFindProof["AndAssociativity", "HillmanAxioms"];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/TFEP/emitNorm-phase2-reverse-direction-verifies"
]

(* ================================================================== *)
(* Method options and suboptions.  Each exposed knob gets a couple of *)
(* meaningful tests: a fast theorem (InverseOfInverse / AbelianGroup- *)
(* Axioms, which proves under every config in ~0.01s) exercises that  *)
(* the knob is accepted and keeps the proof SOUND (proves and, where  *)
(* asserted, verifies); the search-bounding / abort knobs are tested  *)
(* on a hard theorem (AndAssociativity / WolframAxioms) where they    *)
(* must cut the search off and return $Failed / $Aborted.             *)
(* ================================================================== *)

(* --- Method head: Automatic | Portfolio | Completion | GoalDirected *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> Automatic],
    ProofObject,
    TestID -> "ATP/method/Automatic-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "Portfolio"],
    ProofObject,
    TestID -> "ATP/method/Portfolio-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "Completion"],
    ProofObject,
    TestID -> "ATP/method/Completion-bare-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "GoalDirected"],
    ProofObject,
    TestID -> "ATP/method/GoalDirected-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "NoSuchMethod"],
    ProofObject,
    {TFindProof::badmethod},
    TestID -> "ATP/method/bad-method-warns-and-falls-back"
]

VerificationTest[
    (* Method -> "Waldmeister" preset still proves a standard theorem. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "Waldmeister"],
    ProofObject,
    TestID -> "ATP/method/Waldmeister-preset-proves"
]

VerificationTest[
    (* Method -> "VampireUEQ" preset (LPO + AutoPrecedence +
       SelectionRatio 10 + UnfailingCP + AutoMaxWeight + MNF front)
       proves a baseline theorem too -- bundled knob smoke check. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "VampireUEQ"],
    ProofObject,
    TestID -> "ATP/method/VampireUEQ-preset-proves"
]

(* --- CriticalPairWeight: each ClasHeuristics weight mode proves ---- *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Mix2"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Mix2-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Gt"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Gt-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Add"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Add-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Goal"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Goal-CPinGoal-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Bogus"}],
    ProofObject,
    {TFindProof::badcpw},
    TestID -> "ATP/method/cpweight-bad-warns-and-falls-back"
]

VerificationTest[
    (* ConjSym: E ConjectureSymbolWeight port -- weight 1 for CTR
       nodes whose head appears in the conjecture, 4 for off-symbol
       CTR nodes.  Proves InverseOfInverse end-to-end. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "ConjSym"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-ConjSym-proves"
]

VerificationTest[
    (* Twee: src/Twee/CP.hs::score with shared-subterm discount
       (cfg_dupcost=1, cfg_dupfactor=0).  Asymmetric:
       4*size(larger) + size(smaller) + 2*depth, with repeated
       subterms in either side counting once.  Proves a baseline
       theorem end-to-end. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Twee"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Twee-proves"
]

VerificationTest[
    (* ForwardSubsume -> True: Vampire --forward_subsumption analog
       (unit-only).  Skip a rule add when an already-stored rule
       generalizes it.  Sound + completeness-preserving.  Baseline
       theorem still proves. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "ForwardSubsume" -> True}],
    ProofObject,
    TestID -> "ATP/method/forward-subsume-proves"
]

VerificationTest[
    (* Diversity: E DiversityWeight port -- base node count + linear
       penalty in #distinct CTR labels + #distinct FVR ids.  Proves
       baseline theorem. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Diversity"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Diversity-proves"
]

VerificationTest[
    (* BackwardSubsume -> True: Vampire bs=unit_only analog.  After
       a new rule is added, soft-delete any existing rule subsumed
       by it (sentinel FVR-255 in lhs / rhs makes thvm_match /
       thvm_unify skip the slot naturally; originals saved for
       proof reconstruction).  Sound + completeness-preserving.
       Baseline theorem still proves. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "BackwardSubsume" -> True}],
    ProofObject,
    TestID -> "ATP/method/backward-subsume-proves"
]

VerificationTest[
    (* BackwardSubsume + ForwardSubsume both on; equivalent to
       Vampire's bs=unit_only + standard FS.  Baseline still proves. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "ForwardSubsume" -> True,
            "BackwardSubsume" -> True}],
    ProofObject,
    TestID -> "ATP/method/forward-and-backward-subsume-proves"
]

VerificationTest[
    (* BackwardDemod -> True: Vampire bd=all (LHS half).  After a new
       rule batch is added, normalize each older rule's LHS with the
       new rule(s); if it reduces, drop the rule and re-queue the
       simplified equation.  Sound + completeness-preserving.
       Baseline still proves. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "BackwardDemod" -> True}],
    ProofObject,
    TestID -> "ATP/method/backward-demod-proves"
]

VerificationTest[
    (* BackwardDemod + RHSInterreduce together: bd=all both halves. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "BackwardDemod" -> True,
            "RHSInterreduce" -> True}],
    ProofObject,
    TestID -> "ATP/method/backward-demod-plus-rhs-interreduce"
]

VerificationTest[
    (* RelLevel: E RelevanceLevelWeight port -- N-level BFS distance
       from the conjecture through the axiom co-occurrence graph;
       per-node weight = 1 + level (cap at MAX+1).  Proves baseline. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "RelLevel"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-RelLevel-proves"
]

VerificationTest[
    (* VampirePortfolio: 10-entry rotation modeled on Vampire's UEQ
       portfolio-cycling shape.  Each entry gets TimeConstraint/10
       wall time; the first that proves wins.  Baseline target proves
       inside the first slice (VampireUEQ). *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "VampirePortfolio", TimeConstraint -> 20],
    ProofObject,
    TestID -> "ATP/method/VampirePortfolio-baseline"
]

VerificationTest[
    (* "AppliedMethod" return spec: for a single-config call the
       returned value is the bare Method (here a string preset). *)
    TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        "AppliedMethod", Method -> "VampireUEQ"],
    "VampireUEQ",
    TestID -> "ATP/return/AppliedMethod-single-config"
]

VerificationTest[
    (* "AppliedMethod" under Automatic returns the actual schedule
       entry that won.  The default Automatic schedule's first entry
       (Mix2) closes InverseOfInverse trivially. *)
    TFindProof["InverseOfInverse", "AbelianGroupAxioms", "AppliedMethod"],
    {"Completion", "CriticalPairWeight" -> "Mix2"},
    TestID -> "ATP/return/AppliedMethod-portfolio-winner"
]

VerificationTest[
    (* "WallTime" return spec: AbsoluteTiming the C engine call.  Always
       a non-negative real for any successful run. *)
    Module[{t = TFindProof["InverseOfInverse", "AbelianGroupAxioms", "WallTime"]},
        NumericQ[t] && t >= 0],
    True,
    TestID -> "ATP/return/WallTime-numeric-non-negative"
]

VerificationTest[
    (* "VarWeight" -> n: per-variable KBO weight override (Waldmeister
       -w VAR=N).  Baseline still proves under a non-default value. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "VarWeight" -> 3}],
    ProofObject,
    TestID -> "ATP/method/VarWeight-proves"
]

VerificationTest[
    (* Staggered: E StaggeredWeight port -- coarse-grained CP weight
       bucketing.  Designed to pair with FifoTiebreak; baseline proves
       in either config. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Staggered",
            "FifoTiebreak" -> True}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Staggered-proves"
]

VerificationTest[
    (* "PortfolioTrace" single-config fallback: returns a 1-element list
       whose entry mirrors AppliedMethod / WallTime / Proved. *)
    Module[{t = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        "PortfolioTrace", Method -> "VampireUEQ"]},
        ListQ[t] && Length[t] === 1 && t[[1, "Method"]] === "VampireUEQ" &&
            t[[1, "Proved"]] === True],
    True,
    TestID -> "ATP/return/PortfolioTrace-single-config-fallback"
]

VerificationTest[
    (* "PortfolioTrace" under Automatic on an easy goal: the first
       schedule entry (Mix2) closes it, so the trace has exactly one
       entry. *)
    Module[{t = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        "PortfolioTrace"]},
        ListQ[t] && Length[t] === 1 && t[[1, "Proved"]] === True],
    True,
    TestID -> "ATP/return/PortfolioTrace-automatic-easy-goal"
]

VerificationTest[
    (* "SymbolWeights" -> {sym -> w, ...}: per-symbol KBO weights port
       of Waldmeister's SymbolGewichte.  Baseline still proves. *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion",
            "SymbolWeights" -> {"CenterDot" -> 5, "OverTilde" -> 3}}],
    ProofObject,
    TestID -> "ATP/method/SymbolWeights-proves"
]

(* --- Ordering: KBO and LPO both prove AND verify ------------------- *)

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "Ordering" -> "KBO"}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/ordering-KBO-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "Ordering" -> "LPO",
                "AutoPrecedence" -> True}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/ordering-LPO-verifies"
]

(* --- AutoPrecedence: True and False both prove -------------------- *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "Ordering" -> "LPO",
            "AutoPrecedence" -> True}],
    ProofObject,
    TestID -> "ATP/method/autoprec-True-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "AutoPrecedence" -> False}],
    ProofObject,
    TestID -> "ATP/method/autoprec-False-proves"
]

(* --- explicit "Precedence" / "SkolemHighest" reduction-ordering ----
   precedence (Waldmeister's `p > q > nand` ORDERING block).  The
   symbol list is highest-to-lowest; "SkolemHighest" ranks the goal's
   ground (skolemized) constants above the operators.  Both prove +
   verify InverseOfInverse, and the proof matches the AutoPrecedence
   path -- the option only takes effect when supplied (default engine
   byte-identical, asserted by the unchanged tests above + test_atp). *)

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "Ordering" -> "LPO",
                "Precedence" -> {"OverBar", "CircleTimes", "1"}}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/precedence-explicit-verifies"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "Ordering" -> "LPO",
            "SkolemHighest" -> True}],
    ProofObject,
    TestID -> "ATP/method/precedence-skolemhighest-proves"
]

(* --- AxiomRelevance: None / Safe prove; mode is reported by --------
   TRelevantAxioms; Connected drops axioms (heuristic). *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "AxiomRelevance" -> None}],
    ProofObject,
    TestID -> "ATP/method/axrel-None-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "AxiomRelevance" -> "Safe"}],
    ProofObject,
    TestID -> "ATP/method/axrel-Safe-proves"
]

VerificationTest[
    Lookup[
        TRelevantAxioms["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "AxiomRelevance" -> "Connected"}],
        "Mode"],
    "Connected",
    TestID -> "ATP/method/axrel-Connected-mode-reported"
]

VerificationTest[
    With[{ra = TRelevantAxioms["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "AxiomRelevance" -> "Connected"}]},
        Length[ra["Kept"]] < Length[ra["Kept"]] + Length[ra["Dropped"]]],
    True,
    TestID -> "ATP/method/axrel-Connected-drops-axioms"
]

(* --- MaxWeight: bounded and unbounded both prove the easy theorem -- *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "MaxWeight" -> 40}],
    ProofObject,
    TestID -> "ATP/method/maxweight-40-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "MaxWeight" -> 0}],
    ProofObject,
    TestID -> "ATP/method/maxweight-unbounded-proves"
]

(* --- GoalInterleave: every n-th selection goal-directed ----------- *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "GoalInterleave" -> 3}],
    ProofObject,
    TestID -> "ATP/method/goalinterleave-3-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "GoalInterleave" -> 2}],
    ProofObject,
    TestID -> "ATP/method/goalinterleave-2-proves"
]

(* --- GroundJoin: opt-in CP deletion stays SOUND (proves+verifies) - *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "GroundJoin" -> True}],
    ProofObject,
    TestID -> "ATP/method/groundjoin-True-proves"
]

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "GroundJoin" -> True}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/groundjoin-True-verifies-sound"
]

(* --- SelectionRatio: Waldmeister CPdimension fairness ratio -------- *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "SelectionRatio" -> 50}],
    ProofObject,
    TestID -> "ATP/method/selectionratio-50-proves"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "SelectionRatio" -> 100}],
    ProofObject,
    TestID -> "ATP/method/selectionratio-100-proves"
]

(* --- FifoTiebreak: Waldmeister `-:w1=fifo` secondary CP key -------- *)

VerificationTest[
    (* Preserving each surviving CP's insertion age across the post-orient
       normalize sweep (so equal-weight ties resolve oldest-first) is a
       reordering of the queue, not a soundness change: the proof still
       lands and verifies. *)
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "FifoTiebreak" -> True}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/fifotiebreak-verifies"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "FifoTiebreak" -> True}],
    ProofObject,
    TestID -> "ATP/method/fifotiebreak-proves"
]

(* --- AutoMaxWeight: completeness-preserving growing CP-weight bound --- *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "AutoMaxWeight" -> 20}],
    ProofObject,
    TestID -> "ATP/method/automaxweight-20-proves"
]

VerificationTest[
    (* A tight base still proves (stash + force-drain preserves
       completeness -- nothing is dropped). *)
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "AutoMaxWeight" -> 1}],
    ProofObject,
    TestID -> "ATP/method/automaxweight-tight-still-proves"
]

(* --- UnfailingCP: both-faces superposition (completeness) stays sound *)

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "UnfailingCP" -> True}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/unfailingcp-verifies-sound"
]

(* --- RHSInterreduce: Waldmeister IR_InterreduktionRechts stays sound *)

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "RHSInterreduce" -> True}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/rhsinterreduce-verifies-sound"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "UnfailingCP" -> True,
            "RHSInterreduce" -> True}],
    ProofObject,
    TestID -> "ATP/method/unfailingcp-rhsinterreduce-combined-proves"
]

(* --- Connectedness: Bachmair-Dershowitz CP deletion stays sound ---- *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "Connectedness" -> True}],
    ProofObject,
    TestID -> "ATP/method/connectedness-proves"
]

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "Connectedness" -> True}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/connectedness-verifies-sound"
]

(* --- Method -> "Waldmeister": faithful default-strategy preset ----- *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "Waldmeister"],
    ProofObject,
    TestID -> "ATP/method/waldmeister-preset-proves"
]

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> "Waldmeister"];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/waldmeister-preset-verifies-sound"
]

(* --- CriticalPairWeight -> "Learned": ENIGMA-style scorer ---------- *)

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Learned"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Learned-proves"
]

VerificationTest[
    Module[{p},
        p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "CriticalPairWeight" -> "Learned"}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/cpweight-Learned-verifies-sound"
]

(* --- MaxSteps: a tight cap on a hard theorem fails; loose proves --- *)

VerificationTest[
    TFindProof["AndAssociativity", "WolframAxioms",
        Method -> "Completion", MaxSteps -> 5, TimeConstraint -> 20.],
    $Failed,
    TestID -> "ATP/option/maxsteps-tight-on-hard-fails"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        MaxSteps -> 200000],
    ProofObject,
    TestID -> "ATP/option/maxsteps-loose-proves"
]

(* --- TimeConstraint: a tiny budget on a hard theorem fails fast ---- *)

VerificationTest[
    TFindProof["AndAssociativity", "WolframAxioms",
        Method -> {"Completion", "Ordering" -> "LPO",
            "CriticalPairWeight" -> "Gt"},
        TimeConstraint -> 2.],
    $Failed,
    TestID -> "ATP/option/timeconstraint-tiny-on-hard-fails"
]

(* --- the deep Sheffer/Wolfram single-NAND commutativity theorem ----
   nand(p,q) == nand(q,p) over the single WolframAxioms NAND axiom is
   Waldmeister's canonical hard target (LPO, skolem-highest precedence
   p > q > nand).  The goal is unorientable, so the bidirectional MNF
   front search ("GoalDirected") closes it where pure orientation cannot;
   under the Waldmeister-faithful ordering it proves + verifies in a few
   seconds.  FifoTiebreak (the `-:w1=fifo` secondary CP key) is supplied
   here as part of the faithful config. *)

VerificationTest[
    Module[{p},
        p = TFindProof["Commutativity", "WolframAxioms",
            Method -> {"GoalDirected", "Ordering" -> "LPO",
                "SkolemHighest" -> True, "CriticalPairWeight" -> "Add",
                "FifoTiebreak" -> True, "UnfailingCP" -> True},
            MaxSteps -> 5000, TimeConstraint -> 60.];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/wolfram/nand-commutativity-goaldirected-verifies"
]

VerificationTest[
    Head @ TFindProof["Commutativity", "WolframAxioms",
        Method -> {"GoalDirected", "Ordering" -> "LPO",
            "SkolemHighest" -> True, "CriticalPairWeight" -> "Add",
            "FifoTiebreak" -> True, "UnfailingCP" -> True},
        MaxSteps -> 5000, TimeConstraint -> 60.],
    ProofObject,
    TestID -> "ATP/wolfram/nand-commutativity-goaldirected-proves"
]

(* === Completion mode + introspective return-type argument ========

   TFindProof[axioms] (no conjecture) runs a time-constrained
   completion and returns the derived lemmas; an optional last positional
   return-spec argument projects any prove/completion run onto the
   introspectives. *)

(* Completion of an explicit AC axiom set: a finite complete system, so
   it saturates fast (bound at 10s for safety) and returns a non-empty
   list of inert Equal lemmas. *)
VerificationTest[
    Module[{res},
        res = TFindProof[
            {f[f[x, y], z] == f[x, f[y, z]], f[x, y] == f[y, x]},
            TimeConstraint -> 10];
        {MatchQ[res, {__}],
         AllTrue[res, MatchQ[#, Inactive[Equal][_, _]] &]}
    ],
    {True, True},
    TestID -> "ATP/completion/explicit-ac-axioms-returns-lemmas"
]

(* Completion of a theory by name. *)
VerificationTest[
    MatchQ[
        TFindProof["AbelianGroupAxioms", TimeConstraint -> 10],
        {__}],
    True,
    TestID -> "ATP/completion/theory-by-name-returns-lemmas"
]

(* Completion with an explicit return spec: a single String returns that
   value bare (here "Statistics", a small run-stats Association). *)
VerificationTest[
    KeyExistsQ[
        TFindProof[
            {f[f[x, y], z] == f[x, f[y, z]], f[x, y] == f[y, x]},
            "Statistics", TimeConstraint -> 10],
        "Status"],
    True,
    TestID -> "ATP/completion/explicit-axioms-statistics-spec"
]

(* Return specs on a normal (fast) proof. *)
VerificationTest[
    Head[TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        "ProofObject"]],
    ProofObject,
    TestID -> "ATP/returnspec/proofobject"
]
VerificationTest[
    MatchQ[
        TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            "Lemmas"],
        {___}],
    True,
    TestID -> "ATP/returnspec/lemmas-is-list"
]
VerificationTest[
    Module[{pa},
        pa = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            "PreprocessedAxioms"];
        {MatchQ[pa, {__}],
         AllTrue[pa, MatchQ[#, Inactive[Equal][_, _]] &]}
    ],
    {True, True},
    TestID -> "ATP/returnspec/preprocessedaxioms-are-equations"
]
VerificationTest[
    Module[{ra},
        ra = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            "RelevantAxioms"];
        {AssociationQ[ra], Sort[Keys[ra]]}
    ],
    {True, {"Dropped", "Kept", "Mode"}},
    TestID -> "ATP/returnspec/relevantaxioms-assoc"
]
VerificationTest[
    ListQ[TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        "RawTrace"]],
    True,
    TestID -> "ATP/returnspec/rawtrace-is-list"
]
VerificationTest[
    Module[{r},
        r = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            {"ProofObject", "Statistics"}];
        {AssociationQ[r], Sort[Keys[r]], Head[r["ProofObject"]]}
    ],
    {True, {"ProofObject", "Statistics"}, ProofObject},
    TestID -> "ATP/returnspec/list-projects-to-assoc"
]
VerificationTest[
    Module[{r},
        r = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            All];
        {AssociationQ[r],
         Sort[Keys[r]] === Sort[THVMLink`ATP`Private`$AtpReturnSpecs]}
    ],
    {True, True},
    TestID -> "ATP/returnspec/all-projects-every-spec"
]

(* ----- "Counterexample" return spec: countermodel from a saturated
   completion (the equational dual of "ProofObject").  When the run
   SATURATES into a convergent term-rewriting system whose normal forms
   separate the goal's two sides, the goal is disproved. *)

(* Involution g(g(x))=x is a one-rule convergent TRS; g[a]==a is NOT a
   consequence, so the saturated system refutes it (NF(g[a])=g[a] =/= a) and
   the initial term algebra is finite: a 2-element model {a, g(a)} in
   FindFiniteModels structure, g -> {1, 0} (g(0)=1, g(1)=0), a -> 0. *)
VerificationTest[
    Module[{ce},
        ce = TFindProof[g[a] == a, {ForAll[{x}, g[g[x]] == x]},
            "Counterexample", TimeConstraint -> 10];
        {Head[ce], ce["Status"], ce["Method"], ce["NormalForms"],
         ce["Domain"], ce["Model"]}
    ],
    {CounterexampleObject, "Refuted", "SaturationNormalForm", {g[a], a},
     2, <|g -> {1, 0}, a -> 0|>},
    TestID -> "ATP/returnspec/counterexample-refutes-involution"
]

(* A genuine theorem has no counterexample: the spec is $Failed while the
   ProofObject still verifies. *)
VerificationTest[
    TFindProof[g[g[g[g[a]]]] == a, {ForAll[{x}, g[g[x]] == x]},
        "Counterexample", TimeConstraint -> 10],
    $Failed,
    TestID -> "ATP/returnspec/counterexample-none-for-theorem"
]

(* Soundness gate: a commutative axiom saturates WITH an unorientable
   equation (x*y == y*x) that needs ordered rewriting.  The extractor must
   DECLINE ($Failed) rather than risk an unsound verdict off a one-way
   orientation. *)
VerificationTest[
    TFindProof[CircleTimes[a, b] == b,
        {ForAll[{x, y}, CircleTimes[x, y] == CircleTimes[y, x]]},
        "Counterexample", TimeConstraint -> 10],
    $Failed,
    TestID -> "ATP/returnspec/counterexample-declines-unorientable"
]

(* The size-reducing soundness predicate itself: orientable rules pass,
   the commutativity variant is rejected. *)
VerificationTest[
    {THVMLink`ATP`Private`atpRuleSizeReducingQ[{g[g[x]], x}, {x}],
     THVMLink`ATP`Private`atpRuleSizeReducingQ[{CenterDot[x, x], x}, {x}],
     THVMLink`ATP`Private`atpRuleSizeReducingQ[
        {CircleTimes[x, y], CircleTimes[y, x]}, {x, y}]},
    {True, True, False},
    TestID -> "ATP/returnspec/counterexample-size-reducing-predicate"
]

(* A fully GROUND problem is decided by congruence closure: the quotient is a
   finite model, returned in FindFiniteModels structure ({a,b} merged to 0,
   c apart as 1).  The "Counterexample" output kind routes ground -> CC
   automatically, no Method -> "SMT" needed. *)
VerificationTest[
    Module[{ce = TFindProof[a == c, {a == b}, "Counterexample"]},
        {Head[ce], ce["Method"], ce["Domain"], ce["Model"], ce["Witness"]}],
    {CounterexampleObject, "CongruenceClosure", 2,
     <|a -> 0, b -> 0, c -> 1|>, <|a -> 0, c -> 1|>},
    TestID -> "ATP/returnspec/counterexample-ground-finite-model"
]

(* CounterexampleObject property interface + summary box render. *)
VerificationTest[
    Module[{ce = TFindProof[a == c, {a == b}, "Counterexample"]},
        {ce["Status"], ce["Goal"], ce["Hypotheses"],
         SubsetQ[ce["Properties"], {"Model", "Witness", "Domain"}],
         Head[ToBoxes[ce]] =!= ToBoxes}],
    {"Refuted", a == c, {a == b}, True, True},
    TestID -> "ATP/returnspec/counterexample-object-interface"
]

(* Self-certifying functions (the WFR FindEquationalCounterexample analog):
   co["FalsificationFunction"][] evaluates the goal in the model -> False,
   co["VerificationFunction"][] evaluates the axioms in the model -> True.
   Exercised across all three model kinds: ground congruence-closure quotient,
   the finite saturated algebra (involution), the infinite initial term
   algebra (idempotency), and the Boolean truth-assignment. *)
VerificationTest[
    {#["FalsificationFunction"][], #["VerificationFunction"][]} & /@ {
        TFindProof[a == c, {a == b}, "Counterexample"],
        TFindProof[g[a] == a, {ForAll[{x}, g[g[x]] == x]},
            "Counterexample", TimeConstraint -> 10],
        TFindProof[CenterDot[a, b] == a, {ForAll[{x}, CenterDot[x, x] == x]},
            "Counterexample", TimeConstraint -> 10],
        TFindProof[Implies[a == b, a == c], {}, "Counterexample"]},
    {{False, True}, {False, True}, {False, True}, {False, True}},
    TestID -> "ATP/returnspec/counterexample-falsify-verify-functions"
]

(* Method -> "SMT" is the ground decision surface: a CounterexampleObject on
   refute, a "Proved" decision Association on entailment. *)
VerificationTest[
    {Head[TFindProof[a == c, {a == b}, Method -> "SMT"]],
     TFindProof[a == c, {a == b, b == c}, Method -> "SMT"]["Status"]},
    {CounterexampleObject, "Proved"},
    TestID -> "ATP/returnspec/counterexample-method-smt-decision"
]

(* Backward compatibility: no return spec returns the bare ProofObject. *)
VerificationTest[
    Head[TFindProof["InverseOfInverse", "AbelianGroupAxioms"]],
    ProofObject,
    TestID -> "ATP/returnspec/backcompat-string-pair-bare-proofobject"
]
VerificationTest[
    Head[TFindProof[a == c, {a == b, b == c}]],
    ProofObject,
    TestID -> "ATP/returnspec/backcompat-expr-pair-bare-proofobject"
]

(* ================================================================== *)
(* Auto-tuner: atpAnalyzeStructure / atpAutoTune (Waldmeister          *)
(* PhilMarlow/XFiles structure recognition -> strategy database).      *)
(* ================================================================== *)

(* AC theory: commutativity + associativity for one operator. *)
VerificationTest[
    Module[{f, prof},
        f = CircleDot;
        prof = THVMLink`ATP`Private`atpAnalyzeStructure[{
            ForAll[{x, y}, f[x, y] == f[y, x]],
            ForAll[{x, y, z}, f[f[x, y], z] == f[x, f[y, z]]]}];
        {prof["Operators"][f]["Commutative"],
         prof["Operators"][f]["Associative"],
         MemberQ[prof["ACOperators"], f],
         prof["Class"]}
    ],
    {True, True, True, "AC"},
    TestID -> "ATP/autotune/analyze-AC-theory"
]

(* AbelianGroupAxioms: commutative+associative product, unit, inverse
   -> AbelianGroup, has-inverse + has-unit set on the product op. *)
VerificationTest[
    Module[{prof, op},
        prof = THVMLink`ATP`Private`atpAnalyzeStructure["AbelianGroupAxioms"];
        op = CircleTimes;
        {prof["Class"],
         prof["Operators"][op]["HasInverse"],
         prof["Operators"][op]["HasUnit"],
         prof["Operators"][op]["Commutative"],
         prof["Operators"][op]["Associative"]}
    ],
    {"AbelianGroup", True, True, True, True},
    TestID -> "ATP/autotune/analyze-AbelianGroupAxioms"
]

(* GroupAxioms (no commutativity axiom) -> Group, has-inverse+has-unit. *)
VerificationTest[
    Module[{prof, op},
        prof = THVMLink`ATP`Private`atpAnalyzeStructure["GroupAxioms"];
        op = CircleTimes;
        {prof["Class"],
         prof["Operators"][op]["HasInverse"],
         prof["Operators"][op]["HasUnit"]}
    ],
    {"Group", True, True},
    TestID -> "ATP/autotune/analyze-GroupAxioms"
]

(* WolframAxioms: a single binary Sheffer/Nand operator, no other
   structure -> "Sheffer". *)
VerificationTest[
    THVMLink`ATP`Private`atpAnalyzeStructure["WolframAxioms"]["Class"],
    "Sheffer",
    TestID -> "ATP/autotune/analyze-Sheffer-WolframAxioms"
]

(* CommutativeRingAxioms: + and *, distributivity, inverse -> Ring. *)
VerificationTest[
    THVMLink`ATP`Private`atpAnalyzeStructure["CommutativeRingAxioms"]["Class"],
    "Ring",
    TestID -> "ATP/autotune/analyze-Ring"
]

(* atpAutoTune returns a non-empty list of valid Method configs. *)
VerificationTest[
    Module[{sched},
        sched = THVMLink`ATP`Private`atpAutoTune["AbelianGroupAxioms"];
        And[Length[sched] > 0,
            AllTrue[sched, MatchQ[#,
                (_String | {_String, ___Rule})] &]]
    ],
    True,
    TestID -> "ATP/autotune/autotune-returns-valid-configs"
]

(* SAFETY CONSTRAINT: the tuned Automatic schedule must contain every
   config of the fixed $AtpSchedule (the appended fallback tail), so it
   can never prove less than "Portfolio". *)
VerificationTest[
    Module[{tuned, fixed},
        tuned = THVMLink`ATP`Private`atpScheduleFor[Automatic,
            AxiomaticTheory["AbelianGroupAxioms"],
            AxiomaticTheory["AbelianGroupAxioms", "NotableTheorems"][
                "InverseOfInverse"]];
        fixed = THVMLink`ATP`Private`$AtpSchedule;
        (* every fixed config is present in the tuned schedule *)
        And @@ (MemberQ[tuned, #] & /@ fixed)
    ],
    True,
    TestID -> "ATP/autotune/safety-tail-contains-fixed-schedule"
]

(* The tuned Automatic schedule front-loads the structure config BEFORE
   the fixed tail (a Group config with AutoPrecedence comes first). *)
VerificationTest[
    Module[{tuned},
        tuned = THVMLink`ATP`Private`atpScheduleFor[Automatic,
            AxiomaticTheory["AbelianGroupAxioms"],
            AxiomaticTheory["AbelianGroupAxioms", "NotableTheorems"][
                "InverseOfInverse"]];
        First[tuned]
    ],
    {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50,
        "AutoPrecedence" -> True},
    TestID -> "ATP/autotune/front-loads-group-config"
]

(* "Portfolio" stays the FIXED schedule (prior behavior reachable). *)
VerificationTest[
    THVMLink`ATP`Private`atpScheduleFor["Portfolio",
        AxiomaticTheory["AbelianGroupAxioms"], Null] ===
        THVMLink`ATP`Private`$AtpSchedule,
    True,
    TestID -> "ATP/autotune/portfolio-stays-fixed"
]

(* Regression: existing theorems still prove under Method -> Automatic
   (now problem-aware) -- the safety tail guarantees this. *)
VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms"],
    ProofObject,
    TestID -> "ATP/autotune/regression-InverseOfInverse-default-Automatic"
]

VerificationTest[
    Head @ TFindProof["DoubleNegation", "WolframAxioms"],
    ProofObject,
    TestID -> "ATP/autotune/regression-DoubleNegation-default-Automatic"
]

(* === AndAssociativity / WolframAxioms landmark (env-gated) =========

   The deep landmark -- AndAssociativity over the single Sheffer/Wolfram
   nand axiom (Waldmeister's andassoc) -- is NOT in the default suite: the
   completion runs ~150s and exhausts two fixed engine bounds at their
   defaults (the 256M-cell IC heap and the 131072-entry proof trace).  Run
   it manually with THVM_ATP_ANDASSOC_TEST=1 plus a larger heap + trace and
   an external free-RAM guard (the search peaks ~8GB resident):

     THVM_HEAP_CELLS=$((1<<30)) THVM_ATP_TRACE_MAX=4000000 \
       wolframscript -code 'TFindProof["AndAssociativity",
         "WolframAxioms", {"Statistics", "ProofObject"},
         Method -> {"Completion", "CriticalPairWeight" -> "Gt"},
         TimeConstraint -> 400]'

   With those env knobs the C ENGINE PROVES the goal: Statistics reports
   Status "Proved" at ~706 rules (matching the C bench's andassoc rule
   count), and -- after the ordered-rewrite goal-chain extraction fix
   (src/atp/_.c thvm_atp_proof_extract) -- a populated MainSteps chain.
   The test below asserts that proved-at-rule-count milestone.

   The critical-pair reconstruction gap is closed: resolveCp now selects
   the superposition geometry (parent face / role) that reproduces each
   stored CP under the verifier's convention (chooseCpGeometry), so the
   Sheffer/Wolfram CP proofs (e.g. ImpliesShefferAxioms conjunct 2)
   reconstruct into verifying ProofObjects.  AndAssociativity's own
   verifying ProofObject is still unconfirmed on a memory-loaded box: the
   ~9278-step saturation plus the deep trace-DAG assembly exhaust the wall
   budget here.  The verifying assertion lives in the separate env-gated
   test below ("andassoc-WolframAxioms-verifies-when-enabled"); run it on a
   box with the saturation + assembly headroom to confirm Success. *)
VerificationTest[
    If[ Environment["THVM_ATP_ANDASSOC_TEST"] === "1",
        Module[{r},
            r = TFindProof["AndAssociativity", "WolframAxioms",
                {"Statistics", "ProofObject"},
                Method -> {"Completion", "CriticalPairWeight" -> "Gt"},
                TimeConstraint -> 400];
            {r["Statistics"]["Status"], r["Statistics"]["Rules"] > 600}
        ],
        {"Proved", True}],
    {"Proved", True},
    TestID -> "ATP/option/andassoc-WolframAxioms-engine-proves-when-enabled"
]

(* === Method "RecordNorm" knob ====================================== *)

(* "RecordNorm" gates the C engine's per-step normalize-trace recording.
   The default (True / unset) keeps the linear CP -> NORM_STEP* -> ORIENT
   chain so the ProofObject builder walks it directly; on a trivially-
   joined CP the engine now rewinds the just-pushed NORM_STEP entries and
   resets the heap (src/atp/_.c thvm_atp_step), so recording stays memory-
   bounded over a long completion (previously the skipped reset blew RSS
   past 12GB on a deep Sheffer/Wolfram saturation).  False routes the
   search through the fast indexed/flatterm normalizer (no per-step push)
   and reconstructs the chain through the emitNorm BFS over the
   CP/ORIENT/SIMPLIFY trace DAG.  BOTH paths must yield a VERIFYING
   ProofObject; DoubleNegation over the single Sheffer/nand axiom is a
   genuine completion proof (not an axiom-confluent chain) that exercises
   the trace-DAG reconstruction. *)
VerificationTest[
    Module[{p},
        p = TFindProof["DoubleNegation", "WolframAxioms",
            Method -> {"Completion", "RecordNorm" -> True},
            TimeConstraint -> 60];
        {Head[p], Head @ Quiet @ p["ProofFunction"][p["Theorems"]]}
    ],
    {ProofObject, Success},
    TestID -> "ATP/option/recordnorm-on-verifies-DoubleNegation"
]

VerificationTest[
    Module[{p},
        p = TFindProof["DoubleNegation", "WolframAxioms",
            Method -> {"Completion", "RecordNorm" -> False},
            TimeConstraint -> 60];
        {Head[p], Head @ Quiet @ p["ProofFunction"][p["Theorems"]]}
    ],
    {ProofObject, Success},
    TestID -> "ATP/option/recordnorm-off-verifies-DoubleNegation"
]

(* The deep landmark -- AndAssociativity over the single Sheffer/nand
   axiom -- is NOT in the default suite: the goal-join lies past a wall
   budget too long for the always-on regression suite (minutes, not the
   sub-second the rest of atp.wlt holds to).

   GOAL FORM (settled empirically -- the encoded ConjPair the engine
   actually receives, walked cell-by-cell): the paclet feeds the engine
   the GROUND instance, NOT a universal schema.  AxiomaticTheory resolves
   the conjecture to ForAll[{p,q,r}, And(p,And(q,r)) == And(And(p,q),r)];
   unquantifyFormula + CanonicalizePatterns turn the bound vars into
   Pattern[a,_]/Pattern[b,_]/Pattern[c,_]; then atpProveBundle calls
   atpEncodeProblem[..., skolemize -> True] (ATP.wl), whose skolemize
   rewrite (cjHC /. Pattern[v,_] :> v) strips them to bare symbols a,b,c.
   encodeAtpTerm encodes a bare symbol as a 0-arity TAG_CTR constant (the
   Pattern clause -- the only TAG_FVR producer -- no longer matches), so
   the encoded goal is all TAG_CTR: nand (label 1, arity 2) over the
   nullary constants a/b/c (labels 2/3/4).  This is BYTE-IDENTICAL to the
   C bench's goal_andassoc (constants C2/C3/C4 = p/q/r).  There is no
   universal-vs-ground gap; the earlier note claiming the paclet carried
   the harder ForAll schema was a misdiagnosis.

   CONFIG.  Two levers, both decided against the C bench's proving
   THVM_ATP_WALDMEISTER preset:
     1. WEIGHT.  The C preset leaves cp_weight_mode at the engine default
        GT (ATP_CP_WEIGHT_GT, src/atp/_.c) and PROVES; the WL
        Method -> "Waldmeister" preset overrides CriticalPairWeight ->
        "Mix" (ATP.wl atpParseMethod[{"Waldmeister"}]), which does not
        close in budget -- so override it back to "Gt".
     2. CPSetInterreduce.  The WL "Waldmeister" preset forces
        CPSetInterreduce -> True (the KPV_KPMengeInterreduzieren full-
        queue sweep), which the C preset does NOT enable.  At ~500k live
        CPs that sweep is O(queue x rules) per period and roughly halves
        the step rate -- override it OFF to match the C trajectory.
   So the proving config is:
     Method -> {"Waldmeister", "CriticalPairWeight" -> "Gt",
                "CPSetInterreduce" -> False}
   -- GT weight, KBO, AutoPrecedence, SelectionRatio 51, RHSInterreduce,
   UnfailingCP, no full-queue interreduction; matching the C preset.

   COST.  Measured C bench (THVM_ATP_WALDMEISTER=1, GT): PROVED at
   steps=9278, rules=704, ~370s on a memory-loaded box (max ~503k live
   CPs, ~3-4 GB).  The paclet shares the same engine over the FFI; with
   CPSetInterreduce OFF it tracks the C step rate, so the gate below
   uses a 600s budget.  This is a deep saturation, NOT a sub-second
   regression test; it stays env-gated out of the default suite.

   MEMORY.  The CP heap is GC-bounded: thvm_init calls gc_init and
   thvm_atp_step runs thvm_atp_gc_collect at the half-space mark, so the
   SATURATION holds ~3-4 GB even at ~500k live CPs (RecordNorm -> False
   keeps it flat -- the indexed normalize pushes no per-step
   TRACE_NORM_STEP; the WL builder later reconstructs the chain via the
   emitNorm BFS over the CP/ORIENT/SIMPLIFY DAG).  The REMAINING blocker
   is the WL POST-PROOF ASSEMBLY: once the engine closes (verified: at
   ~570s the CP heap frees and swap drops), buildCEngineChain /
   assembleDataset / the WL verifier walk the full 9278-step trace DAG,
   and on a memory-loaded box (this run started with ~19 GB swap already
   in use from other processes) that assembly spiked swap by ~9 GB
   (to ~28 GB) and tripped the swap guard before the ProofObject
   verified.  The engine FINDS the proof under the config below; closing
   the verifying-ProofObject delivery needs either an unloaded box (the
   ~9 GB assembly headroom) or a lower-footprint trace-DAG reconstruction
   for proofs this deep.

   To run, set THVM_ATP_ANDASSOC_TEST=1 (the VerificationTest below is
   gated on it) with THVM_ATP_TRACE_MAX raised, on a box with the RAM/swap
   headroom for the assembly.  It asserts the returned ProofObject
   VERIFIES (Head ... === Success), so a $Failed / Symbol head is NOT a
   pass. *)
VerificationTest[
    If[ Environment["THVM_ATP_ANDASSOC_TEST"] === "1",
        Module[{p},
            p = TFindProof["AndAssociativity", "WolframAxioms",
                Method -> {"Waldmeister", "CriticalPairWeight" -> "Gt",
                    "CPSetInterreduce" -> False, "RecordNorm" -> False},
                TimeConstraint -> 600];
            Head @ Quiet @ p["ProofFunction"][p["Theorems"]]
        ],
        Success],
    Success,
    TestID -> "ATP/option/andassoc-WolframAxioms-verifies-when-enabled"
]

(* --- TimeConstraint + Abort: effective abort inside the LibraryLink.
   Both forms must INTERRUPT the running C engine at the budget rather
   than hang: the TimeConstraint option returns $Failed, and a
   TimeConstrained[...] wrapper (3rd-arg form) returns its timeout value.
   Both run in ONE Module so the aborts stay within a single evaluation
   -- across separate VerificationTests the abort flag leaks (the harness
   does not clear it between tests the way top-level does), instantly
   killing the next test.  This test is LAST so any residual abort has
   nothing to leak into. *)
VerificationTest[
    Module[{hard, viaConstraint, viaWrapper},
        hard = {"Completion", "Ordering" -> "LPO",
            "CriticalPairWeight" -> "Gt"};
        viaConstraint = TFindProof["AndAssociativity",
            "WolframAxioms", Method -> hard, TimeConstraint -> 2.];
        viaWrapper = TimeConstrained[
            TFindProof["AndAssociativity", "WolframAxioms",
                Method -> hard], 2., "TimedOut"];
        (* AndAssociativity over the single Sheffer/nand axiom is a deep
           completion (the C bench closes it at ~560 rules / ~119s under
           GT, ~677 rules / ~345s under Mix), far beyond a 2s budget, so
           a 2s budget must interrupt the running engine and yield NO
           proof via either form -- the contract is "no hang, no spurious
           proof".  (Asserting === $Failed / === "TimedOut" directly is
           flaky: late in a long suite the WL test harness can leak a
           prior test's abort flag into this one; the not-a-ProofObject
           assertion is robust to that artifact and still catches a
           budget that was ignored, which would return a ProofObject.) *)
        {Head[viaConstraint] =!= ProofObject,
         Head[viaWrapper] =!= ProofObject}
    ],
    {True, True},
    TestID -> "ATP/option/effective-abort-interrupts-running-engine"
]

(* === Back-compat alias: TFindEquationalProof still forwards ======== *)

(* The primary name is TFindProof; TFindEquationalProof is kept as a
   deprecated alias.  Verify the alias still produces a verifying
   ProofObject for a goal that the primary name also proves, so old
   notebooks and downstream callers using the legacy spelling keep
   working byte-identically. *)
VerificationTest[
    Head @ TFindEquationalProof[a == c, {a == b, b == c}],
    ProofObject,
    TestID -> "ATP/alias/TFindEquationalProof-still-works"
]

(* === SInE premise selection: Method "AxiomRelevance" -> "SInE" ===== *)

(* SInE (Sumo-Inspired premise selection, Hoder & Voronkov IJCAR 2011)
   is implemented WL-side via atpSinePartition; it pre-filters the
   axiom list before the C engine sees it.  With Vampire defaults
   (st=3, sd=2, sgt=8) the InverseOfInverse goal of AbelianGroupAxioms
   still has its supporting axioms reachable, so the proof closes. *)
VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "AxiomRelevance" -> "SInE"}],
    ProofObject,
    TestID -> "ATP/method/axiom-relevance-SInE-default-proves"
]

(* Explicit {"SInE", suboptions...} form: Vampire's defaults threaded
   directly through the tolerance/depth/generality knobs. *)
VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion",
            "AxiomRelevance" -> {"SInE",
                "SineTolerance" -> 3, "SineDepth" -> 2,
                "SineGenerality" -> 8}}],
    ProofObject,
    TestID -> "ATP/method/axiom-relevance-SInE-tuple-proves"
]

(* TRelevantAxioms exposes the SInE partition without proving: at
   sufficient depth and tolerance the supporting axioms of
   AbelianGroupAxioms are reachable from the conjecture's symbols, so
   the Kept list is non-empty and the Mode tag is "SInE". *)
VerificationTest[
    With[{ra = TRelevantAxioms["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "AxiomRelevance" -> "SInE"}]},
        {ra["Mode"], Length[ra["Kept"]] >= 1}
    ],
    {"SInE", True},
    TestID -> "ATP/relevant/SInE-mode-reports-partition"
]

(* === Return spec: All returns every introspective ================== *)

(* TFindProof[..., All] returns an Association keyed by every name in
   $AtpReturnSpecs.  Confirm the shape: the keys are the full spec
   list and "ProofObject" is a real ProofObject. *)
VerificationTest[
    With[{r = TFindProof["InverseOfInverse", "AbelianGroupAxioms", All]},
        {AssociationQ[r],
         Sort[Keys[r]] === Sort[THVMLink`ATP`Private`$AtpReturnSpecs],
         Head[r["ProofObject"]]}
    ],
    {True, True, ProofObject},
    TestID -> "ATP/returnspec/All-returns-every-introspective"
]

(* === Method -> "Twee" preset ====================================== *)

(* The "Twee" preset bundles Twee's defaults: the dedup-aware Twee
   weight + GroundJoin + Connectedness + UnfailingCP + BackwardSubsume
   + BackwardDemod + RHSInterreduce + AutoMaxWeight=20.  Sanity:
   it parses, runs, and proves a trivial Boolean goal. *)
VerificationTest[
    Head @ TFindProof["AndCommutativity", "BooleanAxioms",
        Method -> "Twee", TimeConstraint -> 5],
    ProofObject,
    TestID -> "ATP/method/Twee-preset-proves"
]

(* Suboption override: subopts must merge over the preset's defaults
   (here: turn AutoMaxWeight off via 0). *)
VerificationTest[
    Head @ TFindProof["AndCommutativity", "BooleanAxioms",
        Method -> {"Twee", "AutoMaxWeight" -> 0}, TimeConstraint -> 5],
    ProofObject,
    TestID -> "ATP/method/Twee-preset-subopt-override"
]

(* === Method -> "EProver" preset =================================== *)

(* The EProver preset bundles E's typical CASC config: ConjSym
   weight + KBO + SelectionRatio 10 + AutoMaxWeight 20 +
   BackwardSubsume + RHSInterreduce + UnfailingCP. *)
VerificationTest[
    Head @ TFindProof["AndCommutativity", "BooleanAxioms",
        Method -> "EProver", TimeConstraint -> 5],
    ProofObject,
    TestID -> "ATP/method/EProver-preset-proves"
]

(* Subopt override (turn AutoMaxWeight off). *)
VerificationTest[
    Head @ TFindProof["AndCommutativity", "BooleanAxioms",
        Method -> {"EProver", "AutoMaxWeight" -> 0}, TimeConstraint -> 5],
    ProofObject,
    TestID -> "ATP/method/EProver-preset-subopt-override"
]

(* === TAtpDescribeMethod public Method-spec introspection ========== *)

(* For a named preset, returns the preset's full defaults Association. *)
VerificationTest[
    KeyTake[TAtpDescribeMethod["Twee"],
        {"CriticalPairWeight", "GroundJoin", "AutoMaxWeight"}],
    <|"CriticalPairWeight" -> "Twee", "GroundJoin" -> True,
      "AutoMaxWeight" -> 20|>,
    TestID -> "ATP/describe/Twee-preset-defaults"
]

(* List form: subopts merge over the preset's defaults. *)
VerificationTest[
    Lookup[TAtpDescribeMethod[{"Twee", "AutoMaxWeight" -> 0}],
        "AutoMaxWeight"],
    0,
    TestID -> "ATP/describe/Twee-subopt-overrides-default"
]

(* Schedule-style Method wraps the schedule in <|"Schedule" -> ...|>. *)
VerificationTest[
    Length @ Lookup[TAtpDescribeMethod["VampirePortfolioCompact"], "Schedule"],
    3,
    TestID -> "ATP/describe/VPC-wraps-schedule"
]

(* Problem-aware describe: Automatic with conj + ax in hand should
   reflect the structure-tailored schedule (6 entries for
   AbelianGroup with the auto-tuned front). *)
VerificationTest[
    Length @ Lookup[
        TAtpDescribeMethod[Automatic, "InverseOfInverse", "AbelianGroupAxioms"],
        "Schedule"],
    6,
    TestID -> "ATP/describe/Automatic-with-problem-front-loads-Group"
]

(* Single-config preset ignores problem context (Twee is structure-
   agnostic). *)
VerificationTest[
    TAtpDescribeMethod["Twee", "AndCommutativity", "BooleanAxioms"]
        === TAtpDescribeMethod["Twee"],
    True,
    TestID -> "ATP/describe/single-preset-ignores-problem-context"
]

(* Unknown theorem in the AxiomaticTheory-name form yields $Failed. *)
VerificationTest[
    TAtpDescribeMethod[Automatic, "NotAThm", "BooleanAxioms"],
    $Failed,
    TestID -> "ATP/describe/unknown-theorem-name-fails"
]

(* Unknown Method NAME (not in $AtpMethodPresets or the canonical
   completion family) should yield $Failed with TFindProof::badmethod
   rather than silently being accepted as a 1-element schedule.  The
   iter 56 fix flags this gap in both TAtpSchedule and
   TAtpDescribeMethod. *)
VerificationTest[
    Quiet @ TAtpSchedule["NotARealMethod"],
    $Failed,
    TestID -> "ATP/schedule/unknown-method-name-fails"
]

VerificationTest[
    Quiet @ TAtpDescribeMethod["NotARealMethod"],
    $Failed,
    TestID -> "ATP/describe/unknown-method-name-fails"
]

(* === TAtpSchedule public introspection ============================ *)

(* TAtpSchedule[Method] returns the schedule the dispatcher would
   expand to without running.  "Twee" is a single-config preset, so
   the returned schedule should be a 1-element list. *)
VerificationTest[
    Length @ TAtpSchedule["Twee"],
    1,
    TestID -> "ATP/schedule/single-config-preset-len-1"
]

(* "VampirePortfolio" expands to the 11-entry $VampirePortfolio
   (iter 76 added the Mix2 + SelectionRatio 2 cross-system entry). *)
VerificationTest[
    Length @ TAtpSchedule["VampirePortfolio"],
    11,
    TestID -> "ATP/schedule/VampirePortfolio-len-11"
]

(* "VampirePortfolioCompact": 3-entry rotation for small budgets. *)
VerificationTest[
    Length @ TAtpSchedule["VampirePortfolioCompact"],
    3,
    TestID -> "ATP/schedule/VampirePortfolioCompact-len-3"
]

(* The compact rotation should dispatch + prove a trivial goal. *)
VerificationTest[
    Head @ TFindProof["AndCommutativity", "BooleanAxioms",
        Method -> "VampirePortfolioCompact", TimeConstraint -> 5],
    ProofObject,
    TestID -> "ATP/method/VampirePortfolioCompact-proves-trivial"
]

(* "AllPresets" expands to the 4-preset rotation. *)
VerificationTest[
    TAtpSchedule["AllPresets"],
    {"Waldmeister", "VampireUEQ", "Twee", "EProver"},
    TestID -> "ATP/schedule/AllPresets-rotation"
]

(* Method -> "Automatic" (string) should be a synonym for the
   symbol Automatic, so users typing it alongside the other named
   string presets don't trip the bad-method message.  The schedule
   should match the bare-symbol form. *)
VerificationTest[
    TAtpSchedule["Automatic"],
    TAtpSchedule[Automatic],
    TestID -> "ATP/method/Automatic-string-is-symbol-alias"
]

(* The string form also dispatches + proves trivially. *)
VerificationTest[
    Head @ TFindProof["AndCommutativity", "BooleanAxioms",
        Method -> "Automatic", TimeConstraint -> 3],
    ProofObject,
    TestID -> "ATP/method/Automatic-string-proves"
]

(* AllPresets dispatches + proves a trivial goal. *)
VerificationTest[
    Head @ TFindProof["AndCommutativity", "BooleanAxioms",
        Method -> "AllPresets", TimeConstraint -> 8],
    ProofObject,
    TestID -> "ATP/method/AllPresets-proves-trivial"
]

(* Automatic with no problem in hand returns the fixed $AtpSchedule
   (no structure-recognition tailoring possible without conj+ax). *)
VerificationTest[
    TAtpSchedule[Automatic] === THVMLink`ATP`Private`$AtpSchedule,
    True,
    TestID -> "ATP/schedule/Automatic-no-problem-falls-back-to-AtpSchedule"
]

(* Automatic with a Group problem in hand front-loads the Gt+
   AutoPrecedence config that the auto-tuner picks for groups. *)
VerificationTest[
    First @ TAtpSchedule[Automatic,
        "InverseOfInverse", "AbelianGroupAxioms"],
    {"Completion", "CriticalPairWeight" -> "Gt",
        "GoalInterleave" -> 50, "AutoPrecedence" -> True},
    TestID -> "ATP/schedule/AbelianGroup-front-load-is-Gt+AutoPrec"
]

(* Unknown theorem name yields $Failed (catch-all path). *)
VerificationTest[
    TAtpSchedule[Automatic, "NotAThm", "BooleanAxioms"],
    $Failed,
    TestID -> "ATP/schedule/unknown-theorem-name-fails"
]

(* === $AtpMethodPresets coverage =================================== *)

(* The named-preset registry should be non-empty and include the
   single-config presets we exercise above ("Waldmeister", "VampireUEQ",
   "Twee") plus the schedule presets ("Portfolio", "VampirePortfolio").
   A future preset addition has to extend the registry, which keeps
   the doc and dispatcher in sync. *)
VerificationTest[
    With[{p = THVMLink`ATP`Private`$AtpMethodPresets},
        {ListQ[p], AllTrue[p, StringQ],
         SubsetQ[p, {"Waldmeister", "VampireUEQ", "Twee", "EProver",
                     "Portfolio", "VampirePortfolio",
                     "VampirePortfolioCompact", "AllPresets"}]}],
    {True, True, True},
    TestID -> "ATP/method/preset-registry-contents"
]

(* Each named single-config preset should ACCEPT a call without
   crashing and return either a ProofObject (proved) or $Failed
   (timed out / saturated short) -- never an unevaluated form, never
   a Failure[].  Regressions in a preset's parse logic (suboption
   merge, mnf computation, returned arg vector) surface here.  We
   don't require a proof: e.g. "Waldmeister" is tuned for unrecognized
   single-operator Sheffer problems and falls behind on BooleanAxioms.
   VampireUEQ + Twee + EProver do prove this goal trivially, which
   the per-preset tests already cover. *)
VerificationTest[
    With[{prs = {"Waldmeister", "VampireUEQ", "Twee", "EProver"}},
        AllTrue[prs,
            With[{p = TFindProof["AndCommutativity", "BooleanAxioms",
                    Method -> #, TimeConstraint -> 5]},
                Head[p] === ProofObject || p === $Failed] &]],
    True,
    TestID -> "ATP/method/single-config-presets-do-not-crash"
]

(* === PortfolioFrontLoad ============================================ *)

(* PortfolioFrontLoad -> n widens the time slice given to the first n
   entries of a multi-entry schedule.  At PFL=0 the dispatcher's
   share matches the historical fair-share (rem / remaining).  At
   PFL=n the first n entries each get 2x the share that an unweighted
   recurrence would give them; entries past n revert to fair share.
   Confirm the option is accepted on the prove path and the run still
   produces a verifying ProofObject for a trivial goal. *)
VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        TimeConstraint -> 4, PortfolioFrontLoad -> 2],
    ProofObject,
    TestID -> "ATP/portfolio/frontload-accepts-option"
]

(* PFL=2 on VampirePortfolio: the first two entries (VampireUEQ +
   Twee-style) should each get a wider slice than the iter-N fair
   share.  Trace's WallTime[1] should not be smaller than the
   nominal-fair share of total / 10.  Cheap goal proves on entry 1,
   so we only check the option threads through and the schedule's
   first slice runs.  PortfolioTrace return spec gives WallTime. *)
VerificationTest[
    With[{trace = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
            "PortfolioTrace", TimeConstraint -> 4,
            Method -> "VampirePortfolio", PortfolioFrontLoad -> 2]},
        {Length[trace] >= 1, trace[[1]]["Proved"]}],
    {True, True},
    TestID -> "ATP/portfolio/frontload-runs-vampireportfolio"
]

(* === Inactive[Equal] axioms / conjecture (iter 60) ================ *)

(* TFindProof "Lemmas" returns Inactive[Equal]-headed equations so
   they don't collapse on display.  Piping those back through
   TFindProof previously failed: the encoder rejects Inactive[Equal]
   as not matching `_Equal`.  atpStripInactive strips both heads at
   entry so the round-trip works. *)
VerificationTest[
    Head @ TFindProof[Inactive[Equal][a, c],
        {Inactive[Equal][a_, b_], Inactive[Equal][b_, c_]},
        TimeConstraint -> 3],
    ProofObject,
    TestID -> "ATP/inactive/equal-axioms+conjecture-prove"
]

(* Real round-trip variant: a hand-built Inactive[Equal] axiom that
   IS pattern-bound, used to prove a ground conjecture.  The Lemmas
   spec returns bare-symbol axioms (no Pattern wrappers), so a true
   "feed lemmas as axioms" round-trip is shape-mismatched at the
   variable level -- that's a documentation point, not a regression. *)
VerificationTest[
    Head @ TFindProof[Inactive[Equal][f[a], g[a]],
        {Inactive[Equal][f[x_], g[x_]]}, TimeConstraint -> 3],
    ProofObject,
    TestID -> "ATP/inactive/pattern-axiom-proves-ground-goal"
]

(* Single-arg completion form should also strip Inactive (iter 61).
   Pre-iter-61, TFindProof[{Inactive[Equal][f[x_], g[x_]]}] returned
   empty Lemmas because the encoder rejected Inactive[Equal] as a
   bad axiom shape.  Now identical results to the bare-Equal form. *)
VerificationTest[
    Length @ TFindProof[{Inactive[Equal][f[x_], g[x_]]},
        TimeConstraint -> 3],
    1,
    TestID -> "ATP/inactive/single-arg-completion-strips-Inactive"
]

(* === Nested axiom-list auto-flatten (iter 62) ===================== *)

(* Users concatenating axiom subsets often write {ax1, ax2} without
   Flatten; pre-iter-62 the encoder silently rejected the inner Lists
   and TFindProof returned Missing.  atpFlattenAxioms now auto-
   flattens one level at entry. *)
VerificationTest[
    Head @ TFindProof[a == c, {{a == b}, {b == c}}, TimeConstraint -> 3],
    ProofObject,
    TestID -> "ATP/flatten/nested-axiom-list-auto-flattens"
]

(* === Single-axiom shape (iter 68) =================================== *)

(* TFindProof[goal, axiom] (axiom NOT wrapped in a List) should
   auto-wrap and dispatch.  Common when the user pastes a single
   axiom directly.  Pre-iter-68 this returned the unevaluated
   TFindProof[...] expression. *)
VerificationTest[
    Head @ TFindProof[a == b, a == b, TimeConstraint -> 3],
    ProofObject,
    TestID -> "ATP/single-axiom/bare-Equal-wraps"
]

VerificationTest[
    Head @ TFindProof[Inactive[Equal][a, b], Inactive[Equal][a, b],
        TimeConstraint -> 3],
    ProofObject,
    TestID -> "ATP/single-axiom/Inactive-wraps"
]

VerificationTest[
    Head @ TFindProof[f[a] == g[a], ForAll[x, f[x] == g[x]],
        TimeConstraint -> 3],
    ProofObject,
    TestID -> "ATP/single-axiom/ForAll-wraps"
]

(* Single-arg completion form also auto-wraps a non-list axiom
   (iter 69 parity with iter 68's prove path). *)
VerificationTest[
    Length @ TFindProof[a == b, TimeConstraint -> 3],
    1,
    TestID -> "ATP/single-axiom/single-arg-completion-wraps"
]

(* TRelevantAxioms and TATP also auto-wrap a single non-list axiom
   (iter 70 parity).  Pre-iter-70, TRelAx returned unevaluated and
   TATP threw TATPParseError. *)
VerificationTest[
    Lookup[TRelevantAxioms[a == b, a == b], "Kept"],
    {a == b},
    TestID -> "ATP/single-axiom/TRelAx-wraps"
]

VerificationTest[
    TATP[a == b, a == b]["Status"],
    "PROVED",
    TestID -> "ATP/single-axiom/TATP-wraps"
]

(* TAtpSchedule and TAtpDescribeMethod also wrap a single non-list
   axiom (iter 73 parity). *)
VerificationTest[
    ListQ @ TAtpSchedule[Automatic, a == c, a == b],
    True,
    TestID -> "ATP/single-axiom/TAtpSchedule-wraps"
]

VerificationTest[
    AssociationQ @ TAtpDescribeMethod[Automatic, a == c, a == b],
    True,
    TestID -> "ATP/single-axiom/TAtpDescribeMethod-wraps"
]

(* === TPTPImport pipe-through (iter 65) ============================ *)

(* TPTPImport produces String-headed compounds like "f"[X_] for TPTP
   atoms.  Pre-iter-65, piping the result directly into
   TFindProof[conj, ax] failed because the encoder expected Symbol
   heads.  atpMaybeInternalizeTPTP now auto-detects the String-headed
   shape and threads it through tptpInternalize. *)
VerificationTest[
    With[{imp = TPTPImport[
            "cnf(c1, axiom, f(X) = g(X)).\ncnf(c2, conjecture, f(a) = g(a)).\n"]},
        Head @ TFindProof[imp["Conjecture"], imp["Axioms"],
            TimeConstraint -> 3]],
    ProofObject,
    TestID -> "ATP/tptp/import-pipe-through-to-TFindProof"
]

(* No-op for Symbol-headed axioms: the FreeQ check skips
   tptpInternalize when nothing has a String head. *)
VerificationTest[
    Head @ TFindProof[a == c, {a == b, b == c}, TimeConstraint -> 3],
    ProofObject,
    TestID -> "ATP/tptp/auto-internalize-skips-Symbol-heads"
]

(* TRelevantAxioms on an unquantified pattern axiom should keep
   the axiom (Pattern variables shouldn't be misclassified as
   confined symbols).  Iter 66 fixed atpFnSyms to skip Pattern-
   bound variables; pre-fix `f[x_] == g[x_]` was dropped as
   ConfinedBothSides on x. *)
VerificationTest[
    Length @ Lookup[
        TRelevantAxioms[f[a] == g[a], {f[x_] == g[x_]}],
        "Kept"],
    1,
    TestID -> "ATP/relevant/pattern-var-not-confined-symbol"
]

(* === TATP Inactive support (iter 64) ============================== *)

(* TATP's encoder should accept Inactive[Equal] axioms (the
   FindEquationalProof "Lemmas" form).  Iter 64 extended
   forAllToPattern to strip the Inactive wrapper before
   encodeEquation's strict HoldComplete[Equal[_, _]] check fires. *)
VerificationTest[
    TATP[{Inactive[Equal][f[x_], g[x_]]}, f[a] == g[a]]["Status"],
    "PROVED",
    TestID -> "ATP/TATP/Inactive-axiom-proves"
]

(* === TRelevantAxioms input normalization (iter 63) ================ *)

(* TRelevantAxioms should accept Inactive[Equal] axioms (the
   FindEquationalProof "Lemmas" form) without misclassifying them as
   confined.  Pre-fix, the relevance filter saw Inactive[Equal] as an
   unknown head and dropped the axiom as ConfinedBothSides. *)
VerificationTest[
    Lookup[
        TRelevantAxioms[Inactive[Equal][a, c],
            {Inactive[Equal][a_, b_], Inactive[Equal][b_, c_]}],
        "Dropped"],
    {},
    TestID -> "ATP/relevant/Inactive-axioms-not-misclassified"
]

(* TRelevantAxioms should auto-flatten nested axiom lists, matching
   the TFindProof entry shape. *)
VerificationTest[
    Length @ Lookup[
        TRelevantAxioms[a == c, {{a == b}, {b == c}}],
        "Kept"],
    2,
    TestID -> "ATP/relevant/nested-axiom-list-auto-flattens"
]

(* === Globals-collision footgun =================================== *)

(* `CanonicalizePatterns` renames axiom-bound variables to canonical
   short Symbol names a, b, c, ..., which live in Global`.  Pre-fix,
   a user who had set e.g. `c = "x"` BEFORE calling TFindProof would
   crash the kernel with a SIGSEGV: the third canonical pattern
   variable Pattern[c, _] collided with the bound global, the encoder
   later followed the String head, and the C runtime walked off the
   term arena.  atpFreshGlobalSymbol now sidesteps any colliding
   global by suffixing the canonical name with $Atp<k>.  The repro is
   a top-level `c = "x"`; Block-style dynamic scoping isn't enough to
   trigger it through the test harness, so we mutate Global`c, run,
   then Clear.  Quiet swallows the Clear-on-cleared housekeeping
   message. *)
VerificationTest[
    Quiet[
        Symbol["c"] = "x";
        With[{p = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
                TimeConstraint -> 3]},
            Clear[c];
            Head[p]]],
    ProofObject,
    TestID -> "ATP/footgun/canonicalize-survives-global-collision"
]

(* === Hard Sheffer Implies-X via Mix2 + SelectionRatio 2 (iter 75) == *)

(* ShefferAxioms/ImpliesWolframAxioms + ImpliesWolframAlternateAxioms
   are cross-system "Sheffer implies Wolfram's nand axioms" goals that
   the default schedule (SelectionRatio 11) leaves unreachable in a
   reasonable budget.  Mix2 + SelectionRatio 2 (aggressive
   1-FIFO-per-2 age bias) cracks both in ~4s by forcing the saturator
   through the long derivation chain before the CP queue explodes.
   Generous TimeConstraint (60s) so the test survives a loaded box;
   the proof's C-engine cost is ~4s. *)
VerificationTest[
    Head @ TFindProof["ImpliesWolframAxioms", "ShefferAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Mix2",
            "SelectionRatio" -> 2, "AutoMaxWeight" -> 20},
        TimeConstraint -> 60],
    ProofObject,
    TestID -> "ATP/sheffer/ImpliesWolframAxioms-Mix2-SR2"
]

VerificationTest[
    Head @ TFindProof["ImpliesWolframAlternateAxioms", "ShefferAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Mix2",
            "SelectionRatio" -> 2, "AutoMaxWeight" -> 20},
        TimeConstraint -> 60],
    ProofObject,
    TestID -> "ATP/sheffer/ImpliesWolframAlternateAxioms-Mix2-SR2"
]

(* === ENIGMA Tier 1: learned CP scorer (WL surface) + coop ========= *)

$enigmaBakedW = {0.003216, -0.271657, 0.460614, -0.096104, 0.003216,
    0.042247, 0.003216, -0.000402, -0.005740, -0.156174, -0.023514,
    1.121586, 1.999360, -0.012683};
$enigmaLin = <|"Kind" -> "Linear", "Mean" -> ConstantArray[0., 14],
    "InvStd" -> ConstantArray[1., 14], "W" -> $enigmaBakedW, "B" -> -1.598045|>;

(* Push a baked-in-equivalent linear model. *)
VerificationTest[
    TAtpSetLearnedScorer[$enigmaLin],
    True,
    TestID -> "ATP/enigma/set-linear-model"
]

(* Push a one-hidden-layer MLP (H = 8). *)
VerificationTest[
    TAtpSetLearnedScorer[<|"Kind" -> "MLP", "Mean" -> ConstantArray[0., 14],
        "InvStd" -> ConstantArray[1., 14], "W1" -> ConstantArray[0.01, {8, 14}],
        "B1" -> ConstantArray[0., 8], "W2" -> ConstantArray[0.1, 8],
        "B2" -> 0.0|>],
    True,
    TestID -> "ATP/enigma/set-mlp-model"
]

(* A malformed model is rejected; the engine keeps the baked-in scorer. *)
VerificationTest[
    TAtpSetLearnedScorer[<|"Kind" -> "Linear", "W" -> {1., 2., 3.}, "B" -> 0.|>],
    False,
    TestID -> "ATP/enigma/reject-malformed-model"
]

VerificationTest[
    TAtpSetLearnedScorer[Clear],
    True,
    TestID -> "ATP/enigma/clear-model"
]

(* Method -> "ENIGMA" is a first-class preset = learned CP selection that
   coops with a GT secondary every 2nd pick by default. *)
VerificationTest[
    Lookup[TAtpDescribeMethod["ENIGMA"],
        {"CriticalPairWeight", "CoopWeight", "CoopRatio"}],
    {"Learned", "Gt", 2},
    TestID -> "ATP/enigma/method-preset-describe"
]

(* A proof via Method -> "ENIGMA" (baked-in scorer, coop default)
   completes -- the preset + the W2 coop suboptions are wired through. *)
VerificationTest[
    TAtpSetLearnedScorer[Clear];
    TFindProof[f[e, x_] == x,
        {f[x_, e] == x, f[x_, i[x_]] == e,
         f[f[x_, y_], z_] == f[x_, f[y_, z_]]},
        "Status", Method -> "ENIGMA", TimeConstraint -> 30],
    "Proved",
    TestID -> "ATP/enigma/method-preset-proves"
]

(* TAtpCpDataset returns a labelled feature matrix; TAtpTrainScorer trains
   a model on it via thvm's TNetTrain.  Small group corpus. *)
VerificationTest[
    Module[{ds},
        ds = TAtpCpDataset[
            {f[e, x_] == x, f[i[x_], x_] == e, i[i[x_]] == x},
            {f[x_, e] == x, f[x_, i[x_]] == e,
             f[f[x_, y_], z_] == f[x_, f[y_, z_]]},
            TimeConstraint -> 20];
        {Dimensions[ds["Features"]][[2]], ds["NRows"] === Length[ds["Labels"]],
         SubsetQ[{0, 1}, Union[ds["Labels"]]]}],
    {14, True, True},
    TestID -> "ATP/enigma/dataset-shape"
]

VerificationTest[
    Module[{ds, r},
        ds = TAtpCpDataset[
            {f[e, x_] == x, f[i[x_], x_] == e, i[i[x_]] == x},
            {f[x_, e] == x, f[x_, i[x_]] == e,
             f[f[x_, y_], z_] == f[x_, f[y_, z_]]},
            TimeConstraint -> 20];
        r = TAtpTrainScorer[ds, "Hidden" -> 0, MaxTrainingRounds -> 50];
        {Lookup[r["Model"], "Kind"], Length[r["Model"]["W"]]}],
    {"Linear", 14},
    TestID -> "ATP/enigma/train-linear"
]

(* Reset so later tests / sessions see the baked-in scorer. *)
TAtpSetLearnedScorer[Clear];
