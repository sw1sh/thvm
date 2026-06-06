(* ::Package:: *)
(* FindFiniteModels.wl - finite model finder over relations / multiplication
   tables.  Verbatim port of the FindFiniteModels Wolfram Function Repository
   function (renamed TFindFiniteModels), plus a "SAT" Method that computes the
   model indices through the built-in Wolfram SAT solver
   (SatisfiabilityInstances) instead of Compile / expression pruning.

   The "ExpressionPrune" (default / Automatic) and "BruteForce" methods are the
   original code, kept identical to ResourceFunction["FindFiniteModels"]. *)

BeginPackage["THVMLink`ATP`", {"THVMLink`", "Wolfram`Parser`"}]

TFindFiniteModels::usage =
    "TFindFiniteModels[rels] finds models in form of multiplication tables " <>
    "consistent with the relations rels for each operator in rels, assuming " <>
    "each variable can have one of two values.  TFindFiniteModels[rels, k] " <>
    "allows k >= 2 values for each variable.  TFindFiniteModels[rels, k, prop] " <>
    "returns a specified property prop (\"Association\", \"Indices\", " <>
    "\"Models\") of found models.  Method -> \"SAT\" computes the models via " <>
    "the built-in SatisfiabilityInstances SAT solver."

Begin["`Private`"]

NestedTupleFromIndex[index_, k_, n_] :=
    ArrayReshape[IntegerDigits[index, k, k^n], Table[k, n]]

MixedTupleFromIndex[index_, bs_] := IntegerDigits[index, MixedRadix[bs], Length[bs]]

mixedTupleFromIndex[index_, bs_] :=
    Rest @ FoldPairList[
        {Quotient[#1, #2], Mod[#1, #2]}&,
        index,
        Reverse @ (FoldList[Times, 1, #]&) @ Reverse @ bs
    ]

wrap[x_, h_ : List, g_ : List] := If[MatchQ[x, Blank[h]], x, g[x]]

MapTake[_, {}, _] := {}
MapTake[f_, expr_List, Infinity] := Catenate @ Map[f, expr]
MapTake[f_, expr_, UpTo[n_Integer]] := MapTake[f, expr, n]
MapTake[f_, expr_List, n_Integer] :=
    With[{first = First @ expr, rest = Rest @ expr, m = Max[n, 0]},
        With[{take = Take[f @ first, UpTo[m]]},
            If[ Length[take] >= m,
                take,
                Join[take, Take[MapTake[f, rest, m - Length[take]], UpTo[m]]]
            ]
        ]
    ]

ParallelMapTake[f_, expr_, Infinity] :=
    Catenate @ ParallelMap[f, expr, DistributedContexts -> Automatic]
ParallelMapTake[f_, expr_List, n_] :=
    Module[{firstBatch, restBatch},
        {firstBatch, restBatch} = TakeDrop[expr, UpTo[$ProcessorCount]];
        Take[
            Catenate @ ParallelMap[
                MapTake[f, Prepend[restBatch, #], n]&,
                firstBatch,
                DistributedContexts -> Automatic
            ],
            n
        ]
    ]

$compilationTarget := $compilationTarget = If[noCCompilerQ[], "WVM", "C"]
noCCompilerQ[] :=
    TrueQ @ Quiet @ Check[
        Compile[{}, 1 + 1, CompilationTarget -> "C"],
        True,
        CCompilerDriver`CreateLibrary::nocomp
    ]

Options[TFindFiniteModels] =
    {"Parallelize" -> False, Method -> Automatic, MaxItems -> Infinity, "ReverseOrdering" -> False}

TFindFiniteModels[relations_, k_Integer : 2, prop_String : "Association", opts : OptionsPattern[]] :=
    Catch[
        Module[{arities, atoms, atomSymbols, strippedRelations, activeRelations},
            (* strip quantifiers *)
            strippedRelations = Inactivate[relations, Equal | Unequal] /. {
                HoldPattern[ForAll[_, expr_]] :> expr,
                HoldPattern[ForAll[_, cond_, expr_]] :> cond && expr,
                HoldPattern[Exists[{xs__}, expr_]] :> (expr /. (Except[#[___], #] -> #[]& /@ {xs})),
                HoldPattern[Exists[{xs__}, cond_, expr_]] :> (cond && expr /. (Except[#[___], #] -> #[]& /@ {xs})),
                HoldPattern[Exists[x_, expr_]] :> (expr /. Except[x[___], x] -> x[]),
                HoldPattern[Exists[x_, cond_, expr_]] :> (cond && expr /. Except[x[___], x] -> x[])
            };
            arities = KeyDrop[
                ResourceFunction["FindHeadArities", ResourceVersion -> "2.0.0"][strippedRelations, "IncludeAtoms" -> True],
                {Inactive, List, Rule, TwoWayRule, Equal, Unequal, Exists, ForAll, And, Or, Not, Implies, Xor, Nand, Nor, Xnor, Alternatives}
            ];
            atoms = Keys @ KeySelect[Select[arities, Length[#] == 0&], Not @* IntegerQ];
            arities = If[Equal @@ #, First[#], Throw[Failure["InconsistentArity", <||>]]]& /@ Select[arities, Length[#] > 0&];

            If[ Length[arities] == 0,
                If[ Length @ atoms > 0,
                    TFindFiniteModels[strippedRelations /. (Except[#[___], #] -> #[]& /@ atoms), k, prop, opts],
                    findFiniteModels[{}, <||>, {}, k, prop, opts]
                ],
                atomSymbols = Switch[#, _String, Symbol["s" <> #], _Symbol, #, _, Throw[Failure["WrongAtomicType", <||>]]]& /@ atoms;
                activeRelations = BooleanConvert[
                    Activate[strippedRelations] /. (f : Equal | Unequal)[xs__] :> And @@ f @@@ Partition[{xs}, 2, 1] /. List -> And /. Thread[atoms -> atomSymbols]
                ];
                If[ MatchQ[activeRelations, _Alternatives],
                    (If[AssociationQ[#], KeySort[#], Sort[#]]&)[
                        Join @@ (findFiniteModels[{#}, arities, atomSymbols, k, prop, opts]& /@ List @@ activeRelations)
                    ],
                    findFiniteModels[activeRelations, arities, atomSymbols, k, prop, opts]
                ]
            ]
        ]
    ]

TFindFiniteModels[
    stringRelations : ({(Rule | TwoWayRule | Equal | Unequal)[_String, _String]..} | (Rule | TwoWayRule | Equal | Unequal)[_String, _String]),
    k_Integer : 2, prop_String : "Association", opts : OptionsPattern[]
] :=
    Module[{vars = Union[Flatten[Characters /@ Cases[stringRelations, _String, Infinity]]]},
        vars = Join[vars, ToString /@ Range[2, 4 - Length[vars]]];
        With[{
            relations = And @@ Join[
                Map[Fold[SmallCircle, Characters[#]]&, wrap[stringRelations], {-1}],
                {
                    (#1 \[SmallCircle] #2) \[SmallCircle] #3 == #1 \[SmallCircle] (#2 \[SmallCircle] #3)& @@ Take[vars, 3],
                    "1"[] \[SmallCircle] vars[[1]] == vars[[1]],
                    vars[[1]] \[SmallCircle] "1"[] == vars[[1]]
                }
            ]
        },
            TFindFiniteModels[relations, k, prop, opts]
        ]
    ]

Options[findFiniteModels] = Options[TFindFiniteModels]
findFiniteModels[relations_, arities_Association, vars_List, k_Integer, prop_, OptionsPattern[]] :=
    Module[{relationFunction, indices},
        relationFunction = Function @@ {vars, ReleaseHold[relations] /. {Rule | TwoWayRule -> Equal, List -> And}};
        Switch[OptionValue[Method] /. Automatic -> "ExpressionPrune",
            "BruteForce",
            Module[{index, selectFunction, numIndices},
                numIndices = Times @@ (k^(k^Values[arities]));
                selectFunction = With[{
                    maxItems = OptionValue[MaxItems] /. Infinity -> -1,
                    checkFunction = Function[index,
                        Evaluate @ With[{integerDigit = Function[Mod[Quotient[#1, k^Total[(k^Range[Length[#2] - 1, 0, -1])(k - #2 - 1)]], k]]},
                            With[{inlineRelation = Evaluate[relationFunction //. MapIndexed[#1[args___] :> integerDigit[mixedTupleFromIndex[index, k^k^Values[arities]][[#2[[1]]]], {args}]&, Keys @ arities]]},
                                Array[inlineRelation, Table[k, Length @ vars], 0, And]
                            ]
                        ], Listable]
                },
                    Compile[{{from, _Integer}, {to, _Integer}},
                        is = {};
                        Do[
                            If[maxItems >= 0 && Length @ is >= maxItems, Break[]]; If[checkFunction[i], AppendTo[is, i]],
                            {i, from, to - 1}
                        ];
                        is,
                        {{is, _Integer, 1}},
                        CompilationTarget -> $compilationTarget, RuntimeOptions -> "Speed"
                    ]
                ];
                indices = Take[
                    If[ TrueQ[OptionValue["Parallelize"]],
                        Catenate @ ParallelMap[Apply[selectFunction], Partition[Ceiling @ Subdivide[numIndices, $ProcessorCount], 2, 1]],
                        selectFunction[0, numIndices]
                    ],
                    UpTo[OptionValue[MaxItems]]
                ];
                indices = MixedTupleFromIndex[#, k^k^Values[arities]]& /@ indices
            ],
            "SAT",
            indices = satIndices[relationFunction, arities, vars, k],
            "CongruenceClosure",
            indices = ccIndices[ccWlDecideQ, relationFunction, arities, vars, k, OptionValue[MaxItems]],
            "CongruenceClosureC",
            indices = ccCIndicesIncremental[relationFunction, arities, vars, k, OptionValue[MaxItems]],
            "ExpressionPruneC",
            indices = epcIndices[relationFunction, arities, vars, k, OptionValue[MaxItems]],
            "Z3",
            indices = z3Indices[relationFunction, arities, vars, k, OptionValue[MaxItems]],
            "ExpressionPrune",
            Module[{expr, digits, toDigitLogic, compatibleQ, pruneSelectTuples, extend},

                digits /: Plus[digits[a_], b_digits] := digits[Map[# + b&, a]];
                digits /: Equal[digits[a_], b_digits] := digits[Map[# - b&, a]] == 0;
                digits /: digits[a_] + b_ := digits[a + b];
                digits /: digits[a_] * b_ := digits[a * b];
                digits /: a_ + digits[b_] := digits[a + b];
                digits /: a_ * digits[b_] := digits[a * b];

                toDigitLogic[expr_] :=
                    FixedPoint[
                        (* shove f through inner digits *)
                        Map[Identity, #, {Depth[#]}]& @ ReplaceAll[#,
                            f_[digits[a_]] :> digits @ Association @ KeyValueMap[{key, vs} |-> key -> Association @ KeyValueMap[#1 -> #2 /. key -> #1&, vs], Map[Map[f], a]]
                        ]&,
                        (* i'th digit equals to itself *)
                        expr /. f_[i_Integer] :> digits[<|f[i] -> AssociationThread[Range[0, k - 1], Range[0, k - 1]]|>]
                    ]
                        (* inner digits must be equal to the outer *)
                        /. digits[<|f_[x_] -> a_|>] :> digits[<|f[x] -> Association @ KeyValueMap[#1 -> #2 /. digits[<|f[x] -> b_|>] :> digits[<|f[x] -> KeySelect[b, EqualTo[#1]]|>]&, a]|>]
                        (* expand equalities into a disjunction/conjunction *)
                        //. {
                            (digits[<|f_[x_] -> a_|>] == y_ | y_ == digits[<|f_[x_] -> a_|>]) :> Or @@ KeyValueMap[f[x] == #1 && #2 == y&, a],
                            (digits[<|f_[x_] -> a_|>] != y_Integer | y_Integer != digits[<|f_[x_] -> a_|>]) :> Or @@ Map[t |-> Or @@ KeyValueMap[f[x] == #1 && #2 == t&, a], DeleteCases[Range[0, k - 1], y]],
                            x_ != y_ :> Or @@ Map[x == # && y != #&, Range[0, k - 1]]
                        };

                (* recursive expression *)
                expr = With[{integerDigit = #1[Total[(k^Range[Length[#2] - 1, 0, -1])(k - #2 - 1)]]&},
                    With[{inlineRelation = relationFunction //. MapIndexed[#1[args___] :> integerDigit[#1, {args}]&, Keys @ arities]},
                        Array[inlineRelation, Table[k, Length @ vars], 0, And]
                    ]
                ];

                expr = If[TrueQ[OptionValue["Parallelize"]], ParallelMap, Map][LogicalExpand, FixedPoint[toDigitLogic, expr] /. digits[<|f_[x_] -> a_|>] :> Or @@ KeyValueMap[f[x] == #1 && #1 == #2&, a]];

                compatibleQ = And @@ GroupBy[List @@ wrap[And[##], And, List], First, Apply[SameQ]]&;

                pruneSelectTuples[{x_}, __] := List /@ wrap[x];
                pruneSelectTuples[x_, __] := {x};
                pruneSelectTuples[list_List, crit_, depth_ : 0] :=
                    With[{sorted = SortBy[list, Length]},
                        With[{first = First @ sorted, rest = Rest @ sorted},
                            With[{rec = x |-> With[{subTuples = pruneSelectTuples[Map[Select[wrap @ #, crit[x, #]&]&, rest], crit, depth + 1]}, Prepend[x] /@ subTuples]},
                                If[ListQ[first], If[depth == 0 && TrueQ[OptionValue["Parallelize"]], ParallelMapTake, MapTake][rec, first, OptionValue[MaxItems]], rec[first]]
                            ]
                        ]
                    ];

                (* extend unresolved/free symbols with all possible values *)
                extend[expr_] :=
                    Module[{freeVars, coeffRules, coeffs, freeCoeff},
                        freeVars = Variables[expr];
                        If[ Length @ freeVars == 0,
                            {expr},
                            coeffRules = CoefficientRules[expr, freeVars];
                            freeCoeff = Lookup[coeffRules, Key @ Table[0, Length @ freeVars], 0];
                            coeffs = IdentityMatrix[Length @ freeVars] /. coeffRules;
                            freeCoeff + Tuples[Range[0, k - 1], Length[freeVars]].coeffs
                        ]
                    ];

                expr = DeleteDuplicates @ Map[If[MatchQ[#, _Or], List @@ #, {#}]&, If[MatchQ[expr, _And], List @@ expr, {expr}]];

                indices =
                    Union @ Catenate @ Map[Map[extend] /* Tuples] @ Map[
                        Map[FromDigits[Reverse @ #, k]&],
                        With[{sol = Map[(And @@ wrap @ #&) /* (If[MatchQ[#, _And], DeleteDuplicates[#], #]&) /* ToRules, pruneSelectTuples[expr, compatibleQ]]},
                            If[Length @ sol > 0, KeyValueMap[Table[#1[n], {n, 0, k^#2 - 1}]&, arities] /. sol, {}]
                        ]
                    ]
            ]
        ];
        indices = Take[indices, UpTo[OptionValue[MaxItems]]];
        If[TrueQ[OptionValue["ReverseOrdering"]] && Length[indices] > 0, indices = Reverse[Threaded[k^(k^Values[arities])] - indices - 1]];
        Switch[prop,
            "Indices",
            indices,
            "Models",
            Map[AssociationThread[Keys[arities], MapThread[NestedTupleFromIndex[#1, k, #2]&, {#, Values[arities]}]]&, indices],
            _,
            AssociationMap[AssociationThread[Keys[arities], MapThread[NestedTupleFromIndex[#1, k, #2]&, {#, Values[arities]}]]&, indices]
        ]
    ]

(* ----- "SAT" Method -----
   Compute the model `indices` through the built-in Wolfram SAT solver.  Each
   operator op of arity ar contributes the cells op @@ tup for tup in
   Tuples[Range[0, k - 1], ar] (row-major); each cell takes a value 0..k-1.  We
   introduce a propositional variable satVar[{op, tup}, v] meaning "cell op[tup]
   holds value v" and add per-cell totality (exactly-one) clauses.  The grounded
   relation closure Array[relationFunction, ...] is encoded propositionally:
   each (sub)term is reduced to an indicator pair[v] -> proposition "this term
   evaluates to v" (satEval), so nested compositions f[g[..], ..] are expanded
   into disjunctions over the intermediate cell values.  SatisfiabilityInstances
   enumerates ALL satisfying assignments, each decoded to a per-operator index
   list in the same format the other methods produce. *)
satIndices[relationFunction_, arities_Association, vars_List, k_] :=
    Module[{ops, cellSpec, allCells, vals, satEval, satEqual, satUnequal, totality, closure, formula, varList, instances},
        ops = Keys[arities];
        (* cellSpec: per-operator { op, ar, cell-tuples (row-major) }. *)
        cellSpec = Map[op |-> {op, arities[op], Tuples[Range[0, k - 1], arities[op]]}, ops];
        (* Flat cell list, each as {op, tup}. *)
        allCells = Catenate @ Map[spec |-> Map[tup |-> {spec[[1]], tup}, spec[[3]]], cellSpec];
        vals = Range[0, k - 1];
        (* satEval[term][v] is a proposition asserting that term evaluates to v.
           A ground integer evaluates to itself; an operator application chooses
           values for its arguments and reads off the corresponding cell. *)
        satEval[i_Integer][v_] := i == v;
        satEval[(op_ /; MemberQ[ops, op])[args___]][v_] :=
            Or @@ Map[
                argVals |-> And @@ MapThread[#1[#2]&, {Map[satEval, {args}], argVals}] && satVar[{op, argVals}, v],
                Tuples[vals, Length[{args}]]
            ];
        (* Two terms are equal iff they share a common value. *)
        satEqual[a_, b_] := Or @@ Map[v |-> satEval[a][v] && satEval[b][v], vals];
        (* Two terms are unequal iff they take distinct values. *)
        satUnequal[a_, b_] := Or @@ Map[pair |-> satEval[a][pair[[1]]] && satEval[b][pair[[2]]], Select[Tuples[vals, 2], #[[1]] =!= #[[2]]&]];
        (* Totality: each cell holds exactly one value. *)
        totality = And @@ Map[
            cell |-> With[{vs = Map[satVar[cell, #]&, vals]},
                Or @@ vs && And @@ Map[Apply[Nand], Subsets[vs, {2}]]
            ],
            allCells
        ];
        (* Grounded relation closure, with each equality atom rewritten to its
           propositional indicator form via satEqual. *)
        closure = Array[relationFunction, Table[k, Length[vars]], 0, And] /. {Equal[a_, b_] :> satEqual[a, b], Unequal[a_, b_] :> satUnequal[a, b]};
        formula = totality && closure;
        varList = Map[Apply[satVar], Tuples[{allCells, vals}]];
        instances = SatisfiabilityInstances[formula, varList, All];
        (* Decode each instance: per cell, the unique value whose satVar is True;
           per-operator index = FromDigits[Flatten[table], k] in cell order. *)
        Union @ Map[
            inst |-> With[{asg = AssociationThread[varList, inst]},
                Map[
                    spec |-> FromDigits[
                        Map[tup |-> First @ Select[vals, asg[satVar[{spec[[1]], tup}, #]]&], spec[[3]]],
                        k
                    ],
                    cellSpec
                ]
            ],
            instances
        ]
    ]

(* ----- "CongruenceClosure" Method -----
   Compute the model `indices` through thvm's own congruence-closure decision
   procedure (TSatEUF), with no external SAT solver.  Backtrack over the
   operation-table cells; at each partial assignment assert the assigned cells
   together with the grounded equational atoms whose cells are all assigned,
   and prune when TSatEUF returns UNSAT (distinct domain elements are asserted
   apart, so congruence propagation rejects an inconsistent partial table).  At
   a complete assignment, verify the full (possibly Boolean) relation closure by
   direct evaluation.  Encodes each model in the same per-operator index format
   the other methods produce. *)
ccCell[assign_, op_, args_] := assign[Key[{op, args}]]

ccEncode[assign_, cellSpec_, k_] :=
    Map[spec |-> FromDigits[Map[tup |-> assign[Key[{spec[[1]], tup}]], spec[[3]]], k], cellSpec]

ccWlDecideQ[eqs_, diseqs_] := TSatEUF[eqs, diseqs]["Status"] === "SAT"

(* ccCDecide is the LibraryLink binding to src/cc's congruence closure
   (thvm_wl_cc_decide); returns 1 (SAT) / 0 (UNSAT).  Encoding:
     terms : flat record stream, term id t (0-based, stream order) is
             {tag, sym, nargs, arg-ids...}, tag 0 atom / 1 application;
     eqPairs / nePairs : flat even-length lists of asserted term-id pairs.
   ccCDecideQ marshals the SAME literal lists ccIndices feeds the WL
   decider into that encoding and agrees with ccWlDecideQ. *)
ccCDecide := ccCDecide = load["thvm_wl_cc_decide",
    {{Integer, 1}, {Integer, 1}, {Integer, 1}}, Integer]

(* Incremental engine: a persistent CcState behind an integer handle, so
   one finite-model search shares a single congruence-closure state and
   backtracks with push / pop instead of re-deciding the whole problem at
   every cell. *)
ccNew        := ccNew        = load["thvm_cc_new", {}, Integer]
ccFreeHandle := ccFreeHandle = load["thvm_cc_free_handle", {Integer}, Integer]
ccInternAtom := ccInternAtom = load["thvm_cc_intern_atom", {Integer, Integer}, Integer]
ccInternApp  := ccInternApp  = load["thvm_cc_intern_app", {Integer, Integer, {Integer, 1}}, Integer]
ccPush       := ccPush       = load["thvm_cc_push", {Integer}, Integer]
ccPop        := ccPop        = load["thvm_cc_pop", {Integer, Integer}, Integer]
ccAssertEq   := ccAssertEq   = load["thvm_cc_assert_eq", {Integer, Integer, Integer}, Integer]
ccAssertNe   := ccAssertNe   = load["thvm_cc_assert_ne", {Integer, Integer, Integer}, Integer]
ccCheck      := ccCheck      = load["thvm_cc_check", {Integer}, Integer]

(* Stable integer symbol ids: each domain element (Integer) and each
   operator head gets a distinct nonnegative id, kept disjoint so an
   element and a same-valued operator label never collide. *)
ccSymId[heads_][x_Integer] := 2 x
ccSymId[heads_][h_] := 2 Lookup[heads, h] + 1

(* Encode all distinct subterms of `terms` to {tag, sym, nargs, args...}
   records in topological order, returning {recordStream, termId function}. *)
ccEncodeTerms[terms_] :=
    Module[{syms, heads, ids = <||>, count = 0, recs = Internal`Bag[], emit},
        (* Every operator head AND every atomic (nullary) symbol needs an id. *)
        syms = DeleteDuplicates @ Join[
            Cases[terms, h_[___] :> h, {0, Infinity}],
            Cases[terms, s_Symbol, {0, Infinity}]
        ];
        heads = AssociationThread[syms, Range[0, Length[syms] - 1]];
        emit[t_] :=
            If[ KeyExistsQ[ids, t],
                ids[t],
                With[{rec = If[ AtomQ[t],
                        {0, ccSymId[heads][t], 0},
                        With[{childIds = emit /@ (List @@ t)},
                            Join[{1, ccSymId[heads][Head[t]], Length[childIds]}, childIds]
                        ]
                    ]},
                    (* Level-1 stuff appends rec's elements individually, so the
                       bag already holds the flat {tag, sym, nargs, args...}
                       stream -- no Catenate needed. *)
                    Internal`StuffBag[recs, rec, 1];
                    ids[t] = count;
                    count = count + 1;
                    ids[t]
                ]
            ];
        Scan[emit, terms];
        {Internal`BagPart[recs, All], emit}
    ]

ccCDecideQ[eqs_, diseqs_] :=
    Module[{pre, eqsC, diseqsC, allTerms, stream, idOf, eqFlat, neFlat},
        pre = ccPreprocess[eqs, diseqs];
        If[ pre === "UNSAT", Return[False, Module]];
        {eqsC, diseqsC} = pre;
        allTerms = DeleteDuplicates @ Catenate[List @@@ Join[eqsC, diseqsC]];
        {stream, idOf} = ccEncodeTerms[allTerms];
        eqFlat = Catenate @ Map[lit |-> {idOf[lit[[1]]], idOf[lit[[2]]]}, eqsC];
        neFlat = Catenate @ Map[lit |-> {idOf[lit[[1]]], idOf[lit[[2]]]}, diseqsC];
        ccCDecide[stream, eqFlat, neFlat] === 1
    ]

(* Mirror TSatEUF's literal preprocessing: drop vacuous True literals,
   short-circuit a literally-False literal to UNSAT.  Returns "UNSAT"
   or {cleanEqs, cleanDiseqs}. *)
ccPreprocess[eqs_, diseqs_] :=
    If[ MemberQ[eqs, False] || MemberQ[diseqs, False],
        "UNSAT",
        {DeleteCases[eqs, True], DeleteCases[diseqs, True]}
    ]

ccIndices[decideQ_, relationFunction_, arities_Association, vars_List, k_, maxItems_] :=
    Module[{ops, cellSpec, allCells, nCells, distinct, groundEqs, assigned, fullClosure, results, dfs},
        ops = Keys[arities];
        cellSpec = Map[op |-> {op, arities[op], Tuples[Range[0, k - 1], arities[op]]}, ops];
        allCells = Catenate @ Map[spec |-> Map[tup |-> {spec[[1]], tup}, spec[[3]]], cellSpec];
        nCells = Length[allCells];
        distinct = Map[Apply[Unequal], Subsets[Range[0, k - 1], {2}]];
        (* The grounded equational atoms the relations impose, over all var-tuples. *)
        groundEqs = DeleteDuplicates @ Flatten @ Map[
            inst |-> Cases[If[ MatchQ[inst, _And], List @@ inst, {inst}], HoldPattern[Equal[_, _]]],
            relationFunction[Sequence @@ #]& /@ Tuples[Range[0, k - 1], Length[vars]]
        ];
        assigned[atom_] := DeleteDuplicates @ Cases[atom, (op_ /; MemberQ[ops, op])[a___] :> {op, {a}}, {0, Infinity}];
        fullClosure[assign_] :=
            With[{inline = relationFunction //. MapIndexed[#1[args___] :> ccCell[assign, #1, {args}]&, ops]},
                TrueQ @ Array[inline, Table[k, Length[vars]], 0, And]
            ];
        results = Internal`Bag[];
        dfs[idx_, assign_, lits_] :=
            If[ maxItems =!= Infinity && Internal`BagLength[results] >= maxItems,
                Null,
                If[ idx > nCells,
                    If[ fullClosure[assign], Internal`StuffBag[results, ccEncode[assign, cellSpec, k]]],
                    With[{cell = allCells[[idx]]},
                        Scan[
                            v |-> Module[{asg, asgCells, newEqs, lits2},
                                asg = Append[assign, Key[cell] -> v];
                                asgCells = Keys[asg];
                                newEqs = Select[groundEqs, MemberQ[assigned[#], cell] && SubsetQ[asgCells, assigned[#]]&];
                                lits2 = Join[lits, {(cell[[1]] @@ cell[[2]]) == v}, newEqs];
                                If[ decideQ[lits2, distinct], dfs[idx + 1, asg, lits2]]
                            ],
                            Range[0, k - 1]
                        ]
                    ]
                ]
            ];
        dfs[1, <||>, {}];
        Union @ Internal`BagPart[results, All]
    ]

(* ----- incremental "C" Method -----
   Same backtracking search as ccIndices, but over ONE persistent
   congruence-closure handle.  The domain elements 0..k-1 are interned as
   atoms and asserted pairwise-distinct ONCE up front; the cell DFS then
   pushes a checkpoint, interns + asserts the chosen cell value (plus any
   ground equality atom whose cells are now all assigned), checks for
   UNSAT to prune, recurses, and pops on backtrack.  A full assignment is
   verified by direct Boolean evaluation, exactly as the other methods.
   Terms are interned bottom-up through a memo so each subterm becomes a
   node at most once for the whole search. *)
ccCIndicesIncremental[relationFunction_, arities_Association, vars_List, k_, maxItems_] :=
    Module[{ops, cellSpec, allCells, nCells, groundEqs, heads, handle, termNode, intern, assertAtom, assigned, fullClosure, results, dfs, out},
        ops = Keys[arities];
        cellSpec = Map[op |-> {op, arities[op], Tuples[Range[0, k - 1], arities[op]]}, ops];
        allCells = Catenate @ Map[spec |-> Map[tup |-> {spec[[1]], tup}, spec[[3]]], cellSpec];
        nCells = Length[allCells];
        groundEqs = DeleteDuplicates @ Flatten @ Map[
            inst |-> Cases[If[ MatchQ[inst, _And], List @@ inst, {inst}], HoldPattern[Equal[_, _]]],
            relationFunction[Sequence @@ #]& /@ Tuples[Range[0, k - 1], Length[vars]]
        ];
        heads = AssociationThread[ops, Range[0, Length[ops] - 1]];
        handle = ccNew[];
        termNode = <||>;
        intern[t_Integer] :=
            Lookup[termNode, t, termNode[t] = ccInternAtom[handle, ccSymId[heads][t]]];
        intern[t_] :=
            Lookup[termNode, t, termNode[t] = ccInternApp[handle, ccSymId[heads][Head[t]], intern /@ (List @@ t)]];
        assertAtom[Equal[a_, b_]] := ccAssertEq[handle, intern[a], intern[b]];
        assigned[atom_] := DeleteDuplicates @ Cases[atom, (op_ /; MemberQ[ops, op])[a___] :> {op, {a}}, {0, Infinity}];
        fullClosure[assign_] :=
            With[{inline = relationFunction //. MapIndexed[#1[args___] :> ccCell[assign, #1, {args}]&, ops]},
                TrueQ @ Array[inline, Table[k, Length[vars]], 0, And]
            ];
        results = Internal`Bag[];
        (* Assert the domain elements pairwise distinct once. *)
        Scan[
            pair |-> ccAssertNe[handle, intern[pair[[1]]], intern[pair[[2]]]],
            Subsets[Range[0, k - 1], {2}]
        ];
        dfs[idx_, assign_, asgCells_] :=
            If[ maxItems =!= Infinity && Internal`BagLength[results] >= maxItems,
                Null,
                If[ idx > nCells,
                    If[ fullClosure[assign], Internal`StuffBag[results, ccEncode[assign, cellSpec, k]]],
                    With[{cell = allCells[[idx]]},
                        Scan[
                            v |-> Module[{asg, cells2, mark, newEqs},
                                asg = Append[assign, Key[cell] -> v];
                                cells2 = Append[asgCells, cell];
                                mark = ccPush[handle];
                                assertAtom[(cell[[1]] @@ cell[[2]]) == v];
                                newEqs = Select[groundEqs, MemberQ[assigned[#], cell] && SubsetQ[cells2, assigned[#]]&];
                                Scan[assertAtom, newEqs];
                                If[ ccCheck[handle] === 1, dfs[idx + 1, asg, cells2]];
                                ccPop[handle, mark]
                            ],
                            Range[0, k - 1]
                        ]
                    ]
                ]
            ];
        dfs[1, <||>, {}];
        ccFreeHandle[handle];
        out = Union @ Internal`BagPart[results, All];
        out
    ]

(* ----- "ExpressionPruneC" Method -----
   A HYBRID of "ExpressionPrune": the symbolic clausification stays in WL,
   the hot enumeration (pruneSelectTuples + extend + FromDigits) runs in C
   (src/ffmep/_.c).  epClauseDB grounds + clausifies the relation EXACTLY
   like the "ExpressionPrune" branch (same digits / toDigitLogic /
   LogicalExpand / split into clause-sets), then encodes the clause DB to
   integers for the C solver, which returns the per-operator model index
   lists -- so "ExpressionPruneC" is differential-identical to
   "ExpressionPrune". *)

(* ffmepSolve is the LibraryLink binding to src/ffmep's enumerator.  It
   takes the integer-encoded clause DB (opSize, litFlat, clauseOff,
   setOff, setClause) + k + maxItems and returns the model index lists as
   an {nmodels, nops} Integer MTensor. *)
ffmepSolve := ffmepSolve = load["thvm_wl_ffmep_solve",
    {{Integer, 1}, {Integer, 1}, {Integer, 1}, {Integer, 1}, {Integer, 1},
     Integer, Integer}, {Integer, 2}]

(* epClauseDB grounds + clausifies the relation into the clause-set list
   `expr` (a list of clause-sets; each clause-set a list of clauses; each
   clause a literal `op[pos] == value` or an And of such literals), and
   returns it together with what the C solver needs: the operator order,
   each operator's table size k^arity, and the per-operator base offset
   (cumulative sum of table sizes).  This is the "ExpressionPrune" pipeline
   up to `expr`, kept identical so the C enumeration reproduces it. *)
epClauseDB[relationFunction_, arities_Association, vars_List, k_Integer] :=
    Module[{expr, digits, toDigitLogic, ops, sizes, offsets},
        digits /: Plus[digits[a_], b_digits] := digits[Map[# + b&, a]];
        digits /: Equal[digits[a_], b_digits] := digits[Map[# - b&, a]] == 0;
        digits /: digits[a_] + b_ := digits[a + b];
        digits /: digits[a_] * b_ := digits[a * b];
        digits /: a_ + digits[b_] := digits[a + b];
        digits /: a_ * digits[b_] := digits[a * b];

        toDigitLogic[expr0_] :=
            FixedPoint[
                Map[Identity, #, {Depth[#]}]& @ ReplaceAll[#,
                    f_[digits[a_]] :> digits @ Association @ KeyValueMap[{key, vs} |-> key -> Association @ KeyValueMap[#1 -> #2 /. key -> #1&, vs], Map[Map[f], a]]
                ]&,
                expr0 /. f_[i_Integer] :> digits[<|f[i] -> AssociationThread[Range[0, k - 1], Range[0, k - 1]]|>]
            ]
                /. digits[<|f_[x_] -> a_|>] :> digits[<|f[x] -> Association @ KeyValueMap[#1 -> #2 /. digits[<|f[x] -> b_|>] :> digits[<|f[x] -> KeySelect[b, EqualTo[#1]]|>]&, a]|>]
                //. {
                    (digits[<|f_[x_] -> a_|>] == y_ | y_ == digits[<|f_[x_] -> a_|>]) :> Or @@ KeyValueMap[f[x] == #1 && #2 == y&, a],
                    (digits[<|f_[x_] -> a_|>] != y_Integer | y_Integer != digits[<|f_[x_] -> a_|>]) :> Or @@ Map[t |-> Or @@ KeyValueMap[f[x] == #1 && #2 == t&, a], DeleteCases[Range[0, k - 1], y]],
                    x_ != y_ :> Or @@ Map[x == # && y != #&, Range[0, k - 1]]
                };

        expr = With[{integerDigit = #1[Total[(k^Range[Length[#2] - 1, 0, -1])(k - #2 - 1)]]&},
            With[{inlineRelation = relationFunction //. MapIndexed[#1[args___] :> integerDigit[#1, {args}]&, Keys @ arities]},
                Array[inlineRelation, Table[k, Length @ vars], 0, And]
            ]
        ];
        expr = Map[LogicalExpand, FixedPoint[toDigitLogic, expr] /. digits[<|f_[x_] -> a_|>] :> Or @@ KeyValueMap[f[x] == #1 && #1 == #2&, a]];
        expr = DeleteDuplicates @ Map[If[MatchQ[#, _Or], List @@ #, {#}]&, If[MatchQ[expr, _And], List @@ expr, {expr}]];

        ops = Keys @ arities;
        sizes = k^Values[arities];
        offsets = Most @ Prepend[Accumulate @ sizes, 0];
        <|"Expr" -> expr, "Ops" -> ops, "Sizes" -> sizes, "Offsets" -> offsets|>
    ]

(* epcIndices marshals epClauseDB's output to the C solver and returns the
   per-operator model index lists in the same {model -> per-operator index
   list} format the other branches feed the shared output tail, Union-
   sorted.  A literal `op[pos] == value` is encoded to the integer
   cellId*k + value, where cellId = offset[op] + pos.

   The C solver's MapTake enumeration order must match the WL
   pruneSelectTuples, whose clause-set MRV is First @ SortBy[_, Length]
   with ties broken by Mathematica's canonical Order.  The C comparator
   reproduces that order ONLY if operators are numbered in canonical
   (Sort[ops]) order, so the cell offsets here are assigned in Sort[ops]
   order.  The C model rows then come back in Sort[ops] column order; they
   are permuted back to the original Keys[arities] order so the output
   matches "ExpressionPrune" exactly. *)
epcIndices[relationFunction_, arities_Association, vars_List, k_, maxItems_] :=
    Module[{db, expr, ops, canonOps, canonSizes, offsetOf, encodeLit,
            encodeClause, clauseLists, litFlat, clauseOff, setOff, setClause,
            raw, canonToOrig},
        db = epClauseDB[relationFunction, arities, vars, k];
        expr = db["Expr"];
        ops = db["Ops"];
        canonOps = Sort @ ops;
        canonSizes = k^Lookup[arities, canonOps];
        offsetOf = AssociationThread[canonOps, Most @ Prepend[Accumulate @ canonSizes, 0]];
        encodeLit[op_[pos_Integer] == value_Integer] := (offsetOf[op] + pos) k + value;
        (* A `True` clause (a tautological clause-set) is the empty literal
           list -- always selectable, binding nothing.  A `False` disjunct is
           dropped (never selected) at the clause-set level below. *)
        encodeClause[True] := {};
        encodeClause[cl_] := encodeLit /@ If[MatchQ[cl, _And], List @@ cl, {cl}];
        (* per clause-set, the list of clauses (each a list of literal-ints);
           drop any False disjunct first. *)
        clauseLists = Map[Map[encodeClause, DeleteCases[#, False]]&, expr];
        (* flatten clause lists across all sets into a single clause stream
           with offsets; record which clauses belong to which set. *)
        {litFlat, clauseOff, setOff, setClause} = epcFlatten[clauseLists];
        raw = ffmepSolve[canonSizes, litFlat, clauseOff, setOff, setClause, k,
            maxItems /. Infinity -> -1];
        (* raw is an {nmodels, nops} matrix in canonOps column order; permute
           columns back to the original Keys[arities] order, then Union-sort to
           match the EP dedup. *)
        canonToOrig = Lookup[PositionIndex[canonOps], ops][[All, 1]];
        Union @ Map[#[[canonToOrig]]&, Normal @ raw]
    ]

(* epcFlatten turns the nested clause lists (per clause-set, per clause, the
   literal-ints) into the four flat integer arrays the C solver reads:
   litFlat (all literals, contiguous per clause), clauseOff (offsets into
   litFlat, length nclauses+1), setOff (offsets into setClause, length
   nsets+1), setClause (clause ids grouped per clause-set). *)
epcFlatten[clauseLists_] :=
    Module[{flatClauses, clauseLens, litFlat, clauseOff, setLens, setOff, setClause},
        flatClauses = Catenate @ clauseLists;
        clauseLens = Length /@ flatClauses;
        litFlat = Catenate @ flatClauses;
        clauseOff = Prepend[Accumulate @ clauseLens, 0];
        setLens = Length /@ clauseLists;
        setOff = Prepend[Accumulate @ setLens, 0];
        setClause = Range[0, Length[flatClauses] - 1];
        {litFlat, clauseOff, setOff, setClause}
    ]

(* ----- "Z3" Method -----
   Solve the finite-model problem with the real Z3 SMT solver through the
   WolframInstitute`Z3Link paclet (loaded lazily; z3 auto-downloads on first
   use).  Each operator becomes an uninterpreted Z3 function (a Z3 integer for
   a nullary operator), every operation cell is constrained to the domain
   0..k-1, and the relations grounded over the variable tuples become Z3
   constraints.  All models are enumerated by the standard block-and-resolve
   loop; each is decoded to the per-operator index format the other methods
   produce. *)
z3Indices[relationFunction_, arities_Association, vars_List, k_, maxItems_] :=
    Module[{ops, z3name, handle, cellTuples, z3cell, z3term, ranges, ground, solver, step,
            results},
        Needs["WolframInstitute`Z3Link`"];
        ops = Keys[arities];
        (* Sanitized, unique Z3 identifiers: an operator's own name may be an
           integer (e.g. the identity 1[]) or a special symbol, neither a valid
           SMT-LIB identifier. *)
        z3name = AssociationThread[ops, Table["ffmOp" <> ToString[i], {i, Length[ops]}]];
        cellTuples = Association @ Map[op |-> op -> Tuples[Range[0, k - 1], arities[op]], ops];
        handle = Association @ Map[
            op |-> op -> If[ arities[op] === 0,
                WolframInstitute`Z3Link`Z3Int[z3name[op]],
                WolframInstitute`Z3Link`Z3Function[z3name[op], Table["Int", arities[op]], "Int"]],
            ops
        ];
        (* a Z3 term for cell op[tup] (tup a list of domain elements). *)
        z3cell[op_, tup_] := If[arities[op] === 0, handle[op], handle[op] @@ tup];
        (* every cell lands in the domain 0..k-1. *)
        ranges = And @@ Catenate @ Map[
            op |-> Map[tup |-> z3cell[op, tup] >= 0 && z3cell[op, tup] <= k - 1, cellTuples[op]], ops];
        (* Convert a ground relation to a Z3 term post-order (children first):
           an integer is a domain element; an operator application becomes a Z3
           term (built only after its arguments are translated, so a Z3Function
           is never applied to an untranslated subterm); the logical heads
           (Equal/Unequal/And/Or/Not) are rebuilt over translated children. *)
        z3term[t_Integer] := t;
        z3term[t_] /; (! AtomQ[t] && MemberQ[ops, Head[t]]) := z3cell[Head[t], z3term /@ (List @@ t)];
        z3term[h_[args___]] /; ! MemberQ[ops, h] := h @@ (z3term /@ {args});
        z3term[t_] := t;
        ground = z3term[Array[relationFunction, Table[k, Length[vars]], 0, And]];
        solver = WolframInstitute`Z3Link`Z3CreateSolver[];
        WolframInstitute`Z3Link`Z3Assert[solver, ranges && ground];
        results = Internal`Bag[];
        (* One block-and-resolve step: read the current model, encode it, and
           forbid it; return False when no further model exists. *)
        step[] := Module[{values, idxList, lits},
            If[ WolframInstitute`Z3Link`Z3CheckSat[solver] =!= "sat", Return[False, Module]];
            values = Association @ Map[
                op |-> op -> Map[tup |-> WolframInstitute`Z3Link`Z3Eval[solver, z3cell[op, tup]], cellTuples[op]], ops];
            idxList = Map[op |-> FromDigits[values[op], k], ops];
            Internal`StuffBag[results, idxList, 1];
            lits = Catenate @ Map[
                op |-> MapThread[z3cell[op, #1] == #2&, {cellTuples[op], values[op]}], ops];
            WolframInstitute`Z3Link`Z3Assert[solver, ! (And @@ lits)];
            True
        ];
        NestWhile[# &, True, step[] && Internal`BagLength[results] < maxItems &];
        Union @ Partition[Internal`BagPart[results, All], Length[ops]]
    ]

End[]

EndPackage[]
