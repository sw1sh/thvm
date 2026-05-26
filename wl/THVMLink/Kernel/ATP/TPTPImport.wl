(* TPTPImport.wl - parse TPTP UEQ-fragment .p files into the
   <|"Axioms" -> {l == r, ...}, "Conjecture" -> l == r|> shape
   TFindProof consumes.

   Subset handled (sufficient for every problem in TPTP's UEQ
   division):
     cnf(name, role, formula).  with one equational literal,
     where role is axiom | hypothesis | lemma | conjecture |
     negated_conjecture and formula is `lhs = rhs` or `lhs != rhs`.

   The full TPTP grammar (~735 lines, github.com/TPTPWorld/SyntaxBNF)
   covers cnf / fof / tff / thf / tcf / ncf / tpi.  fof / tff / thf /
   include directives are skipped with a console message; the cnf
   fragment is what every equational benchmark uses.

   Variables are clause-scoped: each Uppercase identifier inside one
   cnf clause names the same universal variable; the same name in a
   later clause is independent.  We build a fresh Pattern[Unique[]]
   per occurrence so subsequent equations share no bound variables. *)

BeginPackage["THVMLink`TPTPImport`"];

tptpImport::usage =
    "tptpImport[File[\"file.p\"]] | tptpImport[\"... cnf source ...\"] " <>
    "returns <|\"Axioms\" -> {...}, \"Conjecture\" -> ...|>.";

tptpImport::badrole = "Skipping cnf clause with unsupported role `1`.";
tptpImport::badfmla = "Could not parse formula `1` (expected " <>
    "`lhs = rhs` or `lhs != rhs`).";
tptpImport::skipnoncnf =
    "Skipping unsupported `1` directive at offset `2` (only cnf clauses " <>
    "are handled; see github.com/TPTPWorld/SyntaxBNF for the full grammar).";

Begin["`Private`"];

(* ----- entry ----- *)

tptpImport[File[path_String]] := tptpImport @ Import[path, "Text"]

tptpImport[s_String] /; FileExistsQ[s] && ! StringContainsQ[s, "cnf("] :=
    tptpImport @ File[s]

tptpImport[text_String] := Block[{stripped, clauses, results},
    stripped = StringReplace[text, {
        RegularExpression["%[^\n]*"]                 -> "",
        RegularExpression["/\\*([^*]|\\*[^/])*\\*+/"] -> ""
    }];
    clauses = parseClauses[stripped];
    results = Map[clauseToEquation, clauses];
    <|
        "Axioms"     -> Cases[results, {"axiom", eq_} :> eq],
        "Conjecture" -> FirstCase[results, {"conjecture", eq_} :> eq, None]
    |>
]

(* ----- clause splitter -----
   Scan once over the stripped text.  `cnf(` triggers a parser call,
   `fof(`/`tff(`/`thf(`/`include(` skip to the matching `).` (warned),
   anything else is whitespace or noise.

   Returns a list of {role, body} -- one per cnf clause.  Reap/Sow
   builds the list once at the end (no AppendTo). *)

(* Reap returns {value, {{sown...}}} -- we want the sown list, with a
   {} fallback when nothing was Sown. *)
parseClauses[text_String] :=
    Replace[Reap[scanClauses[text, 1]][[2]], {{xs_List} :> xs, _ -> {}}]

scanClauses[text_String, i0_Integer] := Block[{i = i0, len, head},
    len = StringLength[text];
    While[ i <= len,
        Which[
            i + 3 <= len && StringTake[text, {i, i + 3}] === "cnf(",
                i = consumeCnf[text, i + 4]
            ,
            i + 3 <= len && MemberQ[
                {"fof(", "tff(", "thf("}, StringTake[text, {i, i + 3}]],
                head = StringTake[text, {i, i + 2}];
                Message[tptpImport::skipnoncnf, head, i];
                i = skipParenthesised[text, i + 4]
            ,
            i + 7 <= len && StringTake[text, {i, i + 7}] === "include(",
                Message[tptpImport::skipnoncnf, "include", i];
                i = skipParenthesised[text, i + 8]
            ,
            True, i = i + 1
        ]
    ]
]

consumeCnf[text_String, i0_Integer] :=
    Block[{i = i0, dummy, role, body, bodyEnd},
        {dummy, i} = readWord[text, i];   (* clause name; discard *)
        i = skipPast[text, i, ","];
        {role, i} = readWord[text, i];
        i = skipPast[text, i, ","];
        {body, bodyEnd} = readBalanced[text, i];
        Sow[{role, StringTrim[body]}];
        skipPast[text, bodyEnd, "."]
    ]

(* ----- string-position helpers ----- *)

readWord[text_String, i0_Integer] := Block[{i = i0, len, start},
    len = StringLength[text];
    While[ i <= len && StringMatchQ[StringTake[text, {i, i}], Whitespace],
        i = i + 1
    ];
    start = i;
    While[ i <= len &&
        StringMatchQ[StringTake[text, {i, i}],
            RegularExpression["[A-Za-z0-9_$]"]],
        i = i + 1
    ];
    {StringTake[text, {start, i - 1}], i}
]

skipPast[text_String, i0_Integer, ch_String] := Block[{i = i0, len},
    len = StringLength[text];
    While[ i <= len && StringTake[text, {i, i}] =!= ch,
        i = i + 1
    ];
    i + 1
]

(* Read up to the matching closing ')' from a position already INSIDE
   the parentheses.  Returns {body-string, position-after-')'}. *)
readBalanced[text_String, i0_Integer] := Block[{i = i0, len, depth = 1, ch},
    len = StringLength[text];
    While[ i <= len && depth > 0,
        ch = StringTake[text, {i, i}];
        Which[
            ch === "(", depth = depth + 1; i = i + 1,
            ch === ")", depth = depth - 1;
                If[ depth > 0, i = i + 1],
            True, i = i + 1
        ]
    ];
    {StringTake[text, {i0, i - 1}], i + 1}
]

skipParenthesised[text_String, i0_Integer] :=
    skipPast[text, readBalanced[text, i0][[2]], "."]

(* ----- clause body parsing -----
   parseFormula reads `lhs = rhs` or `lhs != rhs`, returning a WL
   Equal[..] or Unequal[..] term whose variables are fresh Pattern[]s
   scoped to THIS clause (the per-clause vmap is $tptpVars, Block-
   scoped from clauseToEquation). *)

$tptpVars

clauseToEquation[{role_String, body_String}] :=
    Block[{$tptpVars = <||>, fmla},
        fmla = parseFormula[body];
        Which[
            fmla === $Failed,
                Message[tptpImport::badfmla, body];
                {"skip", $Failed}
            ,
            MemberQ[{"axiom", "hypothesis", "lemma"}, role],
                {"axiom", fmla}
            ,
            role === "negated_conjecture",
                {"conjecture",
                    If[ MatchQ[fmla, _Unequal], Equal @@ fmla, fmla]}
            ,
            role === "conjecture",
                {"conjecture", fmla}
            ,
            True,
                Message[tptpImport::badrole, role];
                {"skip", $Failed}
        ]
    ]

parseFormula[body_String] := Block[{cut = topLevelEqSplit[body]},
    Which[
        cut === $Failed, $Failed,
        cut[[3]] === "!=",
            Unequal[parseTermString[cut[[1]]], parseTermString[cut[[2]]]],
        True,
            Equal[parseTermString[cut[[1]]], parseTermString[cut[[2]]]]
    ]
]

(* Find the top-level (depth-0) `=` or `!=` in body.  Returns
   {lhs, rhs, op} or $Failed.  Walks the string once. *)
topLevelEqSplit[body_String] := Block[{i = 1, len, depth = 0, ch,
        eq = 0, neq = 0},
    len = StringLength[body];
    While[ i <= len && eq === 0 && neq === 0,
        ch = StringTake[body, {i, i}];
        Which[
            ch === "(" || ch === "[", depth = depth + 1; i = i + 1,
            ch === ")" || ch === "]", depth = depth - 1; i = i + 1,
            depth === 0 && i + 1 <= len &&
                StringTake[body, {i, i + 1}] === "!=",
                neq = i
            ,
            depth === 0 && ch === "=",
                eq = i
            ,
            True, i = i + 1
        ]
    ];
    Which[
        neq > 0, {
            StringTrim @ StringTake[body, {1, neq - 1}],
            StringTrim @ StringTake[body, {neq + 2, len}],
            "!="
        },
        eq  > 0, {
            StringTrim @ StringTake[body, {1, eq - 1}],
            StringTrim @ StringTake[body, {eq + 1, len}],
            "="
        },
        True, $Failed
    ]
]

(* ----- term parser -----
   Reads one term from text starting at position i0; returns
   {term, position-after-term}.  Handles:
     '(' term ')'   -- redundant parens
     functor '(' arg, arg, ... ')'
     atomic functor / constant
     Uppercase variable (clause-scoped via $tptpVars). *)

parseTermString[s_String] := readTerm[StringTrim[s], 1][[1]]

readTerm[text_String, i0_Integer] :=
    Block[{i = i0, len, ch, tok, child, parts},
        len = StringLength[text];
        While[ i <= len &&
            StringMatchQ[StringTake[text, {i, i}], Whitespace],
            i = i + 1
        ];
        If[ i > len, Return[{$Failed, i}]];
        ch = StringTake[text, {i, i}];
        If[ ch === "(",
            {child, i} = readTerm[text, i + 1];
            i = skipPast[text, i, ")"];
            Return[{child, i}]
        ];
        {tok, i} = readWord[text, i];
        If[ tok === "", Return[{$Failed, i}]];
        While[ i <= len &&
            StringMatchQ[StringTake[text, {i, i}], Whitespace],
            i = i + 1
        ];
        If[ i <= len && StringTake[text, {i, i}] === "(",
            {parts, i} = readArgs[text, i + 1];
            {Symbol[mangleHead[tok]] @@ parts, i}
            ,
            If[ tptpVarQ[tok],
                {ensureVar[tok], i},
                {Symbol[mangleHead[tok]], i}
            ]
        ]
    ]

(* Read a comma-separated argument list starting AFTER the open paren.
   Returns {args-list, position-after-close-paren}.  Reap/Sow builds
   the list -- no AppendTo. *)
readArgs[text_String, i0_Integer] := Block[{i = i0, sown, child, len},
    len = StringLength[text];
    sown = Reap[
        While[ True,
            {child, i} = readTerm[text, i];
            Sow[child];
            While[ i <= len &&
                StringMatchQ[StringTake[text, {i, i}], Whitespace],
                i = i + 1
            ];
            If[ i > len || StringTake[text, {i, i}] === ")",
                i = i + 1; Break[]];
            If[ StringTake[text, {i, i}] === ",", i = i + 1]
        ]
    ][[2]];
    {Replace[sown, {{xs_List} :> xs, _ -> {}}], i}
]

tptpVarQ[s_String] :=
    StringLength[s] > 0 &&
    StringMatchQ[StringTake[s, 1], RegularExpression["[A-Z_]"]]

(* Per-clause variable cache.  $tptpVars is Block-scoped from
   clauseToEquation so we assign to it directly (Block bindings are
   mutable inside the block). *)
ensureVar[name_String] := (
    If[ ! KeyExistsQ[$tptpVars, name],
        $tptpVars[name] = Pattern[Evaluate @ Unique["v"], Blank[]]
    ];
    $tptpVars[name]
)

(* WL Symbols disallow underscore and `$` mid-name.  Mangle TPTP
   identifiers so the Symbol[] call succeeds, round-trippable for
   the common cases:
     $true     -> Tptp$True
     sk_c1     -> skC1        (CamelCase fold)
     plain     -> plain       (unchanged) *)
mangleHead[name_String] := Which[
    StringStartsQ[name, "$"],
        "Tptp$" <> StringJoin[
            Capitalize /@ StringSplit[StringDrop[name, 1], "_"]],
    StringContainsQ[name, "_"],
        With[{parts = StringSplit[name, "_"]},
            First[parts] <> StringJoin[Capitalize /@ Rest[parts]]
        ],
    True, name
]

End[];   (* `Private` *)
EndPackage[];
