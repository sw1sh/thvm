(* TPTPImport.wl - parse TPTP .p files into the
   <|"Axioms" -> {phi1, phi2, ...}, "Conjecture" -> phi|> shape
   TFindProof / TFindProofSMT consume.

   Subset handled:
     - cnf(name, role, formula).  formula is a disjunction of
       literals, each literal an equation `lhs = rhs`, a
       disequation `lhs != rhs`, a predicate atom `p(args)`, or a
       negated atom `~ atom`.  Single equational literals come back
       as bare `Equal[l, r]` / `Unequal[l, r]` (the UEQ fragment
       the saturator targets); multi-literal clauses come back as
       `Or[lit1, lit2, ...]`.
     - fof(name, role, formula).  formula is a full first-order
       Boolean combination: `&`, `|`, `~`, `=>`, `<=`, `<=>`,
       `<~>`, `~&`, `~|`, parenthesised subformulas, equational
       atoms, predicate atoms, `$true` / `$false`, and prefix
       quantifiers `! [V1, ..., Vn] :` (universal) / `? [V1, ...,
       Vn] :` (existential).  Leading `!` is stripped (universal
       binding is the cnf default and the saturator's input
       convention); inner quantifiers stay as WL `ForAll` /
       `Exists` so the SMT/DPLL(T) path sees the full structure.
     - tff(name, role, formula).  Same grammar as fof with sort
       annotations (`X:srt`) stripped (thvm's surface is
       homogeneous untyped).  `tff(_, type, ...)` signature
       declarations are silently skipped (no semantic content for
       the untyped saturator).
     - tcf(name, role, formula).  Typed cnf cousin of tff;
       annotations stripped, body parsed as cnf.
     - include('path').  The included file is resolved relative to
       the current file's directory, then $TPTP / $TPTP/Problems
       env-var roots as a fallback, then recursively imported; its
       clauses splice into the enclosing scan.

   Skipped with a console warning: thf (higher-order), ncf
   (non-classical), tpi (process instructions).

   Variables are clause-scoped: each Uppercase identifier inside
   one cnf/fof clause names the same universally-bound variable;
   the same name in a later clause is independent.  Each occurrence
   gets a fresh `Pattern[Unique[]]` so subsequent formulas share
   no bound variables.  Variables under an `Exists` are skolemized
   to fresh `Unique[]` symbols (no Pattern wrapper). *)

BeginPackage["THVMLink`ATP`", {"THVMLink`"}];

TPTPImport::usage =
    "TPTPImport[File[\"file.p\"]] | TPTPImport[\"... source ...\"] " <>
    "returns <|\"Axioms\" -> {...}, \"Conjecture\" -> ...|>.  Function " <>
    "symbols come back as String-headed terms (\"and\"[X, Y] etc.) so " <>
    "they cannot collide with user-level WL symbols.  Handles cnf, " <>
    "fof, tff, tcf clause heads (including multi-literal cnf " <>
    "disjunctions and the full fof Boolean grammar) plus include " <>
    "directives.";

TPTPImport::badrole = "Skipping clause with unsupported role `1`.";
TPTPImport::badfmla = "Could not parse formula `1`.";
TPTPImport::skipnoncnf =
    "Skipping unsupported `1` directive at offset `2` (thf / ncf / tpi " <>
    "clause heads are out of scope; see github.com/TPTPWorld/SyntaxBNF " <>
    "for the full grammar).";
TPTPImport::badinclude =
    "Could not resolve include path `1` (searched relative to `2` and " <>
    "the $TPTP / $TPTP/Problems env-var roots).";

Begin["`Private`"];

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
                clauseHeadMatchQ[text, i, "include"],
                    i = consumeInclude[text, i + 8, baseDir]
                ,
                clauseHeadMatchQ[text, i, "thf"],
                    Message[TPTPImport::skipnoncnf, "thf", i];
                    i = skipParenthesised[text, i + 4]
                ,
                clauseHeadMatchQ[text, i, "ncf"],
                    Message[TPTPImport::skipnoncnf, "ncf", i];
                    i = skipParenthesised[text, i + 4]
                ,
                clauseHeadMatchQ[text, i, "tpi"],
                    i = skipParenthesised[text, i + 4]
                ,
                True, i = i + 1
            ]
        ]
    ]

(* Reads the cnf(...) / fof(...) / tff(...) / tcf(...) tail:
   name, role, body.  Body is Sown verbatim along with the clause
   head; clauseToFormula dispatches on the head.

   For tff/tcf, a `type` role is a signature declaration with no
   semantic content for the untyped saturator -- skip it silently
   without emitting a {head, role, body} tuple. *)
consumeAnnotated[text_String, i0_Integer, head_String] :=
    Block[{i = i0, dummy, role, body, bodyEnd},
        {dummy, i} = readWord[text, i];   (* clause name; discard *)
        i = skipPast[text, i, ","];
        {role, i} = readWord[text, i];
        i = skipPast[text, i, ","];
        {body, bodyEnd} = readBalanced[text, i];
        If[ ! ((head === "tff" || head === "tcf") && role === "type"),
            Sow[{head, role, StringTrim[body]}]
        ];
        skipPast[text, bodyEnd, "."]
    ]

(* Read a single-quoted include path starting AFTER the open paren of
   `include(`.  Resolve it relative to baseDir / $TPTP env-var roots,
   recursively scan the included file, and splice its clauses into
   the enclosing Reap.  On unresolvable path, warn + skip. *)
consumeInclude[text_String, i0_Integer, baseDir_String] :=
    Block[{i = i0, len, start, quoted, resolved, subDir, subText, subClauses},
        len = StringLength[text];
        While[ i <= len &&
                StringMatchQ[StringTake[text, {i, i}], Whitespace],
            i = i + 1
        ];
        (* TPTP single-quoted string; scan to the next unescaped `'`. *)
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
            Scan[Sow, subClauses]
        ];
        (* skip the optional `, [selection]` clause-selector and the
           closing `).` *)
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

clauseToFormula[{head_String, role_String, body_String}] :=
    Block[{$tptpVars = <||>, prepared, fmla},
        prepared = preprocessBody[body, head];
        fmla = If[ head === "cnf" || head === "tcf",
            parseCnfFormula[prepared],
            parseFofFormula[prepared]
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
    (* Strip leading universal quantifier(s) -- iterate so
       `! [X] : ! [Y] : phi` collapses to `phi` with both vars
       bound as Patterns via $tptpVars references in atoms. *)
    peeled = peelLeadingUniversals[trimmed];
    {res, pos} = readFofFormula[peeled, 1];
    res
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
   term, or $true/$false constant. *)
readAtomicFormula[text_String, i0_Integer] := Block[
    {i = i0, len = StringLength[text], ch, lhs, rhs, sub, op, opLen},
    i = skipWS[text, i];
    If[ i > len, Return[{$Failed, i}]];
    ch = StringTake[text, {i, i}];
    If[ ch === "(",
        (* parenthesised subformula *)
        Block[{closePos = matchingBracketPos[text, i, "(", ")"]},
            sub = First @ readFofFormula[
                StringTake[text, {i + 1, closePos - 1}], 1];
            Return[{sub, closePos + 1}]
        ]
    ];
    (* Read a term (predicate / equation lhs / constant). *)
    {lhs, i} = readTerm[text, i];
    i = skipWS[text, i];
    (* Look for `=` or `!=` *)
    Which[
        i + 1 <= len && StringTake[text, {i, i + 1}] === "!=",
            i = i + 2;
            {rhs, i} = readTerm[text, i];
            {Unequal[lhs, rhs], i}
        ,
        i <= len && StringTake[text, {i, i}] === "=" &&
                (i + 1 > len || StringTake[text, {i + 1, i + 1}] =!= ">"),
            i = i + 1;
            {rhs, i} = readTerm[text, i];
            {Equal[lhs, rhs], i}
        ,
        (* Bare predicate / constant -- promote $true/$false. *)
        True, {liftConstant[lhs], i}
    ]
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
