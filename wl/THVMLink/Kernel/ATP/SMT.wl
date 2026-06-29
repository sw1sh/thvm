(* SMT.wl - quantifier-free first-order equality decision procedure.

   Decides QF_UF (quantifier-free equality with uninterpreted
   functions) via congruence closure: the Downey-Sethi-Tarjan
   algorithm with a per-class use-list, the same core every
   modern SMT solver (Z3, CVC5, Yices) runs under DPLL(T) for the
   theory of equality.

   Public surface
     TSatEUF[eqs, diseqs]    Decide a list of `lhs == rhs`
                             equalities together with a list of
                             `lhs != rhs` disequalities.  Returns
                             "UNSAT" (the union of equalities
                             forces some declared disequality to
                             collapse) or "SAT" with the inferred
                             equivalence classes.
     TSmtDecide[formula]     Decide a Boolean combination of
                             equality atoms via lazy DPLL(T).

   The entailment surface is reached through TFindProof, not a
   separate symbol: TFindProof[goal, hyps, Method -> "SMT"] decides a
   ground entailment by congruence closure (the private atpSmtEntail
   helper here), and TFindProof[goal, hyps, "Counterexample"] returns
   the refuting CounterexampleObject.

   Why this is in ATP/
     Congruence closure is the equality-theory satisfiability
     decider that pairs with a propositional CDCL kernel under
     DPLL(T).  Standalone it already decides ground QF_UF goals
     of the form `H1 /\ H2 /\ ... /\ ~G` (assert each Hi as eq,
     assert ~G as a diseq, ask UNSAT), exactly the pattern an
     unfailing-completion saturator handles for ground equational
     conjectures, but in O((n + e) alpha(n)) instead of search.
     A future DPLL(T) shell on top will turn this into a real
     SMT solver. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"GeneralUtilities`", "WolframInstitute`THVMLink`", "Wolfram`Parser`"}];

SetUsage[TSatEUF, "TSatEUF[eqs$, diseqs$] decides the quantifier-free first-order theory of equality with uninterpreted functions (QF_UF) via congruence closure, where eqs$ is a list of equalities lhs$ == rhs$ and diseqs$ a list of disequalities lhs$ != rhs$.
Returns an Association keyed by \"Status\" (\"SAT\" or \"UNSAT\"); on SAT a \"Classes\" key lists the inferred equivalence classes of subterms, on UNSAT a \"Witness\" key names the disequality whose two sides collapsed."];

TFindProof::nonground = "TFindProof Method -> \"SMT\" skipping non-ground input: the term `1` " <> "contains a Pattern[]/Blank[] variable.  Congruence closure is a " <> "quantifier-free decision procedure; use the default completion engine " <> "for variable-bearing axioms.";

TFindProof::noconjecture = "TFindProof Method -> \"SMT\" requires a conjecture in the input; got " <> "axioms only.";

SetUsage[TSmtDecide, "TSmtDecide[formula$] decides a quantifier-free Boolean combination of equality atoms (Equal or Unequal) via lazy DPLL(T): atom-abstraction, a SatisfiabilityInstances propositional kernel, and congruence closure as the theory solver.
Returns an Association keyed by \"Status\" (\"SAT\" or \"UNSAT\"); on SAT a \"Model\" key gives a satisfying atom assignment (each atom to True or False) certified by congruence closure.
Handles the And, Or, Not, Implies, Equivalent, and Xor combinators."];

TSatEUF::badin = "TSatEUF inputs must be lists of equalities (HoldPattern[Equal[_,_]]) " <> "and disequalities (HoldPattern[Unequal[_,_]]); got `1` / `2`.";

Begin["`Private`"];

(* ----- state representation -----
   The decision procedure threads a mutable state Association
   under a Block-scoped pair of symbol-keyed Associations:

       $parent   subterm -> its parent in the union-find (initially
                            self).
       $rank     subterm -> union-by-rank counter.
       $use      subterm -> list of compound subterms whose args
                            mention this term's class rep.  Used
                            to look up congruence candidates after
                            a merge.
       $subterms ordered list of every subterm ever added, in
                            insertion order; lets us enumerate
                            classes deterministically at the end.

   Subterms are stored verbatim (no canonicalization at add time)
   keyed against the parent map by HoldPattern when necessary.
   We keep variables and compound terms in the same forest; only
   compound terms (head =!= Symbol, no atomic numbers) feed
   congruence propagation. *)

ccInit[] := (
    $parent = <||>;
    $rank = <||>;
    $use = <||>;
    $subterms = {};
)

ccAddTerm[t_] := If[ KeyExistsQ[$parent, t],
    t,
    $parent[t] = t;
    $rank[t] = 0;
    $use[t] = {};
    AppendTo[$subterms, t];
    If[ compoundQ[t],
        Scan[(ccAddTerm[#]; AppendTo[$use[ccFind[#]], t])&, args[t]]
    ];
    t
]

compoundQ[t_] := ! AtomQ[t]

args[t_] := List @@ t

ccFind[t_] := Block[{p = $parent[t]},
    If[ p === t,
        t,
        $parent[t] = ccFind[p]   (* path compression *)
    ]
]

ccUnion[a_, b_] := Block[{ra = ccFind[a], rb = ccFind[b]},
    If[ ra === rb,
        Null,
        If[$rank[ra] < $rank[rb], {ra, rb} = {rb, ra}];
        $parent[rb] = ra;
        If[$rank[ra] === $rank[rb], $rank[ra] = $rank[ra] + 1];
        (* relocate b's use list into a's so future congruence
           probes see all parents of the merged class. *)
        $use[ra] = DeleteDuplicates @ Join[$use[ra], $use[rb]];
        $use[rb] = {};
        (* congruence step: for every pair of compound parents
           now sharing the merged class, if their heads and
           arities match and all args are equivalent, recursively
           merge them. *)
        congruencePropagate[ra]
    ]
]

congruencePropagate[rep_] := Module[{parents = $use[rep], n, u, v},
    n = Length[parents];
    Do[
        u = parents[[ii]];
        Do[
            v = parents[[jj]];
            If[congruentQ[u, v] && ccFind[u] =!= ccFind[v], ccUnion[u, v]],
            {jj, ii + 1, n}
        ],
        {ii, 1, n}
    ]
]

congruentQ[u_, v_] := u =!= v && Head[u] === Head[v] && Length[u] === Length[v] && AllTrue[Transpose[{args[u], args[v]}], ccFind[#[[1]]] === ccFind[#[[2]]] &]

(* ----- API ----- *)

(* Drop literally-True equalities and literally-False disequalities;
   either category is vacuously satisfied (a == a or Unequal[1, 2] just
   doesn't constrain anything).  Conversely, literally-False equalities
   (Equal[1, 2]) and literally-True disequalities (Unequal[a, a]; WL
   doesn't auto-evaluate Unequal on bare symbols, but the False form is
   what gets in) are immediate UNSAT contradictions.  Without this
   preprocessing, a user passing a reflexive `a == a` (which WL
   evaluates to True before TSatEUF sees it) hits the badin guard and
   gets $Failed instead of the obvious SAT verdict. *)
atpSmtPreprocess[eqs_List, diseqs_List] := Module[{eqsFalse, diseqsTrue, eqsClean, diseqsClean},
    eqsFalse = MemberQ[eqs, False];
    diseqsTrue = MemberQ[diseqs, False];
    If[eqsFalse, Return[{"UNSAT", "Witness" -> False}, Module]];
    If[diseqsTrue, Return[{"UNSAT", "Witness" -> False}, Module]];
    eqsClean = DeleteCases[eqs, True];
    diseqsClean = DeleteCases[diseqs, True];
    {"Continue", eqsClean, diseqsClean}
]

TSatEUF[eqs_List, diseqs_List] := Module[{pre = atpSmtPreprocess[eqs, diseqs], eqsC, diseqsC},
    If[First[pre] === "UNSAT", Return @ <|"Status" -> "UNSAT", pre[[2]]|>];
    eqsC = pre[[2]]; diseqsC = pre[[3]];
    (* Accept Inactive[Equal][a, b] as an alias for Equal[a, b]
       (the inert form FindEquationalProof's ProofObject "Lemmas"
       spec returns).  Inactive[Unequal] same.  Strip Inactive
       before the strict-head check + congruence closure. *)
    eqsC = Replace[eqsC, Inactive[Equal][a_, b_] :> Equal[a, b], {1}];
    diseqsC = Replace[diseqsC, Inactive[Unequal][a_, b_] :> Unequal[a, b], {1}];
    If[ ! (AllTrue[eqsC, MatchQ[#, _Equal] &] && AllTrue[diseqsC, MatchQ[#, _Unequal] &]),
        Message[TSatEUF::badin, eqs, diseqs]; $Failed,
        Block[{$parent, $rank, $use, $subterms, witness, classes},
            ccInit[];
            Scan[(ccAddTerm[#[[1]]]; ccAddTerm[#[[2]]]) &, eqsC];
            Scan[(ccAddTerm[#[[1]]]; ccAddTerm[#[[2]]]) &, diseqsC];
            Scan[ccUnion[#[[1]], #[[2]]] &, eqsC];
            witness = SelectFirst[diseqsC, ccFind[#[[1]]] === ccFind[#[[2]]] &, None];
            If[ witness =!= None,
                <|"Status" -> "UNSAT", "Witness" -> witness|>,
                classes = GatherBy[$subterms, ccFind];
                <|"Status" -> "SAT", "Classes" -> classes|>
            ]
        ]
    ]
]

(* ----- atpSmtEntail: the shared SMT entailment decider -----
   The implementation behind TFindProof[goal, hyps, Method -> "SMT"] and the
   ground branch of TFindProof[goal, hyps, "Counterexample"].  Decides a ground
   entailment hyps |= goal by congruence closure (or DPLL(T) for a Boolean
   combination):
     - entailment holds  -> a small "Proved" decision Association;
     - entailment fails   -> a CounterexampleObject built from the refuting
                             model (the equational dual of the ProofObject);
     - malformed input    -> $Failed. *)

(* Single non-list hypothesis: auto-wrap. *)
atpSmtEntail[goal_, hyp : (_Equal | _Unequal | Inactive[Equal][_, _] | Inactive[Unequal][_, _])] := atpSmtEntail[goal, {hyp}]

atpSmtEntail[goal_, hypotheses_List : {}] := Block[{eqs, diseqs, res},
    {eqs, diseqs} = collectLiterals[Append[hypotheses, negate[goal]]];
    If[ eqs === $Failed,
        $Failed,
        res = TSatEUF[eqs, diseqs];
        Which[
            res["Status"] === "UNSAT",
            <|
                "Status" -> "Proved",
                "Method" -> "CongruenceClosure",
                "Goal" -> goal,
                "Hypotheses" -> hypotheses,
                "Witness" -> res["Witness"]
            |>,
            (* SAT: hypotheses /\ ~goal is satisfiable, so the goal is NOT
               entailed.  Build a CounterexampleObject from the congruence-
               closure quotient: a finite refuting model in FindFiniteModels
               structure (the ground analog of the finite algebra
               FindEquationalCounterexample returns). *)
            res["Status"] === "SAT",
            atpGroundCounterexample[goal, hypotheses],
            True, $Failed
        ]
    ]
]

negate[Equal[a_, b_]] := Unequal[a, b]
negate[Unequal[a_, b_]] := Equal[a, b]
negate[other_] := (Message[TSatEUF::badin, other, {}]; $Failed)

collectLiterals[lits_List] := Module[{eqAcc = {}, diseqAcc = {}, l},
    Do[ l = lits[[i]];
        Which[
            (* Pre-evaluated True literals (e.g. a == a -> True or
               Unequal[1, 2] -> True after WL evaluates) are vacuously
               satisfied and can be skipped without affecting the
               theory query.  Matches the TSatEUF preprocess shape so
               an `a == a` hypothesis does not kill the whole call. *)
            l === True, Null,
            MatchQ[l, _Equal], AppendTo[eqAcc, l],
            MatchQ[l, _Unequal], AppendTo[diseqAcc, l],
            True, Return[{$Failed, $Failed}]
        ],
        {i, Length[lits]}
    ];
    {eqAcc, diseqAcc}
]

(* ----- TPTP dispatch (ground SMT path) -----
   Reached from tptpDispatch (ATP.wl) when TFindProof gets a TPTP File / cnf-
   fof string under Method -> "SMT".  TPTPImport returns clauses with
   universally-quantified variables as Pattern[Unique[], Blank[]] expressions,
   which congruence closure cannot handle (it is a ground decision procedure);
   reject any non-ground clause with TFindProof::nonground rather than crash. *)

tptpDispatchSMT[imported_Association] := Block[{axioms = imported["Axioms"], conj = imported["Conjecture"], nonGround},
    Which[
        conj === None,
            Message[TFindProof::noconjecture]; $Failed,
        (nonGround = SelectFirst[Append[axioms, conj], ! groundQ[#] &, None]) =!= None,
            Message[TFindProof::nonground, nonGround]; $Failed,
        True,
            atpSmtEntail[conj, axioms]
    ]
]

groundQ[expr_] := FreeQ[expr, _Pattern | _Blank | _BlankSequence | _BlankNullSequence]

(* ----- DPLL(T) shell -----
   Lazy SMT: replace each equality/disequality atom with a fresh
   propositional variable, hand the Boolean abstraction to
   SatisfiabilityInstances, then T-check each model via congruence
   closure.  On T-conflict, add the negation of the model as a
   blocking clause and re-query.  On the first T-consistent model,
   return SAT; if SatisfiabilityInstances ever returns {}, UNSAT.

   The blocking-clause loop is finite because each iteration rules
   out one truth assignment and there are 2^|atoms| assignments
   total. *)

TSmtDecide[formula_] := Block[{atoms, propVars, abstraction, blocking = True, instance, model, theoryRes, eqs, diseqs, normFormula},
    (* Strip Inactive[Equal] / Inactive[Unequal] so the atom collector
       and the SatisfiabilityInstances boolean kernel see bare Equal /
       Unequal heads.  Without this, an Inactive-wrapped atom list
       contributes zero atoms, the Heads === Equal | Unequal Cases
       filter never fires, and the formula falls through to the empty-
       atoms TrueQ branch (UNSAT for every non-literally-True input). *)
    normFormula = formula /. {Inactive[Equal][a_, b_] :> a == b, Inactive[Unequal][a_, b_] :> Unequal[a, b]};
    atoms = collectAtoms[normFormula];
    If[atoms === {}, Return @ <|"Status" -> If[TrueQ[normFormula], "SAT", "UNSAT"], "Model" -> <||>|>];
    propVars = Table[Unique["smt$p"], {Length[atoms]}];
    abstraction = normFormula /. Thread[atoms -> propVars];
    While[ True,
        instance = Quiet @ SatisfiabilityInstances[And[abstraction, blocking], propVars, 1];
        If[instance === {} || Head[instance] =!= List, Return @ <|"Status" -> "UNSAT"|>];
        model = AssociationThread[atoms, First @ instance];
        {eqs, diseqs} = modelToLiterals[model];
        theoryRes = TSatEUF[eqs, diseqs];
        If[theoryRes["Status"] === "SAT", Return @ <|"Status" -> "SAT", "Model" -> model|>];
        (* T-conflict: forbid this exact propositional assignment. *)
        blocking = And[blocking, Not[And @@ MapThread[If[#2, #1, Not[#1]] &, {propVars, First @ instance}]]];
    ]
]

collectAtoms[formula_] := Sort @ DeleteDuplicates @ Cases[formula, (_Equal | _Unequal), {0, Infinity}, Heads -> False]

modelToLiterals[model_Association] := Module[{eqAcc = {}, diseqAcc = {}},
    KeyValueMap[
        {atom, val} |-> Which[
            MatchQ[atom, _Equal] && val, AppendTo[eqAcc, atom],
            MatchQ[atom, _Equal] && ! val, AppendTo[diseqAcc, Unequal @@ atom],
            MatchQ[atom, _Unequal] && val, AppendTo[diseqAcc, atom],
            MatchQ[atom, _Unequal] && ! val, AppendTo[eqAcc, Equal @@ atom]
        ],
        model
    ];
    {eqAcc, diseqAcc}
]

(* Boolean-combination goal.  An entailment hyps |= phi is UNSAT of
   hyps /\ ~phi; ask TSmtDecide on that. *)

atpSmtEntail[goal_ /; ! MatchQ[goal, _Equal | _Unequal | _String | _File], hypotheses_List : {}] := Block[{res = TSmtDecide[And @@ Append[hypotheses, Not[goal]]]},
    Which[
        res["Status"] === "UNSAT",
        <|
            "Status" -> "Proved",
            "Method" -> "DPLL(T)+CongruenceClosure",
            "Goal" -> goal,
            "Hypotheses" -> hypotheses
        |>,
        (* SAT: hyps /\ ~goal has a model; the goal is not entailed.
           TSmtDecide certified a satisfying truth assignment over the
           equality atoms; return it as the refuting CounterexampleObject.
           No finite algebra here, so "Setup" carries the assignment. *)
        res["Status"] === "SAT",
        CounterexampleObject["DPLL(T)+CongruenceClosure", goal, hypotheses, <|"Setup" -> res["Model"], "Counterexample" -> res["Model"]|>],
        True, $Failed
    ]
]

End[];
EndPackage[];
