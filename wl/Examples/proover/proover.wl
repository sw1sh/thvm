(* proover.wl -- a TSTP/FOF proof CHECKER for the ProoVer 2026 competition
   (https://proover-competition.github.io/).

   ProoVer hands a checker both correct and deliberately "buggy" first-order
   refutation proofs and asks for an SZS verdict per proof:

       Verified        the proof is valid          (+1, or -1 if wrong)
       FailedVerified  the proof has a bad step     (+2, or -10 if wrong)
       NotVerified     cannot decide                (0)

   The -10 penalty for calling a bad proof good makes the checker CONSERVATIVE:
   stamp Verified only when every step is independently confirmed; abstain
   (NotVerified) the moment a step cannot be settled.

   Backend is INTERNAL -- it reuses thvm's `Wolfram`Parser`TPTPImport` FOF parser
   and decides each step in the Wolfram kernel; no external prover is called.
   The check is a generalized ProofObject["ProofFunction"]: walk the steps and
   validate each, dispatching on the inference rule.

   A proof is a sequence of `fof(name, role, formula, source).` records.  Each
   derived step carries a rule + SZS status; the proof is a refutation that ends
   in `$false`.  Per-rule obligations:

     instantiate (thm)       conclusion is a substitution instance of the parent
     negated_conjecture(cth) conclusion is logically the negation of the parent
     skolemize (esa)         a FRESH Skolem symbol is introduced and actually
                             used in the conclusion
     consequence/horn/
       deduction (thm)       conclusion is entailed by the parents (bounded
                             Herbrand grounding + propositional tautology)

   The final step must be `$false`.
*)

BeginPackage["ProoVer`", {"Wolfram`Parser`"}]

ProoVerCheck::usage =
    "ProoVerCheck[proofFile] checks a TSTP/FOF refutation proof and returns an " <>
    "Association with \"SZS\" (\"Verified\" | \"FailedVerified\" | \"NotVerified\"), " <>
    "\"Steps\" (a per-step report), and \"Reason\".";

ProoVerParse::usage =
    "ProoVerParse[proofFile] parses a TSTP proof into a list of step Associations " <>
    "with keys Name, Role, Rule, Status, Parents, NewSymbols, FormulaString, Formula.";

ProoVerReport::usage =
    "ProoVerReport[proofFile] prints a human-readable per-step verdict table.";

Begin["`Private`"]

(* ---- top-level (bracket-depth-0) string split -------------------------- *)

topSplit[s_String, seps_List] := Module[
    {chars = Characters[s], depth = 0, cur = "", out = {}, c},
    Do[
        c = chars[[i]];
        Which[
            MemberQ[{"(", "["}, c], depth++; cur = cur <> c,
            MemberQ[{")", "]"}, c], depth--; cur = cur <> c,
            depth === 0 && MemberQ[seps, c], AppendTo[out, cur]; cur = "",
            True, cur = cur <> c
        ],
        {i, Length[chars]}
    ];
    AppendTo[out, cur];
    out
]

(* ---- formula string -> WL expression via the TPTPImport FOF parser ------
   The parser renders a top-level universal `![X]:phi` as a free pattern X_,
   `?[X]:phi` as Exists[{X_}, ...], pushes negations to NNF, and lowercases
   symbols (sK0 -> sk0, in_love -> inLove).  We wrap the formula as a one-axiom
   problem and read back the single parsed axiom. *)

$parseTmp = FileNameJoin[{$TemporaryDirectory, "proover_parse.p"}];

parseFormula[fstr_String] := Module[{r},
    Export[$parseTmp, "fof(t, axiom, " <> fstr <> ").", "Text"];
    r = Quiet @ Check[Wolfram`Parser`TPTPImport[$parseTmp], $Failed];
    If[ ! AssociationQ[r], Return[$Failed]];
    Which[
        r["Axioms"] =!= {}, First[r["Axioms"]],
        KeyExistsQ[r, "Conjecture"], r["Conjecture"],
        True, $Failed
    ]
]

(* ---- parse one proof file into structured steps ------------------------ *)

stripComments[text_String] := StringReplace[text, RegularExpression["(?m)%.*$"] -> ""]

(* pull rule / status / parents / new_symbols out of an `inference(...)` or
   `file(...)` source term *)
parseSource[src0_String] := Module[
    {src = StringTrim[src0], inner, args, rule, status, parents, newsyms},
    If[ StringStartsQ[src, "file"],
        Return[<|"Rule" -> "file", "Status" -> None, "Parents" -> {}, "NewSymbols" -> {}|>]
    ];
    If[ ! StringStartsQ[src, "inference"],
        Return[<|"Rule" -> "unknown", "Status" -> None, "Parents" -> {}, "NewSymbols" -> {}|>]
    ];
    inner = StringTrim @ StringReplace[src,
        {RegularExpression["^inference\\s*\\("] -> "", RegularExpression["\\)\\s*$"] -> ""}];
    args = StringTrim /@ topSplit[inner, {","}];
    rule = First[args, "unknown"];
    status = First[StringCases[src, RegularExpression["status\\(\\s*([a-z]+)\\s*\\)"] -> "$1"], None];
    (* parent list is the final top-level [...] group *)
    parents = Module[{lastBr = Last[StringCases[src, RegularExpression["\\[[^\\[\\]]*\\]"]], "[]"]},
        Select[StringTrim /@ topSplit[StringTake[lastBr, {2, -2}], {","}], # =!= "" &]
    ];
    newsyms = Flatten @ StringCases[src,
        RegularExpression["new_symbols\\(\\s*skolem\\s*,\\s*\\[([^\\]]*)\\]"] :>
            "$1"] /. s_String :> Select[StringTrim /@ StringSplit[s, ","], # =!= "" &];
    <|"Rule" -> rule, "Status" -> status, "Parents" -> parents, "NewSymbols" -> Flatten[{newsyms}]|>
]

ProoVerParse[file_String] := Module[
    {text, body, recs, steps},
    text = stripComments @ Import[file, "Text"];
    (* records are separated by top-level '.' *)
    recs = Select[StringTrim /@ topSplit[text, {"."}], StringMatchQ[#, RegularExpression["(?s)\\s*(fof|cnf)\\s*\\(.*"]] &];
    steps = Function[rec,
        Module[{inner, fields, name, role, fstr, src, sinfo},
            inner = StringTrim @ StringReplace[rec,
                {RegularExpression["^\\s*(fof|cnf)\\s*\\("] -> "", RegularExpression["\\)\\s*$"] -> ""}];
            fields = StringTrim /@ topSplit[inner, {","}];
            name = fields[[1]];
            role = fields[[2]];
            fstr = fields[[3]];
            src = If[Length[fields] >= 4, StringRiffle[fields[[4 ;;]], ","], ""];
            sinfo = parseSource[src];
            <|
                "Name" -> name,
                "Role" -> role,
                "Rule" -> sinfo["Rule"],
                "Status" -> sinfo["Status"],
                "Parents" -> sinfo["Parents"],
                "NewSymbols" -> sinfo["NewSymbols"],
                "FormulaString" -> fstr,
                "Formula" -> parseFormula[fstr]
            |>
        ]
    ] /@ recs;
    steps
]

(* ---- formula helpers --------------------------------------------------- *)

$logicalHeads = {And, Or, Not, Implies, Equivalent, Xor, Nand, Nor};

(* boolean atoms of an NNF/quantified formula: descend only through logical
   heads and (vacuously) through quantifiers; everything else is an atom *)
atomsOf[f_] := Switch[Head[f],
    And | Or, Flatten[atomsOf /@ (List @@ f)],
    Not, atomsOf[f[[1]]],
    Implies | Equivalent | Xor | Nand | Nor, Flatten[atomsOf /@ (List @@ f)],
    ForAll | Exists, atomsOf[f[[2]]],
    _, {f}
]

patternVars[f_] := DeleteDuplicates @ Cases[f, Verbatim[Pattern][s_, Blank[]] :> s, Infinity]

(* ground (variable-free) function/constant terms that occur as ARGUMENTS --
   the Herbrand universe for instantiating universal premises *)
termUniverse[exprs_] := Module[{args},
    args = Flatten @ Cases[{exprs},
        (_String[a___] | Equal[a___] | Unequal[a___]) :> {a}, Infinity];
    DeleteDuplicates @ Select[args, FreeQ[#, Verbatim[Pattern]] && ! AtomQ[#] &]
]

(* strip the Pattern wrapper, leaving a bare symbol, for grounding *)
bareVars[f_] := f /. Verbatim[Pattern][s_, Blank[]] :> s

(* all instances of a (possibly universally-quantified) premise over `univ` *)
instancesOf[p_, univ_] := Module[{pv = patternVars[p], bp = bareVars[p], vs},
    If[ pv === {}, Return[{bp}]];
    vs = pv;
    (bp /. Thread[vs -> #]) & /@ Tuples[univ, Length[vs]]
]

(* bounded-Herbrand propositional entailment: parents ?|= concl *)
entailsQ[parents_List, concl_] := Module[
    {univ, insts, conclG, whole, atoms, subs},
    If[ MemberQ[parents, $Failed] || concl === $Failed, Return[$Failed]];
    univ = termUniverse[{parents, concl}];
    If[ univ === {}, univ = {groundConst}];
    insts = Flatten[instancesOf[#, univ] & /@ parents];
    (* ground any free (universal) vars left in the conclusion to a universe term *)
    conclG = bareVars[concl] /. Thread[patternVars[concl] -> First[univ]];
    whole = Implies[And @@ insts, conclG];
    atoms = DeleteDuplicates @ atomsOf[whole];
    subs = Thread[atoms -> Table[Unique["b"], {Length[atoms]}]];
    TrueQ @ TautologyQ[whole /. subs]
]

(* canonicalize bound/free variable names by first-appearance order so two
   formulas equal up to renaming compare equal *)
canon[f_] := Module[{vs = patternVars[f]},
    f /. Thread[vs -> (Symbol["q" <> ToString[#]] & /@ Range[Length[vs]])] /.
        Verbatim[Pattern][s_, Blank[]] :> s
]

(* ---- per-step checks --------------------------------------------------- *)

(* lookup parsed formula of a named earlier step *)
lookupFormula[steps_, name_] := SelectFirst[steps, #["Name"] === name &, <||>]["Formula"]
lookupFstr[steps_, name_] := SelectFirst[steps, #["Name"] === name &, <||>]["FormulaString"]

checkStep[steps_, idx_] := Module[
    {st = steps[[idx]], rule, concl, parents, pforms, res},
    rule = st["Rule"];
    concl = st["Formula"];
    parents = st["Parents"];
    pforms = lookupFormula[steps, #] & /@ parents;
    If[ concl === $Failed, Return[<|"Verdict" -> "unknown", "Why" -> "formula did not parse"|>]];
    Switch[rule,

        "file",
            <|"Verdict" -> "valid", "Why" -> "input formula"|>,

        "instantiate",
            (* conclusion must be a substitution instance of (some) parent *)
            res = AnyTrue[pforms, # =!= $Failed && MatchQ[concl, #] &];
            If[ res,
                <|"Verdict" -> "valid", "Why" -> "instance of parent"|>,
                <|"Verdict" -> "unknown", "Why" -> "not a syntactic instance of any parent"|>
            ],

        "negated_conjecture",
            (* conclusion must be the logical negation of the (single) parent *)
            Module[{pstr = lookupFstr[steps, First[parents, ""]], properNeg},
                properNeg = parseFormula["~(" <> pstr <> ")"];
                If[ properNeg === $Failed,
                    <|"Verdict" -> "unknown", "Why" -> "could not parse parent negation"|>,
                    If[ canon[concl] === canon[properNeg],
                        <|"Verdict" -> "valid", "Why" -> "negation of parent"|>,
                        <|"Verdict" -> "invalid",
                          "Why" -> "not the negation of the conjecture (quantifier/connective mismatch)"|>
                    ]
                ]
            ],

        "skolemize",
            (* a FRESH Skolem symbol must be introduced and used in the result *)
            Module[{newsyms = st["NewSymbols"], prior, fresh, used},
                prior = StringJoin[#["FormulaString"] <> " " <> StringRiffle[#["NewSymbols"], " "] & /@ steps[[;; idx - 1]]];
                If[ newsyms === {},
                    Return[<|"Verdict" -> "invalid", "Why" -> "skolemize declared no new symbol"|>]
                ];
                fresh = AllTrue[newsyms, ! StringContainsQ[prior, #] &];
                used = AllTrue[newsyms, StringContainsQ[st["FormulaString"], #] &];
                Which[
                    ! fresh, <|"Verdict" -> "invalid", "Why" -> "Skolem symbol is not fresh (reused)"|>,
                    ! used, <|"Verdict" -> "invalid", "Why" -> "declared Skolem symbol absent from the result formula"|>,
                    True, <|"Verdict" -> "valid", "Why" -> "introduces a fresh Skolem symbol"|>
                ]
            ],

        "consequence" | "horn" | "deduction" | "resolution" | "fol",
            res = entailsQ[pforms, concl];
            Which[
                res === True, <|"Verdict" -> "valid", "Why" -> "entailed by parents"|>,
                res === $Failed, <|"Verdict" -> "unknown", "Why" -> "a parent did not parse"|>,
                True, <|"Verdict" -> "invalid", "Why" -> "conclusion is NOT entailed by the parents"|>
            ],

        _,
            <|"Verdict" -> "unknown", "Why" -> "unhandled rule: " <> ToString[rule]|>
    ]
]

(* ---- top-level verdict ------------------------------------------------- *)

ProoVerCheck[file_String] := Module[
    {steps, reports, verdicts, lastConcl, finalIsFalse, szs, reason},
    steps = ProoVerParse[file];
    If[ steps === {}, Return[<|"SZS" -> "NotVerified", "Reason" -> "no steps parsed", "Steps" -> {}|>]];
    reports = Table[
        Module[{r = checkStep[steps, i]},
            Append[KeyTake[steps[[i]], {"Name", "Role", "Rule"}], r]
        ], {i, Length[steps]}];
    verdicts = #["Verdict"] & /@ reports;
    lastConcl = Last[steps]["Formula"];
    finalIsFalse = lastConcl === False;
    Which[
        MemberQ[verdicts, "invalid"],
            szs = "FailedVerified";
            reason = "bad step: " <> StringRiffle[
                (#["Name"] <> " (" <> #["Why"] <> ")") & /@ Select[reports, #["Verdict"] === "invalid" &], "; "],

        ! finalIsFalse,
            szs = "NotVerified"; reason = "proof does not end in $false",

        MemberQ[verdicts, "unknown"],
            szs = "NotVerified";
            reason = "undecided step(s): " <> StringRiffle[
                #["Name"] & /@ Select[reports, #["Verdict"] === "unknown" &], ", "],

        True,
            szs = "Verified"; reason = "every step checks and the proof closes with $false"
    ];
    <|"SZS" -> szs, "Reason" -> reason, "Steps" -> reports|>
]

ProoVerReport[file_String] := Module[{res = ProoVerCheck[file]},
    Print["FILE: ", FileNameTake[file]];
    Print[StringPadRight["  step", 16], StringPadRight["role", 20], StringPadRight["rule", 20], "verdict"];
    Function[s,
        Print[
            StringPadRight["  " <> s["Name"], 16],
            StringPadRight[s["Role"], 20],
            StringPadRight[ToString[s["Rule"]], 20],
            s["Verdict"], If[s["Verdict"] =!= "valid", "  <- " <> s["Why"], ""]]
    ] /@ res["Steps"];
    Print["  => SZS: ", res["SZS"], "   (", res["Reason"], ")"];
    res
]

End[]
EndPackage[]
