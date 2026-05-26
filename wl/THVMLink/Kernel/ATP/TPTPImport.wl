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

BeginPackage["THVMLink`ATP`", {"THVMLink`"}];

TPTPImport::usage =
    "TPTPImport[File[\"file.p\"]] | TPTPImport[\"... cnf source ...\"] " <>
    "returns <|\"Axioms\" -> {...}, \"Conjecture\" -> ...|>.  Function " <>
    "symbols come back as String-headed terms (\"and\"[X, Y] etc.) so " <>
    "they cannot collide with user-level WL symbols.";

TPTPImport::badrole = "Skipping cnf clause with unsupported role `1`.";
TPTPImport::badfmla = "Could not parse formula `1` (expected " <>
    "`lhs = rhs` or `lhs != rhs`).";
TPTPImport::skipnoncnf =
    "Skipping unsupported `1` directive at offset `2` (only cnf clauses " <>
    "are handled; see github.com/TPTPWorld/SyntaxBNF for the full grammar).";

Begin["`Private`"];

(* ----- entry ----- *)

TPTPImport[File[path_String]] := TPTPImport @ Import[path, "Text"]

TPTPImport[s_String] /; FileExistsQ[s] && ! StringContainsQ[s, "cnf("] :=
    TPTPImport @ File[s]

TPTPImport[text_String] := Block[{stripped, clauses, results},
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
                i = consumeAnnotated[text, i + 4]
            ,
            i + 3 <= len && StringTake[text, {i, i + 3}] === "fof(",
                i = consumeAnnotated[text, i + 4]
            ,
            i + 3 <= len && MemberQ[
                {"tff(", "thf("}, StringTake[text, {i, i + 3}]],
                head = StringTake[text, {i, i + 2}];
                Message[TPTPImport::skipnoncnf, head, i];
                i = skipParenthesised[text, i + 4]
            ,
            i + 7 <= len && StringTake[text, {i, i + 7}] === "include(",
                Message[TPTPImport::skipnoncnf, "include", i];
                i = skipParenthesised[text, i + 8]
            ,
            True, i = i + 1
        ]
    ]
]

(* Reads the cnf(...) / fof(...) tail: name, role, body.  Body is
   passed unmodified to clauseToEquation, which handles both cnf flat
   equations and fof universal-prefixed equations via stripUniversal. *)
consumeAnnotated[text_String, i0_Integer] :=
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
                Message[TPTPImport::badfmla, body];
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
                Message[TPTPImport::badrole, role];
                {"skip", $Failed}
        ]
    ]

parseFormula[body_String] :=
    Block[{stripped = stripUniversal[body], cut},
        cut = topLevelEqSplit[stripped];
        Which[
            cut === $Failed, $Failed,
            cut[[3]] === "!=",
                Unequal[parseTermString[cut[[1]]], parseTermString[cut[[2]]]],
            True,
                Equal[parseTermString[cut[[1]]], parseTermString[cut[[2]]]]
        ]
    ]

(* Strip a leading `! [V1, V2, ...] :` universal quantifier (any
   number of bound vars, optional whitespace).  Universal binding is
   the default for cnf clauses anyway, so we treat the FOF prefix as
   a no-op and parse the body that follows.  Conjunction / disjunction
   / existential / negation outside this single-equation shape stays
   un-handled (topLevelEqSplit will return $Failed and clauseToEquation
   warns).

   Also strips a single outer pair of parentheses that often surrounds
   FOF bodies in pretty-printed inputs. *)
stripUniversal[body_String] := Block[{trim = StringTrim[body], inner},
    If[ StringStartsQ[trim, "!"],
        inner = StringTrim @ StringDrop[trim, 1];
        If[ StringStartsQ[inner, "["],
            (* skip the variable list up to the matching ']' *)
            Block[{i = 2, depth = 1, len = StringLength[inner]},
                While[ i <= len && depth > 0,
                    Switch[ StringTake[inner, {i, i}],
                        "[", depth = depth + 1,
                        "]", depth = depth - 1,
                        _,   Null
                    ];
                    i = i + 1
                ];
                inner = StringTrim @ StringDrop[inner, i - 1];
                If[ StringStartsQ[inner, ":"],
                    inner = StringTrim @ StringDrop[inner, 1];
                    stripUniversal[stripOuterParens @ inner]
                    ,
                    trim   (* malformed -- bail with the original *)
                ]
            ]
            ,
            trim   (* "!" without "[" -- not a quantifier *)
        ]
        ,
        stripOuterParens[trim]
    ]
]

(* If the body is exactly `( ... )`, strip one paired layer. *)
stripOuterParens[s_String] := Block[{trim = StringTrim[s], len, depth, ok},
    len = StringLength[trim];
    If[ len < 2 || StringTake[trim, 1] =!= "(" ||
            StringTake[trim, {len, len}] =!= ")",
        Return[trim]
    ];
    (* Confirm the first '(' matches the LAST ')' (not an early
       close).  Scan once, depth-counting. *)
    depth = 0; ok = True;
    Do[ Switch[ StringTake[trim, {k, k}],
            "(", depth = depth + 1,
            ")", depth = depth - 1,
            _,   Null
        ];
        If[ depth === 0 && k < len, ok = False; Break[]],
        {k, len}
    ];
    If[ ok, StringTrim @ StringTake[trim, {2, len - 1}], trim]
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
            (* String-headed term: "and"[x, y] etc.  Strings as Heads
               sidestep the THVMLink`TPTPImport`Tptp` namespacing dance
               (no Symbol collisions with user globals because Strings
               are not bound to anything in any context). *)
            {tok @@ parts, i}
            ,
            If[ tptpVarQ[tok],
                {ensureVar[tok], i},
                (* Nullary constant: "a"[] (empty argument list) rather
                   than the bare string "a".  WL evaluates `"a" == "b"`
                   to False eagerly because Equal short-circuits on
                   distinct string literals, but Equal on distinct
                   compound expressions (`"a"[]` vs `"b"[]`) stays
                   unevaluated -- the form TFindProof and TSatEUF
                   expect.  Equivalent shape to the n-ary case. *)
                {tok[], i}
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

(* (Function-symbol heads are kept as bare Strings -- "and"[X, Y]
   instead of a Symbol in a private context.  Strings are not bound
   in any WL context, so they cannot collide with the user's globals
   and don't need a Tptp` namespace dance.) *)

End[];   (* `Private` *)
EndPackage[];
