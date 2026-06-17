(* atp_equational_path.wlt -- VerificationTest specs for TFindEquationalPath,
   the replacement-path surface ported from the Wolfram Function Repository
   FindEquationalPath (ATP_EquationalPath.wl).  All eight properties are
   exercised on a one-sided re-association proof: the path that fully
   right-associates ((a.b).c).d into a.(b.(c.d)) by the directed rule
   (x.y).z -> x.(y.z).  CircleTimes is the inert non-Orderless operator. *)

VerificationTest[
    TInit[],
    True,
    TestID -> "ATPEqPath/init"
]

(* === The default 'Path' property =================================== *)

VerificationTest[
    TFindEquationalPath[
        ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
        {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
        TimeConstraint -> 20],
    {((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d,
     (a \[CircleTimes] b) \[CircleTimes] (c \[CircleTimes] d),
     a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))},
    TestID -> "ATPEqPath/path/reassociation"
]

(* === All eight properties ========================================== *)

VerificationTest[
    (* `All` returns the full Association FindEquationalPath carries. *)
    Sort @ Keys @ TFindEquationalPath[
        ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
        {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
        All, TimeConstraint -> 20],
    Sort @ {"ProofObject", "RewriteTest", "Justification", "Rewrites",
            "Rules", "Substitutions", "Bindings", "Path"},
    TestID -> "ATPEqPath/all/keys"
]

VerificationTest[
    (* The RewriteTest re-runs the rewrite sequence and confirms it
       reproduces the path. *)
    Head @ TFindEquationalPath[
        ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
        {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
        "RewriteTest", TimeConstraint -> 20],
    Success,
    TestID -> "ATPEqPath/rewritetest/success"
]

VerificationTest[
    (* One Rule, Rewrite, and Substitution per path step (two here);
       each rewrite, applied to its source term, yields the next. *)
    Block[{path, rules, rewrites},
        {path, rules, rewrites} = Values @ TFindEquationalPath[
            ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
            {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
            {"Path", "Rules", "Rewrites"}, TimeConstraint -> 20];
        {Length[rules], Length[rewrites], Length[path] - 1,
         MapThread[#1[#2] &, {rewrites, Most[path]}] === Rest[path]}
    ],
    {2, 2, 2, True},
    TestID -> "ATPEqPath/steps/rewrites-reproduce-path"
]

VerificationTest[
    (* Justification is a {lemma, orientation, position} triple per step:
       two top-level ({}) applications of a numbered lemma here. *)
    MatchQ[
        TFindEquationalPath[
            ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
            {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
            "Justification", TimeConstraint -> 20],
        {Repeated[{{_String, 1}, Left | Right, {}}, {2}]}],
    True,
    TestID -> "ATPEqPath/justification/triples"
]

(* === A list of properties returns a sub-Association ================ *)

VerificationTest[
    Keys @ TFindEquationalPath[
        ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
        {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
        {"Substitutions", "Bindings"}, TimeConstraint -> 20],
    {"Substitutions", "Bindings"},
    TestID -> "ATPEqPath/list/sub-association"
]

(* === The ProofObject form ========================================== *)

VerificationTest[
    (* Off a precomputed ProofObject, the path is the same. *)
    Block[{po = TFindProof[
        ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
        {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
        TimeConstraint -> 20]},
        TFindEquationalPath[po]
    ],
    {((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d,
     (a \[CircleTimes] b) \[CircleTimes] (c \[CircleTimes] d),
     a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))},
    TestID -> "ATPEqPath/proofobject/path"
]

(* === An unproved goal is $Failed ==================================== *)

VerificationTest[
    TFindEquationalPath[
        ForAll[{a, b}, a \[CircleTimes] b == b \[CircleTimes] a],
        {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
        TimeConstraint -> 5],
    $Failed,
    TestID -> "ATPEqPath/unproved/failed"
]
