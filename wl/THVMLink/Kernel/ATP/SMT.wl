(* SMT.wl - quantifier-free first-order equality decision procedure.

   Decides QF_UF (quantifier-free equality with uninterpreted
   functions) via congruence closure -- the Downey-Sethi-Tarjan
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
     TFindProofSMT[query]    Convenience surface: decide a single
                             ground equational goal under a list
                             of ground hypotheses.  Returns a
                             small ProofObject-shaped Association
                             on UNSAT (with the participating
                             hypotheses + diseq witness) and
                             $Failed on SAT.

   Why this is in ATP/
     Congruence closure is the equality-theory satisfiability
     decider that pairs with a propositional CDCL kernel under
     DPLL(T).  Standalone it already decides ground QF_UF goals
     of the form `H1 /\ H2 /\ ... /\ ~G` (assert each Hi as eq,
     assert ~G as a diseq, ask UNSAT) -- exactly the pattern an
     unfailing-completion saturator handles for ground equational
     conjectures, but in O((n + e) alpha(n)) instead of search.
     A future DPLL(T) shell on top will turn this into a real
     SMT solver. *)

BeginPackage["THVMLink`SMT`"];

TSatEUF::usage =
    "TSatEUF[{lhs == rhs, ...}, {lhs != rhs, ...}] decides the " <>
    "quantifier-free first-order theory of equality with " <>
    "uninterpreted functions (QF_UF) via congruence closure.  " <>
    "Returns an Association with \"Status\" -> \"UNSAT\" | \"SAT\", " <>
    "and on SAT a \"Classes\" key listing the inferred equivalence " <>
    "classes of subterms, on UNSAT a \"Witness\" key naming the " <>
    "disequality whose two sides collapsed.";

TFindProofSMT::usage =
    "TFindProofSMT[goal, hypotheses] (goal an Equal[...] or " <>
    "Unequal[...]) decides the ground entailment hypotheses |= goal " <>
    "by reducing it to a QF_UF satisfiability query: assert each " <>
    "hypothesis, assert the negation of goal, and run congruence " <>
    "closure.  Returns a small ProofObject-shaped Association on " <>
    "UNSAT, $Failed on SAT.  TFindProofSMT[\"...cnf/fof string...\"] " <>
    "and TFindProofSMT[File[\"path.p\"]] parse a TPTP fragment via " <>
    "THVMLink`TPTPImport`tptpImport and dispatch the same way -- the " <>
    "input must be ground (no variables) since congruence closure " <>
    "is a quantifier-free decision procedure; non-ground inputs " <>
    "return $Failed with a console message.";

TFindProofSMT::nonground =
    "TFindProofSMT skipping non-ground input: the term `1` contains a " <>
    "Pattern[]/Blank[] variable.  Congruence closure is a quantifier-" <>
    "free decision procedure -- use TFindProof for variable-bearing " <>
    "axioms.";

TFindProofSMT::noconjecture =
    "TFindProofSMT requires a conjecture in the input; got axioms only.";

TSatEUF::badin =
    "TSatEUF inputs must be lists of equalities (HoldPattern[Equal[_,_]]) " <>
    "and disequalities (HoldPattern[Unequal[_,_]]); got `1` / `2`.";

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
   We keep variables and compound terms in the same forest -- only
   compound terms (head =!= Symbol, no atomic numbers) feed
   congruence propagation. *)

ccInit[] := (
    $parent   = <||>;
    $rank     = <||>;
    $use      = <||>;
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

compoundQ[t_] := !AtomQ[t]

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
        If[ $rank[ra] < $rank[rb],
            {ra, rb} = {rb, ra}
        ];
        $parent[rb] = ra;
        If[ $rank[ra] === $rank[rb],
            $rank[ra] = $rank[ra] + 1
        ];
        (* relocate b's use list into a's so future congruence
           probes see all parents of the merged class. *)
        $use[ra] = Join[$use[ra], $use[rb]];
        $use[rb] = {};
        (* congruence step: for every pair of compound parents
           now sharing the merged class, if their heads and
           arities match and all args are equivalent, recursively
           merge them. *)
        congruencePropagate[ra]
    ]
]

congruencePropagate[rep_] := Block[{parents = $use[rep], i, j, n, u, v},
    n = Length[parents];
    Do[
        u = parents[[i]];
        Do[
            v = parents[[j]];
            If[ congruentQ[u, v] && ccFind[u] =!= ccFind[v],
                ccUnion[u, v]
            ],
            {j, i + 1, n}
        ],
        {i, 1, n}
    ]
]

congruentQ[u_, v_] :=
    Head[u] === Head[v] &&
    Length[u] === Length[v] &&
    AllTrue[
        Transpose[{args[u], args[v]}],
        ccFind[#[[1]]] === ccFind[#[[2]]] &
    ]

(* ----- API ----- *)

TSatEUF[eqs_List, diseqs_List] :=
    If[ ! (AllTrue[eqs, MatchQ[#, _Equal] &] &&
           AllTrue[diseqs, MatchQ[#, _Unequal] &]),
        Message[TSatEUF::badin, eqs, diseqs]; $Failed,
        Block[{$parent, $rank, $use, $subterms, witness, classes},
            ccInit[];
            Scan[(ccAddTerm[#[[1]]]; ccAddTerm[#[[2]]]) &, eqs];
            Scan[(ccAddTerm[#[[1]]]; ccAddTerm[#[[2]]]) &, diseqs];
            Scan[ccUnion[#[[1]], #[[2]]] &, eqs];
            witness = SelectFirst[
                diseqs,
                ccFind[#[[1]]] === ccFind[#[[2]]] &,
                None
            ];
            If[ witness =!= None,
                <|"Status" -> "UNSAT", "Witness" -> witness|>,
                classes = GatherBy[$subterms, ccFind];
                <|"Status" -> "SAT", "Classes" -> classes|>
            ]
        ]
    ]

TFindProofSMT[goal_, hypotheses_List : {}] :=
    Block[{eqs, diseqs, res},
        {eqs, diseqs} = collectLiterals[
            Append[hypotheses, negate[goal]]
        ];
        If[ eqs === $Failed,
            $Failed,
            res = TSatEUF[eqs, diseqs];
            If[ res["Status"] === "UNSAT",
                <|
                    "Status"     -> "Proved",
                    "Method"     -> "CongruenceClosure",
                    "Goal"       -> goal,
                    "Hypotheses" -> hypotheses,
                    "Witness"    -> res["Witness"]
                |>,
                $Failed
            ]
        ]
    ]

negate[Equal[a_, b_]]    := Unequal[a, b]
negate[Unequal[a_, b_]]  := Equal[a, b]
negate[other_]           := (Message[TSatEUF::badin, other, {}]; $Failed)

collectLiterals[lits_List] := Block[{e = {}, d = {}, l},
    Do[ l = lits[[i]];
        Which[
            MatchQ[l, _Equal],   AppendTo[e, l],
            MatchQ[l, _Unequal], AppendTo[d, l],
            True, Return[{$Failed, $Failed}]
        ],
        {i, Length[lits]}
    ];
    {e, d}
]

(* ----- TPTP-string / File overloads -----
   TPTPImport returns clauses with universally-quantified
   variables as Pattern[Unique[], Blank[]] expressions, which
   congruence closure cannot handle (it is a ground decision
   procedure).  Reject any clause that contains a Blank[] head
   so the user gets a clear message instead of an internal
   crash. *)

TFindProofSMT[File[path_String]] :=
    tptpDispatchSMT @ THVMLink`TPTPImport`tptpImport[File[path]]

TFindProofSMT[s_String] /;
        StringContainsQ[s, "cnf("] || StringContainsQ[s, "fof("] :=
    tptpDispatchSMT @ THVMLink`TPTPImport`tptpImport[s]

tptpDispatchSMT[imported_Association] := Block[
    {axioms = imported["Axioms"], conj = imported["Conjecture"],
     nonGround},
    Which[
        conj === None,
            Message[TFindProofSMT::noconjecture]; $Failed,
        (nonGround = SelectFirst[Append[axioms, conj], ! groundQ[#] &,
            None]) =!= None,
            Message[TFindProofSMT::nonground, nonGround]; $Failed,
        True,
            TFindProofSMT[conj, axioms]
    ]
]

groundQ[expr_] := FreeQ[expr, _Pattern | _Blank | _BlankSequence |
    _BlankNullSequence]

End[];
EndPackage[];
