#!/usr/bin/env wolframscript
(* atp_ic/debug/bisect_aot_metal.wls -- regression fixture for the
   AOT-Metal off-by-1 in the IC-native ATP search (now fixed).

   B0..B4 all MATCH between CPU and Metal.  B4 was the trigger:
   `TApp[rSup, NUM]` in both elseTerms, where `rSup` is a SUP of
   two TRefs.  Each TRef inlining re-walked the same book template,
   so the auto-dup DP0/DP1 inside the ref'd lambda bodies carried
   identical book dup_locs -- and metal_emit.c's dup memo (keyed on
   dup_loc) aliased inlining #2's projections onto inlining #1's
   GPU dup cell.  Fixed by scoping the dup memo per TAG_REF inline
   (save/restore g_msl_dup_n / g_msl_dup_term_n around the body
   emit).  Kept as a fixture so the off-by-1 can't silently
   regress. *)

PacletDirectoryLoad[FileNameJoin[{Directory[], "wl", "THVMLink"}]];
Needs["WolframInstitute`THVMLink`"];
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
