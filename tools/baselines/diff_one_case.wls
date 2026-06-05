#!/usr/bin/env wolframscript
(* Single-case diff harness -- one (theory, theorem, preset) cell
   per wolframscript invocation.  Stdout: a single TSV row.

   Invocation:
     diff_one_case.wls THEORY THM PRESET [CLI_BUILDER]

   PRESET:        "Waldmeister" / "VampireUEQ" / "Automatic" (the
                  thvm-side Method).  ToExpression'd so Automatic
                  becomes the Symbol.
   CLI_BUILDER:   "wm" (default; uses TWaldmeisterProofObject) or
                  "vampire" (TVampireProofObject).

   Designed for shell-loop parallelism + memory isolation -- each
   case gets a fresh kernel, so a hard theorem blowing up memory
   exits cleanly instead of killing a multi-case sweep. *)
SetDirectory["/Users/swish/src/thvm"];
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
$RecursionLimit = 16384;

(* Pull the canon stack from the main harness as a library. *)
Get["tools/baselines/diff_proofobject_lib.wl"];

If[ Length[$ScriptCommandLine] < 4,
    Print["usage: diff_one_case.wls THEORY THM PRESET [wm|vampire]"];
    Exit[2]];

theory     = $ScriptCommandLine[[2]];
thm        = $ScriptCommandLine[[3]];
presetArg  = $ScriptCommandLine[[4]];
cliKind    = If[ Length[$ScriptCommandLine] >= 5,
    $ScriptCommandLine[[5]], "wm"];

presetMethod = If[ presetArg === "Automatic",
    Automatic, presetArg];
cliBuilder = Switch[cliKind,
    "wm",      THVMLink`ATP`TWaldmeisterProofObject,
    "vampire", THVMLink`ATP`TVampireProofObject,
    _,         THVMLink`ATP`TWaldmeisterProofObject];

(* Tiny warmup so the C ATP DLL loads. *)
Quiet @ TFindProof["InverseOfInverse", "GroupAxioms",
    Method -> "Completion", TimeConstraint -> 2.0];

pP = Quiet @ TFindProof[thm, theory,
    Method -> presetMethod, TimeConstraint -> 12];
pC = Quiet @ cliBuilder[theory, thm,
    TimeConstraint -> 15, "ParseFormulas" -> True];

result = THVMBaselines`runDiff[pP, pC];

(* Emit one TSV row. *)
Print[
    StringJoin @@ Riffle[
        {presetArg, theory, thm,
         ToString[result["PresetN"]],
         ToString[result["CliN"]],
         ToString[result["Matched"]],
         ToString[result["PresetOnly"]],
         ToString[result["CliOnly"]]},
        "\t"]];
