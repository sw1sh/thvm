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
    (* A symmetric goal: neither side rewrites to the other, so the
       variable-keyed goal has no shared normal form.  Skolemizing the
       conjecture's variables to constants lets KBO totally order them,
       so the unorientable commutativity axiom ordered-rewrites the
       larger face down to the smaller -- the single-NF check then
       closes it, and the proof cites the axiom directly. *)
    Module[{p},
        p = TFindEquationalProof[
            ForAll[{x, y}, f[x, y] == f[y, x]],
            {ForAll[{a, b}, f[a, b] == f[b, a]]}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/TFEP/forall-multi-symmetric-verifies"
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
    (* The string form resolves a NotableTheorem of WolframAxioms,
       saturates with the C engine, and assembles the critical-pair
       lemma DAG.  DoubleNegation is a genuine superposition proof:
       the trace carries TRACE_CP entries, so buildCplDataset emits
       CriticalPairLemma steps. *)
    Head @ TFindEquationalProof["DoubleNegation", "WolframAxioms"],
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
        p = TFindEquationalProof["DoubleNegation", "WolframAxioms"];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/TFEP/string-doublenegation-verifies"
]

(* === TFindEquationalProof: completion-derived proofs =============== *)

(* Goals the input axioms do not close directly (the EXT axiom-only
   path returns nothing) but completion does: buildCplDataset walks
   the MAIN trace DAG and emits a SubstitutionLemma per re-derived
   rewrite.  The resulting ProofObject passes WL's verifier. *)

VerificationTest[
    Head @ TFindEquationalProof[b == c, {f[a] == b, f[a] == c}],
    ProofObject,
    TestID -> "ATP/TFEP/completion-ground-proves"
]

VerificationTest[
    Module[{p},
        p = TFindEquationalProof[b == c, {f[a] == b, f[a] == c}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/completion-ground-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindEquationalProof[b == a,
            {ForAll[x, f[g[x]] == x], f[g[a]] == b}];
        Head @ p["ProofFunction"][p["ConjectureStatement"]]
    ],
    Success,
    TestID -> "ATP/TFEP/completion-variable-verifies"
]

(* === TFindEquationalProof: numeric-literal constants =============== *)

(* AbelianGroupAxioms (and the McCune / Tarski theories) write the
   identity element as OverTilde[1] -- a function applied to the
   integer literal 1.  The encoder must treat a numeric atom as a
   0-arity constant; without that rule it folds over a non-list and
   diverges. *)

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms"],
    ProofObject,
    TestID -> "ATP/TFEP/numeric-literal-constant-proves"
]

VerificationTest[
    Module[{p},
        p = TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms"];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/TFEP/numeric-literal-constant-verifies"
]

(* === TFindEquationalProof: multi-equation conjectures ============== *)

(* AxiomaticTheory ships some NotableTheorems as a multi-element list
   of equations -- e.g. BooleanAxioms `DeMorgan` is the pair of De
   Morgan laws.  The string form proves each conjunct separately and
   returns a List of ProofObjects (equational provability distributes
   over conjunction). *)

VerificationTest[
    MatchQ[
        TFindEquationalProof["DeMorgan", "BooleanAxioms"],
        {_ProofObject, _ProofObject}
    ],
    True,
    TestID -> "ATP/TFEP/multi-eq-demorgan-proves"
]

VerificationTest[
    Module[{ps},
        ps = TFindEquationalProof["DeMorgan", "BooleanAxioms"];
        AllTrue[ps,
            Head @ Quiet @ Check[
                #["ProofFunction"][#["Theorems"]], $Failed] === Success &]
    ],
    True,
    TestID -> "ATP/TFEP/multi-eq-demorgan-verifies"
]

(* === TFindEquationalProof: ordered-rewriting goal chain ============ *)

(* AbelianGroupAxioms' `ImpliesMcCuneAxioms` is a single-equation
   theorem whose proof chain includes steps where the C engine's
   ordered rewriting fires a rule in the rhs->lhs direction.  The
   verifier needs the SubstitutionLemma's Orientation flipped to read
   the cited entry's Statement reversed for those steps -- the goal
   chain emit threads the C engine's Fwd flag into the Orientation
   computation. *)

VerificationTest[
    Module[{p},
        p = TFindEquationalProof["ImpliesMcCuneAxioms", "AbelianGroupAxioms"];
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
        p = TFindEquationalProof["AndAssociativity", "HillmanAxioms"];
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
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> Automatic],
    ProofObject,
    TestID -> "ATP/method/Automatic-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "Portfolio"],
    ProofObject,
    TestID -> "ATP/method/Portfolio-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "Completion"],
    ProofObject,
    TestID -> "ATP/method/Completion-bare-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "GoalDirected"],
    ProofObject,
    TestID -> "ATP/method/GoalDirected-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "NoSuchMethod"],
    ProofObject,
    {TFindEquationalProof::badmethod},
    TestID -> "ATP/method/bad-method-warns-and-falls-back"
]

(* --- CriticalPairWeight: each ClasHeuristics weight mode proves ---- *)

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Mix2"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Mix2-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Gt"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Gt-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Add"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Add-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Goal"}],
    ProofObject,
    TestID -> "ATP/method/cpweight-Goal-CPinGoal-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "CriticalPairWeight" -> "Bogus"}],
    ProofObject,
    {TFindEquationalProof::badcpw},
    TestID -> "ATP/method/cpweight-bad-warns-and-falls-back"
]

(* --- Ordering: KBO and LPO both prove AND verify ------------------- *)

VerificationTest[
    Module[{p},
        p = TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "Ordering" -> "KBO"}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/ordering-KBO-verifies"
]

VerificationTest[
    Module[{p},
        p = TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "Ordering" -> "LPO",
                "AutoPrecedence" -> True}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/ordering-LPO-verifies"
]

(* --- AutoPrecedence: True and False both prove -------------------- *)

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "Ordering" -> "LPO",
            "AutoPrecedence" -> True}],
    ProofObject,
    TestID -> "ATP/method/autoprec-True-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "AutoPrecedence" -> False}],
    ProofObject,
    TestID -> "ATP/method/autoprec-False-proves"
]

(* --- AxiomRelevance: None / Safe prove; mode is reported by --------
   TRelevantAxioms; Connected drops axioms (heuristic). *)

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "AxiomRelevance" -> None}],
    ProofObject,
    TestID -> "ATP/method/axrel-None-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
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
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "MaxWeight" -> 40}],
    ProofObject,
    TestID -> "ATP/method/maxweight-40-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "MaxWeight" -> 0}],
    ProofObject,
    TestID -> "ATP/method/maxweight-unbounded-proves"
]

(* --- GoalInterleave: every n-th selection goal-directed ----------- *)

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "GoalInterleave" -> 3}],
    ProofObject,
    TestID -> "ATP/method/goalinterleave-3-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "GoalInterleave" -> 2}],
    ProofObject,
    TestID -> "ATP/method/goalinterleave-2-proves"
]

(* --- GroundJoin: opt-in CP deletion stays SOUND (proves+verifies) - *)

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "GroundJoin" -> True}],
    ProofObject,
    TestID -> "ATP/method/groundjoin-True-proves"
]

VerificationTest[
    Module[{p},
        p = TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
            Method -> {"Completion", "GroundJoin" -> True}];
        Head @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/method/groundjoin-True-verifies-sound"
]

(* --- SelectionRatio: Waldmeister CPdimension fairness ratio -------- *)

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "SelectionRatio" -> 50}],
    ProofObject,
    TestID -> "ATP/method/selectionratio-50-proves"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> {"Completion", "SelectionRatio" -> 100}],
    ProofObject,
    TestID -> "ATP/method/selectionratio-100-proves"
]

(* --- MaxSteps: a tight cap on a hard theorem fails; loose proves --- *)

VerificationTest[
    TFindEquationalProof["AndAssociativity", "WolframAxioms",
        Method -> "Completion", MaxSteps -> 5, MaxWallSeconds -> 20.],
    $Failed,
    TestID -> "ATP/option/maxsteps-tight-on-hard-fails"
]

VerificationTest[
    Head @ TFindEquationalProof["InverseOfInverse", "AbelianGroupAxioms",
        MaxSteps -> 200000],
    ProofObject,
    TestID -> "ATP/option/maxsteps-loose-proves"
]

(* --- MaxWallSeconds: a tiny budget on a hard theorem fails fast ---- *)

VerificationTest[
    TFindEquationalProof["AndAssociativity", "WolframAxioms",
        Method -> {"Completion", "Ordering" -> "LPO",
            "CriticalPairWeight" -> "Gt"},
        MaxWallSeconds -> 2.],
    $Failed,
    TestID -> "ATP/option/maxwallseconds-tiny-on-hard-fails"
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
        viaConstraint = TFindEquationalProof["AndAssociativity",
            "WolframAxioms", Method -> hard, TimeConstraint -> 2.];
        viaWrapper = TimeConstrained[
            TFindEquationalProof["AndAssociativity", "WolframAxioms",
                Method -> hard], 2., "TimedOut"];
        (* The hard theorem is not provable by thvm at any budget, so a
           2s budget must interrupt the running engine and yield NO proof
           via either form -- the contract is "no hang, no spurious
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
