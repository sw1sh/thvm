#!/usr/bin/env wolframscript
(* Run Vampire CLI on a single NotableTheorem TPTP file, parse the
   SZS proof, and emit one TSV line.  Output schema mirrors
   run_thvm_one_conjunct.wls so plots/rollups can join cleanly:

   theory \t thm \t conjunct \t status \t seconds \t derivationLen

   - "conjunct" is always 1/1 here because the TPTP files in
     tools/baselines/vampire_tptp/ are already per-(theory, thm)
     (multi-conjunct theorems get one combined file).
   - "derivationLen" is the parsed SZS derivation length, which
     differs from thvm's "proofLength" (=#axioms + #lemmas + 1) but is
     the closest CLI analogue.
   - Status: PROVED / TimedOut / Failed.

   Usage: run_vampire_process_one.wls <theory> <thm> <TC_seconds> *)
PacletDirectoryLoad["/Users/swish/src/wolfram/AISkills/examples/WolframParser"];
Needs["Wolfram`Parser`"];

args = $ScriptCommandLine;
theory = args[[2]];
thm    = args[[3]];
TC     = ToExpression[args[[4]]];

tptpFile = FileNameJoin[{"tools/baselines/vampire_tptp",
    theory <> "__" <> thm <> ".p"}];

If[ ! FileExistsQ[tptpFile],
    WriteString["stdout", theory, "\t", thm, "\t1/1\tNoFile\t-\t-\n"];
    Exit[1]];

{wallT, runResult} = AbsoluteTiming @ Module[{cmd, out},
    cmd = StringJoin["vampire --mode casc --proof tptp -t ",
        ToString[TC], " ", tptpFile, " 2>&1"];
    out = RunProcess[{"sh", "-c", cmd}, "StandardOutput"];
    out];

(* Parse SZS status + derivation *)
status = Which[
    StringContainsQ[runResult, "SZS status Unsatisfiable"], "PROVED",
    StringContainsQ[runResult, "SZS status Theorem"],       "PROVED",
    StringContainsQ[runResult, "SZS status Timeout"]
        || StringContainsQ[runResult, "Time limit reached"],
        "TimedOut",
    True, "Failed"];

derivationLen = If[ status === "PROVED",
    Module[{parsed},
        parsed = Quiet @ Check[TPTPImport[runResult, "SZS"], $Failed];
        Which[
            AssociationQ[parsed] && KeyExistsQ[parsed, "Derivation"],
                ToString[Length[parsed["Derivation"]]],
            True, "?"]],
    "-"];

WriteString["stdout", theory, "\t", thm, "\t1/1\t",
    status, "\t", ToString[Round[wallT, 0.01]], "\t",
    derivationLen, "\n"];
