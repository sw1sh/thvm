(* ::Package:: *)
(* ATP_Method.wl -- Method / strategy machinery: TAtpSchedule and TAtpDescribeMethod schedule
   introspection, the atpAnalyzeStructure problem classifier, and atpAutoTune
   (the Waldmeister-style structure-driven schedule front-loader).

   Sibling of ATP.wl in the THVMLink`ATP` context; the recursive Kernel
   loader Gets it after ATP.wl (files sorted by {depth, lowercased path}),
   and it shares the THVMLink`ATP`Private` context, so it references
   ATP.wl's loaders, encoder, and helpers by bare name. *)

BeginPackage["THVMLink`ATP`", {"THVMLink`", "Wolfram`Parser`"}];

TAtpSchedule::usage = "TAtpSchedule[Method] returns the schedule (a list of single-config Methods) that TFindProof[..., Method -> spec] would expand to, without running the C engine.  Useful for debugging a Method choice or counting portfolio entries before allocating TimeConstraint.  TAtpSchedule[Method, conjecture, axioms] threads the conjecture + axioms through Automatic's structure-recognized auto-tune, so the returned schedule matches what TFindProof[conjecture, axioms, Method -> spec] would dispatch.  TAtpSchedule[Method, \"Theorem\", \"Theory\"] resolves names through AxiomaticTheory.";

TAtpDescribeMethod::usage = "TAtpDescribeMethod[Method] returns an Association describing what a Method spec resolves to.  For a named preset (Waldmeister, VampireUEQ, Twee, EProver), returns the preset's full defaults Association (the suboptions the dispatcher merges with the user's subopts).  For a list spec like {\"Twee\", subopts...}, returns the preset's defaults merged with the user's overrides -- the actual options that will reach the C engine.  For a non-preset config like {\"Completion\", subopts...}, returns Association[subopts].  For Automatic / \"Portfolio\" / \"VampirePortfolio\" / \"VampirePortfolioCompact\", returns <|\"Schedule\" -> ...|> describing the multi-entry rotation rather than a single config.";

Begin["`Private`"];

(* === Public schedule introspection ================================
   TAtpSchedule mirrors what the portfolio dispatcher sees: the list
   of single-config Methods TFindProof would try, given the same
   Method spec (and optionally the same conjecture + axioms for
   structure-recognized Automatic).  Used by callers who want to
   inspect / count / cherry-pick entries before running.  The
   underlying atpScheduleFor is private; TAtpSchedule is the public
   alias. *)
(* Catch unrecognized string Method names BEFORE atpScheduleFor's
   catch-all wraps them in a 1-element list -- otherwise
   TAtpSchedule["NotARealMethod"] silently returns {"NotARealMethod"}.
   Recognized strings are the named presets + the three completion-
   family heads + "Automatic". *)
$AtpKnownMethodNames := Join[$AtpMethodPresets,
    {"Completion", "GoalDirected", "MNF", "Automatic"}];
TAtpSchedule[m_String] /; ! MemberQ[$AtpKnownMethodNames, m] :=
    (Message[TFindProof::badmethod, m]; $Failed);
TAtpSchedule[m_] := atpScheduleFor[m];
TAtpSchedule[m_String, cj_, ax_List] /;
        ! MemberQ[$AtpKnownMethodNames, m] :=
    (Message[TFindProof::badmethod, m]; $Failed);
TAtpSchedule[m_, cj_,
        axiom : (_Equal | _Unequal | _ForAll
            | Inactive[Equal][_, _] | Inactive[Unequal][_, _])] :=
    TAtpSchedule[m, cj, {axiom}];
TAtpSchedule[m_, cj_, ax_List] := atpScheduleFor[m, ax, cj];
TAtpSchedule[m_, thm_String, theory_String] := With[{
        cj = AxiomaticTheory[theory, "NotableTheorems"][thm],
        ax = AxiomaticTheory[theory]},
    If[ MissingQ[cj] || ! ListQ[ax],
        $Failed,
        TAtpSchedule[m, cj, ax]]
];
(* Conjecture-expression against a NAMED theory: resolve the axioms
   through AxiomaticTheory, then thread through atpScheduleFor. *)
TAtpSchedule[m_,
        cj : (_List | _ForAll | _Equal | _Unequal
            | Inactive[Equal][_, _] | Inactive[Unequal][_, _]),
        theory_String] := With[{ax = AxiomaticTheory[theory]},
    If[ ! ListQ[ax], $Failed, TAtpSchedule[m, cj, ax]]
];

(* TAtpDescribeMethod: human-readable Method expansion.  Returns the
   options Association a Method spec resolves to, so users can see
   what `Method -> "Twee"` (or `{"Twee", ...}`) actually means before
   dispatch.  Schedule-style Methods (Automatic / Portfolio /
   VampirePortfolio[Compact]) return a <|"Schedule" -> ...|> wrapper
   describing the multi-entry rotation. *)
TAtpDescribeMethod[name_String] /;
        KeyExistsQ[$AtpPresetDefaults, name] :=
    $AtpPresetDefaults[name];
TAtpDescribeMethod[{name_String, subopts___Rule}] /;
        KeyExistsQ[$AtpPresetDefaults, name] :=
    Join[$AtpPresetDefaults[name], Association[{subopts}]];
TAtpDescribeMethod[name : ("Portfolio" | "VampirePortfolio"
        | "VampirePortfolioCompact" | "AllPresets"
        | "Automatic")] :=
    <|"Schedule" -> atpScheduleFor[name]|>;
TAtpDescribeMethod[Automatic] :=
    <|"Schedule" -> atpScheduleFor[Automatic]|>;
(* Catch unrecognized string Method names BEFORE the catch-all
   silently returns <||>.  Same recognized set as TAtpSchedule. *)
TAtpDescribeMethod[m_String] /;
        ! MemberQ[$AtpKnownMethodNames, m] :=
    (Message[TFindProof::badmethod, m]; $Failed);
TAtpDescribeMethod[{m_String, subopts___Rule}] /;
        ! MemberQ[$AtpKnownMethodNames, m] :=
    (Message[TFindProof::badmethod, m]; $Failed);
TAtpDescribeMethod[{m_String, subopts___Rule}] :=
    Association[{subopts}];
TAtpDescribeMethod[m_String] := <||>;

(* Problem-aware variants: Automatic / Portfolio / VampirePortfolio /
   etc. produce structure-tailored schedules when conjecture + axioms
   are in hand.  Single-config presets ignore the problem context;
   pass-through to the 1-arg form. *)
TAtpDescribeMethod[m_, cj_,
        axiom : (_Equal | _Unequal | _ForAll
            | Inactive[Equal][_, _] | Inactive[Unequal][_, _])] :=
    TAtpDescribeMethod[m, cj, {axiom}];
TAtpDescribeMethod[m_, cj_, ax_List] :=
    With[{s = atpScheduleFor[m, ax, cj]},
        If[ ListQ[s] && Length[s] > 1,
            <|"Schedule" -> s|>,
            TAtpDescribeMethod[m]]];
TAtpDescribeMethod[m_, thm_String, theory_String] := With[{
        cj = AxiomaticTheory[theory, "NotableTheorems"][thm],
        ax = AxiomaticTheory[theory]},
    If[ MissingQ[cj] || ! ListQ[ax],
        $Failed,
        TAtpDescribeMethod[m, cj, ax]]];

(* ====================================================================
   Problem-analysis auto-tuner  (port of Waldmeister's PhilMarlow /
   XFiles "structure recognition -> strategy database").

   PORTED EXACTLY (cited per function):
     - The law detectors (commutativity / associativity / idempotence /
       left+right unit / left+right inverse / distributivity) are a
       faithful WL re-statement of the C predicates in
       src/atp/precedence.c (prec_is_commutativity etc.), which are
       themselves ports of waldmeister TermOperationen.c's
       TO_IstKommutativitaet / TO_IstAssoziativitaet /
       TO_IstDistribution and the Sinai.h Tafel1 law equations
       (Sinai.h:54-101).  The arg-order and orientation handling match
       precedence.c:51-189 line for line.
     - The structure -> strategy assignments reproduce the relevant
       rows of waldmeister Tafel2 (Sinai.h:113-131), decoded through
       YFiles.c into thvm Method knobs:
         StdS  = kbo(std),itl(mi),zb(mnf)        (Sinai.h:109)
                 -> KBO, GoalInterleave 50, GoalDirected(mnf front)
         GtS   = kbo(std),cph(gt),itl(mi)        (Sinai.h:110)
                 -> KBO, CriticalPairWeight Gt, GoalInterleave 50
         KombS = rose(),kbo(std),cph(add),...    (Sinai.h:111)
                 -> KBO, CriticalPairWeight Add (combinator logic)
       The Tafel2 assignments used: Gruppe / BoolescheAlgebra ->
       GtS (Sinai.h:114-116); WajsbergAlgebra / LDAlgebra -> StdS
       (Sinai.h:118,124); Ring -> kbo(Std) (Sinai.h:122);
       Kombinatorlogik* -> KombS (Sinai.h:125-128); itl/cph/zb decoded
       via YFiles.c:78-122 (itl(mi) = -pq interleave=1.50;
       cph(gt) = gtweight; cph(add) = addweight; zb(mnf) = MNF goal
       normal-form front search; kbo = -ord kbo).

   APPROXIMATED-IN-SPIRIT (NOT a literal port):
     - Structure RECOGNITION.  Waldmeister's XFiles.c builds Komprimate
       and runs the Tafel3 premise-matching subsumption engine
       (PhilMarlow.c PraemissenDurchgenudelt / ZuordnungQuasiSpezieller)
       to pick the most-specific structure.  We do NOT port that table
       machinery; instead we classify directly from the per-operator law
       flags into a coarse class (AC / Group / AbelianGroup / Monoid /
       Ring / Sheffer / Combinatory / Lattice / General).  Faithful in
       intent, not in mechanism.
     - The exact YFiles parametrizations (mnfanalysis tuples, DEF/VAR
       weights, the 400/6000/14000 bis() step bounds) are NOT carried
       across; thvm's knobs are coarser (a single GoalInterleave int, a
       weight name, KBO/LPO, GroundJoin).  We pick the closest knob.
   ==================================================================== *)

(* {boundVars, lhs, rhs} of an axiom.  Handles ForAll[vars, l == r]
   (vars a single symbol or a List), a bare l == r, and an Inactive
   Equal.  Returns $Failed for a non-equation. *)
atpAxiomParts[ax_] := Block[{vars, eq, l, r},
    vars = Switch[ax,
        _ForAll, Flatten[{ax[[1]]}],
        _, {}];
    eq = ax /. ForAll[_, e_] :> e;
    eq = eq /. Inactive[Equal] -> Equal;
    If[ Head[eq] =!= Equal || Length[eq] =!= 2, Return[$Failed]];
    {l, r} = List @@ eq;
    (* Also extract Pattern-bound variables from an UNQUANTIFIED axiom
       (Pattern[v, Blank[]] form).  atpProveBundle passes axioms post-
       unquantifyFormula + CanonicalizePatterns (atpProveFromTheory
       in ATP.wl), so the ForAll wrapper is gone and the Switch above
       yields vars = {}.  Without this fallback, every symbol looks
       like an operator -- atpIsVar always false -- and the Sheffer /
       Boolean / AC discriminators in atpAnalyzeStructure can never
       fire.  Consequence: Method->Automatic loses its tuned schedule
       and falls back to the generic $AtpSchedule (debugged via
       Hillman/Commutativity 27-vs-57 dispatch divergence). *)
    If[ vars === {},
        vars = DeleteDuplicates @ Cases[eq,
            Verbatim[Pattern][s_Symbol, _] :> s,
            {0, Infinity}];
        (* Strip the Pattern wrappers from l/r too so downstream
           atpBinHead etc. don't misclassify Pattern[a, Blank[]] as a
           binary op (its 2 children make `MatchQ[Pattern[a,_], _[_,_]]`
           true). *)
        l = l /. Verbatim[Pattern][s_Symbol, _] :> s;
        r = r /. Verbatim[Pattern][s_Symbol, _] :> s
    ];
    {vars, l, r}
];

(* a term is the i-th bound variable. *)
atpIsVar[t_, vars_] := MemberQ[vars, t];

(* a binary application op[_, _] with op a non-variable head.  Returns
   the head, or $Failed.  Mirrors prec_is_binop (precedence.c:43). *)
atpBinHead[t_, vars_] := If[
    MatchQ[t, _[_, _]] && ! atpIsVar[Head[t], vars] && Head[t] =!= List,
    Head[t], $Failed];

(* Commutativity  f[x,y] == f[y,x].  Port of prec_is_commutativity
   (precedence.c:51) / TO_IstKommutativitaet. *)
atpLawCommutative[l_, r_, vars_] := Block[{f},
    f = atpBinHead[l, vars];
    f =!= $Failed && atpBinHead[r, vars] === f &&
        MatchQ[{l, r}, {f[a_, b_], f[b_, a_]}] &&
        atpIsVar[l[[1]], vars] && atpIsVar[l[[2]], vars] &&
        l[[1]] =!= l[[2]]
];

(* one orientation of associativity: a = f[f[x,y],z], b = f[x,f[y,z]].
   Port of prec_assoc_dir (precedence.c:65). *)
atpAssocDir[a_, b_, f_, vars_] :=
    MatchQ[a, f[f[x_, y_], z_] /;
        atpIsVar[x, vars] && atpIsVar[y, vars] && atpIsVar[z, vars] &&
        MatchQ[b, f[x, f[y, z]]]];

(* Associativity (either side flattened).  Port of
   prec_is_associativity (precedence.c:90). *)
atpLawAssociative[l_, r_, vars_] := Block[{f},
    f = atpBinHead[l, vars];
    If[ f === $Failed, f = atpBinHead[r, vars]];
    f =!= $Failed &&
        (atpAssocDir[l, r, f, vars] || atpAssocDir[r, l, f, vars])
];

(* Idempotence  f[x,x] == x.  Port of prec_is_idempotence
   (precedence.c:104) / Sinai Idempotenz. *)
atpLawIdempotent[l_, r_, vars_] := Block[{f},
    f = atpBinHead[l, vars];
    f =!= $Failed && atpIsVar[l[[1]], vars] && l[[1]] === l[[2]] &&
        r === l[[1]]
];

(* a constant (a nullary CTR): an atom that is not a bound variable, or
   a unit-wrapper like OverTilde[1] / OverBar[0] / a CircleTimes-free
   integer.  Waldmeister's prec_is_identity wants the unit slot to be a
   nullary CTR (precedence.c:126); in WL the "unit" is a ground term
   with no bound variable. *)
atpIsConstTerm[t_, vars_] := FreeQ[t, Alternatives @@ vars] &&
    ! atpIsVar[t, vars];

(* Left/right identity  f[e,x]==x  /  f[x,e]==x, e a constant.  Port of
   prec_is_identity (precedence.c:118).  side 0 = left, 1 = right. *)
atpLawIdentity[l_, r_, vars_, side_] := Block[{f, unit, var},
    f = atpBinHead[l, vars];
    If[ f === $Failed || ! atpIsVar[r, vars], Return[False]];
    unit = l[[side + 1]];
    var = l[[2 - side]];
    atpIsVar[var, vars] && var === r && atpIsConstTerm[unit, vars]
];

(* Left/right inverse  f[i[x],x]==e  /  f[x,i[x]]==e, i unary, e const.
   Port of prec_is_inverse (precedence.c:136).  side 0 = left. *)
atpLawInverse[l_, r_, vars_, side_] := Block[{f, inv, var},
    f = atpBinHead[l, vars];
    If[ f === $Failed || ! atpIsConstTerm[r, vars], Return[False]];
    inv = l[[side + 1]];
    var = l[[2 - side]];
    atpIsVar[var, vars] && MatchQ[inv, _[_]] && ! atpIsVar[Head[inv], vars] &&
        atpIsVar[inv[[1]], vars] && inv[[1]] === var
];

(* one orientation of left distributivity: a = f[x,g[y,z]],
   b = g[f[x,y],f[x,z]], f != g.  Port of prec_distrib_dir
   (precedence.c:155). *)
atpDistribDir[a_, b_, vars_] := Block[{f, g, x, inner},
    f = atpBinHead[a, vars];
    If[ f === $Failed || ! atpIsVar[a[[1]], vars], Return[$Failed]];
    x = a[[1]];
    inner = a[[2]];
    g = atpBinHead[inner, vars];
    If[ g === $Failed || g === f, Return[$Failed]];
    If[ ! atpIsVar[inner[[1]], vars] || ! atpIsVar[inner[[2]], vars] ||
        inner[[1]] === inner[[2]], Return[$Failed]];
    If[ MatchQ[b, g[f[x, inner[[1]]], f[x, inner[[2]]]]],
        {f, g}, $Failed]
];

(* Distributivity f over g (either orientation).  Port of
   prec_is_distributivity (precedence.c:186). *)
atpLawDistributes[l_, r_, vars_] := Block[{d},
    d = atpDistribDir[l, r, vars];
    If[ d === $Failed, d = atpDistribDir[r, l, vars]];
    d
];

(* === atpAnalyzeStructure ===========================================
   Walk the axiom list once, tag each operator with its laws, then
   classify into a coarse structure class.  Returns an Association:
     "Operators"      <|head -> <|"Commutative", "Associative",
                                   "Idempotent", "HasUnit", "HasInverse",
                                   "Distributes", "Arity"|> ...|>
     "ACOperators"    {heads that are commutative AND associative}
     "Class"          "AC" | "Group" | "AbelianGroup" | "Monoid" |
                      "Ring" | "Sheffer" | "Combinatory" | "Lattice" |
                      "General"
     "NOperators", "MaxArity", "NAxioms"
   conjecture is accepted for symmetry with atpAutoTune but does not
   change the class (Waldmeister classifies from the equation set;
   PhilMarlow.c:1422-1429). *)
atpAnalyzeStructure[axioms_List, conjecture_ : Null] := Block[{
    parts, vars, ops, allHeads, combinatorConsts, isSheffer, isComb,
    acOps, hasAssoc, hasComm, hasInv, hasUnit, hasIdem, hasDistrib,
    class, maxArity, arities},
    parts = DeleteCases[atpAxiomParts /@ axioms, $Failed];
    (* per-operator law flags *)
    ops = <||>;
    Module[{tag},
        tag[h_, key_] := ops[h] = Append[Lookup[ops, h, <|
            "Commutative" -> False, "Associative" -> False,
            "Idempotent" -> False, "HasUnit" -> False,
            "HasInverse" -> False, "Distributes" -> False,
            "Arity" -> 0|>], key -> True];
        Do[ Block[{vs = p[[1]], l = p[[2]], r = p[[3]], f, d},
            (* record arity of every applied non-variable head *)
            Scan[Function[t,
                If[ MatchQ[t, _[___]] && ! atpIsVar[Head[t], vs] &&
                    Head[t] =!= List && Head[t] =!= Equal,
                    ops[Head[t]] = Append[
                        Lookup[ops, Head[t], <|
                            "Commutative" -> False, "Associative" -> False,
                            "Idempotent" -> False, "HasUnit" -> False,
                            "HasInverse" -> False, "Distributes" -> False,
                            "Arity" -> 0|>],
                        "Arity" -> Length[t]]]],
                {l, r}, {0, Infinity}, Heads -> False];
            If[ atpLawCommutative[l, r, vs], tag[atpBinHead[l, vs], "Commutative"]];
            If[ atpLawAssociative[l, r, vs],
                f = atpBinHead[l, vs]; If[ f === $Failed, f = atpBinHead[r, vs]];
                tag[f, "Associative"]];
            If[ atpLawIdempotent[l, r, vs], tag[atpBinHead[l, vs], "Idempotent"]];
            Do[ Block[{a = If[sw == 1, r, l], b = If[sw == 1, l, r]},
                If[ atpLawIdentity[a, b, vs, 0], tag[atpBinHead[a, vs], "HasUnit"]];
                If[ atpLawIdentity[a, b, vs, 1], tag[atpBinHead[a, vs], "HasUnit"]];
                If[ atpLawInverse[a, b, vs, 0], tag[atpBinHead[a, vs], "HasInverse"]];
                If[ atpLawInverse[a, b, vs, 1], tag[atpBinHead[a, vs], "HasInverse"]]],
                {sw, 0, 1}];
            d = atpLawDistributes[l, r, vs];
            If[ d =!= $Failed, tag[First[d], "Distributes"]]],
            {p, parts}]
    ];
    acOps = Keys @ Select[ops, #["Commutative"] && #["Associative"] &];
    hasAssoc = AnyTrue[Values[ops], #["Associative"] &];
    hasComm = AnyTrue[Values[ops], #["Commutative"] &];
    hasInv = AnyTrue[Values[ops], #["HasInverse"] &];
    hasUnit = AnyTrue[Values[ops], #["HasUnit"] &];
    hasIdem = AnyTrue[Values[ops], #["Idempotent"] &];
    hasDistrib = AnyTrue[Values[ops], #["Distributes"] &];
    arities = #["Arity"] & /@ Values[ops];
    maxArity = If[arities === {}, 0, Max[arities]];
    (* Combinatory logic: a single binary "application" operator
       (Sinai Tafel3 Kombinatorlogik* use the "_" application op,
       Sinai.h:95-102, 348-362).  The combinator constants
       (S,K,B,W,...) appear as atomic SYMBOLS inside axiom subterms
       (e.g. Application[CombinatorS, x] -- CombinatorS is a leaf, not
       an operator HEAD with its own arity slot), so the nullary-op
       check that earlier classifiers used misses them.  Discriminator:
       one binary op + no commutativity / inverse + >=4 axioms (Sheffer
       / Wolfram / Meredith / McCune have at most 3, so this cleanly
       separates).  HasUnit allowed -- some AxiomaticTheory combinator
       presentations set HasUnit on Application (the Y axiom shape). *)
    allHeads = Keys[ops];
    isComb = Length[Select[Values[ops], #["Arity"] == 2 &]] == 1 &&
        ! hasComm && ! hasInv && Length[parts] >= 4;
    (* Sheffer / Nand: a single binary operator, no other structure
       (no associativity, commutativity is the GOAL not an axiom).
       WolframAxioms is the canonical case (one CenterDot axiom). *)
    isSheffer = Length[allHeads] == 1 && First[Values[ops]]["Arity"] == 2 &&
        ! hasAssoc && ! hasComm && ! hasUnit && ! hasInv && ! hasDistrib;
    (* Boolean lattice presentation: two binary ops that are both
       commutative AND distribute, plus a unary "complement" head
       (Huntington-style axioms where associativity is a THEOREM, not
       an axiom, so the assoc detector does NOT fire; that's the gap
       AC-class detection misses).  Symmetric goals dominate here
       (DeMorgan, ExcludedMiddle, Noncontradiction), so GoalDirected
       must come first in the front-load. *)
    isBoolean = Length[Select[Values[ops],
            #["Arity"] == 2 && #["Commutative"] && #["Distributes"] &]] >= 2 &&
        AnyTrue[Values[ops], #["Arity"] == 1 &];
    (* AC theory with a unary complement (Huntington, Robbins): the
       symmetric goals (DoubleNegation, ImpliesRobbins) need MNF/
       GoalDirected, but the bulk completion still wants the AC weight
       schedule -- so this is a "GoalDirected first, then AC" variant. *)
    isACWithComplement = ! isBoolean && hasComm && hasAssoc &&
        AnyTrue[Values[ops], #["Arity"] == 1 &];
    (* CancellativeAbelianGroup -- the single-axiom McCune / AbelianMcCune
       characterization.  Shape: ONE axiom over a single binary op + a
       single unary op (the inverse), with no standard law detector
       firing (assoc, comm, unit, inverse, distrib all hidden inside a
       complex compound).  Example: `and(and(and(x,y),z), not(and(x,z)))
       = y` (AbelianMcCune single CAG axiom).  Front-load GT weight --
       Mix2 from the default $AtpSchedule fallback doesn't crack these
       in reasonable time, but GT-bare cracks in 0.0s on the C bench
       (see test_atp_wolfram_bench's amc_assoc probe). *)
    isCAG = Length[parts] == 1 &&
        Length[Select[Values[ops], #["Arity"] == 2 &]] == 1 &&
        AnyTrue[Values[ops], #["Arity"] == 1 &] &&
        ! hasAssoc && ! hasComm && ! hasUnit && ! hasInv && ! hasDistrib;
    class = Which[
        isComb, "Combinatory",
        isSheffer, "Sheffer",
        isBoolean, "Boolean",
        hasDistrib && hasInv, "Ring",
        hasInv && hasUnit && hasAssoc && hasComm, "AbelianGroup",
        hasInv && hasUnit && hasAssoc, "Group",
        hasIdem && hasComm && hasAssoc && ! hasInv, "Lattice",
        hasUnit && hasAssoc, "Monoid",
        isACWithComplement, "ACWithComplement",
        hasComm && hasAssoc, "AC",
        isCAG, "CancellativeAbelianGroup",
        True, "General"];
    <|"Operators" -> ops, "ACOperators" -> acOps, "Class" -> class,
      "NOperators" -> Length[ops], "MaxArity" -> maxArity,
      "NAxioms" -> Length[parts]|>
];
(* the named-theory / single-arg conveniences resolve through
   AxiomaticTheory like the rest of the surface. *)
atpAnalyzeStructure[theory_String] :=
    atpAnalyzeStructure[AxiomaticTheory[theory]];

(* === atpAutoTune ===================================================
   Map the detected structure class to an ORDERED list of tailored
   Method configs, FRONT of the schedule.  The mappings reproduce
   waldmeister Tafel2 (Sinai.h:113-131) decoded into thvm knobs (see
   the citation block above atpAxiomParts).  These are best-GUESS
   front-loads only -- the safety tail in atpTunedSchedule guarantees
   the fixed portfolio still runs if none close. *)
atpAutoTune[axioms_List, conjecture_ : Null] := Block[{prof = atpAnalyzeStructure[axioms, conjecture]},
    atpAutoTuneForClass[prof["Class"]]
];
atpAutoTune[theory_String, conjecture_ : Null] :=
    atpAutoTune[AxiomaticTheory[theory], conjecture];

(* Tafel2 row -> thvm config list.  Decoding key:
   GtS  -> Gt weight, KBO, GoalInterleave 50  (Sinai.h:110)
   StdS -> KBO, GoalInterleave 50, GoalDirected MNF front (Sinai.h:109)
   KombS-> Add weight, KBO                    (Sinai.h:111) *)
atpGtS = {"Completion", "CriticalPairWeight" -> "Gt",
    "GoalInterleave" -> 50};
atpStdS = {"Completion", "GoalInterleave" -> 50};
atpKombS = {"Completion", "CriticalPairWeight" -> "Add",
    "Ordering" -> "LPO", "AutoPrecedence" -> True};

atpAutoTuneForClass["AbelianGroup" | "Group"] := {
    (* Mix + KBO + AutoPrec + SR=51 + RHSI + UnfailingCP + CPSetIR --
       the Waldmeister-faithful default config.  Measured on the
       AbelianGroup NotableTheorems sweep post-05e6a63c structure-
       detection fix (iter 27):
         IOI: 12     IOC: 18
         ImpliesMcCune: 28    ImpliesAbelianMcCune: 20
       Total 78 constructs vs Gt-first's 81 (3 shorter overall);
       hits 2 byte-identical with WM CLI (IOI, ImpliesMcCune,
       ImpliesAbelianMcCune) + 1 thvm-shorter (IOC).  Tafel2's
       GtS (below) wins on IOC + ImpliesMcCune but loses on
       ImpliesAbelianMcCune by +8; Mix-first hedges.  Front-load
       Mix; keep GtS as the fallback. *)
    {"Completion", "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO",
        "AutoPrecedence" -> True, "SelectionRatio" -> 51,
        "RHSInterreduce" -> True, "UnfailingCP" -> True,
        "CPSetInterreduce" -> True},
    (* Tafel2: Gruppe -> GtS (Sinai.h:115).  AutoPrecedence puts the
       unary inverse on top (Sinai Group precedence "+-0", Sinai.h:117/
       precedence.c:329) so i(i(x)) -> x orients cleanly. *)
    {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50,
        "AutoPrecedence" -> True},
    atpGtS};
atpAutoTuneForClass["Ring"] := {
    (* Tafel2: Ring -> kbo(Std) (Sinai.h:122); structure precedence
       puts the distributor "*" above "+" (Tafel3 Ring "*+",
       Sinai.h:243 / precedence.c:331). *)
    {"Completion", "Ordering" -> "KBO", "AutoPrecedence" -> True},
    atpGtS};
atpAutoTuneForClass["AC"] := {
    (* AC theories: GtS family -- a KBO with ordering-directed (gt) CP
       weight handles commutative+associative operators (Sinai Verband /
       group rows all use GtS / kbo, Sinai.h:114-122). *)
    atpGtS,
    {"Completion", "CriticalPairWeight" -> "Mix2", "GoalInterleave" -> 50}};
atpAutoTuneForClass["Lattice"] := {
    (* Tafel2: Verband -> kbo(Std),cph(gt),gj() ... lpo(std),itl(re)
       (Sinai.h:120) -- GroundJoin on, then an LPO pass. *)
    {"Completion", "CriticalPairWeight" -> "Gt", "GroundJoin" -> True,
        "GoalInterleave" -> 50},
    {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True,
        "GoalInterleave" -> 100}};
atpAutoTuneForClass["Monoid"] := {atpGtS, atpStdS};
atpAutoTuneForClass["Combinatory"] := {
    (* Tafel2: Kombinatorlogik* -> KombS = cph(add) (Sinai.h:111,125).
       Variable-duplicating combinator rules (S,W,M) need LPO.  Front-load
       GoalDirected too -- the cross-system equivalence direction (e.g.
       SKIToBCKW: S via B,C,W) is symmetric and closes in MNF (~0.03s) but
       walls plain completion (~10s under LPO+AutoPrec). *)
    atpKombS,
    "GoalDirected",
    {"Completion", "CriticalPairWeight" -> "Add"}};
atpAutoTuneForClass["Sheffer"] := {
    (* No Tafel2 Sheffer row.  Measured per-entry behavior:
       - Mix2 + SelectionRatio 2 (aggressive 1-FIFO-per-2 age bias)
         cracks the cross-axiom Implies-X family FASTER than Add:
         ImpliesWolframAxioms ~3.7s + ImpliesWolframAlternate ~3.7s
         + Commutativity ~0.55s, vs Add's ~7.9s on the same goals.
         The default SelectionRatio (11) leaves these unreachable; the
         tight age bias forces the saturator through the long
         derivation chain these cross-system goals need before the
         CP queue blows up.  Iter 75.  FRONT-LOAD it.
       - Add weight also closes the Implies-X family (slower, ~7.9s)
         but is more general across nand goals -- kept as the second
         entry.
       - GoalDirected (MNF bidirectional front) is the closer for the
         symmetric goals (nand-Commutativity, etc.) and a known
         engine bug can hang MNF on hard cross-axiom Implies-X if it's
         the first attempt.  Run AFTER the completion configs so any
         hang is bounded by the remaining-budget slice and the
         shell-level kill-after.
       - SInE relevance pruning helps the cross-system Meredith-class
         Implies-X (Vampire benchmark winner).
       All entries cap CP weight via AutoMaxWeight -> 20 -- no memory
       thrashing.  AndAssociativity-class deep saturations stay out of
       this lean schedule (need Waldmeister preset + minutes of
       saturation; the safety tail's Gt entry still picks them up). *)
    (* Mix2 + SR=2 without AutoMaxWeight cracks Hillman/Commutativity at
       len 27 in 0.04s; the AMW=20 variant below FAILS Hillman (AMW
       defers + drains CPs that this trajectory needs immediately).
       Run the AMW-free variant FIRST so Hillman + ImpliesWolfram cases
       crack on entry 1 instead of falling through to the safety tail. *)
    {"Completion", "CriticalPairWeight" -> "Mix2", "SelectionRatio" -> 2},
    {"Completion", "CriticalPairWeight" -> "Mix2",
        "SelectionRatio" -> 2, "AutoMaxWeight" -> 20},
    {"Completion", "CriticalPairWeight" -> "Add", "AutoMaxWeight" -> 20},
    {"GoalDirected", "CriticalPairWeight" -> "Mix2", "AutoMaxWeight" -> 20},
    {"Completion", "CriticalPairWeight" -> "Mix2",
        "AxiomRelevance" -> "SInE", "AutoMaxWeight" -> 20},
    {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True,
        "AutoMaxWeight" -> 20},
    (* Vampire LRS (Limited Resource Strategy): predicts how many CPs
       the saturator can REACH in the remaining time budget and prunes
       the queue to those.  Sound (discarded CPs are unreachable in
       budget; same incomplete-in-budget tradeoff Vampire ships) and
       cheap (one quickselect + O(queue) prune per recompute period).
       For the deep cross-axiom Sheffer Implies-X family where the
       lean configs above wall, LRS narrows the heap-min selection to
       the budget-tractable subset. *)
    {"Completion", "CriticalPairWeight" -> "Mix2", "LRS" -> True,
        "AutoMaxWeight" -> 20},
    (* Twee-style config: Twee's CP weight (asymmetric, biases small
       reduct) + GroundJoin + Connectedness BOTH on (Twee.Join.Config
       defaults cfg_ground_join=True, cfg_use_connectedness_standalone=
       True).  These redundancy criteria delete CPs whose two sides
       join through a path strictly below the peak; the combination
       prunes the CP queue without losing completeness.  Iters 18 / 20
       also bundle BS / BD / RHSI here (sentinel-LHS soft-delete +
       backward demodulation): every redundancy criterion the engine
       has, on the same entry, so the CP queue stays tightest for the
       deep Implies-X family that walls every cheaper config. *)
    {"Completion", "CriticalPairWeight" -> "Twee",
        "GroundJoin" -> True, "Connectedness" -> True,
        "AutoMaxWeight" -> 20,
        "BackwardSubsume" -> True, "BackwardDemod" -> True,
        "RHSInterreduce" -> True},
    (* WM-style p>q>nand scheme (LPO + SkolemHighest, the structural
       precedence Waldmeister's wolfram.pr / thm.pr ORDERING block
       encodes -- see project_atp_wm_sheffer_lpo memory).  The
       skolemized goal constants rank above the operator, so the goal
       inequality x*y = y*x orients deterministically by the constant
       precedence rather than depending on AutoPrecedence's arity
       layering.  Working recipe from atp.wlt:1415 (closes
       WolframAxioms/Commutativity in 60s).  Front-load it for the
       cross-axiom Implies-X / *-Associativity cases the lean Mix2/Add
       entries above wall on. *)
    {"GoalDirected", "Ordering" -> "LPO", "SkolemHighest" -> True,
        "CriticalPairWeight" -> "Add", "FifoTiebreak" -> True,
        "UnfailingCP" -> True, "AutoMaxWeight" -> 20}};
atpAutoTuneForClass["Boolean"] := {
    (* BooleanAxioms has both asymmetric (DeMorgan / Absorption /
       OrAssociativity / Distributivity) and symmetric (ExcludedMiddle /
       Noncontradiction / DoubleNegation) NotableTheorems.  Measured per-
       entry: GoalDirected closes Noncontradiction in 0.48s but Mix2
       walls (the symmetric goals never meet at one normal form); Mix2
       closes DeMorgan in 1.06s and GoalDirected in 3.15s.  GoalDirected
       first wins NET because the symmetric cases save more than DeMorgan
       loses (DeMorgan still proves on the second Mix2 entry). *)
    "GoalDirected",
    {"Completion", "CriticalPairWeight" -> "Mix2"}};
atpAutoTuneForClass["ACWithComplement"] := {
    (* Huntington / Robbins: AC operator + unary complement.  The
       symmetric goals (DoubleNegation, ImpliesRobbins) need MNF
       first; bulk completion (GtS) is the fallback. *)
    "GoalDirected",
    atpGtS};
(* CancellativeAbelianGroup: GT weight cracks the single-axiom CAG
   (AbelianMcCune / McCune characterization) in 0.0s on the C bench
   where Mix2-bare from the default tail can wall.  Empirical: GT
   without preset is the cracker.  Verified via test_atp_wolfram_bench
   amc_assoc probe (commit 1464ab1a). *)
atpAutoTuneForClass["CancellativeAbelianGroup"] := {
    {"Completion", "CriticalPairWeight" -> "Gt"},
    atpGtS,
    {"Completion"}};

atpAutoTuneForClass[_] := {};   (* "General": no front-load, just tail *)

(* Front-load the tuned configs, then APPEND the full fixed portfolio
   as a fallback tail and DeleteDuplicates.  This is the safety
   constraint: Automatic only REORDERS -- every config the fixed
   portfolio runs is still present, so the tuned schedule can never
   prove strictly less than "Portfolio".

   Many-axiom tail: when the input is sufficiently large (>=8 axioms),
   ALSO append a SInE-pruned variant of the broadest weight (Mix2) and
   the goal-directed config.  SInE is Vampire's single most successful
   premise-selection technique for the parallel benchmark of thvm's
   uncrackable cross-system Implies* theorems (35/74 wins under the
   strategy lrs+10_1 st=3:sd=2:ss=axioms:sgt=8).  Since SInE may drop
   needed axioms, it runs LAST -- only after the un-pruned portfolio
   has been exhausted -- so a proof reachable without pruning is never
   missed. *)
atpTunedSchedule[axioms_, conjecture_] := Block[{base, sineTail},
    base = DeleteDuplicates @ Join[
        Quiet @ Check[atpAutoTune[axioms, conjecture], {}],
        $AtpSchedule];
    sineTail = If[ Length[axioms] >= 8,
        {{"Completion", "CriticalPairWeight" -> "Mix2",
            "AxiomRelevance" -> "SInE"},
         {"GoalDirected", "AxiomRelevance" -> "SInE"}}, {}];
    DeleteDuplicates @ Join[base, sineTail]];

End[];
EndPackage[];
