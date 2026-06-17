#!/usr/bin/env wolframscript
(* atp_ic/metal.wls -- IC-native ATP search dispatched on AOT-Metal.

   Same battery as transitivity.wls + substitutivity.wls but each
   buildSearchTerm is registered as a TDef and dispatched via
   TAOTRun[..., Method -> "Metal"].  CPU runs the same TDef as a
   sanity check; results must match. *)

PacletDirectoryLoad[FileNameJoin[{Directory[], "wl", "THVMLink"}]];
Needs["WolframInstitute`THVMLink`"];
TInitContext[];

(* Reuse the encoders from substitutivity.wls (it covers atomic +
   structured cases). *)
Get["wl/Examples/atp_ic/substitutivity.wls"];

(* Run one case via both CPU and Metal; compare. *)
runOnBoth[case_, idx_] := Module[{
    name = case["Name"], cnj = case["Conjecture"], axs = case["Axioms"],
    d = case["Depth"], expected = case["Expected"],
    defName, term, cpuLeaves, metalRoot, metalLeaves, cpuAny, metalAny
},
    defName = "atp_metal_" <> ToString[idx];
    term = buildSearchTerm[cnj, axs, d];
    TDef[defName, term];
    cpuLeaves = FromTTerm /@ TCollapse[TRef[defName]];
    cpuAny = AnyTrue[cpuLeaves, # === 1 &];
    metalRoot = Quiet @ Check[
        TAOTRun[defName, {}, Method -> "Metal"],
        $Failed];
    metalLeaves = If[ metalRoot === $Failed, {},
        FromTTerm /@ TCollapse[metalRoot]];
    metalAny = AnyTrue[metalLeaves, # === 1 &];
    Print[StringPadRight[name, 38],
        "  cpu=", cpuAny,
        "  metal=", metalAny,
        "  exp=", expected,
        If[ cpuAny === expected && metalAny === expected, "  OK", "  FAIL"]];
    cpuAny === expected && metalAny === expected
]

(* Compact subset that's tractable for Metal at modest depth. *)
metalCases = {
    <|"Name" -> "atomic transitivity-3 (d=2)",
      "Conjecture" -> {a, c},
      "Axioms" -> {{a, b}, {b, c}},
      "Depth" -> 2,
      "Expected" -> True|>,
    <|"Name" -> "atomic symmetry (d=1)",
      "Conjecture" -> {b, a},
      "Axioms" -> {{a, b}},
      "Depth" -> 1,
      "Expected" -> True|>,
    <|"Name" -> "atomic unprovable (d=2)",
      "Conjecture" -> {a, d},
      "Axioms" -> {{a, b}, {b, c}},
      "Depth" -> 2,
      "Expected" -> False|>,
    <|"Name" -> "f-substitutivity-1pos (d=1)",
      "Conjecture" -> {f[a], f[b]},
      "Axioms" -> {{a, b}},
      "Depth" -> 1,
      "Expected" -> True|>,
    <|"Name" -> "f-binary-2pos (d=2)",
      "Conjecture" -> {f[a, b], f[c, d]},
      "Axioms" -> {{a, c}, {b, d}},
      "Depth" -> 2,
      "Expected" -> True|>,
    <|"Name" -> "head-mismatch (vacuous)",
      "Conjecture" -> {f[a], g[a]},
      "Axioms" -> {{a, b}},
      "Depth" -> 2,
      "Expected" -> False|>
};

Print[""];
Print["=== IC-native ATP via AOT-Metal ==="];
results = MapIndexed[runOnBoth[#1, First[#2]] &, metalCases];
Print[""];
Print["Pass: ", Count[results, True], "/", Length[metalCases]];
