#!/usr/bin/env wolframscript
(* export_tptp.wls - dump AxiomaticTheory axioms + a NotableTheorem
   conjecture to TPTP CNF format for Vampire.

   Usage: wolframscript tools/vampire/export_tptp.wls <Theory> <Theorem> <OutFile>

   Variables -> upper-case TPTP variables (X1, X2, ...).
   Operators -> lower-case TPTP functors.
   The conjecture is converted to negated_conjecture by Skolemizing
   its universal variables to fresh constants (sk_c1, sk_c2, ...) and
   emitting an inequality (lhs != rhs).
*)

If[ Length[$ScriptCommandLine] < 4,
    Print["usage: export_tptp.wls <Theory> <Theorem> <OutFile>\n"];
    Exit[2]];

theoryName  = $ScriptCommandLine[[2]];
theoremName = $ScriptCommandLine[[3]];
outFile     = $ScriptCommandLine[[4]];

(* Lower-case functor names for the standard WL heads
   AxiomaticTheory uses.  Anything not listed gets `ToLowerCase` of
   its SymbolName as a fallback. *)
$opNames = <|
    CircleTimes -> "and",      (* WL ⊗ *)
    CirclePlus  -> "or",       (* WL ⊕ *)
    OverBar     -> "not",
    CircleMinus -> "nand_op",
    CircleDot   -> "fop",
    Diamond     -> "diamond",
    Star        -> "star",
    Wedge       -> "wedge",
    Vee         -> "vee",
    SmallCircle -> "circ",
    Times       -> "mul",
    Plus        -> "add",
    Equivalent  -> "equiv",
    Implies     -> "implies",
    Not         -> "lnot",
    And         -> "land",
    Or          -> "lor",
    Nand        -> "nand",
    Nor         -> "nor"
|>;

(* Combinator-like uppercase formal symbols (S, K, I, B, C, W, ...)
   used by CombinatorAxioms.  Anything else -- treated as a generic
   constant via a lookup table built lazily. *)
$constSeen   = <||>;
$nextConstId = 0;

opNameForHead[h_] := Lookup[$opNames, h,
    Block[{n = ToString[h, InputForm]},
        StringJoin["op_", ToLowerCase[n]]
    ]];

(* TPTP variable map.  Built per equation by indexing into the bound
   variable list and recording each formal symbol by Hold-identity. *)

(* Build maps using Function[..., HoldFirst] so the formal variables
   (\[FormalA], etc.) are NOT evaluated by Set/Rule before we capture
   them as the LHS of the replacement rule. *)
makeVarMap[vars_List] := MapIndexed[
    Function[{v, i}, v -> "X" <> ToString[i[[1]]], HoldFirst],
    vars
];

makeSkolemMap[vars_List] := MapIndexed[
    Function[{v, i}, v -> "sk_c" <> ToString[i[[1]]], HoldFirst],
    vars
];

(* Top-level-only lookup: returns the RHS if expr `===` one of the
   rule LHSes (no recursive ReplaceAll), else $Failed. *)
lookupHold[expr_, rules_List] := Block[{found = $Failed},
    Do[ If[ expr === First[rules[[i]]],
            found = Last[rules[[i]]]; Break[]],
        {i, Length[rules]}];
    found
];

renderConst[v_] := Block[{key = HoldPattern[v], existing, id, nm},
    existing = Lookup[$constSeen, key, None];
    If[ existing =!= None, Return[existing]];
    nm = ToString[Unevaluated[v], InputForm];
    (* Drop \[Formal...] wrapper if present. *)
    nm = StringReplace[nm, {
        "\\[Formal" ~~ x : LetterCharacter .. ~~ "]" :>
            "f_" <> ToLowerCase[x],
        "\\[" ~~ x__ ~~ "]" :> "u_" <> ToLowerCase[x]
    }];
    nm = StringReplace[nm, RegularExpression["[^a-z0-9_]"] -> "_"];
    If[ nm === "" || StringMatchQ[nm, RegularExpression["[0-9].*"]],
        $nextConstId += 1;
        nm = "k" <> ToString[$nextConstId]];
    $constSeen[key] = nm;
    nm
];

renderTerm[expr_, vmap_List] := Block[{v},
    v = lookupHold[expr, vmap];
    If[ v =!= $Failed, Return[v]];
    Which[
        AtomQ[expr],
            renderConst[expr],
        True,
            Block[{h = Head[expr], parts},
                parts = renderTerm[#, vmap] & /@ List @@ expr;
                opNameForHead[h] <> "(" <>
                    StringRiffle[parts, ","] <> ")"
            ]
    ]
];

(* Positional access avoids ForAll's HoldAll pattern-matching trap.
   When axiom isn't wrapped in ForAll (no bound vars), bail to {}. *)
extractVars[expr_] := If[ Head[expr] === ForAll,
    Block[{v = expr[[1]]},
        If[ Head[v] === List, List @@ v, {v}]
    ],
    {}
];

stripForAll[expr_] := If[ Head[expr] === ForAll, expr[[2]], expr];

axToCnf[expr_, name_String] := Block[{
    body = stripForAll[expr], vars = extractVars[expr],
    vmap, lhs, rhs
},
    vmap = makeVarMap[vars];
    If[ ! MatchQ[body, _Equal], Return[$Failed]];
    lhs = body[[1]]; rhs = body[[2]];
    StringJoin["cnf(", name, ", axiom, ",
        renderTerm[lhs, vmap], " = ", renderTerm[rhs, vmap], ")."]
];

conjToNegatedCnf[expr_, name_String] := Block[{
    body = stripForAll[expr], vars = extractVars[expr],
    vmap, lhs, rhs
},
    vmap = makeSkolemMap[vars];
    If[ ! MatchQ[body, _Equal], Return[$Failed]];
    lhs = body[[1]]; rhs = body[[2]];
    StringJoin["cnf(", name, ", negated_conjecture, ",
        renderTerm[lhs, vmap], " != ", renderTerm[rhs, vmap], ")."]
];

axioms = AxiomaticTheory[theoryName, "Axioms"];
If[ ! ListQ[axioms] || Length[axioms] === 0,
    Print["error: no axioms for theory ", theoryName, "\n"]; Exit[3]];

ntAssoc = AxiomaticTheory[theoryName, "NotableTheorems"];
If[ ! AssociationQ[ntAssoc],
    Print["error: notable theorems not available for ", theoryName, "\n"]; Exit[3]];

cjRaw = Lookup[ntAssoc, theoremName, None];
If[ cjRaw === None,
    (* It may instead live in another theory's NotableTheorems if the
       theorem name is "ImpliesXAxioms" -- not our concern here. *)
    Print["error: no theorem ", theoremName, " in ", theoryName, "\n"]; Exit[3]];

conjuncts = If[ MatchQ[cjRaw, _List], cjRaw, {cjRaw}];

axClauses = MapIndexed[
    axToCnf[#1, "ax" <> ToString[#2[[1]]]] &, axioms];
axClauses = Select[axClauses, # =!= $Failed &];

emit1[idx_, conj_] := Block[{lines, fn, gcl},
    fn = If[ Length[conjuncts] === 1, outFile,
        StringReplace[outFile, ".p" -> "__c" <> ToString[idx] <> ".p"]];
    gcl = conjToNegatedCnf[conj, "goal"];
    If[ gcl === $Failed, Print["skip ", fn, " (non-equation)\n"]; Return[]];
    lines = Join[
        {"% TPTP problem: " <> theoryName <> "::" <> theoremName <>
            If[ Length[conjuncts] > 1, " (conjunct " <> ToString[idx] <> ")", ""],
         "% axioms = " <> ToString[Length[axClauses]] <>
            ", conjuncts total = " <> ToString[Length[conjuncts]]},
        axClauses,
        {gcl}
    ];
    Export[fn, StringRiffle[lines, "\n"] <> "\n", "Text"];
    Print[fn, "\n"];
];

MapIndexed[ emit1[#2[[1]], #1] &, conjuncts ];
Exit[0]
