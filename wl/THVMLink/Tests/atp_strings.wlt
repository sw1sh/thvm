(* atp_strings.wlt -- VerificationTest specs for the string-rewriting
   surface: TFindStringProof (WFR FindStringProof port: words as
   CenterDot terms + the associativity bridge axiom, with axioms
   written as Rules installed PRE-ORIENTED for one-directional
   string rewriting) and TFindEquationalPath / TStringPath (a proved
   goal's witnessing rewrite path).
*)

VerificationTest[
    TInit[],
    True,
    TestID -> "ATPStrings/init"
]

(* === Word encoding ================================================= *)

VerificationTest[
    (* "ABC" encodes to the right-nested CenterDot word A.(B.C); the
       single-character word is the bare letter symbol.  Letter symbols
       are constructed in Global` so System single-letter symbols
       (C, D, E, I, N, O) stay inert word letters. *)
    {THVMLink`ATP`Private`atpStringToWord["ABC"],
     THVMLink`ATP`Private`atpStringToWord["A"]},
    {CenterDot[Global`A, CenterDot[Global`B, Symbol["Global`C"]]],
     Global`A},
    TestID -> "ATPStrings/encode/right-nested-word"
]

VerificationTest[
    (* The decode flattens ANY bracketing back to the plain string --
       path intermediates re-bracket through the associativity
       bridge. *)
    THVMLink`ATP`Private`atpWordToString[
        CenterDot[CenterDot[Global`A, Global`B], Symbol["Global`C"]]],
    "ABC",
    TestID -> "ATPStrings/encode/decode-any-bracketing"
]

(* === Equational baseline =========================================== *)

VerificationTest[
    (* A pair axiom is a bidirectional equation -- the WFR
       FindStringProof behavior.  {"AB", "BA"} as the theorem is the
       unambiguous single-pair shape. *)
    Head @ TFindStringProof[{"AB", "BA"}, {{"AB", "BA"}},
        TimeConstraint -> 10],
    ProofObject,
    TestID -> "ATPStrings/equational/pair-axiom-proves"
]

(* === Directional (pre-oriented Rule) axioms ======================== *)

VerificationTest[
    (* An axiom written "BA" -> "AB" is installed as a PRE-ORIENTED
       rewrite rule -- one-directional string rewriting.  The
       bubble-sort system proves BBAA ->* AABB, and the ProofObject
       VERIFIES (the engine stamps a proper trace lineage for
       pre-oriented rules, so the lift survives). *)
    Module[{po},
        po = TFindStringProof["BBAA" -> "AABB", {"BA" -> "AB"},
            TimeConstraint -> 30];
        {Head[po],
         If[ Head[po] === ProofObject,
             po["ProofFunction"][] // Head, $Failed]}
    ],
    {ProofObject, Success},
    TestID -> "ATPStrings/directional/bubble-sort-proves-and-verifies"
]

VerificationTest[
    (* Multi-goal: a list of string theorems is ONE ProofObject with a
       Hypothesis row per conjunct (the TFindProof multi-goal
       contract). *)
    Module[{po, keys},
        po = TFindStringProof[{"BA" -> "AB", "BBA" -> "ABB"},
            {"BA" -> "AB"}, TimeConstraint -> 30];
        keys = Keys @ Normal @ po["ProofDataset"];
        {Head[po],
         ContainsAll[keys, {{"Hypothesis", 1}, {"Hypothesis", 2},
            {"Conclusion", 1}, {"Conclusion", 2}}]}
    ],
    {ProofObject, True},
    TestID -> "ATPStrings/directional/multi-goal-one-object"
]

(* === The witnessing rewrite path =================================== *)

(* Forward reachability under the one-directional axiom: t is
   reachable from s by at most k one-step "BA" -> "AB" string
   replacements.  A path step produced by a DERIVED completion rule is
   a composition of the directional axiom, so it is forward-reachable
   in a few axiom applications -- and never backward. *)
fwdReach[s_, t_, k_] := Block[{frontier = {s}, seen = {s}, found = False},
    Do[
        frontier = Complement[DeleteDuplicates @ Flatten[
            StringReplaceList[#, "BA" -> "AB"] & /@ frontier], seen];
        seen = Join[seen, frontier];
        If[ MemberQ[frontier, t], found = True; Break[]],
        {k}];
    found]

VerificationTest[
    (* The nontrivial directional path: BBBAAA ->* AAABBB from the
       single one-directional rule.  Asserts the endpoints, a
       nontrivial length, per-step FORWARD reachability, and that the
       reverse direction is NOT reachable (the orientation is real). *)
    Module[{path, stepsOk},
        path = TStringPath["BBBAAA" -> "AAABBB", {"BA" -> "AB"},
            TimeConstraint -> 60];
        stepsOk = ListQ[path] &&
            AllTrue[Range[Length[path] - 1],
                fwdReach[path[[#]], path[[# + 1]], 5] &];
        {First[path], Last[path], Length[path] >= 4, stepsOk,
         fwdReach["AAABBB", "BBBAAA", 9]}
    ],
    {"BBBAAA", "AAABBB", True, True, False},
    TestID -> "ATPStrings/path/nontrivial-directional-sort"
]

VerificationTest[
    (* TFindEquationalPath on a PRECOMPUTED ProofObject re-walks the
       dataset into the same join shape: endpoints are the theorem's
       sides and consecutive entries are single rewrites by rules of
       the completed system. *)
    Module[{po, path, words},
        po = TFindStringProof["BBAA" -> "AABB", {"BA" -> "AB"},
            TimeConstraint -> 30];
        path = TFindEquationalPath[po];
        words = THVMLink`ATP`Private`atpWordToString /@ path;
        {ListQ[path],
         First[words], Last[words],
         AllTrue[Range[Length[words] - 1],
            fwdReach[words[[#]], words[[# + 1]], 5] &]}
    ],
    {True, "BBAA", "AABB", True},
    TestID -> "ATPStrings/path/from-precomputed-proofobject"
]

VerificationTest[
    (* The generic TFindEquationalPath[thm, axioms] form on a plain
       (non-string) ground chain. *)
    TFindEquationalPath[Global`a == Global`c,
        {Global`a -> Global`b, Global`b -> Global`c},
        TimeConstraint -> 10],
    {Global`a, Global`b, Global`c},
    TestID -> "ATPStrings/path/generic-ground-chain"
]

VerificationTest[
    (* A goal is an EQUATION, so "AB" -> "BA" still proves -- both
       sides JOIN at the normal form AB (the rhs chain runs backward
       through the join).  The path makes the traversal visible:
       lhs AB is already normal, rhs BA rewrites forward to AB, so the
       lhs-to-rhs path is {AB, BA} with the second step a reversed
       rewrite.  Directionality lives in the AXIOM (the engine never
       rewrites AB back to BA during search), not in goal
       provability. *)
    TStringPath["AB" -> "BA", {"BA" -> "AB"}, TimeConstraint -> 10],
    {"AB", "BA"},
    TestID -> "ATPStrings/path/goal-joins-through-reversed-side"
]

VerificationTest[
    (* Genuinely unjoinable words -> $Failed: AB and BB have distinct
       normal forms under BA -> AB, and saturation of the single
       ground rule terminates immediately. *)
    TStringPath["AB" -> "BB", {"BA" -> "AB"}, TimeConstraint -> 10],
    $Failed,
    TestID -> "ATPStrings/path/unjoinable-is-failed"
]
