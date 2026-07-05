(* Predicate-logic -> equational preprocessing tests (ATP_Equationalize.wl).

   TFindProof auto-equationalizes first-order / predicate-logic problems
   (predicate atoms, logical connectives, quantifiers) exactly as
   FindEquationalProof does, then proves the resulting equational problem
   with the same completion engine.  These cover the FindEquationalProof
   documentation's predicate examples end to end: each returns a
   ProofObject whose ProofFunction round-trips, matching FindEquationalProof.
   The file is exercised by wl/THVMLink/Tests/run.wls. *)

(* ===== the four documented FindEquationalProof predicate examples ===== *)

VerificationTest[
    (* Socrates syllogism: all men are mortal, Socrates is a man. *)
    Head @ TFindProof[mortal[socrates],
        {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]},
        TimeConstraint -> 60],
    ProofObject,
    TestID -> "ATP/eqz/socrates-syllogism"
]

VerificationTest[
    (* The reconstructed ProofObject is a predicate proof displaying the
       ORIGINAL goal, matching FindEquationalProof. *)
    Module[{p = TFindProof[mortal[socrates],
        {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]},
        TimeConstraint -> 60]},
        {p["Logic"], p["Theorems"]}],
    {"Predicate/EquationalLogic", mortal[socrates]},
    TestID -> "ATP/eqz/socrates-predicate-form"
]

VerificationTest[
    (* Socrates proof round-trips through its own ProofFunction.  A
       predicate ("Predicate/EquationalLogic") proof verifies against its
       "Theorems" (it has no "ConjectureStatement" property). *)
    Module[{p = TFindProof[mortal[socrates],
        {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]},
        TimeConstraint -> 60]},
        Head @ p["ProofFunction"][p["Theorems"]]],
    Success,
    TestID -> "ATP/eqz/socrates-verifies"
]

VerificationTest[
    (* Existential goal + existential hypothesis. *)
    Head @ TFindProof[Exists[x, Mortal[x]],
        {ForAll[x, Implies[Man[x], Mortal[x]]], Exists[x, Man[x]]},
        TimeConstraint -> 60],
    ProofObject,
    TestID -> "ATP/eqz/existential-quantifier"
]

VerificationTest[
    (* Lewis Carroll's crocodile puzzle: negated existential goal with a
       conjunction, three implication axioms. *)
    Head @ TFindProof[! Exists[x, baby[x] && manageCrocodile[x]],
        {ForAll[x, Implies[baby[x], ! logical[x]]],
         ForAll[x, Implies[manageCrocodile[x], ! despised[x]]],
         ForAll[x, Implies[! logical[x], despised[x]]]},
        TimeConstraint -> 90],
    ProofObject,
    TestID -> "ATP/eqz/crocodile-puzzle"
]

VerificationTest[
    (* Knights and Knaves: ground And goal but ForAll-quantified axioms,
       so it must route to equationalization (NOT the ground SMT decider)
       and yield a verifiable ProofObject. *)
    Head @ TFindProof[knight[zoey] && knave[mel],
        {ForAll[p, ForAll[s, Implies[knight[p] && says[p, s], s]]],
         ForAll[p, ForAll[s, Implies[knave[p] && says[p, s], ! s]]],
         ForAll[p, knight[p] || knave[p]],
         says[zoey, knave[mel]],
         says[mel, ! knave[zoey] && ! knave[mel]]},
        TimeConstraint -> 120],
    ProofObject,
    TestID -> "ATP/eqz/knights-and-knaves"
]

VerificationTest[
    (* Knights and Knaves proof round-trips through its Theorems. *)
    Module[{p = TFindProof[knight[zoey] && knave[mel],
        {ForAll[p, ForAll[s, Implies[knight[p] && says[p, s], s]]],
         ForAll[p, ForAll[s, Implies[knave[p] && says[p, s], ! s]]],
         ForAll[p, knight[p] || knave[p]],
         says[zoey, knave[mel]],
         says[mel, ! knave[zoey] && ! knave[mel]]},
        TimeConstraint -> 120]},
        Head @ p["ProofFunction"][p["Theorems"]]],
    Success,
    TestID -> "ATP/eqz/knights-and-knaves-verifies"
]

(* ===== modus ponens chain (bare predicate atoms, one quantifier) ===== *)

VerificationTest[
    (* {ForAll[x, p[x] => p[f[x]]], p[a]} |- p[f[f[a]]]. *)
    Head @ TFindProof[p[f[f[a]]],
        {ForAll[x, Implies[p[x], p[f[x]]]], p[a]}, TimeConstraint -> 60],
    ProofObject,
    TestID -> "ATP/eqz/modus-ponens-chain"
]

(* ===== routing: equational problems are NOT equationalized ===== *)

VerificationTest[
    (* A plain equation problem still proves via the direct equational
       path (equationalization must not intercept it). *)
    Head @ TFindProof[a == c, {a == b, b == c}],
    ProofObject,
    TestID -> "ATP/eqz/plain-equation-not-equationalized"
]

VerificationTest[
    (* A disequation goal (Unequal) is engine-native, not predicate
       logic: eqzEquationalProblemQ must classify it as already-
       equational so the equationalization route leaves it alone (it
       would otherwise wrongly encode `a != d == or[x, not[x]]`). *)
    WolframInstitute`THVMLink`ATP`Private`eqzEquationalProblemQ[
        a != d, {a == b, b == c}],
    True,
    TestID -> "ATP/eqz/disequation-goal-not-equationalized"
]

VerificationTest[
    (* ForAll-wrapped equational axioms/conjecture stay on the equational
       path (Last[ForAll] is an Equal, so already-equational). *)
    Head @ TFindProof[ForAll[x, f[g[x]] == g[f[x]]], {ForAll[x, f[x] == g[x]]}],
    ProofObject,
    TestID -> "ATP/eqz/forall-equational-not-equationalized"
]

(* ===== faithfulness: the vendored transform matches the FindEquational-
   Proof source (Language`EquationalProofDump`Equationalize) byte for
   byte on representative forms. ===== *)

VerificationTest[
    With[{
        fq = Language`EquationalProofDump`foldQuantifiers,
        eqzInt = Language`EquationalProofDump`Equationalize,
        eqzOurs = WolframInstitute`THVMLink`ATP`Private`eqzEquationalize,
        foldOurs = WolframInstitute`THVMLink`ATP`Private`eqzFoldQuantifiers},
        AllTrue[
            {mortal[socrates], ForAll[x, Implies[man[x], mortal[x]]],
             Exists[x, Mortal[x]], ! Exists[x, baby[x] && manageCrocodile[x]],
             ForAll[p, ForAll[s, Implies[knight[p] && says[p, s], s]]],
             ForAll[p, knight[p] || knave[p]]},
            (eqzInt[fq[#]] === eqzOurs[foldOurs[#]]) &]],
    True,
    TestID -> "ATP/eqz/faithful-to-source"
]

(* ===== engine-form witness atomization: the internal engine must see
   the SAME term structure the .pr writer hands the real Waldmeister
   binary. ===== *)

VerificationTest[
    (* The excluded-middle witness Subscript[\[FormalA], 0] is a
       3-symbol compound; unatomized it flips the equationalized
       implication axiom `or[not[man[x]], mortal[x]] == or[wit, not[wit]]`
       from orientable (KBO 6 > 4) to unorientable (6 < 8), sending the
       internal engine down a different completion trajectory than the
       binary and leaking the FVI grounding constant cAtp1 into the
       proof.  eqzAtomRules must rename it to the atomic Global`emwit0
       (matching eqzWmSymRules in the .pr writer), leaving no Subscript
       in the engine form. *)
    FreeQ[WolframInstitute`THVMLink`ATP`Private`atpEquationalizeGoal[
        mortal[socrates], {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]}], Subscript],
    True,
    TestID -> "ATP/eqz/engine-witness-atomized"
]

VerificationTest[
    (* Socrates under the internal engine (Method -> "Waldmeister"):
       with the atomized witness the selection trajectory matches the
       real binary on the same problem (13 selections), so the proof is
       cAtp1-free and lands at PL 19.  The WaldmeisterProcess reference
       is 18: the remaining +1 CriticalPairLemma is the post-hoc goal-
       chain replay citing the live-at-end general rule
       `or[x, not[x]] -> man[socrates]` where Waldmeister's protocol
       keeps the historical instance reduction (proof-extract replays
       against the final rule set instead of recording goal reductions
       as they fire). *)
    Module[{prf = TFindProof[mortal[socrates],
        {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]},
        Method -> "Waldmeister", TimeConstraint -> 60]["Proof"]},
        {FreeQ[prf, s_Symbol /; SymbolName[Unevaluated[s]] === "cAtp1"], Length[prf]}],
    {True, 19},
    TestID -> "ATP/eqz/socrates-emu-catp1-free"
]

VerificationTest[
    (* Load-order regression probe: the Kernel loader parses
       ATP_Equationalize.wl BEFORE ATP.wl, so without its TFindProof
       forward declaration the `OptionValue[TFindProof, ...]` there
       binds a fresh Private-context symbol -- OptionValue::nodef on
       every WaldmeisterProcess predicate call, with the user's
       TimeConstraint silently ignored. *)
    Names["WolframInstitute`THVMLink`ATP`Private`TFindProof"],
    {},
    TestID -> "ATP/eqz/tfindproof-not-private"
]
