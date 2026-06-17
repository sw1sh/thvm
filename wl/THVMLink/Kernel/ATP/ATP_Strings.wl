(* ::Package:: *)
(* ATP_Strings.wl: string rewriting over the equational engine.
   TFindStringProof (the WFR FindStringProof surface: words as
   CenterDot terms + the associativity bridge axiom) and
   TFindEquationalPath (the WFR FindEquationalPath surface: a proved
   goal's witnessing rewrite path, from the engine's goal chain or
   re-walked off a ProofObject's dataset).

   Sibling of ATP.wl in the WolframInstitute`THVMLink`ATP` context, sharing the
   WolframInstitute`THVMLink`ATP`Private` context, so it references ATP.wl's
   dispatches and helpers by bare name. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"WolframInstitute`THVMLink`"}];

GeneralUtilities`SetUsage[TFindStringProof, "TFindStringProof[thm$, axioms$] proves a string-rewriting theorem over the multiway/semi-Thue axioms and returns the ProofObject.
Words are encoded as right-nested CenterDot terms over one symbol per character ('ABC' encodes to A\[CenterDot](B\[CenterDot]C)), with the associativity bridge axiom appended so a rewrite applies at any position of a word; the encoding follows the Wolfram Function Repository FindStringProof.
An axiom written 'BA' -> 'AB' is installed as a PRE-ORIENTED rewrite rule (one-directional string rewriting); an axiom written {'BA', 'AB'}, 'BA' == 'AB', or 'BA' <-> 'AB' is an equation the engine orients itself.
A theorem is a pair in any of the same shapes (direction is meaningless for a goal); a list of theorems is a multi-goal conjunction returning ONE ProofObject with a Hypothesis/Conclusion row pair per conjunct.
An optional last argument picks the TFindProof return spec ('Status', 'Path', $$); options are TFindProof's (MaxSteps, TimeConstraint, Method, PortfolioFrontLoad)."];

(* TFindEquationalPath (the full replacement-path surface ported from the
   Wolfram Function Repository) lives in ATP_EquationalPath.wl.  Forward-
   declare it so TStringPath's fallback and the shared Private re-walk
   below resolve to the same public symbol regardless of load order. *)
TFindEquationalPath;

GeneralUtilities`SetUsage[TStringPath, "TStringPath[thm$, axioms$] proves a string-rewriting theorem (TFindStringProof shapes) and returns the rewrite path decoded back to plain strings: the list of words from the theorem's source word to its target word.
Returns $Failed when the goal is not proved.  Intermediate CenterDot re-bracketings introduced by the associativity bridge decode to the same word; adjacent duplicates are deleted."];

(* Forward declarations: the Kernel loader's canonical string sort
   places this file BEFORE ATP.wl ("atp_strings" sorts ahead of
   "atp." once punctuation is canonically ranked), so the sibling's
   public symbols may not exist yet at parse time.  A bare mention
   here creates them in WolframInstitute`THVMLink`ATP` (the current context), and
   ATP.wl later attaches its definitions to the SAME symbols.
   Without this, the Private bodies below would mint shadow
   WolframInstitute`THVMLink`ATP`Private` copies and call an undefined function. *)
TFindProof;

Begin["`Private`"];

(* === Word encoding ================================================= *)

(* "ABC" -> A\[CenterDot](B\[CenterDot]C): one Global symbol per
   character, right-nested CenterDot, the WFR FindStringProof
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
    (* Quiet the shdw chatter a fresh Global`C / Global`D / etc emits
       when it shadows the System symbol of the same name. *)
    Fold[CenterDot[#2, #1] &,
        Reverse[Quiet[Symbol["Global`" <> #], {General::shdw}] & /@ chars]]
]

(* Inverse: flatten ANY CenterDot bracketing back to the plain string.
   Path intermediates re-bracket through the associativity bridge, so
   the decode must not assume the right-comb shape. *)
atpWordToString[w_] := Block[{leaves = Flatten[w //. CenterDot[x_, y_] :> {x, y}]},
    If[ AllTrue[leaves, MatchQ[#, _Symbol] &],
        StringJoin[SymbolName /@ leaves],
        $Failed]
]

(* One string-rewriting item (axiom or theorem) in engine shape.  A
   Rule of words stays a Rule (pre-oriented, one-directional); every
   other pair shape becomes an equation.  Inactive[Equal] output
   keeps a reflexive pair from collapsing to True. *)
atpStringItem[l_String -> r_String] := atpStringToWord[l] -> atpStringToWord[r]
atpStringItem[TwoWayRule[l_String, r_String]] :=
    Inactive[Equal][atpStringToWord[l], atpStringToWord[r]]
atpStringItem[l_String == r_String] :=
    Inactive[Equal][atpStringToWord[l], atpStringToWord[r]]
atpStringItem[{l_String, r_String}] :=
    Inactive[Equal][atpStringToWord[l], atpStringToWord[r]]
atpStringItem[bad_] := Throw[Failure["TATPParseError",
    <|"Item" -> bad,
      "Reason" -> "expected a word pair: \"l\" -> \"r\", \"l\" == \"r\", \"l\" <-> \"r\", or {\"l\", \"r\"}"|>],
    "TATPError"]

(* A theorem's direction is meaningless (the goal is an equation), so
   normalize Rule theorems to equations; only AXIOMS keep Rule's
   pre-orientation. *)
atpStringGoal[l_String -> r_String] :=
    Inactive[Equal][atpStringToWord[l], atpStringToWord[r]]
atpStringGoal[item_] := atpStringItem[item]

(* The associativity bridge: a rewrite fires at any position of a word
   only because the word can re-bracket around it.  The bound
   variables must be GLOBAL symbols: the proof lifter round-trips
   variable NAMES through the C engine and reconstructs them in
   Global`, so a Private-context variable never compares equal on the
   way back and the ProofObject collapses to $Failed.  Multi-character
   names cannot collide with single-letter word symbols. *)
$atpStringAssoc = With[{
    x = Symbol["Global`x$w"], y = Symbol["Global`y$w"],
    z = Symbol["Global`z$w"]},
    ForAll[{x, y, z},
        CenterDot[x, CenterDot[y, z]] == CenterDot[CenterDot[x, y], z]]]

(* === TFindStringProof ============================================== *)

(* A single-pair item at top level: {"l", "r"} means ONE pair (a bare
   word is never a theorem or axiom, so the 2-string list is
   unambiguous). *)
atpStringGoals[{l_String, r_String}] := {atpStringGoal[{l, r}]}
atpStringGoals[thms_List] := atpStringGoal /@ thms
atpStringGoals[thm_] := {atpStringGoal[thm]}

atpStringAxioms[{l_String, r_String}] := {atpStringItem[{l, r}]}
atpStringAxioms[axs_List] := atpStringItem /@ axs
atpStringAxioms[ax_] := {atpStringItem[ax]}

TFindStringProof[thms_, axioms_, opts:OptionsPattern[TFindProof]] :=
    TFindStringProof[thms, axioms, "ProofObject", opts]
TFindStringProof[thms_, axioms_,
        returnSpec_?atpReturnSpecQ, opts:OptionsPattern[TFindProof]] := Catch[
    Block[{
        goals = Replace[atpStringGoals[thms], {g_} :> g],
        axs = Append[atpStringAxioms[axioms], $atpStringAssoc],
        (* Word letters and the bridge variables are forced into Global`
           (atpStringToWord / $atpStringAssoc).  decodeAtpTerm now restores
           the held originals, so the decoded statements / path are correct
           in any caller context -- but the built-in ProofObject verifier
           ([ProofFunction]) the goal-directed lift gates on is still
           context-fragile on a non-Global ambient context, so pin the proof
           to Global` until the native TProofObject verifier replaces it. *)
        $Context = "Global`",
        $ContextPath = Prepend[DeleteCases[$ContextPath, "Global`"], "Global`"]
    },
        TFindProof[goals, axs, returnSpec, opts] /.
            EquationalLogic -> StringLogic
    ],
    "TATPError"
]

(* === Dataset path re-walk (shared with ATP_EquationalPath.wl) ====== *)

(* Strip the HoldForm / Inactive wrappers of a dataset row's
   Statement down to the bare {lhs, rhs}. *)
atpRowEq[row_] := Replace[row["Statement"], {
    HoldForm[Inactive[Equal][l_, r_]] :> {l, r},
    HoldForm[l_ == r_] :> {l, r},
    Inactive[Equal][l_, r_] :> {l, r},
    (l_ == r_) :> {l, r},
    _ :> $Failed}]

(* Re-walk the dataset's per-goal linear chain
   {Hypothesis, g} -> SubstitutionLemma* -> {Conclusion, g}.  Each
   row's Statement is the running equation after its step; collapsing
   adjacent duplicates in the lhs column gives the lhs-side rewrite
   sequence (ditto rhs), and the two sequences meet at the shared
   normal form: the same join shape atpGoalPaths assembles from the
   live chain.  This is the robust fallback the ported FindEquationalPath
   walker (ATP_EquationalPath.wl) leans on for proofs whose steps its
   upstream unifier cannot reconstruct -- notably string-rewriting
   CriticalPairLemmas. *)
atpDatasetGoalPath[rows_Association, g_Integer] := Block[{
    hypKey = {"Hypothesis", g}, concKey = {"Conclusion", g},
    walk, eqs, lSeq, rSeq
},
    If[ ! KeyExistsQ[rows, hypKey] || ! KeyExistsQ[rows, concKey],
        Return[$Failed]];
    (* The per-goal chain is linear: walk backward from the Conclusion
       via Proof.Input until the Hypothesis (bounded by the row count,
       so a malformed cyclic Input chain cannot loop). *)
    walk = NestWhileList[rows[#]["Proof"]["Input"] &, concKey,
        # =!= hypKey && KeyExistsQ[rows, #] &, 1, Length[rows] + 1];
    If[ Last[walk] =!= hypKey, Return[$Failed]];
    eqs = atpRowEq[rows[#]] & /@ Reverse[walk];
    If[ MemberQ[eqs, $Failed], Return[$Failed]];
    lSeq = First /@ Split[eqs[[All, 1]]];
    rSeq = First /@ Split[eqs[[All, 2]]];
    If[ Last[lSeq] === Last[rSeq],
        Join[lSeq, Rest @ Reverse @ rSeq],
        $Failed]
]

(* The bare path off a ProofObject: one path per Hypothesis, the single
   path when there is one goal.  $Failed when no chain reconstructs. *)
atpDatasetEqPath[po_ProofObject] := Block[{
    rows = Association @ Normal @ po["ProofDataset"], goals
},
    If[ ! AssociationQ[rows], Return[$Failed]];
    goals = Sort @ Cases[Keys[rows], {"Hypothesis", g_Integer} :> g];
    Replace[atpDatasetGoalPath[rows, #] & /@ goals, {
        {} -> $Failed,
        {single_} :> single
    }]
]

(* === TStringPath ==================================================== *)

atpSquashDups[l_List] := First /@ Split[l]

TStringPath[thms_, axioms_, opts:OptionsPattern[TFindProof]] := Block[{
    path = TFindStringProof[thms, axioms, "Path", opts]
},
    (* The prove-time "Path" reads the engine's live goal chain, which is
       not always recorded for a multi-goal conjunction.  Fall back to
       re-walking a ProofObject's proof dataset, which reconstructs the
       same per-goal chains -- the complete route at the cost of building
       the ProofObject. *)
    If[ path === $Failed,
        With[{po = TFindStringProof[thms, axioms, "ProofObject", opts]},
            If[ MatchQ[po, _ProofObject], path = atpDatasetEqPath[po]]]
    ];
    Which[
        path === $Failed, $Failed,
        ListQ[path] && path =!= {} && ListQ[First[path]],
            atpSquashDups[atpWordToString /@ #] & /@ path,
        True, atpSquashDups[atpWordToString /@ path]
    ]
]

End[];

EndPackage[];
