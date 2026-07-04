(* thvm/atp: SZS derivation -> thvm-shaped ProofObject builder.

   Build a thvm `ProofObject["EquationalLogic", goal, axioms, ds]`
   from a parsed SZS derivation (the Association list returned by
   `Wolfram`Parser`TPTPImport[..., "SZS"]`).  ATP-AGNOSTIC: works
   for any prover that emits SZS-framed fof-with-inference output
   (Vampire, E, iProver, Twee with --tstp, Otter, ...).  The
   per-prover wrappers (TVampireProof, TEProverProof, ...) just
   produce the parsed derivation; this module turns it into the
   canonical WL ProofObject shape.

   With both an internal preset and an external CLI returning the
   same ProofObject shape, downstream code (the proof shape
   comparator, the WL ProofObject UI, the ProofFunction verifier)
   sees a uniform interface.

   Limitations of the produced ProofObject:
   * Statement fields are the parsed SZS formulas as-given: they
     do NOT auto-translate to the WL operator names the original
     theory uses.  For shape comparison this is fine; the
     ProofFunction verifier needs a theory-specific symbol-name
     reverse-translator (a follow-up).
   * Proof-field metadata (Construct/Rule/Orientation/Position) is
     STUBBED: only Construct/MatchingConstruct/Input parent links
     are populated, so the shape comparator sees the right number
     of parents but the verifier path is unsupported.
   * Rule names that aren't in `$SZSRuleToConstruct` fall through
     to "SubstitutionLemma" (a benign default; tweak the table to
     add a prover's idiosyncratic inference). *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"GeneralUtilities`", "Wolfram`Parser`"}]

SetUsage[TSZSDerivationToProofObject, "TSZSDerivationToProofObject[derivation$] builds a thvm-shaped proof Association from a parsed SZS derivation (the list returned by Wolfram`Parser`TPTPImport[$$, \"SZS\"]).
Works for any ATP that emits SZS-framed fof+inference output (Vampire, E, iProver, Twee --tstp, Otter, ...).
The SZS-rule to thvm-construct mapping comes from $SZSRuleToConstruct.
Option ParseFormulas (default False) parses formula bodies into WL expressions instead of keeping raw SZS strings."];

SetUsage[$SZSRuleToConstruct, "$SZSRuleToConstruct is an Association mapping SZS inference-rule names (superposition, forward_demodulation, ...) to thvm ProofObject construct types (CriticalPairLemma, SubstitutionLemma, ...).
Edit this Association to support a prover's idiosyncratic inference rule; unmapped rules fall through to SubstitutionLemma."];

SetUsage[TVampireProofObject, "TVampireProofObject[theory$, thm$] runs the Vampire CLI via TVampireProof and converts the result through TSZSDerivationToProofObject to a thvm-shaped proof Association.
Used as the Method \[Rule] \"VampireProcess\" dispatch target in TFindProof, so proofs from differing Methods (internal preset vs external CLI) can be compared structurally.
Options: TimeConstraint, Mode, Binary, ParseFormulas, LiftToProofObject; see the ATP documentation."];

SetUsage[TWaldmeisterProofObject, "TWaldmeisterProofObject[file$.pr] runs the local Waldmeister binary via TWaldmeisterProof on a .pr problem file and converts the proof protocol through TSZSDerivationToProofObject to a thvm-shaped proof Association.
TWaldmeisterProofObject[theory$, thm$] resolves to a banked .pr file under tools/baselines/wm_pr/ when present, and otherwise GENERATES the .pr at runtime from the theory's symbolic axioms + the NotableTheorem conjecture (byte-identical to the banked-file construction).
Used as the Method \[Rule] \"WaldmeisterProcess\" dispatch target in TFindProof.
Options: TimeConstraint, Binary, MathlinkPath, ParseFormulas, LiftToProofObject; see the ATP documentation."];

SetUsage[TTweeProofObject, "TTweeProofObject[theory$, thm$] runs the Twee CLI via TTweeProof and returns a thvm-shaped proof Association.
Twee's --tstp output is SZS-framed but its proof body is a human-readable equation chain (not TPTP fof inferences), so the lemma-list shape is built directly rather than via TSZSDerivationToProofObject; the dataset is keyed by {\"Axiom\", n$} and {\"Lemma\", n$} only, with no per-step construct-class metadata.
Used as the Method \[Rule] \"TweeProcess\" dispatch target in TFindProof.
Options: TimeConstraint, Binary."];

SetUsage[TEproverProofObject, "TEproverProofObject[theory$, thm$] runs the E prover CLI on the canonical TPTP problem file and lifts the SZS proof into a thvm-shaped proof Association via TSZSDerivationToProofObject.
E's --proof-object --tstp-format emits the same SZS-framed fof+inference DAG as Vampire's --proof tptp, so the lift path is shared.
Options: TimeConstraint, Binary, ParseFormulas, LiftToProofObject; LiftToProofObject wraps the Association into a literal ProofObject head. See the ATP documentation."];

Begin["`Private`"]

(* TPTP-symbol -> WL-operator reverse map.  Mirrors the encoder
   table in tools/vampire/export_all.wls's $opNames (the script
   that generated the TPTP problem files in
   tools/baselines/vampire_tptp/).  Keep in sync with that table.

   For operators NOT in this map, parsed TPTP heads stay as
   String-headed compounds ("op_overtilde"[...], etc.) which the
   shape comparator already handles. *)
$tptpToWlOp = <|
    "and" -> CircleTimes,
    "or" -> CirclePlus,
    "not" -> OverBar,
    "nand_op" -> CircleMinus,
    "fop" -> CircleDot,
    "diamond" -> Diamond,
    "star" -> Star,
    "wedge" -> Wedge,
    "vee" -> Vee,
    "circ" -> SmallCircle,
    (* Active System` heads must be Inactive: Times/Plus/Equivalent are
       Orderless (they canonically sort operands) and And/Or/Not auto-
       flatten / collapse double negation, which would silently destroy
       the prover's literal operand structure on rewrite.  Inactive keeps
       the head identity while holding the structure, matching the
       Inactive[Equal] goal representation the lift verifier already uses. *)
    "mul" -> Inactive[Times],
    "add" -> Inactive[Plus],
    "equiv" -> Inactive[Equivalent],
    "implies" -> Inactive[Implies],
    "lnot" -> Inactive[Not],
    "land" -> Inactive[And],
    "lor" -> Inactive[Or],
    "nand" -> Inactive[Nand],
    "nor" -> Inactive[Nor],
    (* Unicode/diacritic operators emitted as op_<name> by
       tools/baselines/tptp_to_pr.wls (and then sanitized through
       TPTPImport's lowercasing path to opovertilde / opoverbar /
       etc.).  Map both spellings so reverse-encode lands on the
       WL symbol regardless of casing. *)
    "op_overtilde" -> OverTilde,
    "opovertilde" -> OverTilde,
    "op_overbar" -> OverBar,
    "opoverbar" -> OverBar,
    "op_circle" -> SmallCircle,
    "opcircle" -> SmallCircle
|>

(* Walk a parsed TPTP expression and replace String-headed
   compounds with their WL-symbol heads where the map knows them.
   `op_overtilde` etc. (unknown) stay String-headed. *)
reverseEncodeFormula[expr_] := expr //. {
    h_String[args___] /; KeyExistsQ[$tptpToWlOp, h] :> $tptpToWlOp[h][args]
}

(* Parse a single SZS formula-body string into a WL expression by
   wrapping it in a fof(p, axiom, ...).  on the way out, the body
   parses to WL form via TPTPImport's regular (non-SZS) mode; the
   top-level ForAll is stripped by TPTPImport.

   Pre-process: TPTPImport's grammar rejects nullary functor calls
   written `name()` (only `name` is accepted for arity-zero), but
   Vampire/WM SZS output writes skolem constants as `skC1()` etc.
   Strip the trailing empty parens before parsing. *)
parseFormulaBody[body_String] := Block[{cleaned, wrapped, parsed},
    cleaned = StringReplace[body, RegularExpression["([a-zA-Z_][a-zA-Z_0-9]*)\\(\\)"] :> "$1"];
    wrapped = "fof(p, axiom, " <> cleaned <> ").";
    parsed = Quiet @ Check[Wolfram`Parser`TPTPImport[wrapped], $Failed];
    Which[
        AssociationQ[parsed] && Length[parsed["Axioms"]] > 0,
            reverseEncodeFormula @ First @ parsed["Axioms"],
        AssociationQ[parsed] && parsed["Conjecture"] =!= Missing[],
            reverseEncodeFormula @ parsed["Conjecture"],
        True,
            body  (* fall back to raw string; comparator still works *)
    ]
]
parseFormulaBody[other_] := other

(* Batched variant: wrap EVERY body as its own fof clause in ONE string
   and run TPTPImport a single time, mapping reverseEncodeFormula over the
   parsed axioms (TPTPImport preserves clause order).  TPTPImport's EBNF
   parser setup is per-CALL, so one call over N clauses is dramatically
   faster than N calls -- the 274-step WolframAxioms proof dropped from
   minutes (parseFormulaBody per step) to seconds.  Falls back to the
   per-formula path if the batch doesn't round-trip the clause count (a
   parse error in any clause), keeping behaviour identical. *)
(* Fast direct parser: an SZS equation body is just `f(a,b) = c` term
   syntax -- functor calls, atoms, and a single top-level = / != .  The
   full TPTPImport EBNF parser is ~0.3s/formula (minutes on the 274-step
   WolframAxioms proof); this string-transforms each side to WL syntax
   (`f(` -> "f"[ , bare atom -> "a"[] , ) -> ] , names lower-cased exactly
   as TPTPImport folds them) and ToExpression's it, then maps op heads via
   reverseEncodeFormula -- byte-identical output to parseFormulaBody
   (verified) at a fraction of the cost.  Falls back to parseFormulaBody on
   anything that isn't a plain equation or fails ToExpression. *)
fastParseFormula[body_String] := Block[{op, parts, tx, l, r, sep},
    (* Pure-equation fast path only.  Non-equational SZS clauses (Eprover/
       Vampire connectives / quantifiers) share this builder; splitting them
       on `=` would be wrong, so hand those to parseFormulaBody. *)
    If[ StringContainsQ[body, Alternatives["|", "&", "~", "=>", "<=>", "<~>", "![", "?["]],
        Return[parseFormulaBody[body]]];
    (* Waldmeister emits ORIENTED rewrite rules `lhs -> rhs` (tes-rule
       lines) alongside equations `lhs = rhs`.  A rule is an oriented
       equation; parse it to the SAME Equal head as an equation (the
       orientation lives in the reconstructed per-step metadata, not the
       Statement) so it verifies uniformly AND equalSides can read its
       two sides when it is a superposition parent -- else the body stays
       a raw string and CriticalPairLemma reconstruction over it fails.
       Check `->` before `=` (a rule has no `=`). *)
    {op, sep} = Which[
        StringContainsQ[body, "->"], {Equal, "->"},
        StringContainsQ[body, "!="], {Unequal, "!="},
        True, {Equal, "="}];
    parts = StringSplit[body, sep, 2];
    If[ Length[parts] =!= 2, Return[parseFormulaBody[body]]];
    tx[side_] := Block[{s2},
        s2 = StringReplace[StringTrim[side],
            x : RegularExpression["[A-Za-z0-9_]+\\(?"] :>
                If[ StringEndsQ[x, "("],
                    "\"" <> ToLowerCase[StringDrop[x, -1]] <> "\"[",
                    "\"" <> ToLowerCase[x] <> "\"[]"]];
        s2 = StringReplace[s2, ")" -> "]"];
        Quiet @ Check[ToExpression[s2], $Failed]];
    l = tx[parts[[1]]]; r = tx[parts[[2]]];
    If[ l === $Failed || r === $Failed,
        Return[parseFormulaBody[body]]];
    (* A reflexive `x = x` (Waldmeister's tes-final closing step) would
       collapse `Equal[x, x]` to True and lose the statement.  Emit it as
       Inactive[Equal] so it stays inert; holdEqual rewraps it as
       HoldForm[x == x] downstream (matching FindEquationalProof's
       Conclusion). *)
    If[ op === Equal && l === r,
        reverseEncodeFormula[Inactive[Equal][l, r]],
        reverseEncodeFormula[op[l, r]]]
]
fastParseFormula[other_] := other

(* The canonical SZS-rule -> thvm-construct mapping.  Editable as a
   public Association.  Defaults cover the standard saturation-prover
   inference vocabulary; unmapped rules default to SubstitutionLemma
   in `constructTypeOf`. *)
$SZSRuleToConstruct = <|
    (* file = input axiom or negated conjecture; caller's
       assignConstructKeys must distinguish via Role. *)
    "file" -> "Axiom",
    "superposition" -> "CriticalPairLemma",
    "forward_demodulation" -> "SubstitutionLemma",
    "backward_demodulation" -> "SubstitutionLemma",
    "demodulation" -> "SubstitutionLemma",
    "rewrite" -> "SubstitutionLemma",
    "subsumption_resolution" -> "Conclusion",
    "forward_subsumption_resolution" -> "Conclusion",
    "backward_subsumption_resolution" -> "Conclusion",
    "trivial_inequality_removal" -> "Conclusion",
    "equality_resolution" -> "SubstitutionLemma",
    "equality_factoring" -> "SubstitutionLemma",
    "factoring" -> "SubstitutionLemma",
    "resolution" -> "CriticalPairLemma",
    "binary_resolution" -> "CriticalPairLemma",
    "paramodulation" -> "CriticalPairLemma"
|>

constructTypeOf[rule_String] := Lookup[$SZSRuleToConstruct, rule, "SubstitutionLemma"]

(* Inference rules that are PURE BOOKKEEPING: they don't carry a
   semantic step the proof reconstructor should emit.  Examples:
   `orient` (turning an equation into a directed rule, where the rule
   is implicit in any later cp/red that uses it), `reorient_equations`
   (Vampire's parse-time equation-flip), bare axiom renames.

   Drop these from the derivation BEFORE assigning construct keys.
   References to a dropped step transitively resolve to the step's
   own first parent (or the source axiom if there is no parent).
   This is the generalisation of foldReorients in Vampire.wl. *)
$BookkeepingRules = {
    "orient",
    "reorient_equations",
    "equation_copy"  (* WM's `tes-eqn : ... : N` renaming of equation N *)
}

(* Build a name->name alias from every dropped step to its (first)
   parent.  Resolve transitively. *)
foldBookkeeping[derivation_List] := Block[{aliases, resolve},
    aliases = Association @ Cases[
        derivation,
        s_Association /; MemberQ[$BookkeepingRules, s["Rule"]] && ListQ[s["Parents"]] && Length[s["Parents"]] >= 1 :> (s["Name"] -> First[s["Parents"]])
    ];
    resolve[n_] := If[KeyExistsQ[aliases, n], resolve[aliases[n]], n];
    (* Rewrite Parents of non-dropped steps via the alias map. *)
    Map[
        s |-> If[ MemberQ[$BookkeepingRules, s["Rule"]],
            Nothing,
            Append[s, "Parents" -> Map[resolve, Lookup[s, "Parents", {}]]]
        ],
        derivation
    ] /. Nothing -> Sequence[]
]

isNegatedConjectureQ[step_Association] := step["Rule"] === "file" && step["Role"] === "negated_conjecture"

(* Walk derivation in order, assign each step a thvm-shaped key
   {ConstructType, n}, sequential per type.  Returns Association
   from SZS-step-name (f1, f2, ...) to the assigned key. *)
assignConstructKeys[derivation_List] := Block[{nameToKey = <||>, perTypeCount = <||>, key, t},
    Scan[
        step |-> (
            t = If[isNegatedConjectureQ[step], "Hypothesis", constructTypeOf[step["Rule"]]];
            perTypeCount[t] = Lookup[perTypeCount, t, 0] + 1;
            nameToKey[step["Name"]] = {t, perTypeCount[t]}
        ),
        derivation
    ];
    nameToKey
]

(* Build the per-entry Proof-field Association.  Axioms +
   hypotheses get an empty <||>; everything else routes the first
   parent into Construct + the second into either MatchingConstruct
   (for CP-shaped inferences like superposition) or Input (for
   rewrite-shaped inferences like forward_demodulation). *)
proofFieldFor[step_Association, nameToKey_Association] := Block[{parents, parentKeys, t = constructTypeOf[step["Rule"]]},
    parents = Lookup[step, "Parents", {}];
    parentKeys = Map[Lookup[nameToKey, #, Missing["NotFound"]] &, parents];
    Which[
        step["Rule"] === "file" || Length[parentKeys] === 0,
            <||>,
        Length[parentKeys] === 1,
            <|"Construct" -> parentKeys[[1]]|>,
        True,
            If[ t === "SubstitutionLemma" || t === "Conclusion",
                <|"Input" -> parentKeys[[1]], "Construct" -> parentKeys[[2]]|>,
                <|"Construct" -> parentKeys[[1]], "MatchingConstruct" -> parentKeys[[2]]|>
            ]
    ]
]

(* ---- Single-rewrite reconstruction ------------------------------
   Given Input.eq, Construct.eq, Step.eq (all `Inactive[Equal][lhs, rhs]`
   shaped, with Construct's vars as Pattern[v, Blank[]]), find which
   (Side in {1,2}, ConstructSide in {1,2}, Position in Positions(Input[Side]))
   tuple yields Step.eq when Construct is used as a rewrite rule.

   Returns an Association with Side / ConstructSide / Position /
   Orientation / Rule keys, or $Failed if no fit.  Handles only
   SubstitutionLemma + Conclusion (single-rewrite) steps; CriticalPairLemma
   superposition reconstruction is deferred. *)
equalSides[Inactive[Equal][a_, b_]] := {a, b}
equalSides[Equal[a_, b_]] := {a, b}
(* A rewrite-rule statement (Waldmeister tes-rule / a demodulator) is an
   oriented equation: read its two sides so a rule-valued superposition
   parent still reconstructs. *)
equalSides[Rule[a_, b_]] := {a, b}
equalSides[Inactive[Rule][a_, b_]] := {a, b}
equalSides[h_HoldForm] := equalSides[ReleaseHold[h]]
equalSides[_] := $Failed

(* Strip Pattern[v, _] -> v so the matched bindings (decorating
   the LHS) substitute correctly into the RHS. *)
stripPatternHeads[expr_] := expr //. Verbatim[Pattern][v_, _] :> v

(* Hand-construct a Rule that matches the preset's stored shape:
   LHS is Pattern-wrapped (vars rendered as `name_`); RHS uses
   the same Symbol names bare so bound vars substitute correctly. *)
mkRule[lhsBase_, rhsBase_, vars_List : {}] := With[{lh = withVariablePatterns[stripPatternHeads[lhsBase], vars], rh = stripPatternHeads[rhsBase]},
    Rule @@ {lh, rh}
]

(* For Replace: defers RHS evaluation past binding capture. *)
mkRewriteRule[lhsBase_, rhsBase_, vars_List : {}] := With[{lh = withVariablePatterns[stripPatternHeads[lhsBase], vars], rh = stripPatternHeads[rhsBase]},
    RuleDelayed @@ {lh, rh}
]

reconstructSingleRewrite[inputEq_, constructEq_, stepEq_, varSet_List : {}] := Block[{iSides, cSides, sSides, candidates, hit, closureMode, closureTarget},
    iSides = equalSides[inputEq];
    cSides = equalSides[constructEq];
    (* True closure: the closing rewrite makes both sides of Input
       syntactically equal.  The step reads as `True` (Equal collapsed)
       or, once fastParseFormula keeps reflexive equations inert, as a
       held reflexive `HoldForm[t == t]`.  Either way accept candidates
       whose rewritten equation has lhs === rhs (equalSides on a held
       reflexive would ReleaseHold and re-collapse it to True -> $Failed,
       so this must gate BEFORE sSides is taken). *)
    closureMode = stepEq === True || reflexiveStmtQ[stepEq];
    (* When the closing statement is a KNOWN reflexive `HoldForm[t == t]`
       (not a bare True), the rewrite must reproduce exactly `t == t` --
       a plain lhs===rhs test would also accept the other reflexive the
       enumeration reaches first (rewriting Input's OTHER side), yielding
       metadata whose replay disagrees with the stored statement. *)
    closureTarget = Replace[stepEq, {HoldForm[Equal[a_, b_]] /; a === b :> a, _ :> Missing[]}];
    sSides = If[ closureMode, Missing[], equalSides[stepEq]];
    If[ iSides === $Failed || cSides === $Failed || (! closureMode && sSides === $Failed), Return[$Failed]];
    (* Statements are stored with BARE variables in the dataset; we
       need Pattern[v, Blank[]] form for MatchQ to fire correctly. *)
    cSides = withVariablePatterns[#, varSet] & /@ cSides;
    (* Short-circuit search: Throw the first (s,cs,pos) whose rewrite
       reproduces the step -- same first-hit as the old eager
       Flatten[Table]+SelectFirst, but stops instead of MatchQ-ing every
       position over both sides. *)
    hit = Catch[
        Do[
            Block[{rule, lhsPat, allPositions, subterm, newSubterm, newSide, rewrittenEq},
                lhsPat = cSides[[cs]];  (* already Pattern-wrapped earlier *)
                rule = mkRewriteRule[lhsPat, cSides[[3 - cs]], varSet];
                allPositions = Prepend[Position[iSides[[s]], _], {}];
                Do[
                    subterm = If[ pos === {}, iSides[[s]], Extract[iSides[[s]], pos]];
                    If[ MatchQ[subterm, lhsPat],
                        newSubterm = Replace[subterm, rule];
                        newSide = If[ pos === {}, newSubterm, ReplacePart[iSides[[s]], pos -> newSubterm]];
                        rewrittenEq = ReplacePart[iSides, s -> newSide];
                        If[ If[ closureMode,
                                rewrittenEq[[1]] === rewrittenEq[[2]] && (MissingQ[closureTarget] || rewrittenEq[[1]] === closureTarget),
                                (Sort @ rewrittenEq) === (Sort @ sSides)],
                            Throw[{s, cs, pos, rewrittenEq}]]],
                    {pos, allPositions}]],
            {s, 2}, {cs, 2}];
        Missing["NoFit"]];
    If[ MissingQ[hit], $Failed,
        <|
            "Side" -> hit[[1]],
            "ConstructSide" -> hit[[2]],
            "Position" -> hit[[3]],
            "Orientation" -> If[hit[[2]] === 1, 1, -1],
            "Rule" -> mkRule[cSides[[hit[[2]]]], cSides[[3 - hit[[2]]]], varSet]
        |>]
]

(* ---- Superposition (CriticalPairLemma) reconstruction -----------
   A CPL inference superposes two rules: the Construct rule's LHS
   contains a non-variable subterm at some Position, and that
   subterm unifies with the MatchingConstruct rule's LHS.  The
   resulting critical pair is:
     sigma(Construct.RHS)  ==  sigma(Construct.LHS[Position <- MatchingConstruct.RHS])
   where sigma is the most-general unifier.

   Enumerates (Side, Orientation, MatchingSide, MatchingOrientation,
   Position) and runs cplUnify on each candidate.  Returns metadata
   with Side / ConstructSide / Orientation / Subpattern /
   MatchingConstruct (passed through) / MatchingOrientation /
   MatchingSide / Position / Rule / MatchingRule keys, or $Failed. *)
applySubstitution[expr_, sub_Association] := expr //. Normal[sub]

(* Strip Pattern[v, Blank[]] -> v from an expression so we can
   walk it as a plain term-tree for subpattern enumeration. *)
unpatternize[expr_] := expr //. Verbatim[Pattern][v_, _] :> v

(* A position is "non-variable" if the subterm at that position
   isn't a Pattern-headed leaf: superposition rules require the
   matched subterm to be a non-variable position. *)
nonVarPositions[expr_, varSet_List] := Block[{plain = unpatternize[expr], positions},
    (* Heads -> False so the head-slot {0} doesn't appear (rewriting
       at the head of a compound term is undefined for superposition). *)
    positions = Position[plain, _, Heads -> False];
    Select[positions, ! MemberQ[varSet, Extract[plain, #]] &]
]

reconstructSuperposition[constructEq_, matchingEq_, stepEq_, varSet_List] := Block[{cSides, mSides, sSides, candidates, hit, freshen, freshVars, allVars},
    cSides = equalSides[constructEq];
    mSides = equalSides[matchingEq];
    sSides = equalSides[stepEq];
    If[ cSides === $Failed || mSides === $Failed || sSides === $Failed, Return[$Failed]];
    cSides = unpatternize /@ cSides;
    mSides = unpatternize /@ mSides;
    sSides = unpatternize /@ sSides;
    (* Freshen MatchingConstruct's variables so they don't collide
       with Construct's identical-name vars during unification. *)
    freshen = AssociationThread[varSet, Table[Unique["cplR"], {Length[varSet]}]];
    mSides = mSides /. freshen;
    freshVars = Values[freshen];
    allVars = Join[varSet, freshVars];
    (* Two host/applied orientations: the SZS labeling might map
       Construct->host + MatchingConstruct->applied OR the swap.
       Try both: hostSides = either cSides or mSides; appliedSides
       = the other.  Record `swap` so the caller knows which role
       Construct ended up playing. *)
    (* Alpha-equivalence check: a single substitution must
       simultaneously map both sides.  Conjoint unification via
       List[a, b] vs List[c, d]: cplUnify recurses pair-wise
       with one shared sigma, so an inconsistency in one slot fails
       the whole match. *)
    (* Short-circuit search: fuse the candidate enumeration with the
       alpha-check and Throw the FIRST (swap,s1,o1,s2,o2,pos) whose
       critical pair matches the step -- identical first-hit to the old
       eager Flatten[Table]+SelectFirst, but it stops the moment a fit is
       found instead of running cplUnify over EVERY face-combo x position
       (the 145-CPL WolframAxioms builder dropped from minutes to seconds). *)
    Block[{checkPair},
        checkPair[{a_, b_}] := Block[{sub2},
            sub2 = Quiet @ WolframInstitute`THVMLink`ATP`Private`cplUnify[List[a, b], List[sSides[[1]], sSides[[2]]], allVars];
            If[ AssociationQ[sub2], True,
                sub2 = Quiet @ WolframInstitute`THVMLink`ATP`Private`cplUnify[List[a, b], List[sSides[[2]], sSides[[1]]], allVars];
                AssociationQ[sub2]]];
        hit = Catch[
            Do[
                Block[{hostSides, appliedSides, ruleLhs1, ruleRhs1, ruleLhs2, ruleRhs2, positions, sub, cpLhs, cpRhs, subterm, ruleLhs1New},
                    If[ swap === 0,
                        hostSides = cSides; appliedSides = mSides,
                        hostSides = mSides; appliedSides = cSides];
                    ruleLhs1 = If[ o1 === 1, hostSides[[s1]], hostSides[[3 - s1]]];
                    ruleRhs1 = If[ o1 === 1, hostSides[[3 - s1]], hostSides[[s1]]];
                    ruleLhs2 = If[ o2 === 1, appliedSides[[s2]], appliedSides[[3 - s2]]];
                    ruleRhs2 = If[ o2 === 1, appliedSides[[3 - s2]], appliedSides[[s2]]];
                    positions = nonVarPositions[ruleLhs1, allVars];
                    Do[
                        subterm = If[ pos === {}, ruleLhs1, Extract[ruleLhs1, pos]];
                        sub = Quiet @ WolframInstitute`THVMLink`ATP`Private`cplUnify[subterm, ruleLhs2, allVars];
                        If[ AssociationQ[sub],
                            cpLhs = applySubstitution[ruleRhs1, sub];
                            ruleLhs1New = If[ pos === {}, ruleRhs2, ReplacePart[ruleLhs1, pos -> ruleRhs2]];
                            cpRhs = applySubstitution[ruleLhs1New, sub];
                            If[ checkPair[{cpLhs, cpRhs}],
                                Throw[{swap, s1, o1, s2, o2, pos, subterm, {cpLhs, cpRhs}}]]],
                        {pos, positions}]],
                {swap, 0, 1}, {s1, 2}, {o1, {1, -1}}, {s2, 2}, {o2, {1, -1}}];
            Missing["NoFit"]]];
    If[ MissingQ[hit], $Failed,
        Block[{cs, ms, swapHit = hit[[1]], s1 = hit[[2]], o1 = hit[[3]], s2 = hit[[4]], o2 = hit[[5]], pos = hit[[6]], subp = hit[[7]]},
            (* When swap=1, MatchingConstruct played host: the
               Rule comes from mSides + the MatchingRule from cSides. *)
            If[ swapHit === 0,
                cs = cSides; ms = mSides,
                cs = mSides; ms = cSides];
            <|
                "Side" -> s1,
                "Orientation" -> o1,
                "MatchingSide" -> s2,
                "MatchingOrientation" -> o2,
                "Position" -> pos,
                "Subpattern" -> withVariablePatterns[subp, Join[varSet, freshVars]],
                "Rule" -> mkRule[If[o1 === 1, cs[[s1]], cs[[3 - s1]]], If[o1 === 1, cs[[3 - s1]], cs[[s1]]], varSet],
                (* MatchingRule's vars rename back to varSet names
                   (the cplR<n> Unique symbols would otherwise leak
                   into the verifier's pattern machinery and trip
                   internal Unique[] calls). *)
                "MatchingRule" -> Block[{lhsBase = If[o2 === 1, ms[[s2]], ms[[3 - s2]]], rhsBase = If[o2 === 1, ms[[3 - s2]], ms[[s2]]], reverseMap = AssociationThread[freshVars, varSet]},
                    mkRule[lhsBase /. reverseMap, rhsBase /. reverseMap, varSet]]
            |>]]
]

(* Take an already-lifted prfList (Association of {Type, n} ->
   <|Statement, Proof|>) and, for each SubstitutionLemma /
   Conclusion / CriticalPairLemma entry, reconstruct the rewrite
   metadata.  Falls back to leaving the skeleton Proof unchanged
   when reconstruction fails (so the lifted object still works
   for property dispatch). *)
augmentSingleRewriteEntries[prfList_List, varSet_List : {}] := Block[{prfAssoc = Association[prfList], augment},
    augment[key_, entry_Association] := Block[{type = First[key], proof, constructKey, matchingKey, inputKey, constructEntry, matchingEntry, inputEntry, stepEq, recon},
        proof = Lookup[entry, "Proof", <||>];
        stepEq = entry["Statement"];
        Switch[type,
            "SubstitutionLemma" | "Conclusion",
                inputKey = Lookup[proof, "Input", Missing[]];
                constructKey = Lookup[proof, "Construct", Missing[]];
                (* Conclusion can omit Input: the implicit Input is
                   the Hypothesis (the goal being proved). *)
                If[ type === "Conclusion" && MissingQ[inputKey],
                    inputKey = {"Hypothesis", 1};
                    proof = Association[proof, "Input" -> inputKey]];
                If[ MissingQ[inputKey] || MissingQ[constructKey],
                    Return[entry]];
                inputEntry = Lookup[prfAssoc, Key[inputKey], Missing[]];
                constructEntry = Lookup[prfAssoc, Key[constructKey], Missing[]];
                (* If Construct points at a True-closure entry (an
                   SL/Conclusion whose Statement collapsed to True),
                   walk one level back to the actual rewrite rule. *)
                If[ AssociationQ[constructEntry] && constructEntry["Statement"] === True,
                    Block[{innerKey = Lookup[Lookup[constructEntry, "Proof", <||>], "Construct", Missing[]]},
                        If[ ! MissingQ[innerKey],
                            constructKey = innerKey;
                            constructEntry = Lookup[prfAssoc, Key[innerKey], Missing[]];
                            proof = Association[proof, "Construct" -> innerKey]]]];
                If[ MissingQ[inputEntry] || MissingQ[constructEntry],
                    Return[entry]];
                recon = reconstructSingleRewrite[inputEntry["Statement"], constructEntry["Statement"], stepEq, varSet];
                If[ recon === $Failed, Return[entry]];
                Return[ReplacePart[entry,
                    "Proof" -> Association[proof, recon, "Source" -> If[type === "Conclusion", "cpl", "norm"], "InputOrientation" -> 1, "OutputExpression" -> stepEq]]],
            "CriticalPairLemma",
                constructKey = Lookup[proof, "Construct", Missing[]];
                matchingKey = Lookup[proof, "MatchingConstruct", Missing[]];
                If[ MissingQ[constructKey] || MissingQ[matchingKey],
                    Return[entry]];
                constructEntry = Lookup[prfAssoc, Key[constructKey], Missing[]];
                matchingEntry = Lookup[prfAssoc, Key[matchingKey], Missing[]];
                If[ MissingQ[constructEntry] || MissingQ[matchingEntry],
                    Return[entry]];
                recon = reconstructSuperposition[constructEntry["Statement"], matchingEntry["Statement"], stepEq, varSet];
                (* A placeholder rule that doesn't ACTUALLY derive the
                   Statement causes a verifier Failure with "Can't unify
                   <axiom> with <placeholder>".  So writing a fake identity
                   rule is worse than leaving the entry unaugmented: leave
                   it skeleton-only and let the verifier report its own
                   KeyAbsent failure when the enumeration cannot find a
                   valid superposition. *)
                If[ recon === $Failed, Return[entry]];
                Return[ReplacePart[entry, "Proof" -> Association[proof, recon]]]];
        entry
    ];
    KeyValueMap[#1 -> augment[#1, #2] &, prfAssoc]
]

(* ---- CLI -> verifiable ProofObject lift -------------------------
   Take the Association that TSZSDerivationToProofObject returns and
   produce a literal `ProofObject["EquationalLogic", goal, axioms,
   data]` that WL's property machinery accepts (ProofFunction etc.).

   Three transforms compose:
     1. TPTPImport renders TPTP terms as String tokens wrapped in a
        zero-arg call: `"x1"[]` for variables, `"k1"[]` for
        constants.  liftStringLeaves promotes both to Global` Symbols.
     2. Axiom variables must appear as Pattern[name, Blank[]] (the
        `name_` form the AxiomaticTheory schemas expect).
        withVariablePatterns rewrites a chosen set of symbols into
        that pattern form.
     3. arg3 axioms use Inactive[Equal][lhs, rhs]; arg4 Proof entries'
        Statement field uses HoldForm[lhs == rhs].  Different
        wrappers around the same canonicalized equation. *)
liftStringLeaves[expr_] := expr //. {
    h_String[] /; StringLength[h] > 0 :> Symbol["Global`" <> h]
}

collectVarSymbols[expr_] := DeleteDuplicates @ Cases[
    {expr},
    s_Symbol /; (Context[s] === "Global`" && StringMatchQ[SymbolName[s], "x" ~~ DigitCharacter ..]),
    Infinity]

(* `expr /. v -> Pattern[v, Blank[]]` would loop since the RHS
   contains the LHS literally.  Build the substitution map with
   Hold and ReleaseHold once at the end. *)
withVariablePatterns[expr_, vars_List] := ReleaseHold @ Block[{pairs},
    pairs = Map[# -> Hold[Pattern][#, Blank[]] &, vars];
    Hold[expr] /. pairs /. Hold[Pattern] -> Pattern
]

inactivateEqual[Equal[a_, b_]] := Inactive[Equal][a, b]
inactivateEqual[other_] := other

holdEqual[Equal[a_, b_]] := With[{x = a, y = b}, HoldForm[x == y]]
(* A reflexive equation reaches here as Inactive[Equal] (fastParseFormula
   kept it inert so it would not collapse to True); rewrap it as the same
   HoldForm[x == y] a non-reflexive equation gets. *)
holdEqual[Inactive[Equal][a_, b_]] := With[{x = a, y = b}, HoldForm[x == y]]
holdEqual[other_] := other

(* A statement is reflexive (its two sides are identical) whether it is
   a raw True (Equal collapsed), a held HoldForm[a == a], or an inert
   Inactive[Equal][a, a].  Used to fold the degenerate goal-closure
   SubstitutionLemmas -- fastParseFormula now keeps reflexive equations
   inert (as HoldForm[a == a]) so the closing step's statement survives,
   which means the fold can no longer test bare TrueQ. *)
reflexiveStmtQ[True] := True
reflexiveStmtQ[HoldForm[Equal[a_, b_]]] := a === b
reflexiveStmtQ[Inactive[Equal][a_, b_]] := a === b
reflexiveStmtQ[Equal[a_, b_]] := a === b
reflexiveStmtQ[_] := False

liftToProofObject[assoc_Association] := Block[{goalRaw, axiomsRaw, dsRaw, goalLifted, axiomsLifted, varSyms, allSyms, constSyms, axList, prfList},
    goalRaw = assoc["Goal"];
    axiomsRaw = assoc["Axioms"];
    dsRaw = assoc["ProofDataset"];
    goalLifted = liftStringLeaves[goalRaw];
    axiomsLifted = liftStringLeaves /@ axiomsRaw;
    (* Variables come from axioms (the universally-quantified `x`
       names).  The goal's skolem-constants live in constSyms. *)
    varSyms = collectVarSymbols[axiomsLifted];
    allSyms = DeleteDuplicates @ Cases[
        {axiomsLifted, goalLifted},
        s_Symbol /; Context[s] === "Global`",
        Infinity];
    constSyms = Complement[allSyms, varSyms];
    axList = Map[inactivateEqual @ withVariablePatterns[#, varSyms] &, axiomsLifted];
    (* First pass: lift each Statement / preserve skeleton Proof links.
       NOTE: preset stores arg4-Proof Statements with BARE variables
       (HoldForm[CircleTimes[a, b] == ...]) and arg3 axioms with
       Pattern[v, _] (Inactive[Equal][CircleTimes[a_, b_], ...]).  So
       withVariablePatterns is applied to arg3 above but NOT to the
       Statement here. *)
    prfList = KeyValueMap[
        #1 -> Association[
            "Statement" -> Block[{raw = liftStringLeaves @ Lookup[#2, "Statement", Missing[]]},
                If[ raw === True || MissingQ[raw],
                    raw,
                    holdEqual[raw]
                ]],
            "Proof" -> Lookup[#2, "Proof", <||>]] &,
        dsRaw];
    (* Fold a reflexive goal-closure step into the Conclusion BEFORE
       reconstruction.  Waldmeister closes the goal via `hyp -> ... ->
       (t = t)`, emitting the penultimate reflexive rewrite as its own
       tes-goal step and letting the tes-final Conclusion merely cite it.
       FindEquationalProof folds both into ONE Conclusion that performs
       the closing rewrite directly.  When the Conclusion's only parent
       (its Construct) is a reflexive SubstitutionLemma, inherit THAT
       lemma's Input + Construct (the actual closing rewrite: Input = the
       last non-reflexive lemma, Construct = the closing rule), so the
       Conclusion reconstructs against the right rule and the now-orphan
       reflexive lemma drops below. *)
    prfList = With[{prfAssoc = Association[prfList]},
        Module[{reflSLs = Select[Keys[prfAssoc],
                First[#] === "SubstitutionLemma" && reflexiveStmtQ[prfAssoc[#]["Statement"]] &]},
            Map[Function[kv, Block[{k = First[kv], e = Last[kv], cst},
                cst = Lookup[e["Proof"], "Construct", Null];
                If[ First[k] === "Conclusion" && MemberQ[reflSLs, cst],
                    With[{slProof = prfAssoc[cst]["Proof"]},
                        k -> Association[e, "Proof" -> <|
                            "Input" -> Lookup[slProof, "Input", Missing[]],
                            "Construct" -> Lookup[slProof, "Construct", Missing[]]|>]],
                    kv]]],
                prfList]]];
    (* Second pass: for SubstitutionLemma + Conclusion entries (which
       have a single-rewrite shape: Input parent + Construct parent),
       reconstruct the rewrite metadata the verifier needs.  Walks
       prfList for parent lookups (an Association of {Type, n} ->
       <|Statement, Proof|>).  Augments the Proof field in-place;
       leaves Axioms/Hypotheses/CPLs untouched. *)
    prfList = augmentSingleRewriteEntries[prfList, varSyms];
    (* Drop degenerate True-closure SubstitutionLemmas: a SubstitutionLemma
       whose Statement collapsed to True is the final goal-reduction step, which
       FindEquationalProof folds into the Conclusion rather than emitting as a
       separate lemma.  Before removing them, re-point every reference (the
       Conclusion's Construct, or a later lemma's Input/Construct) onto the
       True-lemma's OWN predecessor (its Input, else Construct), chaining through
       consecutive True-closures, so no pointer is left dangling.  Then remove
       the True entries.  Makes Method->"WaldmeisterProcess" match FEQ's proof
       length while keeping the proof DAG well-formed. *)
    Module[{relinkAssoc, trueSLs, predOf, resolve, relink},
        trueSLs = Cases[prfList,
            (k_ -> e_) /; First[k] === "SubstitutionLemma" && reflexiveStmtQ[e["Statement"]] :> k];
        If[ trueSLs =!= {},
            relinkAssoc = Association[prfList];
            predOf[k_] := With[{pr = Lookup[relinkAssoc[k], "Proof", <||>]},
                Lookup[pr, "Input", Lookup[pr, "Construct", k]]];
            resolve[k_] := FixedPoint[If[MemberQ[trueSLs, #], predOf[#], #] &, k, 32];
            relink = (# -> resolve[#]) & /@ trueSLs;
            prfList = DeleteCases[prfList, (k_ -> _) /; MemberQ[trueSLs, k]];
            prfList = prfList /. relink
        ]];
    ProofObject[
        "EquationalLogic",
        inactivateEqual @ withVariablePatterns[goalLifted, varSyms],
        axList,
        <|
            "Variables" -> varSyms,
            "Constants" -> constSyms,
            "Proof" -> prfList
        |>]
]

buildDatasetFromDerivation[derivation_List, parseFormulasQ_:False] := Block[{folded, nameToKey, entries, stmts},
    (* Drop bookkeeping steps (orient, reorient_equations,
       equation_copy) BEFORE construct-key assignment; aliases
       route later references to the step's first real parent. *)
    folded = foldBookkeeping[derivation];
    nameToKey = assignConstructKeys[folded];
    (* Per-formula wrap-and-parse is SLOW (TPTPImport runs the
       full EBNF parser per call, 5s/formula on AbelianGroup
       cases, dominating wall on a multi-step proof).  Default to
       raw String statements; opt-in to parsed-WL mode via
       parseFormulasQ when full ProofObject identity / property
       inspection is needed. *)
    stmts = If[ parseFormulasQ,
        Map[fastParseFormula[#["Formula"]] &, folded],
        Map[#["Formula"] &, folded]];
    entries = MapThread[
        {step, stmt} |-> (nameToKey[step["Name"]] -> <|"Statement" -> stmt, "Proof" -> proofFieldFor[step, nameToKey]|>),
        {folded, stmts}];
    Association @@ entries
]

(* ----------------------------------------------------------------- *)
(* Runtime symbolic -> Waldmeister .pr generator.

   Turns AxiomaticTheory axioms + a NotableTheorem conjecture (or
   explicit ForAll/Equal axioms + a goal) into a Waldmeister .pr
   problem string, so TWaldmeisterProofObject no longer hard-fails on
   a cache-miss against the banked tools/baselines/wm_pr/ files.

   Path: symbolic -> TPTP CNF strings (in-memory, the same encoding
   tools/vampire/export_tptp.wls bakes) -> .pr (the same construction
   tools/baselines/tptp_to_pr.wls bakes).  Composing the two existing
   pipelines in-process keeps the output byte-identical to the banked
   .pr files (verified against tools/baselines/wm_pr/).

   No external process: pure WL string surgery. *)

(* Lower-case TPTP functor names for the standard WL heads
   AxiomaticTheory uses (mirrors export_tptp.wls's $opNames).  Heads
   not listed fall through to "op_" <> ToLowerCase[SymbolName]. *)
$wmOpNames = <|
    CircleTimes -> "and",
    CirclePlus -> "or",
    OverBar -> "not",
    CircleMinus -> "nand_op",
    CircleDot -> "fop",
    Diamond -> "diamond",
    Star -> "star",
    Wedge -> "wedge",
    Vee -> "vee",
    SmallCircle -> "circ",
    Times -> "mul",
    Plus -> "add",
    Equivalent -> "equiv",
    Implies -> "implies",
    Not -> "lnot",
    And -> "land",
    Or -> "lor",
    Nand -> "nand",
    Nor -> "nor"
|>;

wmOpNameForHead[h_] := Lookup[$wmOpNames, h, "op_" <> ToLowerCase[ToString[h, InputForm]]];

(* Camel-case-fold underscore names so WM's lexer accepts them
   (mirrors tptp_to_pr.wls's sanitize): sk_c1 -> skC1, op_centerdot
   -> opCenterdot.  A leading-$ name folds to "dollar"<>rest. *)
wmSanitize[s_String] := Which[
    StringStartsQ[s, "$"], "dollar" <> Capitalize[StringDrop[s, 1]],
    StringContainsQ[s, "_"],
        With[{parts = StringSplit[s, "_"]},
            First[parts] <> StringJoin[Capitalize /@ Rest[parts]]],
    True, s];

(* TPTP-string-level term renderer.  Mirrors export_tptp.wls's
   renderTerm + renderConst.  The AxiomaticTheory formal variables
   (\[FormalA] etc.) are inert symbols (no DownValues), so a plain
   `===` lookup against the var-map and `List @@ expr` recursion are
   safe -- no Hold gymnastics needed. *)
wmLookupVar[expr_, rules_List] := Block[{found = $Failed},
    Do[ If[ expr === First[rules[[i]]], found = Last[rules[[i]]]; Break[]], {i, Length[rules]}];
    found];

wmRenderConst[v_] := Block[{nm},
    nm = ToString[v, InputForm];
    nm = StringReplace[nm, {
        "\\[Formal" ~~ x : LetterCharacter .. ~~ "]" :> "f_" <> ToLowerCase[x],
        "\\[" ~~ x__ ~~ "]" :> "u_" <> ToLowerCase[x]}];
    nm = StringReplace[nm, RegularExpression["[^a-z0-9_]"] -> "_"];
    If[ nm === "" || StringMatchQ[nm, RegularExpression["[0-9].*"]], nm = "k" <> nm];
    nm];

wmRenderTerm[expr_, vmap_List] := Block[{v = wmLookupVar[expr, vmap]},
    Which[
        v =!= $Failed, v,
        AtomQ[expr], wmRenderConst[expr],
        True, wmOpNameForHead[Head[expr]] <> "(" <> StringRiffle[wmRenderTerm[#, vmap] & /@ (List @@ expr), ","] <> ")"]];

(* The bound variables of a ForAll-wrapped axiom (else {}). *)
wmExtractVars[expr_] := If[MatchQ[expr, _ForAll], With[{v = expr[[1]]}, If[MatchQ[v, _List], List @@ v, {v}]], {}];
wmStripForAll[expr_] := If[MatchQ[expr, _ForAll], expr[[2]], expr];

(* X1.. for axiom variables; sk_c1.. (skolem CONSTANTS) for the goal's
   universally-quantified variables. *)
wmVarMap[vars_List] := MapIndexed[#1 -> "X" <> ToString[#2[[1]]] &, vars];
wmSkolemMap[vars_List] := MapIndexed[#1 -> "sk_c" <> ToString[#2[[1]]] &, vars];

(* One axiom -> "lhs = rhs" TPTP body (raw, unsanitized); $Failed if
   not an equation. *)
wmAxBody[expr_] := Block[{body = wmStripForAll[expr], vars = wmExtractVars[expr], vmap},
    vmap = wmVarMap[vars];
    If[ ! MatchQ[body, _Equal | Inactive[Equal][_, _]], Return[$Failed]];
    wmRenderTerm[body[[1]], vmap] <> " = " <> wmRenderTerm[body[[2]], vmap]];

(* The conjecture -> "lhs = rhs" with its universal vars skolemized to
   fresh sk_c constants (the CONCLUSION body). *)
wmGoalBody[expr_] := Block[{body = wmStripForAll[expr], vars = wmExtractVars[expr], vmap},
    vmap = wmSkolemMap[vars];
    If[ ! MatchQ[body, _Equal | Inactive[Equal][_, _] | _Unequal | Inactive[Unequal][_, _]], Return[$Failed]];
    wmRenderTerm[body[[1]], vmap] <> " = " <> wmRenderTerm[body[[2]], vmap]];

(* Walk a rendered TPTP-body string and collect every (name, arity)
   pair (mirrors tptp_to_pr.wls's collectSig): an identifier followed
   by `(` is a functor whose arity = comma-count+1 at depth 1; a bare
   identifier is a nullary constant.  Variables (X<n>) are dropped by
   the caller.  Names are sanitized on the way out. *)
wmCollectSig[term_String] := Module[{out = <||>, n = StringLength[term], pos = 1, c, k, name, ipos, idepth, iargs},
    While[ pos <= n,
        c = StringTake[term, {pos, pos}];
        If[ StringMatchQ[c, LetterCharacter],
            k = pos;
            While[ k <= n && StringMatchQ[StringTake[term, {k, k}], LetterCharacter | DigitCharacter | "_"], k++];
            name = wmSanitize[StringTake[term, {pos, k - 1}]];
            If[ k <= n && StringTake[term, {k, k}] === "(",
                ipos = k + 1; idepth = 1;
                iargs = If[ ipos <= n && StringTake[term, {ipos, ipos}] === ")", 0, 1];
                While[ idepth > 0 && ipos <= n,
                    Switch[ StringTake[term, {ipos, ipos}],
                        "(", idepth++,
                        ")", idepth--,
                        ",", If[ idepth === 1, iargs++]];
                    ipos++];
                If[ ! KeyExistsQ[out, name], out[name] = iargs];
                pos = k + 1,
                If[ ! KeyExistsQ[out, name], out[name] = 0];
                pos = k],
            pos++]];
    out];

(* Sanitize a rendered TPTP body for .pr emit (drop underscores from
   every identifier, preserving arity since parens are untouched). *)
wmSanitizeBody[term_String] := StringReplace[term, id : (LetterCharacter ~~ (LetterCharacter | DigitCharacter | "_") ...) :> wmSanitize[id]];

wmCollectVars[term_String] := DeleteDuplicates @ StringCases[term, RegularExpression["\\bX\\d+\\b"]];

(* Assemble the full .pr text from rendered (unsanitized) TPTP bodies.
   nameRaw is the problem NAME (e.g. "WolframAxioms__Commutativity"),
   sanitized for the NAME field.  Construction is byte-identical to
   tools/baselines/tptp_to_pr.wls. *)
wmBuildPr[nameRaw_String, axBodies_List, goalBody_String] := Module[{allTerms, sigByName, sig, vars, precList, sigBlock, varBlock, precBlock, weightBlock, eqBlock, conclusion},
    allTerms = Join[axBodies, {goalBody}];
    (* split each "lhs = rhs" body into its two term-strings for sig/var
       collection (the bodies are unsanitized TPTP, " = " separated). *)
    allTerms = Flatten[StringTrim /@ StringSplit[#, " = ", 2] & /@ allTerms];
    sigByName = Merge[wmCollectSig /@ allTerms, First];
    sigByName = KeySelect[sigByName, ! StringMatchQ[#, "X" ~~ DigitCharacter ..] &];
    sig = Sort @ Normal @ sigByName;
    vars = DeleteDuplicates @ Sort @ Flatten[wmCollectVars /@ allTerms];
    (* precedence: arity ascending, then alphabetical -- skolem
       constants (arity 0) first (highest), operators last. *)
    precList = First /@ SortBy[sig, {Last, First}];
    sigBlock = StringRiffle[
        Replace[#, (nm_ -> a_Integer) :> nm <> ": " <> If[ a > 0, StringJoin @@ ConstantArray["ANY ", a], ""] <> "-> ANY"] & /@ sig,
        "\n              "];
    varBlock = StringRiffle[vars, ", "] <> ": ANY";
    precBlock = StringRiffle[precList, " > "];
    weightBlock = StringRiffle[Replace[#, (nm_ -> _Integer) :> nm <> "=1"] & /@ sig, ", "];
    eqBlock = StringRiffle[wmSanitizeBody /@ axBodies, "\n              "];
    conclusion = wmSanitizeBody[goalBody];
    StringJoin[
        "NAME          ", wmSanitize[nameRaw], "\n",
        "MODE          PROOF\n",
        "SORTS         ANY\n",
        "SIGNATURE     ", sigBlock, "\n",
        "ORDERING      KBO\n",
        "              ", weightBlock, "\n",
        "              ", precBlock, "\n",
        "VARIABLES     ", varBlock, "\n",
        "EQUATIONS     ", eqBlock, "\n",
        "CONCLUSION    ", conclusion, "\n"]];

(* Top-level: (axioms, conjecture) -> .pr string.  axioms is a list of
   ForAll/Equal forms (as AxiomaticTheory[theory] returns), conjecture
   a single ForAll/Equal/Unequal.  nameRaw labels the problem.  The
   AxiomaticTheory formal variables (\[FormalA] etc.) are inert symbols,
   so no Hold is needed: wmRenderTerm's own HoldFirst protects subterm
   capture during the walk. *)
wmGenerateProblem[nameRaw_String, axioms_List, conjecture_] := Block[{axBodies, goalBody},
    axBodies = DeleteCases[wmAxBody /@ axioms, $Failed];
    goalBody = wmGoalBody[conjecture];
    If[ axBodies === {} || goalBody === $Failed,
        Return[Failure["WmGenerate", <|"Reason" -> "no equational axioms or non-equational goal", "Name" -> nameRaw|>]]];
    wmBuildPr[nameRaw, axBodies, goalBody]];

(* (theory, thm) -> .pr string, resolving axioms + the NotableTheorem
   the SAME way TFindProof's atpProveFromTheory does: axioms from
   AxiomaticTheory[theory], the conjecture from
   AxiomaticTheory[theory, "NotableTheorems"][thm].  A multi-conjunct
   theorem keys each conjunct under a trailing "__c<i>" name (mirrors
   the banked-file convention); takeConjunct picks one (1-based, the
   default 1).  Returns the .pr string or a Failure. *)
wmGenerateProblemFor[theory_String, thm_String, takeConjunct_Integer : 1] := Block[{axioms, nt, cjRaw, conjuncts, idx, nm},
    axioms = AxiomaticTheory[theory];
    If[ ! ListQ[axioms],
        Return[Failure["WmGenerate", <|"Reason" -> "AxiomaticTheory[\"" <> theory <> "\"] did not resolve to an axiom list"|>]]];
    nt = AxiomaticTheory[theory, "NotableTheorems"];
    cjRaw = If[ AssociationQ[nt], Lookup[nt, thm, Missing[]], Missing[]];
    If[ MissingQ[cjRaw],
        Return[Failure["WmGenerate", <|"Reason" -> "theorem \"" <> thm <> "\" not in AxiomaticTheory[\"" <> theory <> "\", \"NotableTheorems\"]"|>]]];
    conjuncts = If[ MatchQ[cjRaw, _List], cjRaw, {cjRaw}];
    idx = Clip[takeConjunct, {1, Length[conjuncts]}];
    nm = theory <> "__" <> thm <> If[ Length[conjuncts] > 1, "__c" <> ToString[idx], ""];
    wmGenerateProblem[nm, axioms, conjuncts[[idx]]]];

(* ----------------------------------------------------------------- *)

End[]

Options[TSZSDerivationToProofObject] = {"ParseFormulas" -> False}

(* Public: build a thvm-shaped proof Association from a parsed SZS
   derivation list.  ATP-agnostic, works for Vampire's
   `--proof tptp`, E's `--proof-object`, Twee's `--tstp`, iProver,
   Otter, anyone that emits SZS-framed fof+inference output.

   Returns an Association with the same keys the internal
   ProofObject exposes via property access, so a comparator can
   dispatch on shape without caring whether the input came from
   the internal saturator or an external CLI.

   Shape:
       <|
         "Backend"     -> "SZS",            (* tag for routing *)
         "Status"      -> "Proved",
         "Goal"        -> the negated_conjecture formula,
         "Axioms"      -> {axiom_formula, ...},
         "ProofDataset"-> Association: {Type, n} -> <|Statement, Proof|>,
         "ProofLength" -> #non-axiom inferences,
         "RuleHistogram" -> KeySort @ Counts[rule names]
       |>

   NOT a literal `ProofObject[...]`: WL's ProofObject auto-validates
   axioms against its theory schemas and downgrades to
   FindEquationalProof when the formulas are raw SZS String-headed
   text (TPTPImport's SZS mode does NOT parse formulas into WL
   expressions, only the outer wrapper).  A future parser pass over
   the formula bodies + theory-specific symbol-name reverse
   translation can lift this back into a real ProofObject. *)
TSZSDerivationToProofObject[derivation_List, opts : OptionsPattern[]] := Block[{ds, axiomEntries, hypothesisEntries, axioms, goal, hist, parseFlag = TrueQ @ OptionValue["ParseFormulas"]},
    ds = WolframInstitute`THVMLink`ATP`Private`buildDatasetFromDerivation[derivation, parseFlag];
    axiomEntries = KeySelect[ds, MatchQ[#, {"Axiom", _}] &];
    hypothesisEntries = KeySelect[ds, MatchQ[#, {"Hypothesis", _}] &];
    axioms = Values @ axiomEntries /. e_Association :> e["Statement"];
    goal = If[
        Length[hypothesisEntries] > 0,
        First[Values @ hypothesisEntries]["Statement"],
        Missing["NoHypothesis"]
    ];
    hist = KeySort @ Counts[Cases[
        derivation,
        s_Association /; s["Rule"] =!= "file" :> s["Rule"]
    ]];
    <|
        "Backend" -> "SZS",
        "Status" -> "Proved",
        "Goal" -> goal,
        "Axioms" -> axioms,
        "ProofDataset" -> ds,
        "ProofLength" -> Length[derivation] - Length[axiomEntries],
        "RuleHistogram" -> hist
    |>
]

(* Hand-enumerated: Vampire.wl loads AFTER ProcessProofObject.wl in
   the alphabetical autoloader order, so `Options[TVampireProof]`
   evaluates to {} at file-load time.  Keep this list in sync with
   Options[TVampireProof] in Vampire.wl + the "ParseFormulas" toggle. *)
Options[TVampireProofObject] = {
    TimeConstraint -> 30,
    "Mode" -> "casc",
    "Binary" -> Automatic,
    "ParseFormulas" -> False,
    "LiftToProofObject" -> True
}

(* E-prover wrapper: chain TEproverProof + the generic SZS-to-
   ProofObject builder.  E's --proof-object --tstp-format emits
   the same SZS-framed fof+inference DAG Vampire does, so the
   downstream path is shared. *)
Options[TEproverProofObject] = {
    TimeConstraint -> 30,
    "Binary" -> Automatic,
    "ParseFormulas" -> False,
    "LiftToProofObject" -> True
}

TEproverProofObject[theory_String, thm_String, opts : OptionsPattern[]] := Block[{epR = TEproverProof[theory, thm, FilterRules[{opts}, Options[TEproverProof]]], liftQ = TrueQ @ OptionValue["LiftToProofObject"], parseOpt, assoc},
    parseOpt = "ParseFormulas" -> (liftQ || TrueQ @ OptionValue["ParseFormulas"]);
    If[ epR["Status"] =!= "Proved",
        Failure["ExternalNoProof", <|
            "Tool" -> "Eprover",
            "Status" -> epR["Status"],
            "Seconds" -> epR["Seconds"],
            "Strategy" -> epR["Strategy"]
        |>],
        assoc = TSZSDerivationToProofObject[epR["Inferences"], parseOpt];
        If[ liftQ,
            WolframInstitute`THVMLink`ATP`Private`liftToProofObject[assoc],
            assoc
        ]
    ]
]

(* Vampire-specific wrapper: chain TVampireProof + the generic
   SZS-to-ProofObject builder. *)
TVampireProofObject[theory_String, thm_String, opts : OptionsPattern[]] := Block[{vampR = TVampireProof[theory, thm, FilterRules[{opts}, Options[TVampireProof]]], liftQ = TrueQ @ OptionValue["LiftToProofObject"], parseOpt, assoc},
    parseOpt = "ParseFormulas" -> (liftQ || TrueQ @ OptionValue["ParseFormulas"]);
    If[ vampR["Status"] =!= "Proved",
        Failure["ExternalNoProof", <|
            "Tool" -> "Vampire",
            "Status" -> vampR["Status"],
            "Seconds" -> vampR["Seconds"],
            "Strategy" -> vampR["Strategy"]
        |>],
        assoc = TSZSDerivationToProofObject[vampR["Inferences"], parseOpt];
        If[ liftQ,
            WolframInstitute`THVMLink`ATP`Private`liftToProofObject[assoc],
            assoc
        ]
    ]
]

(* Waldmeister wrapper: chain TWaldmeisterProof + the generic
   SZS-shaped builder.  WM's proof protocol parses into the same
   inference-record Association shape as Vampire's SZS, so the
   exact same TSZSDerivationToProofObject path lifts it.  The
   (Theory, thm) form resolves a banked .pr under
   tools/baselines/wm_pr/ when present (byte-stable fast path) and
   otherwise GENERATES the .pr at runtime via wmGenerateProblemFor. *)
Options[TWaldmeisterProofObject] = {
    TimeConstraint -> 30,
    "WMCLI" -> Automatic,
    "Binary" -> Automatic,
    "MathlinkPath" -> Automatic,
    "ParseFormulas" -> False,
    (* LiftToProofObject -> True forces ParseFormulas and rewraps
       the resulting Association into a literal 4-arg ProofObject
       so the WL property machinery (ProofFunction, ProofGraph,
       Theorems) dispatches.  pf[Theorems] still fails verification
       because the Proof entries lack the rewrite metadata
       (Orientation, Rule, Side, Position) the verifier needs;
       extracting those from the SZS DAG is unsupported. *)
    "LiftToProofObject" -> True
}

(* Two-arg (Theory, thm) form: resolve to a banked .pr file under
   tools/baselines/wm_pr/ when present (byte-stable fast path); on
   cache-miss, GENERATE the .pr at runtime from the symbolic axioms +
   conjecture (wmGenerateProblemFor) to a temp file and proceed.  Never
   hard-fails on a missing banked file. *)
TWaldmeisterProofObject[theory_String, thm_String, opts : OptionsPattern[]] /; FileExtension[theory] =!= "pr" := Block[{path = FileNameJoin[{Directory[], "tools", "baselines", "wm_pr", theory <> "__" <> thm <> ".pr"}], pr, tmp},
    If[ FileExistsQ[path],
        TWaldmeisterProofObject[path, opts],
        (* cache-miss: generate the .pr from the symbolic theory. *)
        pr = WolframInstitute`THVMLink`ATP`Private`wmGenerateProblemFor[theory, thm];
        If[ FailureQ[pr], Return[pr]];
        tmp = FileNameJoin[{$TemporaryDirectory, WolframInstitute`THVMLink`ATP`Private`wmSanitize[theory <> "__" <> thm] <> ".pr"}];
        Export[tmp, pr, "Text"];
        TWaldmeisterProofObject[tmp, opts]
    ]
]

TWaldmeisterProofObject[problemFile_String, opts : OptionsPattern[]] /; FileExtension[problemFile] === "pr" := Block[{wmR = TWaldmeisterProof[problemFile, FilterRules[{opts}, {TimeConstraint, "WMCLI", "Binary", "MathlinkPath"}]], liftQ = TrueQ @ OptionValue["LiftToProofObject"], parseOpt, assoc},
    (* LiftToProofObject implies ParseFormulas: the lift needs
       WL-parsed formula bodies to walk. *)
    parseOpt = "ParseFormulas" -> (liftQ || TrueQ @ OptionValue["ParseFormulas"]);
    If[ wmR["Status"] =!= "Proved",
        Failure["ExternalNoProof", <|
            "Tool" -> "Waldmeister",
            "Status" -> wmR["Status"],
            "Seconds" -> wmR["Seconds"]
        |>],
        assoc = TSZSDerivationToProofObject[wmR["Inferences"], parseOpt];
        If[ liftQ,
            WolframInstitute`THVMLink`ATP`Private`liftToProofObject[assoc],
            assoc
        ]
    ]
]

(* Twee wrapper: Twee's --tstp proof body is not TPTP fof, so we
   build a coarser dataset (Axioms + Lemmas with no inference
   metadata) that the shape comparator still reads. *)
Options[TTweeProofObject] = {
    TimeConstraint -> 30,
    "Binary" -> Automatic
}

TTweeProofObject[theory_String, thm_String, opts : OptionsPattern[]] := Block[{tR = TTweeProof[theory, thm, FilterRules[{opts}, {TimeConstraint, "Binary"}]], ds, axs, lems},
    If[ tR["Status"] =!= "Proved",
        Failure["ExternalNoProof", <|
            "Tool" -> "Twee",
            "Status" -> tR["Status"],
            "Seconds" -> tR["Seconds"]
        |>],
        axs = tR["Axioms"];
        lems = tR["Lemmas"];
        ds = Association @@ Join[
            Table[
                {"Axiom", i} -> <|"Statement" -> axs[[i]]["Statement"], "Proof" -> <||>|>,
                {i, Length[axs]}
            ],
            Table[
                {"Lemma", i} -> <|"Statement" -> lems[[i]]["Statement"], "Proof" -> <||>|>,
                {i, Length[lems]}
            ]
        ];
        <|
            "Backend" -> "Twee-TSTP",
            "Status" -> "Proved",
            "Goal" -> Missing["NotEmittedByTwee"],
            "Axioms" -> #["Statement"] & /@ axs,
            "ProofDataset" -> ds,
            "ProofLength" -> tR["ProofLength"],
            "RuleHistogram" -> <|"twee-rewrite" -> Length[lems]|>
        |>
    ]
]

EndPackage[]
