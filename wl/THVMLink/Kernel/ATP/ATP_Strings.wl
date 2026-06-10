(* ::Package:: *)
(* ATP_Strings.wl -- string rewriting over the equational engine:
   TFindStringProof (the WFR FindStringProof surface: words as
   CenterDot terms + the associativity bridge axiom) and
   TFindEquationalPath (the WFR FindEquationalPath surface: a proved
   goal's witnessing rewrite path, from the engine's goal chain or
   re-walked off a ProofObject's dataset).

   Sibling of ATP.wl in the THVMLink`ATP` context; the recursive
   Kernel loader Gets it after ATP.wl (files sorted by {depth,
   lowercased path}), and it shares the THVMLink`ATP`Private`
   context, so it references ATP.wl's dispatches and helpers by
   bare name. *)

BeginPackage["THVMLink`ATP`", {"THVMLink`"}];

GeneralUtilities`SetUsage[TFindStringProof, "TFindStringProof[thm$, axioms$] proves a string-rewriting theorem over the multiway/semi-Thue axioms and returns the ProofObject.
Words are encoded as right-nested CenterDot terms over one symbol per character ('ABC' encodes to A\[CenterDot](B\[CenterDot]C)), with the associativity bridge axiom appended so a rewrite applies at any position of a word; the encoding follows the Wolfram Function Repository FindStringProof.
An axiom written 'BA' -> 'AB' is installed as a PRE-ORIENTED rewrite rule (one-directional string rewriting); an axiom written {'BA', 'AB'}, 'BA' == 'AB', or 'BA' <-> 'AB' is an equation the engine orients itself.
A theorem is a pair in any of the same shapes (direction is meaningless for a goal); a list of theorems is a multi-goal conjunction returning ONE ProofObject with a Hypothesis/Conclusion row pair per conjunct.
An optional last argument picks the TFindProof return spec ('Status', 'Path', $$); options are TFindProof's (MaxSteps, TimeConstraint, Method, PortfolioFrontLoad)."];

GeneralUtilities`SetUsage[TFindEquationalPath, "TFindEquationalPath[thm$, axioms$] proves thm$ over axioms$ and returns the witnessing rewrite path: the list of terms from the theorem's lhs to its rhs, where consecutive entries differ by one rewrite.
TFindEquationalPath[proof$] re-walks a precomputed ProofObject's dataset into the same path (one path per Hypothesis for a multi-goal proof).
Returns $Failed when the goal is not proved or no chain is recorded.  The path is assembled lhs-chain-forward then rhs-chain-reversed through the shared normal form, matching the Wolfram Function Repository FindEquationalPath's default 'Path' property."];

GeneralUtilities`SetUsage[TStringPath, "TStringPath[thm$, axioms$] proves a string-rewriting theorem (TFindStringProof shapes) and returns the rewrite path decoded back to plain strings -- the list of words from the theorem's source word to its target word.
Returns $Failed when the goal is not proved.  Intermediate CenterDot re-bracketings introduced by the associativity bridge are collapsed by the decode, so consecutive entries may coincide; duplicates are deleted."];

(* Forward declarations: the Kernel loader's canonical string sort
   places this file BEFORE ATP.wl ("atp_strings" < "atp." once
   punctuation is canonically ranked), so the sibling's public
   symbols may not exist yet at parse time.  A bare mention here
   creates them in THVMLink`ATP` (the current context), and ATP.wl
   later attaches its definitions to the SAME symbols -- without
   this, the Private bodies below would mint shadow
   THVMLink`ATP`Private` copies and call an undefined function. *)
TFindProof;

Begin["`Private`"];

(* === Word encoding ================================================= *)

(* "ABC" -> A\[CenterDot](B\[CenterDot]C): one Global symbol per
   character, right-nested CenterDot -- the WFR FindStringProof
   encoding (convertStringToOperator).  Symbols are constructed via
   Symbol["Global`" <> ch] rather than ToExpression so single capital
   letters that name System symbols (C, D, E, I, N, O) stay inert
   word letters instead of evaluating.  Letters only; anything else
   throws the TATPError shape TFindProof surfaces as $Failed. *)
atpStringToWord[s_String] := Block[{chars = Characters[s]},
    If[ chars === {} || ! AllTrue[chars, StringMatchQ[#, LetterCharacter] &],
        Throw[Failure["TATPParseError",
            <|"Word" -> s, "Reason" -> "expected a nonempty word of letters"|>],
            "TATPError"]
    ];
    (* Quiet the shdw chatter a fresh Global`C / Global`D / ... emits
       when it shadows the System symbol of the same name. *)
    Fold[CenterDot[#2, #1] &,
        Reverse[Quiet[Symbol["Global`" <> #], {General::shdw}] & /@ chars]]
]

(* Inverse: flatten ANY CenterDot bracketing back to the plain string.
   Path intermediates re-bracket through the associativity bridge, so
   the decode must not assume the right-comb shape. *)
atpWordToString[w_] := Block[{leaves},
    leaves = Flatten[w //. CenterDot[x_, y_] :> {x, y}];
    If[ AllTrue[leaves, MatchQ[#, _Symbol] &],
        StringJoin[SymbolName /@ leaves],
        $Failed]
]

(* One string-rewriting item (axiom or theorem) -> the engine shape.
   A Rule of words stays a Rule (pre-oriented, one-directional); every
   other pair shape becomes an equation.  `Inactive[Equal]` output
   keeps a reflexive pair from collapsing to True. *)
atpStringItem[l_String -> r_String] := atpStringToWord[l] -> atpStringToWord[r];
atpStringItem[TwoWayRule[l_String, r_String]] :=
    Inactive[Equal][atpStringToWord[l], atpStringToWord[r]];
atpStringItem[l_String == r_String] :=
    Inactive[Equal][atpStringToWord[l], atpStringToWord[r]];
atpStringItem[{l_String, r_String}] :=
    Inactive[Equal][atpStringToWord[l], atpStringToWord[r]];
atpStringItem[bad_] := Throw[Failure["TATPParseError",
    <|"Item" -> bad,
      "Reason" -> "expected a word pair: \"l\" -> \"r\", \"l\" == \"r\", \"l\" <-> \"r\", or {\"l\", \"r\"}"|>],
    "TATPError"]

(* A theorem's direction is meaningless (the goal is an equation), so
   normalize Rule theorems to equations -- only AXIOMS keep Rule's
   pre-orientation. *)
atpStringGoal[l_String -> r_String] :=
    Inactive[Equal][atpStringToWord[l], atpStringToWord[r]];
atpStringGoal[item_] := atpStringItem[item]

(* The associativity bridge: a rewrite fires at any position of a word
   only because the word can re-bracket around it.  The bound
   variables must be GLOBAL symbols -- the proof lifter round-trips
   variable NAMES through the C engine and reconstructs them in
   Global`, so a Private-context variable never compares equal on the
   way back and the ProofObject collapses to $Failed.  Multi-character
   names cannot collide with single-letter word symbols. *)
$atpStringAssoc = With[{
    x = Symbol["Global`x$w"], y = Symbol["Global`y$w"],
    z = Symbol["Global`z$w"]},
    ForAll[{x, y, z},
        CenterDot[x, CenterDot[y, z]] == CenterDot[CenterDot[x, y], z]]];

(* === TFindStringProof ============================================== *)

(* A single-pair item at top level: {"l", "r"} means ONE pair (a bare
   word is never a theorem or axiom, so the 2-string list is
   unambiguous). *)
atpStringGoals[{l_String, r_String}] := {atpStringGoal[{l, r}]};
atpStringGoals[thms_List] := atpStringGoal /@ thms;
atpStringGoals[thm_] := {atpStringGoal[thm]}

atpStringAxioms[{l_String, r_String}] := {atpStringItem[{l, r}]};
atpStringAxioms[axs_List] := atpStringItem /@ axs;
atpStringAxioms[ax_] := {atpStringItem[ax]}

TFindStringProof[thms_, axioms_, opts:OptionsPattern[TFindProof]] :=
    TFindStringProof[thms, axioms, "ProofObject", opts];
TFindStringProof[thms_, axioms_,
        returnSpec_?atpReturnSpecQ, opts:OptionsPattern[TFindProof]] := Catch[
    Block[{goals, axs},
        goals = atpStringGoals[thms];
        axs = Append[atpStringAxioms[axioms], $atpStringAssoc];
        If[ Length[goals] === 1, goals = First[goals]];
        TFindProof[goals, axs, returnSpec, opts] /.
            EquationalLogic -> StringLogic
    ],
    "TATPError"
]

(* === TFindEquationalPath =========================================== *)

(* Prove-time form: the engine's goal chain IS the path ("Path"
   return spec, assembled by atpGoalPaths in ATP.wl). *)
TFindEquationalPath[thm_, axioms_, opts:OptionsPattern[TFindProof]] :=
    TFindProof[thm, axioms, "Path", opts];

(* ProofObject form: re-walk the dataset's per-goal linear chain
   {Hypothesis, g} -> SubstitutionLemma* -> {Conclusion, g}.  Each
   row's Statement is the running equation after its step; the lhs
   terms of side-1 steps advance forward and the rhs terms of side-2
   steps advance backward, meeting at the shared normal form --
   the same join shape atpGoalPaths assembles from the live chain. *)
atpDatasetGoalPath[rows_Association, g_Integer] := Block[{
    hypKey = {"Hypothesis", g}, concKey = {"Conclusion", g},
    byInput, eqOf, chain, cur, lPath, rPath, prevEq, stepEq
},
    If[ ! KeyExistsQ[rows, hypKey] || ! KeyExistsQ[rows, concKey],
        Return[$Failed]];
    (* Strip HoldForm / Inactive wrappers down to {lhs, rhs}. *)
    eqOf = Replace[#["Statement"], {
        HoldForm[Inactive[Equal][l_, r_]] :> {l, r},
        HoldForm[l_ == r_] :> {l, r},
        Inactive[Equal][l_, r_] :> {l, r},
        (l_ == r_) :> {l, r},
        _ :> $Failed}] &;
    (* The per-goal chain is linear: walk backward from the Conclusion
       via Proof.Input until the Hypothesis, then reverse. *)
    chain = Block[{key = concKey, acc = {}},
        While[ key =!= hypKey,
            If[ ! KeyExistsQ[rows, key], Return[$Failed]];
            AppendTo[acc, key];
            key = rows[key]["Proof"]["Input"];
            If[ Length[acc] > Length[rows], Return[$Failed]]
        ];
        Reverse[acc]];
    If[ chain === $Failed, Return[$Failed]];
    prevEq = eqOf[rows[hypKey]];
    If[ prevEq === $Failed, Return[$Failed]];
    lPath = {prevEq[[1]]};
    rPath = {prevEq[[2]]};
    Scan[
        Function[key, Block[{row = rows[key]},
            stepEq = eqOf[row];
            If[ stepEq === $Failed, Return[$Failed, Block]];
            (* A trivial Conclusion repeats the Hypothesis statement;
               anything else changed exactly one side. *)
            Which[
                stepEq[[1]] =!= prevEq[[1]], AppendTo[lPath, stepEq[[1]]],
                stepEq[[2]] =!= prevEq[[2]], AppendTo[rPath, stepEq[[2]]]
            ];
            prevEq = stepEq
        ]],
        chain];
    If[ Last[lPath] =!= Last[rPath] && Last[prevEq] =!= First[prevEq],
        (* The chain closed by rewriting one side INTO the other:
           the running equation's final form is reflexive.  Accept
           when the two side-paths meet; otherwise no join. *)
        If[ prevEq[[1]] =!= prevEq[[2]], Return[$Failed]]
    ];
    If[ Last[lPath] === Last[rPath],
        Join[lPath, Rest @ Reverse @ rPath],
        Join[lPath, Reverse @ rPath]]
]

TFindEquationalPath[po_ProofObject, opts:OptionsPattern[]] := Block[{
    rows, goals
},
    rows = Association @ Normal @ po["ProofDataset"];
    If[ ! AssociationQ[rows], Return[$Failed]];
    goals = Cases[Keys[rows], {"Hypothesis", g_Integer} :> g];
    If[ goals === {}, Return[$Failed]];
    Which[
        Length[goals] === 1, atpDatasetGoalPath[rows, First[goals]],
        True, atpDatasetGoalPath[rows, #] & /@ Sort[goals]
    ]
]

(* === TStringPath ==================================================== *)

TStringPath[thms_, axioms_, opts:OptionsPattern[TFindProof]] := Block[{
    path
},
    path = TFindStringProof[thms, axioms, "Path", opts];
    If[ path === $Failed, Return[$Failed]];
    If[ ListQ[path] && path =!= {} && ListQ[First[path]],
        atpSquashDups[atpWordToString /@ #] & /@ path,
        atpSquashDups[atpWordToString /@ path]]
]

atpSquashDups[l_List] := First /@ Split[l]

End[];

EndPackage[];
