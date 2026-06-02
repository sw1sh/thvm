#!/usr/bin/env wolframscript
(* TPTP CNF -> Waldmeister .pr converter (no TPTPImport dependency).

   Parses simple TPTP CNF lines (only unit equational, which is
   what tools/vampire/export_all.wls emits) via direct string
   surgery -- avoids Wolfram`Parser`'s first-call EBNF-fetch
   stall.

   Usage:
     tptp_to_pr.wls <in.p> <out.pr> *)

debugPrint[a___] := WriteString[
    "stderr", StringJoin @@ Map[ToString, {a}], "\n"
]
print[a___] := WriteString[
    "stdout", StringJoin @@ Map[ToString, {a}], "\n"
]

If[ Length[$ScriptCommandLine] < 3,
    print["usage: tptp_to_pr.wls <in.p> <out.pr>"];
    Exit[2]
]
tptpFile = $ScriptCommandLine[[2]]
outFile  = $ScriptCommandLine[[3]]

If[ ! FileExistsQ[tptpFile],
    print["TPTP file not found: ", tptpFile];
    Exit[1]
]

(* Camel-case-fold underscore names so WM's lexer accepts them. *)
sanitize[s_String] :=
    Which[
        StringStartsQ[s, "$"], "dollar" <> Capitalize @ StringDrop[s, 1],
        StringContainsQ[s, "_"],
            With[{parts = StringSplit[s, "_"]},
                First[parts] <> StringJoin[Capitalize /@ Rest[parts]]
            ],
        True, s
    ]

(* Parse a single `cnf(name, role, body).` line.  body is either
   `lhs = rhs` (axiom) or `lhs != rhs` (negated_conjecture). *)
parseCnf[line_String] := Module[{m, role, body, op, lhs, rhs},
    m = StringCases[
        line,
        "cnf(" ~~ __ ~~ ", " ~~ r : (LetterCharacter | "_") .. ~~
            ", " ~~ b__ ~~ ")." :> {r, b}
    ];
    If[ Length[m] === 0, Return[Nothing]];
    {role, body} = First[m];
    {op, {lhs, rhs}} = Which[
        StringContainsQ[body, " != "],
            {"ineq", StringTrim /@ StringSplit[body, " != ", 2]},
        StringContainsQ[body, " = "],
            {"eq", StringTrim /@ StringSplit[body, " = ", 2]},
        True, {Missing[], {body, body}}
    ];
    <|"Role" -> role, "Op" -> op, "LHS" -> lhs, "RHS" -> rhs|>
]

(* Walk a term-string and extract every (name, arity) pair.
   For each identifier:
     * if immediately followed by `(` -> count comma-separated args
       at depth 1; arity = that count (or 0 if empty parens)
     * else -> arity 0 (a nullary constant)
   Recurses into nested function calls by scanning every identifier
   in the source, not just top-level ones. *)
collectSig[term_String] := Module[{out = <||>, n = StringLength[term], pos = 1,
        c, k, name, depth, args},
    While[ pos <= n,
        c = StringTake[term, {pos, pos}];
        If[ StringMatchQ[c, LetterCharacter],
            k = pos;
            While[ k <= n && StringMatchQ[
                    StringTake[term, {k, k}],
                    LetterCharacter | DigitCharacter | "_"
                ],
                k++
            ];
            name = sanitize @ StringTake[term, {pos, k - 1}];
            (* peek next char for `(` to decide arity *)
            If[ k <= n && StringTake[term, {k, k}] === "(",
                (* count args at depth 1 -- but DON'T advance pos past
                   the closing `)`; we want the outer scan to find
                   identifiers nested in the args. *)
                Block[{ipos = k + 1, idepth = 1, iargs},
                    iargs = If[ ipos <= n &&
                            StringTake[term, {ipos, ipos}] === ")", 0, 1];
                    While[ idepth > 0 && ipos <= n,
                        Switch[ StringTake[term, {ipos, ipos}],
                            "(", idepth++,
                            ")", idepth--,
                            ",", If[ idepth === 1, iargs++]
                        ];
                        ipos++
                    ];
                    If[ ! KeyExistsQ[out, name], out[name] = iargs];
                ];
                pos = k + 1   (* step INTO the parens for nested ids *)
                ,
                If[ ! KeyExistsQ[out, name], out[name] = 0];
                pos = k
            ],
            pos++
        ]
    ];
    out
]

(* Sanitize term-string for WM .pr emit: drop underscores from
   identifiers (preserves arity since we don't change parens). *)
sanitizeTerm[term_String] := StringReplace[
    term,
    id : (LetterCharacter ~~ (LetterCharacter | DigitCharacter | "_") ...) :>
        sanitize[id]
]

(* Collect variables: identifiers starting with uppercase X then
   digits.  Sanitized form is the same (no underscores). *)
collectVars[term_String] := DeleteDuplicates @
    StringCases[term, RegularExpression["\\bX\\d+\\b"]]

text = Import[tptpFile, "Text"]
lines = Select[StringSplit[text, "\n"],
    StringStartsQ[StringTrim[#], "cnf("] &]
records = DeleteCases[parseCnf /@ lines, Nothing]
debugPrint["Parsed ", Length[records], " CNF records"]

(* Split axioms vs negated conjecture *)
axRecs   = Select[records, #["Role"] === "axiom" &]
goalRecs = Select[records, #["Role"] === "negated_conjecture" &]
If[ Length[goalRecs] =!= 1,
    print["Expected exactly 1 negated_conjecture, got ", Length[goalRecs]];
    Exit[1]
]
goal = First[goalRecs]

allTermStrings = Join[
    {#["LHS"], #["RHS"]} & /@ axRecs // Flatten,
    {goal["LHS"], goal["RHS"]}
]

(* Union signature *)
sigByName = Merge[Map[collectSig, allTermStrings], First]
(* Drop variables (Xn) from signature *)
sigByName = KeySelect[sigByName, ! StringMatchQ[#, "X" ~~ DigitCharacter ..] &]
sig = Sort @ Normal @ sigByName
debugPrint["Signature: ", sig]

(* Variables across all terms *)
vars = DeleteDuplicates @ Sort @ Flatten[collectVars /@ allTermStrings]

(* Precedence: constants > unary > binary > ..., alphabetical within.
   `sig` entries are name->arity; sort ascending by arity, then
   alphabetical -- gives constants first (lowest arity); the
   precedence reads top-to-bottom as highest-to-lowest. *)
precList = Map[First,
    SortBy[sig, {#[[2]] &, First[#] &}]
]

(* Emit *)
(* sig is a List of Rules name->arity; destructure with /. or Apply. *)
sigBlock = StringRiffle[
    Map[
        Replace[#,
            (n_String -> a_Integer) :>
                n <> ": " <>
                If[ a > 0,
                    StringJoin @@ ConstantArray["ANY ", a],
                    ""
                ] <> "-> ANY"
        ] &,
        sig
    ],
    "\n              "
]
varBlock = StringRiffle[vars, ", "] <> ": ANY"
precBlock = StringRiffle[precList, " > "]
eqBlock = StringRiffle[
    Map[(sanitizeTerm[#["LHS"]] <> " = " <> sanitizeTerm[#["RHS"]]) &, axRecs],
    "\n              "
]
conclusion = sanitizeTerm[goal["LHS"]] <> " = " <> sanitizeTerm[goal["RHS"]]

pr = StringJoin[
    "NAME          ", sanitize @ FileBaseName[tptpFile], "\n",
    "MODE          PROOF\n",
    "SORTS         ANY\n",
    "SIGNATURE     ", sigBlock, "\n",
    "ORDERING      LPO\n",
    "              ", precBlock, "\n",
    "VARIABLES     ", varBlock, "\n",
    "EQUATIONS     ", eqBlock, "\n",
    "CONCLUSION    ", conclusion, "\n"
]

Export[outFile, pr, "Text"]
print["Wrote ", outFile, " (", StringLength[pr], " bytes; ",
    Length[sig], " symbols, ", Length[vars], " variables, ",
    Length[axRecs], " axioms)"]
