#!/usr/bin/env wolframscript
(* atp_ic/debug/bisect_aot_metal.wls -- bisect probe for the residual
   AOT-Metal off-by-1 in the IC-native ATP search.

   See docs/plans/atp_ic_native.md "Findings from milestone-3" for
   the analysis and two attack angles.

   B0..B3 all MATCH between CPU and Metal.  B4 mismatches (cpu
   {0,0,0,0} vs metal {0,0,1,1}) -- adding `TApp[rSup, NUM]` in the
   elseTerms is what triggers the bug.  Use this file as a fixture
   to track down the regression once the runtime / encoder fix
   lands. *)

PacletDirectoryLoad[FileNameJoin[{Directory[], "wl", "THVMLink"}]];
Needs["THVMLink`"];
TInitContext[];

probe[name_, term_] := Module[{cpu, metal},
    TDef[name, term];
    cpu = Sort[FromTTerm /@ TCollapse[TRef[name]]];
    metal = Sort[FromTTerm /@ TCollapse @ TAOTRun[name, {}, Method -> "Metal"]];
    WriteString["stdout", name, ":  cpu=", ToString[cpu],
        "  metal=", ToString[metal],
        If[ cpu === metal, "  MATCH", "  MISMATCH"], "\n"];
];

(* Two atomic rewrite TDefs (1 -> 2 forward, 2 -> 1 backward). *)
With[{v = vfwd}, TDef["fwd", TLam[v, TIfZero[TOp2["==", v, TNum[1]], v, TNum[2]]]]];
With[{v = vbwd}, TDef["bwd", TLam[v, TIfZero[TOp2["==", v, TNum[2]], v, TNum[1]]]]];

(* Shared SUPs at distinct labels (sideSup at 100, rSup at 200). *)
sideSup = TSup[100, TNum[0], TNum[1]];
rSup    = TSup[200, TRef["fwd"], TRef["bwd"]];

(* B0: OP2 of two SUPs at same label -- annihilation, expect 1==1=1. *)
probe["B0_op2_two_sups",
    TOp2["==", sideSup, sideSup]];

(* B1: TIfZero with SUP scrutinee, simple branches. *)
probe["B1_ifz_sup_simple",
    TIfZero[sideSup, TNum[5], TNum[7]]];

(* B2: TIfZero gated by OP2-of-SUP, simple branches. *)
probe["B2_ifz_op2_simple",
    TIfZero[TOp2["==", sideSup, TNum[0]], TNum[5], TNum[7]]];

(* B3: two TIfZeros sharing sideSup, OP2'd -- still match. *)
probe["B3_two_ifz_op2",
    TOp2["==",
         TIfZero[TOp2["==", sideSup, TNum[0]], TNum[5], TNum[7]],
         TIfZero[TOp2["==", sideSup, TNum[1]], TNum[6], TNum[8]]]];

(* B4: B3 with TApp[rSup, NUM] in else branches.  TRIGGERS the bug. *)
probe["B4_two_ifz_with_app",
    TOp2["==",
         TIfZero[TOp2["==", sideSup, TNum[0]], TNum[5], TApp[rSup, TNum[5]]],
         TIfZero[TOp2["==", sideSup, TNum[1]], TNum[6], TApp[rSup, TNum[6]]]]];
