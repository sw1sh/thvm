(* TPTPImport.resource.wl - parse TPTP problem files into the
   <|"Axioms" -> {phi1, phi2, ...}, "Conjecture" -> phi|> shape.

   This file is the self-contained implementation of the public
   `TPTPImport` function.  It is the resource body of the WFR-style
   submission: top-level definitions only, no `BeginPackage` /
   `Begin` shell, so a caller can `Get` it into any context they
   choose.  The companion `TPTPImport.md` document inlines this file
   via the `#| file:` cell option and wraps it with the Function
   Repository definition-notebook metadata.

   Inside the thvm tree, `THVMLink/Kernel/ATP/TPTPImport.wl` is a
   thin shell that `BeginPackage`s into `THVMLink`ATP``, pre-declares
   `TPTPImport` in the public context, and `Get`s this file from the
   private context so helpers land in `Private` while `TPTPImport`
   resolves to the public symbol.

   Subset handled (full TPTPWorld BNF surface):
     - cnf(name, role, formula).  formula is a disjunction of
       literals, each literal an equation `lhs = rhs`, a disequation
       `lhs != rhs`, a predicate atom `p(args)`, or a negated atom
       `~ atom`.  Single equational literals come back as bare
       `Equal[l, r]` / `Unequal[l, r]`; multi-literal clauses come
       back as `Or[lit1, lit2, ...]`.
     - fof(name, role, formula).  Full first-order Boolean grammar
       with `&`, `|`, `~`, `=>`, `<=`, `<=>`, `<~>`, `~&`, `~|`,
       parenthesised subformulas, equational atoms, predicate atoms,
       `$true`/`$false`, and prefix `! [V1, ..., Vn] :` / `? [V1,
       ..., Vn] :` quantifiers.  Leading `!` is stripped (universal
       binding is the cnf default); inner quantifiers stay as WL
       `ForAll`/`Exists` so downstream sees the full structure.
     - tff(name, role, formula).  Typed first-order.  `tff(_, type,
       ...)` signature declarations skipped; sort annotations
       (`X:srt`) stripped.
     - tcf(name, role, formula).  Typed cnf cousin of tff.
     - thf(name, role, formula).  Typed higher-order.  `^ [vars] :
       body` lambda -> `Function[{vars}, body]`; `f @ x @ y` left-
       associative application -> `f[x][y]` (with a special-case
       collapse of `"name"[][...]` to `"name"[...]` on first apply).
     - ncf(name, role, formula).  Non-classical / modal.  Parses via
       the fof Boolean grammar; modal operators ($box, $dia) ride
       the `$`-defined identifier path as generic compounds.
     - include('path') and include('path', [n1, n2, ...]).  Resolved
       relative to the current file's directory, then $TPTP /
       $TPTP/Problems env-var roots; the optional clause-name
       selector filters which clauses are admitted.

   Term-level coverage: bare identifiers, variables (uppercase ->
   Pattern), single-quoted atoms (`'a b c'` -> `"a b c"[]`), double-
   quoted distinct objects (`"foo"` -> `"\"foo\""[]` with literal
   quote chars preserved in the head), signed and unsigned numeric
   literals (integer, rational, real, scientific).

   Sequent rewrites: `lhs1, lhs2 --> rhs1, rhs2` becomes
   `Implies[And[lhs_i], Or[rhs_j]]` with optional `[...]` fof_tuple
   brackets on either side.

   Variables are clause-scoped: each Uppercase identifier inside one
   clause names the same universal variable; the same name in a
   later clause is independent.  Each occurrence gets a fresh
   `Pattern[Unique[]]` so subsequent formulas share no bound
   variables.  Variables under an `Exists` are skolemised to fresh
   `Unique[]` symbols (bare Symbol, no Pattern wrapper); scope is
   snapshot+restore around the quantifier body. *)

(* ----- messages ----- *)

TPTPImport::badrole    = "Skipping clause with unsupported role `1`.";
TPTPImport::badfmla    = "Could not parse formula `1`.";
TPTPImport::skipnoncnf =
    "Skipping unsupported `1` directive at offset `2` (thf / ncf / tpi " <>
    "clause heads are out of scope; see github.com/TPTPWorld/SyntaxBNF " <>
    "for the full grammar).";
TPTPImport::badinclude =
    "Could not resolve include path `1` (searched relative to `2` and " <>
    "the $TPTP / $TPTP/Problems env-var roots).";


(* ----- entry ----- *)

TPTPImport[File[path_String]] := tptpImportText[
    Import[path, "Text"],
    DirectoryName[AbsoluteFileName[path]]
]

TPTPImport[s_String] /; FileExistsQ[s] &&
        ! StringContainsQ[s, "cnf("] && ! StringContainsQ[s, "fof("] :=
    TPTPImport @ File[s]

TPTPImport[text_String] := tptpImportText[text, Directory[]]

(* Pure-text + base-directory entry point.  base directory governs
   relative-path resolution of `include('...')` directives. *)
tptpImportText[text_String, baseDir_String] := Block[
    {stripped, clauses, results},
    stripped = StringReplace[text, {
        RegularExpression["%[^\n]*"]                 -> "",
        RegularExpression["/\\*([^*]|\\*[^/])*\\*+/"] -> ""
    }];
    clauses = parseClauses[stripped, baseDir];
    results = Map[clauseToFormula, clauses];
    <|
        "Axioms"     -> Cases[results, {"axiom", phi_} :> phi],
        "Conjecture" -> FirstCase[results, {"conjecture", phi_} :> phi, None]
    |>
]

(* ----- clause splitter -----
   Scan once over the stripped text.  `cnf(` / `fof(` / `tff(` /
   `tcf(` trigger a parser call (the {head, role, body} tuple is
   Sown for clauseToFormula to dispatch on); `include(` recurses
   into the named file; `thf(` / `ncf(` / `tpi(` skip to the
   matching `).` with a console warning.

   Returns a list of {head, role, body} tuples.  Reap/Sow builds
   the list once at the end (no AppendTo). *)

(* Reap returns {value, {{sown...}}} -- we want the sown list, with a
   {} fallback when nothing was Sown. *)
parseClauses[text_String, baseDir_String] :=
    Replace[Reap[scanClauses[text, 1, baseDir]][[2]],
        {{xs_List} :> xs, _ -> {}}]

(* Match a clause-head prefix `head(` starting at i in text. *)
clauseHeadMatchQ[text_String, i_Integer, head_String] := Block[
    {n = StringLength[head]},
    i + n <= StringLength[text] &&
        StringTake[text, {i, i + n}] === head <> "("
]

scanClauses[text_String, i0_Integer, baseDir_String] :=
    Block[{i = i0, len},
        len = StringLength[text];
        While[ i <= len,
            Which[
                clauseHeadMatchQ[text, i, "cnf"],
                    i = consumeAnnotated[text, i + 4, "cnf"]
                ,
                clauseHeadMatchQ[text, i, "fof"],
                    i = consumeAnnotated[text, i + 4, "fof"]
                ,
                clauseHeadMatchQ[text, i, "tff"],
                    i = consumeAnnotated[text, i + 4, "tff"]
                ,
                clauseHeadMatchQ[text, i, "tcf"],
                    i = consumeAnnotated[text, i + 4, "tcf"]
                ,
                clauseHeadMatchQ[text, i, "thf"],
                    i = consumeAnnotated[text, i + 4, "thf"]
                ,
                clauseHeadMatchQ[text, i, "ncf"],
                    i = consumeAnnotated[text, i + 4, "ncf"]
                ,
                clauseHeadMatchQ[text, i, "include"],
                    i = consumeInclude[text, i + 8, baseDir]
                ,
                clauseHeadMatchQ[text, i, "tpi"],
                    i = skipParenthesised[text, i + 4]
                ,
                True, i = i + 1
            ]
        ]
    ]

(* Reads the cnf(...) / fof(...) / tff(...) / tcf(...) tail:
   name, role, body, [optional annotations].  Sows a 4-tuple
   {name, head, role, body} so downstream consumers (clauseToFormula
   for the parser, the include-selector filter for `include(...,
   [name1, ...])`) can both see the clause name.  clauseToFormula
   ignores the name; the include filter uses it.

   TPTP optionally allows a 4th positional argument (source
   annotation: file/inference/introduced/etc.) and a 5th (useful_info
   list).  These are discarded -- the parser stops at the body's
   trailing comma or its closing paren, and skipParenthesised winds
   the cursor past whatever annotation follows.

   For tff/tcf/thf, a `type` role is a signature declaration with no
   semantic content for the untyped homogeneous saturator -- skip it
   silently without emitting a tuple. *)
consumeAnnotated[text_String, i0_Integer, head_String] :=
    Block[{i = i0, name, role, body, bodyEnd, clauseEnd},
        {name, i} = readWord[text, i];
        i = skipPast[text, i, ","];
        {role, i} = readWord[text, i];
        i = skipPast[text, i, ","];
        {body, bodyEnd} = readBalancedUpTo[text, i, ","];
        If[ ! (MemberQ[{"tff", "tcf", "thf"}, head] && role === "type"),
            Sow[{name, head, role, StringTrim[body]}]
        ];
        (* Wind the cursor past any remaining annotations + the
           outer `).` terminator.  readBalancedUpTo stopped at a
           top-level `,` or `)` of the cnf(...) outer call; in either
           case, scanning to the next `).` finishes the clause. *)
        clauseEnd = bodyEnd;
        While[ clauseEnd <= StringLength[text] &&
                StringTake[text, {clauseEnd, clauseEnd}] =!= ")",
            clauseEnd = clauseEnd + 1];
        skipPast[text, clauseEnd, "."]
    ]

(* Read a balanced sub-expression starting at i0; stop just BEFORE the
   first depth-0 character in `delims` (a string of one-char
   delimiters) or just BEFORE a closing `)` at depth 0.  Returns
   {content, cursor-AT-the-delimiter}.  Used by consumeAnnotated to
   peel the body off `cnf(name, role, body, ...)` without grabbing
   the trailing annotations. *)
readBalancedUpTo[text_String, i0_Integer, delims_String] := Block[
    {i = i0, len = StringLength[text], depth = 0, ch, done = False},
    While[ ! done && i <= len,
        ch = StringTake[text, {i, i}];
        Which[
            ch === "(" || ch === "[" || ch === "{",
                depth = depth + 1; i = i + 1,
            ch === "]" || ch === "}",
                depth = depth - 1; i = i + 1,
            ch === ")",
                If[ depth === 0, done = True, depth = depth - 1; i = i + 1],
            depth === 0 && StringContainsQ[delims, ch], done = True,
            True, i = i + 1
        ]
    ];
    {StringTake[text, {i0, i - 1}], i}
]

(* Read an include directive starting AFTER the open paren of
   `include(`.  The TPTP form is

       include('path').
       include('path', [name1, name2, ...]).

   The optional 2nd argument is a clause-name selector: when present
   only clauses whose name appears in the list are admitted.  Resolve
   the path relative to baseDir / $TPTP env-var roots, recursively
   scan the included file, splice the (possibly filtered) clauses into
   the enclosing Reap.  On unresolvable path, warn + skip. *)
consumeInclude[text_String, i0_Integer, baseDir_String] :=
    Block[{i = i0, len, start, quoted, selector, resolved, subDir,
           subText, subClauses, filteredClauses},
        len = StringLength[text];
        i = skipWS[text, i];
        (* TPTP single-quoted string; scan to the next `'`. *)
        If[ i <= len && StringTake[text, {i, i}] === "'",
            i = i + 1; start = i;
            While[ i <= len && StringTake[text, {i, i}] =!= "'",
                i = i + 1];
            quoted = StringTake[text, {start, i - 1}];
            i = i + 1   (* skip the closing quote *)
            ,
            (* tolerate bare (unquoted) path -- scan to ',' or ')' *)
            start = i;
            While[ i <= len &&
                    StringTake[text, {i, i}] =!= "," &&
                    StringTake[text, {i, i}] =!= ")",
                i = i + 1];
            quoted = StringTrim @ StringTake[text, {start, i - 1}]
        ];
        (* Optional clause-name selector `[name1, name2, ...]` after a
           comma; `All` (no selector) -> admit every clause. *)
        i = skipWS[text, i];
        selector = All;
        If[ i <= len && StringTake[text, {i, i}] === ",",
            i = i + 1;
            i = skipWS[text, i];
            If[ i <= len && StringTake[text, {i, i}] === "[",
                Block[{listEnd = matchingBracketPos[text, i, "[", "]"]},
                    selector = parseQuantVarList @ StringTake[text,
                        {i + 1, listEnd - 1}];
                    i = listEnd + 1
                ]
            ]
        ];
        resolved = resolveIncludePath[quoted, baseDir];
        If[ resolved === $Failed,
            Message[TPTPImport::badinclude, quoted, baseDir]
            ,
            subDir = DirectoryName[AbsoluteFileName[resolved]];
            subText = Import[resolved, "Text"];
            subText = StringReplace[subText, {
                RegularExpression["%[^\n]*"]                 -> "",
                RegularExpression["/\\*([^*]|\\*[^/])*\\*+/"] -> ""
            }];
            subClauses = parseClauses[subText, subDir];
            filteredClauses = If[ selector === All,
                subClauses,
                Select[subClauses, MemberQ[selector, #[[1]]] &]];
            Scan[Sow, filteredClauses]
        ];
        skipPast[text, i, "."]
    ]

(* TPTP convention: absolute paths used as-is; relative paths
   resolved against (in order) baseDir, $TPTP, $TPTP/Problems. *)
absolutePathQ[path_String] :=
    StringStartsQ[path, "/"] ||
    StringMatchQ[path, RegularExpression["^[A-Za-z]:.*"]]

resolveIncludePath[path_String, baseDir_String] := Block[
    {tptpRoot = Environment["TPTP"], candidates},
    candidates = Join[
        {path},
        If[ absolutePathQ[path],
            {},
            {FileNameJoin[{baseDir, path}]}
        ],
        If[ tptpRoot === $Failed || tptpRoot === None,
            {},
            {FileNameJoin[{tptpRoot, path}],
             FileNameJoin[{tptpRoot, "Problems", path}]}
        ]
    ];
    SelectFirst[candidates, FileExistsQ, $Failed]
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
   Dispatches on clause head:
     cnf / tcf -- body is a disjunction of literals (single literal
                  preserved as bare Equal / Unequal for backward
                  compatibility; multi-literal returns Or[...]).
     fof / tff -- body is a full Boolean combination of atoms with
                  optional leading `! [vars] :` universal that is
                  stripped (the saturator already binds free vars
                  universally).
     tff / tcf  also have sort annotations (`X:srt`) stripped at the
                preprocessing stage (thvm's surface is homogeneous
                untyped).

   Per-clause variable scope: $tptpVars maps each upper-case
   variable NAME to a fresh `Pattern[Unique["v"], Blank[]]` on first
   reference inside the clause; subsequent uses of the same name
   within this clause reuse the same Pattern.  Different clauses
   share no variables. *)

$tptpVars

clauseToFormula[{_String, head_String, role_String, body_String}] :=
    Block[{$tptpVars = <||>, prepared, fmla},
        prepared = preprocessBody[body, head];
        fmla = Which[
            head === "cnf" || head === "tcf", parseCnfFormula[prepared],
            (* fof, tff, thf all use the same Boolean grammar; the thf
               extensions (`^` lambda, `@` application) are recognised
               by readUnary / readAtomicFormula unconditionally. *)
            True, parseFofFormula[prepared]
        ];
        Which[
            fmla === $Failed,
                Message[TPTPImport::badfmla, body];
                {"skip", $Failed}
            ,
            MemberQ[{"axiom", "hypothesis", "lemma"}, role],
                {"axiom", fmla}
            ,
            role === "negated_conjecture",
                {"conjecture", negatedConjectureFlip[fmla]}
            ,
            role === "conjecture",
                {"conjecture", fmla}
            ,
            True,
                Message[TPTPImport::badrole, role];
                {"skip", $Failed}
        ]
    ]

(* `negated_conjecture` in TPTP carries the NEGATION of the original
   conjecture; the saturator wants the un-negated goal under the
   "Conjecture" key.  For the dominant single-disequation shape, flip
   `s != t` to `s = t`.  For a multi-literal negated conjecture (a
   disjunction of disequations), the un-negated goal is the conjunction
   of the equations; emit that.  Other shapes get a plain `Not[...]`
   wrapper. *)
negatedConjectureFlip[Unequal[a_, b_]] := Equal[a, b]
negatedConjectureFlip[Or[lits__]] /;
        AllTrue[{lits}, MatchQ[#, _Unequal] &] :=
    And @@ ((Equal @@ # &) /@ {lits})
negatedConjectureFlip[fmla_] := Not[fmla]

(* tff/tcf: drop `:srt` sort annotations everywhere they appear (in
   quantifier variable lists, in atom argument positions).  thvm's
   surface is homogeneous untyped, so the sort information has no
   semantic content for the saturator and the SMT path.  Other heads:
   no-op. *)
preprocessBody[body_String, "tff"] := stripTypeAnnotations[body]
preprocessBody[body_String, "tcf"] := stripTypeAnnotations[body]
preprocessBody[body_String, "thf"] := stripTypeAnnotations[body]
preprocessBody[body_String, _]     := body

(* Strip every `:` followed by a sort identifier and optional sort
   expression.  Matches:
     X:nat            -> X
     X : nat          -> X
     X:($i > $o)      -> X
     X:nat * int      -> X    (product types)
   Requires the colon to be directly preceded by an identifier
   character (capturing that char in $1 so the replacement preserves
   it).  This guard keeps the quantifier-separator colon -- `! [V1,
   V2] : body` -- from being eaten, since that colon is preceded by
   `]`, not a word character.  The sort expression accepts identifiers,
   $-prefixed type names, `>`, `*`, balanced parens, and whitespace. *)
stripTypeAnnotations[body_String] := StringReplace[body,
    RegularExpression[
        "([A-Za-z0-9_\\$])\\s*:\\s*" <>
        "(?:\\([^)]*\\)|[\\$A-Za-z_][\\$A-Za-z0-9_]*)" <>
        "(?:\\s*[>*]\\s*(?:\\([^)]*\\)|[\\$A-Za-z_][\\$A-Za-z0-9_]*))*"
    ] -> "$1"
]

(* ----- cnf body: disjunction of literals -----
   A cnf body is a top-level `|`-separated list of literals; each
   literal is `atom`, `~ atom`, `lhs = rhs`, or `lhs != rhs`.
   Single literal returns the bare WL form (Equal/Unequal/term/Not);
   multi-literal returns `Or[...]`. *)
parseCnfFormula[body_String] := Block[{trimmed, parts, lits},
    trimmed = StringTrim @ stripOuterParens[StringTrim[body]];
    parts = splitTopLevel[trimmed, "|"];
    If[ parts === $Failed, Return[$Failed]];
    lits = Map[parseCnfLiteral, parts];
    If[ MemberQ[lits, $Failed], Return[$Failed]];
    If[ Length[lits] === 1, lits[[1]], Or @@ lits]
]

parseCnfLiteral[lit_String] := Block[{trimmed, atom},
    trimmed = StringTrim[lit];
    Which[
        StringStartsQ[trimmed, "~"],
            atom = parseAtomString @ StringTrim @ StringDrop[trimmed, 1];
            If[ atom === $Failed, $Failed, negateAtom[atom]]
        ,
        True, parseAtomString[trimmed]
    ]
]

(* Atom in cnf: `lhs = rhs`, `lhs != rhs`, or a predicate term.  *)
parseAtomString[s_String] := Block[{trimmed, cut},
    trimmed = stripOuterParens @ StringTrim[s];
    cut = topLevelEqSplit[trimmed];
    Which[
        cut === $Failed, parseTermString[trimmed],
        cut[[3]] === "!=", Unequal[
            parseTermString[cut[[1]]], parseTermString[cut[[2]]]],
        True, Equal[
            parseTermString[cut[[1]]], parseTermString[cut[[2]]]]
    ]
]

(* Negate a parsed atom: fold !=/= into their duals; wrap anything
   else (predicate term, $true/$false) in `Not[...]`. *)
negateAtom[Equal[a_, b_]]    := Unequal[a, b]
negateAtom[Unequal[a_, b_]]  := Equal[a, b]
negateAtom[True]             := False
negateAtom[False]            := True
negateAtom[atom_]            := Not[atom]

(* ----- fof body: full Boolean formula with quantifiers -----
   Recursive-descent over precedence layers, lowest to highest:
     1. Non-assoc binary: <=>, =>, <=, <~>, ~|, ~&
     2. Or  (|)   left-associative
     3. And (&)   left-associative
     4. Unary: ~ (negation), !/? (quantifiers)
     5. Atom: parenthesised formula, equation, predicate term, $true/$false

   Returns just the parsed formula (the position cursor is internal
   to the parser).  Strips leading `! [V1, ..., Vn] :` universal
   quantifier so the result has the cnf default (universal binding
   via Pattern variables in $tptpVars). *)
parseFofFormula[body_String] := Block[{trimmed, peeled, res, pos},
    trimmed = StringTrim @ stripOuterParens @ StringTrim[body];
    (* Sequent shape `lhs --> rhs`: comma-separated formula lists on
       each side, semantically (lhs1 & ... & lhsN) => (rhs1 | ... |
       rhsM).  Detect and rewrite to the equivalent implication so the
       standard parser handles it. *)
    If[ StringContainsQ[trimmed, "-->"],
        Return @ parseSequent[trimmed]
    ];
    (* Strip leading universal quantifier(s) -- iterate so
       `! [X] : ! [Y] : phi` collapses to `phi` with both vars
       bound as Patterns via $tptpVars references in atoms. *)
    peeled = peelLeadingUniversals[trimmed];
    {res, pos} = readFofFormula[peeled, 1];
    res
]

(* Sequent body: `A1, ..., An --> B1, ..., Bm`.  Each side may be
   wrapped in `[...]` brackets per TPTP fof_tuple syntax.  Returns
   `Implies[And[A_i], Or[B_j]]`; empty lhs -> Or-side alone;
   empty rhs -> Not[And-side]. *)
parseSequent[body_String] := Block[
    {parts, lhsStr, rhsStr, lhsFmlas, rhsFmlas, lhs, rhs},
    parts = StringSplit[body, "-->", 2];
    If[ Length[parts] =!= 2,
        Message[TPTPImport::badfmla, body];
        Return[$Failed]
    ];
    lhsStr = stripTupleBrackets @ StringTrim @ parts[[1]];
    rhsStr = stripTupleBrackets @ StringTrim @ parts[[2]];
    lhsFmlas = If[ lhsStr === "", {},
        parseFofFormula /@ splitTopLevel[lhsStr, ","]];
    rhsFmlas = If[ rhsStr === "", {},
        parseFofFormula /@ splitTopLevel[rhsStr, ","]];
    lhs = Which[
        lhsFmlas === {},          True,
        Length[lhsFmlas] === 1,   lhsFmlas[[1]],
        True,                     And @@ lhsFmlas
    ];
    rhs = Which[
        rhsFmlas === {},          False,
        Length[rhsFmlas] === 1,   rhsFmlas[[1]],
        True,                     Or @@ rhsFmlas
    ];
    Implies[lhs, rhs]
]

(* `[ phi1, phi2, ... ]` -> `phi1, phi2, ...`.  No brackets -> pass
   through unchanged. *)
stripTupleBrackets[s_String] := Block[{trim = StringTrim[s], n},
    n = StringLength[trim];
    If[ n >= 2 && StringTake[trim, 1] === "[" &&
            StringTake[trim, {n, n}] === "]",
        StringTrim @ StringTake[trim, {2, n - 1}],
        trim
    ]
]

peelLeadingUniversals[body_String] := Block[{trim = StringTrim[body], inner, pos},
    If[ ! StringStartsQ[trim, "!"], Return[trim]];
    inner = StringTrim @ StringDrop[trim, 1];
    If[ ! StringStartsQ[inner, "["], Return[trim]];
    pos = matchingBracketPos[inner, 1, "[", "]"];
    If[ pos === $Failed, Return[trim]];
    (* Pre-register each variable name in $tptpVars so its first
       reference inside the body resolves to a Pattern.  (The lookup
       in readTerm does ensureVar on demand anyway; this pre-walk
       just keeps the binding intent explicit and tolerates clauses
       where a variable appears nowhere in the body.) *)
    Block[{vars = parseQuantVarList @ StringTake[inner, {2, pos - 1}]},
        Scan[ensureVar, vars]
    ];
    inner = StringTrim @ StringDrop[inner, pos];
    If[ ! StringStartsQ[inner, ":"], Return[trim]];
    inner = StringTrim @ StringDrop[inner, 1];
    peelLeadingUniversals @ stripOuterParens[inner]
]

(* Variable list of a quantifier: comma-separated names, possibly
   followed by `:sort` annotations (already stripped by preprocessBody
   in tff mode; tolerate them anyway in case a user supplies raw tff).
   Returns a list of bare upper-case names. *)
parseQuantVarList[s_String] := Block[{parts},
    parts = StringSplit[s, ","];
    Map[
        Function[part,
            StringTrim @ First @ StringSplit[StringTrim[part], ":"]
        ],
        parts
    ]
]

readFofFormula[text_String, i0_Integer] :=
    readBinaryConnective[text, i0]

(* Non-assoc binary: parse lhs, peek for one of <=>, =>, <=, <~>,
   ~|, ~&; if present consume it and recurse on rhs.  Right-
   associative parse (a => b => c) parses as a => (b => c) which
   matches the common reading. *)
readBinaryConnective[text_String, i0_Integer] := Block[
    {i = i0, lhs, op, rhs},
    {lhs, i} = readOr[text, i];
    i = skipWS[text, i];
    op = readBinaryOp[text, i];
    If[ op =!= "",
        i = i + StringLength[op];
        {rhs, i} = readBinaryConnective[text, i];
        lhs = applyBinaryConnective[op, lhs, rhs]
    ];
    {lhs, i}
]

readBinaryOp[text_String, i_Integer] := Block[{len = StringLength[text]},
    Which[
        i + 2 <= len && StringTake[text, {i, i + 2}] === "<=>", "<=>",
        i + 2 <= len && StringTake[text, {i, i + 2}] === "<~>", "<~>",
        i + 1 <= len && StringTake[text, {i, i + 1}] === "=>",  "=>",
        (* <= is implied-by, distinct from <=> already eaten above *)
        i + 1 <= len && StringTake[text, {i, i + 1}] === "<=",  "<=",
        i + 1 <= len && StringTake[text, {i, i + 1}] === "~&",  "~&",
        i + 1 <= len && StringTake[text, {i, i + 1}] === "~|",  "~|",
        True, ""
    ]
]

applyBinaryConnective["<=>", a_, b_] := Equivalent[a, b]
applyBinaryConnective["<~>", a_, b_] := Xor[a, b]
applyBinaryConnective["=>",  a_, b_] := Implies[a, b]
applyBinaryConnective["<=",  a_, b_] := Implies[b, a]
applyBinaryConnective["~&",  a_, b_] := Not[And[a, b]]
applyBinaryConnective["~|",  a_, b_] := Not[Or[a, b]]

(* Left-associative `|`.  `~|` is a non-assoc connective caught at
   readBinaryConnective; readOr only sees plain `|` because the
   `~` was already past the cursor before readOr re-checked. *)
readOr[text_String, i0_Integer] := Block[
    {i = i0, lhs, rhs, kids, len = StringLength[text]},
    {lhs, i} = readAnd[text, i];
    i = skipWS[text, i];
    kids = {lhs};
    While[ i <= len && StringTake[text, {i, i}] === "|",
        i = i + 1;
        {rhs, i} = readAnd[text, i];
        AppendTo[kids, rhs];
        i = skipWS[text, i]
    ];
    {If[Length[kids] === 1, kids[[1]], Or @@ kids], i}
]

(* Left-associative `&`. *)
readAnd[text_String, i0_Integer] := Block[
    {i = i0, lhs, rhs, kids, len = StringLength[text]},
    {lhs, i} = readUnary[text, i];
    i = skipWS[text, i];
    kids = {lhs};
    While[ i <= len && StringTake[text, {i, i}] === "&",
        i = i + 1;
        {rhs, i} = readUnary[text, i];
        AppendTo[kids, rhs];
        i = skipWS[text, i]
    ];
    {If[Length[kids] === 1, kids[[1]], And @@ kids], i}
]

(* Unary: `~` negation, `!`/`?` quantifiers, otherwise atom.  After
   the `!` / `?` opener the spec allows whitespace before `[`, so
   probe the next non-whitespace char rather than the immediate one.
   `jAfter` and `jOpens[` are pre-computed for the quantifier-detection
   conditions; doing the assignment inline inside `&&` would interact
   badly with `Which`'s eager-evaluation pattern. *)
readUnary[text_String, i0_Integer] := Block[
    {i = i0, len = StringLength[text], ch, ch2, jAfter, jOpensBracket,
     sub, body, boundSyms},
    i = skipWS[text, i];
    If[ i > len, Return[{$Failed, i}]];
    ch = StringTake[text, {i, i}];
    ch2 = If[ i + 1 <= len, StringTake[text, {i + 1, i + 1}], ""];
    jAfter = skipWS[text, i + 1];
    jOpensBracket = jAfter <= len &&
        StringTake[text, {jAfter, jAfter}] === "[";
    Which[
        (* Bare `~` (not `~&`/`~|`): unary negation. *)
        ch === "~" && ch2 =!= "&" && ch2 =!= "|",
            i = i + 1;
            {sub, i} = readUnary[text, i];
            {If[ MatchQ[sub, _Equal | _Unequal | True | False],
                negateAtom[sub], Not[sub]], i}
        ,
        ch === "!" && jOpensBracket,
            {body, boundSyms, i} = readQuantBody[text, jAfter, ensureVar];
            (* ForAll has HoldAll, so construct via With to force the
               local boundSyms/body values into the quantifier rather
               than capture the bare symbol names. *)
            {With[{bs = boundSyms, bd = body}, ForAll[bs, bd]], i}
        ,
        ch === "?" && jOpensBracket,
            {body, boundSyms, i} = readQuantBody[text, jAfter, ensureFreshSym];
            (* Same as ForAll: Exists is HoldAll. *)
            {With[{bs = boundSyms, bd = body}, Exists[bs, bd]], i}
        ,
        (* thf `^ [V1, ..., Vn] : body` lambda abstraction.  Bound vars
           are fresh bare Symbols; body parsed with that scope; emit a
           WL `Function[{vars}, body]`.  Function is HoldAll, so use
           `With` to force the local values in (same fix as ForAll /
           Exists). *)
        ch === "^" && jOpensBracket,
            {body, boundSyms, i} = readQuantBody[text, jAfter, ensureFreshSym];
            {With[{bs = boundSyms, bd = body}, Function[bs, bd]], i}
        ,
        True, readAtomicFormula[text, i]
    ]
]

(* Parse the quantifier prefix `[V1, ..., Vn] :` (cursor sits right
   after the `!` / `?` opener), bind each variable via the supplied
   `binder` (ensureVar -> Pattern; ensureFreshSym -> bare Symbol),
   recursively parse the body, restore the variable-scope snapshot,
   and return {body, boundSyms, cursorAfterBody}.

   The snapshot-and-restore on $tptpVars gives correct scoping for
   nested quantifiers (an inner ? doesn't bleed bare-symbol bindings
   back into the surrounding universal scope). *)
readQuantBody[text_String, i0_Integer, binder_] := Block[
    {i = i0, len = StringLength[text], listEnd, varNames, body,
     savedVars, boundSyms},
    i = skipWS[text, i];
    listEnd = matchingBracketPos[text, i, "[", "]"];
    varNames = parseQuantVarList @ StringTake[text, {i + 1, listEnd - 1}];
    i = listEnd + 1;
    i = skipWS[text, i];
    If[ i <= len && StringTake[text, {i, i}] === ":", i = i + 1];
    i = skipWS[text, i];
    savedVars = $tptpVars;
    boundSyms = Map[binder, varNames];
    {body, i} = readUnary[text, i];
    $tptpVars = savedVars;
    {body, boundSyms, i}
]

(* Existential binder: assign a fresh BARE Symbol (not Pattern) for
   the bound name; the body's references resolve to that symbol via
   the standard $tptpVars lookup, and the Exists wrapper carries the
   same symbol in its bound-var list. *)
ensureFreshSym[name_String] := ($tptpVars[name] = Unique["ev"])

(* Find the position of the closing `close` bracket that matches the
   `open` bracket at position i0.  Depth-counted, supports nested
   brackets.  Returns the 1-based position of the matching close, or
   $Failed if none. *)
matchingBracketPos[text_String, i0_Integer, open_String, close_String] :=
    Block[{i = i0, len = StringLength[text], depth = 0, ch},
        While[ i <= len,
            ch = StringTake[text, {i, i}];
            Which[
                ch === open,  depth = depth + 1,
                ch === close, depth = depth - 1;
                    If[ depth === 0, Return[i]]
            ];
            i = i + 1
        ];
        $Failed
    ]

(* Atomic formula: parenthesised subformula, equation, predicate
   term, or $true/$false constant.  In thf mode, `@`-chained
   application appears at this level (`f @ x @ y` = `f[x][y]`,
   left-associative).  The `@` chain is parsed unconditionally; fof
   inputs without `@` aren't affected. *)
readAtomicFormula[text_String, i0_Integer] := Block[
    {i = i0, len = StringLength[text], ch, lhs, rhs, sub, closePos},
    i = skipWS[text, i];
    If[ i > len, Return[{$Failed, i}]];
    ch = StringTake[text, {i, i}];
    If[ ch === "(",
        (* parenthesised subformula -- but it may be the lhs of `=`
           / `!=`, so we don't Return here; just bind lhs + cursor
           and fall through to the equation/@-chain check below. *)
        closePos = matchingBracketPos[text, i, "(", ")"];
        lhs = First @ readFofFormula[
            StringTake[text, {i + 1, closePos - 1}], 1];
        i = closePos + 1
        ,
        (* Read a term (predicate / equation lhs / constant). *)
        {lhs, i} = readTerm[text, i]
    ];
    {lhs, i} = readAtChain[text, i, lhs];
    i = skipWS[text, i];
    (* Look for `=` or `!=` *)
    Which[
        i + 1 <= len && StringTake[text, {i, i + 1}] === "!=",
            i = i + 2;
            {rhs, i} = readTerm[text, i];
            {rhs, i} = readAtChain[text, i, rhs];
            {Unequal[lhs, rhs], i}
        ,
        i <= len && StringTake[text, {i, i}] === "=" &&
                (i + 1 > len || StringTake[text, {i + 1, i + 1}] =!= ">"),
            i = i + 1;
            {rhs, i} = readTerm[text, i];
            {rhs, i} = readAtChain[text, i, rhs];
            {Equal[lhs, rhs], i}
        ,
        (* Bare predicate / constant -- promote $true/$false. *)
        True, {liftConstant[lhs], i}
    ]
]

(* Left-associative `@` application chain.  Given a left-hand atom
   `head` and a cursor positioned after it, loop while the next
   non-WS char is `@`, reading the right-hand atom and curry-applying.

   Special case: a 0-arity constant `"name"[]` on the left collapses
   to the bare String head on the first application, so `f @ x` reads
   as `"f"[x]` rather than the doubly-nested `"f"[][x]`.  This matches
   the thf semantic that `f @ x` is the higher-order application
   `f(x)`. *)
readAtChain[text_String, i0_Integer, head_] := Block[
    {i = i0, len = StringLength[text], h = head, rhs},
    i = skipWS[text, i];
    While[ i <= len && StringTake[text, {i, i}] === "@",
        i = i + 1;
        i = skipWS[text, i];
        {rhs, i} = readTerm[text, i];
        h = If[ MatchQ[h, Blank[String][]], Head[h][rhs], h[rhs]];
        i = skipWS[text, i]
    ];
    {h, i}
]

liftConstant["$true"[]]  := True
liftConstant["$false"[]] := False
liftConstant[x_]         := x

skipWS[text_String, i_Integer] := Block[{j = i, len = StringLength[text]},
    While[ j <= len && StringMatchQ[StringTake[text, {j, j}], Whitespace],
        j = j + 1
    ];
    j
]

(* Split body at top-level (depth-0) occurrences of a single-char
   separator `sep` (must not appear inside parens or brackets).
   Returns a list of trimmed sub-strings.  Single-char separator only --
   adequate for the cnf `|` split (the parser proper handles multi-char
   binary connectives). *)
splitTopLevel[body_String, sep_String] := Block[
    {i = 1, len = StringLength[body], depth = 0, start = 1, parts = {}, ch},
    While[ i <= len,
        ch = StringTake[body, {i, i}];
        Which[
            ch === "(" || ch === "[", depth = depth + 1; i = i + 1,
            ch === ")" || ch === "]", depth = depth - 1; i = i + 1,
            depth === 0 && ch === sep,
                AppendTo[parts, StringTake[body, {start, i - 1}]];
                start = i + 1;
                i = i + 1
            ,
            True, i = i + 1
        ]
    ];
    AppendTo[parts, StringTake[body, {start, len}]];
    StringTrim /@ parts
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
   {lhs, rhs, op} or $Failed.  Walks the string once.  Skips `=` when
   followed by `>` (the implies connective) and skips `<` followed by
   `=` (the implied-by connective). *)
topLevelEqSplit[body_String] := Block[{i = 1, len, depth = 0, ch, ch2,
        eq = 0, neq = 0},
    len = StringLength[body];
    While[ i <= len && eq === 0 && neq === 0,
        ch = StringTake[body, {i, i}];
        ch2 = If[ i + 1 <= len, StringTake[body, {i + 1, i + 1}], ""];
        Which[
            ch === "(" || ch === "[", depth = depth + 1; i = i + 1,
            ch === ")" || ch === "]", depth = depth - 1; i = i + 1,
            depth === 0 && ch === "!" && ch2 === "=",
                neq = i
            ,
            depth === 0 && ch === "=" && ch2 =!= ">",
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
    Block[{i = i0, len, ch, ch2, tok, child, parts, numEnd},
        len = StringLength[text];
        i = skipWS[text, i];
        If[ i > len, Return[{$Failed, i}]];
        ch = StringTake[text, {i, i}];
        ch2 = If[ i + 1 <= len, StringTake[text, {i + 1, i + 1}], ""];
        Which[
            (* '(' subterm ')' *)
            ch === "(",
                {child, i} = readTerm[text, i + 1];
                i = skipPast[text, i, ")"];
                {child, i}
            ,
            (* Single-quoted atom: 'foo bar'.  Contents become the
               String head of a 0-arity compound.  Per TPTP, a
               single-quoted atom is always a constant, never a
               variable -- pass the `quoted -> True` flag so the
               post-token path doesn't try to promote it to a
               Pattern even if the contents happen to start with an
               upper-case letter. *)
            ch === "'",
                {tok, i} = readQuoted[text, i, "'"];
                readTermAfterToken[text, i, tok, True]
            ,
            (* Double-quoted distinct object: "hello world".  TPTP
               semantics: distinct objects are pairwise non-equal by
               built-in axiom.  We preserve the literal quote chars in
               the String head (`"\"hello world\""[]`) so the shape
               round-trips visually and stays distinguishable from a
               plain quoted atom -- downstream code that cares about
               distinct-object semantics can match on the leading `"`
               in the head. *)
            ch === "\"",
                {tok, i} = readQuoted[text, i, "\""];
                {("\"" <> tok <> "\"")[], i}
            ,
            (* Signed / unsigned numeric literal: -?[0-9]+(/[0-9]+)?
               or -?[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?  Returned wrapped
               as `"<numstr>"[]` to match the existing 0-arity-constant
               convention (avoids `Equal[42, 43] -> False` eager-eval). *)
            (ch === "-" && StringMatchQ[ch2, RegularExpression["[0-9]"]]) ||
                    StringMatchQ[ch, RegularExpression["[0-9]"]],
                numEnd = readNumericEnd[text, i];
                tok = StringTake[text, {i, numEnd - 1}];
                {tok[], numEnd}
            ,
            (* Bare identifier / variable / $-defined / unsigned int. *)
            True,
                {tok, i} = readWord[text, i];
                If[ tok === "", Return[{$Failed, i}]];
                readTermAfterToken[text, i, tok, False]
        ]
    ]

(* After reading the leading token, check for an optional `(args)`
   argument list.  Without args, the token is either:
     - a TPTP variable (bare upper-case-prefixed word) -> Pattern via
       $tptpVars
     - a 0-arity constant (everything else, including any quoted atom
       regardless of capitalisation) -> `tok[]` (String-headed empty-
       arg compound, sidesteps Equal's eager evaluation on distinct
       String atoms).
   The `quoted` flag forces the constant branch even for a token that
   looks like an upper-case word, since TPTP's single-quoted atoms
   are always constants. *)
readTermAfterToken[text_String, i0_Integer, tok_String, quoted_] := Block[
    {i = i0, len = StringLength[text], parts},
    i = skipWS[text, i];
    If[ i <= len && StringTake[text, {i, i}] === "(",
        {parts, i} = readArgs[text, i + 1];
        {tok @@ parts, i}
        ,
        If[ ! quoted && tptpVarQ[tok],
            {ensureVar[tok], i},
            {tok[], i}
        ]
    ]
]

(* Read a quoted string starting AT the opening quote `quote`.  Honours
   TPTP-style backslash escapes (`\\` -> `\`, `\'` -> `'`, `\"` -> `"`).
   Returns {contents, cursor-after-closing-quote}. *)
readQuoted[text_String, i0_Integer, quote_String] := Block[
    {i = i0 + 1, len = StringLength[text], ch, sb = ""},
    While[ i <= len && StringTake[text, {i, i}] =!= quote,
        ch = StringTake[text, {i, i}];
        If[ ch === "\\" && i + 1 <= len,
            sb = sb <> StringTake[text, {i + 1, i + 1}];
            i = i + 2,
            sb = sb <> ch;
            i = i + 1
        ]
    ];
    {sb, i + 1}
]

(* Scan a numeric literal starting at i; return the cursor position
   one past the last numeric character.  Recognises:
     -?[0-9]+                        signed/unsigned integer
     -?[0-9]+/[0-9]+                 signed/unsigned rational
     -?[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?  signed/unsigned real *)
readNumericEnd[text_String, i0_Integer] := Block[
    {i = i0, len = StringLength[text], ch},
    If[ i <= len && StringTake[text, {i, i}] === "-", i = i + 1];
    While[ i <= len &&
            StringMatchQ[StringTake[text, {i, i}], RegularExpression["[0-9]"]],
        i = i + 1];
    (* rational `/integer` or real `.integer` *)
    If[ i <= len && StringTake[text, {i, i}] === "/",
        i = i + 1;
        While[ i <= len &&
                StringMatchQ[StringTake[text, {i, i}], RegularExpression["[0-9]"]],
            i = i + 1]
    ];
    If[ i <= len && StringTake[text, {i, i}] === ".",
        i = i + 1;
        While[ i <= len &&
                StringMatchQ[StringTake[text, {i, i}], RegularExpression["[0-9]"]],
            i = i + 1];
        (* exponent *)
        If[ i <= len && StringMatchQ[StringTake[text, {i, i}],
                RegularExpression["[eE]"]],
            i = i + 1;
            If[ i <= len && StringMatchQ[StringTake[text, {i, i}],
                    RegularExpression["[+-]"]],
                i = i + 1
            ];
            While[ i <= len &&
                    StringMatchQ[StringTake[text, {i, i}],
                        RegularExpression["[0-9]"]],
                i = i + 1]
        ]
    ];
    i
]

(* Read a comma-separated argument list starting AFTER the open paren.
   Returns {args-list, position-after-close-paren}.  Reap/Sow builds
   the list -- no AppendTo.

   Anti-loop guard: tracks the cursor at the top of each iteration; if
   nothing advanced we bail out so malformed input (a Boolean op inside
   a term position, etc.) can't wedge the parser. *)
readArgs[text_String, i0_Integer] := Block[
    {i = i0, sown, child, len, prev},
    len = StringLength[text];
    sown = Reap[
        While[ True,
            prev = i;
            {child, i} = readTerm[text, i];
            Sow[child];
            i = skipWS[text, i];
            If[ i > len || StringTake[text, {i, i}] === ")",
                i = i + 1; Break[]];
            If[ StringTake[text, {i, i}] === ",", i = i + 1];
            (* If the iteration didn't advance the cursor, bail rather
               than spin forever. *)
            If[ i === prev, Break[]]
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
(* Per-clause variable cache.  Preserve the TPTP name as the Pattern
   symbol so a clause's parse displays as `"foo"[X_] == X_` rather
   than `"foo"[v$123_] == v$123_`.  Pattern variables are scope-local
   to a rule, so two clauses both referencing `X` don't cross-bind.

   The symbol lives in `Global`` so the rendered axiom reads as the
   plain variable name (`X_`) without context decoration.  Underscores
   in TPTP variable names (rare but legal -- `X_var`) get folded to
   `$` since WL symbol identifiers cannot contain `_`. *)
ensureVar[name_String] := (
    If[ ! KeyExistsQ[$tptpVars, name],
        $tptpVars[name] = Pattern[
            Evaluate @ Symbol["Global`" <> StringReplace[name, "_" -> "$"]],
            Blank[]]
    ];
    $tptpVars[name]
)

(* (Function-symbol heads are kept as bare Strings -- "and"[X, Y]
   instead of a Symbol in a private context.  Strings are not bound
   in any WL context, so they cannot collide with the user's globals
   and don't need a Tptp` namespace dance.) *)

