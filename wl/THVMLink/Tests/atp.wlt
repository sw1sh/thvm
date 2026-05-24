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
        Method -> "Completion", MaxSteps -> 5, MaxWallSeconds -> 20.],
    $Failed,
    TestID -> "ATP/option/maxsteps-tight-on-hard-fails"
]

VerificationTest[
    Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        MaxSteps -> 200000],
    ProofObject,
    TestID -> "ATP/option/maxsteps-loose-proves"
]

(* --- MaxWallSeconds: a tiny budget on a hard theorem fails fast ---- *)

VerificationTest[
    TFindProof["AndAssociativity", "WolframAxioms",
        Method -> {"Completion", "Ordering" -> "LPO",
            "CriticalPairWeight" -> "Gt"},
        MaxWallSeconds -> 2.],
    $Failed,
    TestID -> "ATP/option/maxwallseconds-tiny-on-hard-fails"
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
            MaxSteps -> 5000, MaxWallSeconds -> 60.];
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
        MaxSteps -> 5000, MaxWallSeconds -> 60.],
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
            MaxWallSeconds -> 10];
        {MatchQ[res, {__}],
         AllTrue[res, MatchQ[#, Inactive[Equal][_, _]] &]}
    ],
    {True, True},
    TestID -> "ATP/completion/explicit-ac-axioms-returns-lemmas"
]

(* Completion of a theory by name. *)
VerificationTest[
    MatchQ[
        TFindProof["AbelianGroupAxioms", MaxWallSeconds -> 10],
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
            "Statistics", MaxWallSeconds -> 10],
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
         Sort[Keys[r]] === Sort[THVMLink`Private`$AtpReturnSpecs]}
    ],
    {True, True},
    TestID -> "ATP/returnspec/all-projects-every-spec"
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
        prof = THVMLink`Private`atpAnalyzeStructure[{
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
        prof = THVMLink`Private`atpAnalyzeStructure["AbelianGroupAxioms"];
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
        prof = THVMLink`Private`atpAnalyzeStructure["GroupAxioms"];
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
    THVMLink`Private`atpAnalyzeStructure["WolframAxioms"]["Class"],
    "Sheffer",
    TestID -> "ATP/autotune/analyze-Sheffer-WolframAxioms"
]

(* CommutativeRingAxioms: + and *, distributivity, inverse -> Ring. *)
VerificationTest[
    THVMLink`Private`atpAnalyzeStructure["CommutativeRingAxioms"]["Class"],
    "Ring",
    TestID -> "ATP/autotune/analyze-Ring"
]

(* atpAutoTune returns a non-empty list of valid Method configs. *)
VerificationTest[
    Module[{sched},
        sched = THVMLink`Private`atpAutoTune["AbelianGroupAxioms"];
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
        tuned = THVMLink`Private`atpScheduleFor[Automatic,
            AxiomaticTheory["AbelianGroupAxioms"],
            AxiomaticTheory["AbelianGroupAxioms", "NotableTheorems"][
                "InverseOfInverse"]];
        fixed = THVMLink`Private`$AtpSchedule;
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
        tuned = THVMLink`Private`atpScheduleFor[Automatic,
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
    THVMLink`Private`atpScheduleFor["Portfolio",
        AxiomaticTheory["AbelianGroupAxioms"], Null] ===
        THVMLink`Private`$AtpSchedule,
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
         MaxWallSeconds -> 400]'

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
                MaxWallSeconds -> 400];
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
            MaxWallSeconds -> 60];
        {Head[p], Head @ Quiet @ p["ProofFunction"][p["Theorems"]]}
    ],
    {ProofObject, Success},
    TestID -> "ATP/option/recordnorm-on-verifies-DoubleNegation"
]

VerificationTest[
    Module[{p},
        p = TFindProof["DoubleNegation", "WolframAxioms",
            Method -> {"Completion", "RecordNorm" -> False},
            MaxWallSeconds -> 60];
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
                MaxWallSeconds -> 600];
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
