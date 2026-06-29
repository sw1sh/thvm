(* ::Package:: *)
(* TProofObject.wl: a native, extensible thvm proof object.

   TProofObject[data$] wraps a proof as a plain Association keyed by a
   "ProofType" discriminator ("Equational" today; resolution / congruence
   / string / geometry proofs slot in under the same head later), the
   held Theorem and Axioms, the Variables / Constants signature, the keyed
   proof "Steps" DAG (the same per-step {Statement, Proof -> {Input,
   Construct, Position, Rule, Orientation, Side}} shape the built-in
   ProofObject's ProofDataset uses), and a Status.

   It mirrors enough of the ProofObject property interface to be a drop-in
   (ProofDataset, ProofAssociation, ProofGraph, ProofFunction, ProofLength,
   Theorem, Axioms, Status), and TToProofObject converts it to a genuine
   built-in ProofObject.  TFindProof emits one or the other per the
   "ProofForm" option (default "ProofObject").

   Phase 1 keeps the graph / verification accessors delegating to a
   built-in ProofObject built on the fly; later phases make them native so
   the object no longer leans on ProofObject at all.

   Sibling of ATP.wl in the WolframInstitute`THVMLink`ATP` context, sharing
   WolframInstitute`THVMLink`ATP`Private`. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"GeneralUtilities`", "WolframInstitute`THVMLink`"}];

SetUsage[TProofObject, "TProofObject[data$] is a native thvm proof object: data$ is an Association carrying the proof's 'ProofType', held 'Theorem' and 'Axioms', 'Variables' / 'Constants', the keyed proof 'Steps', and 'Status'.
TProofObject[data$][prop$] returns a property: 'ProofType', 'Theorem' (or 'Theorems'), 'Axioms', 'Variables', 'Constants', 'Status', 'Statistics', 'ProofAssociation', 'ProofDataset', 'ProofLength', 'ProofGraph', 'ProofFunction', or 'Properties'.
It mirrors the built-in ProofObject property interface; TToProofObject[obj$] converts it to a genuine ProofObject.
TFindProof returns a TProofObject instead of a ProofObject when called with the option 'ProofForm' -> 'TProofObject'."];

SetUsage[TToProofObject, "TToProofObject[TProofObject[$$]] converts a thvm TProofObject to the built-in ProofObject it shadows (logic, theorem, axioms, and the proof Association)."];

Begin["`Private`"];

(* === Construction ================================================== *)

(* Adapt a built-in ProofObject (the shape ATP.wl currently emits:
   ProofObject[logic, theorem, axioms, <|Variables, Constants, Proof|>])
   into a TProofObject.  Pure structural lift (no proof rebuilding), so it
   shares the underlying dataset and stays cheap.  ProofObject's "Proof" is
   an ordered list of {Type, n} -> row rules; it is held as an Association
   for keyed access (order is recoverable via Keys / Normal).  $Failed
   passes through so the emitter can stay branch-free. *)
atpProofObjectToTProofObject[po_ProofObject] := Block[{data = po[[4]]},
    TProofObject[<|
        "ProofType" -> "Equational",
        "Logic" -> po[[1]],
        "Theorem" -> po[[2]],
        "Axioms" -> po[[3]],
        "Variables" -> Lookup[data, "Variables", {}],
        "Constants" -> Lookup[data, "Constants", {}],
        "Steps" -> Association @ Lookup[data, "Proof", <||>],
        "Status" -> "Proved"
    |>]
]
atpProofObjectToTProofObject[other_] := other

(* === Conversion back to the built-in ProofObject =================== *)

tproofToProofObject[a_Association] := ProofObject[
    Lookup[a, "Logic", "EquationalLogic"],
    a["Theorem"],
    a["Axioms"],
    <|
        "Variables" -> Lookup[a, "Variables", {}],
        "Constants" -> Lookup[a, "Constants", {}],
        "Proof" -> Normal @ Lookup[a, "Steps", <||>]
    |>
]

TToProofObject[TProofObject[a_Association]] := tproofToProofObject[a]

(* === Property accessors ============================================ *)

$tproofProps = {
    "ProofType", "Theorem", "Theorems", "Axioms", "Variables", "Constants",
    "Status", "Statistics", "ProofAssociation", "ProofDataset", "ProofLength",
    "ProofGraph", "ProofFunction", "ProofObject", "Properties"
};

(* Count the genuine derivation steps: a keyed row with a non-empty Proof
   (Axiom / Hypothesis rows carry an empty Proof and are premises). *)
tproofLength[steps_Association] := Count[
    Values[steps],
    r_ /; AssociationQ[Lookup[r, "Proof", <||>]] && Lookup[r, "Proof", <||>] =!= <||>
]

tproofProp[a_, "ProofType"] := Lookup[a, "ProofType", "Equational"]
tproofProp[a_, "Theorem" | "Theorems"] := a["Theorem"]
tproofProp[a_, "Axioms"] := a["Axioms"]
tproofProp[a_, "Variables"] := Lookup[a, "Variables", {}]
tproofProp[a_, "Constants"] := Lookup[a, "Constants", {}]
tproofProp[a_, "Status"] := Lookup[a, "Status", "Proved"]
tproofProp[a_, "Statistics"] := Lookup[a, "Statistics", <||>]
tproofProp[a_, "ProofAssociation"] := Lookup[a, "Steps", <||>]
tproofProp[a_, "ProofDataset"] := Dataset[Lookup[a, "Steps", <||>]]
tproofProp[a_, "ProofLength"] := tproofLength[Lookup[a, "Steps", <||>]]
tproofProp[a_, "Properties"] := $tproofProps
tproofProp[a_, "ProofObject"] := tproofToProofObject[a]

(* Phase 1: the graph / verifier / notebook lean on a built-in ProofObject
   built from the same data.  Phases 4+ make these native. *)
tproofProp[a_, p : ("ProofGraph" | "ProofFunction" | "ProofNotebook")] :=
    tproofToProofObject[a][p]
tproofProp[a_, other_] := Missing["UnknownProperty", other]

TProofObject[a_Association][prop_String] := tproofProp[a, prop]
TProofObject[a_Association][prop_String, rest__] := tproofProp[a, prop][rest]

(* === Summary box =================================================== *)

TProofObject /: MakeBoxes[obj : TProofObject[a_Association], fmt_] := Block[{
    type = Lookup[a, "ProofType", "Equational"],
    status = Lookup[a, "Status", "Proved"],
    len = tproofLength[Lookup[a, "Steps", <||>]],
    thm = Lookup[a, "Theorem", None]
},
    BoxForm`ArrangeSummaryBox[
        TProofObject,
        obj,
        None,
        {
            BoxForm`SummaryItem[{"Type: ", type}],
            BoxForm`SummaryItem[{"Status: ", status}],
            BoxForm`SummaryItem[{"Steps: ", len}]
        },
        {
            BoxForm`SummaryItem[{"Theorem: ", thm}],
            BoxForm`SummaryItem[{"Axioms: ", Length @ Lookup[a, "Axioms", {}]}]
        },
        fmt
    ]
]

End[];

EndPackage[];
