#!/usr/bin/env wolframscript
(* kernel-introspect.wls -- end-to-end walkthrough of thvm's
   kernel-pipeline introspection surface.

   Every stage of the materialise + dispatch pipeline is observable
   from WL.  Starting from a small TTerm expression we walk through:

       1. UOP graph         (TUOpKind / TUOpSrcs)
       2. KernelEntry        (TKernel object, TKernelInfo, TKernelTable)
       3. KProgOp[] program  (TKernelProgram, KIn / KOp source tags)
       4. Axis structure     (TKernelOpts, Opt[...] typed actions)
       5. Rendered source    (TKernelSource / TKernelSource[k, backend])
       6. Memory plan        (TMemoryPlan, TCpuBufTable, TMemoryPlanReport)
       7. Dispatch profile   (TKernelDispatchKind, TKernelTotalUs,
                              TKernelDispatchCount, TKernelFlops)

   Plus the per-axis Opt rewrites that mirror tinygrad's
   `Kernel.apply_opt(Opt(OptOps.UPCAST, axis, 4))`:

       UPCAST / UNROLL split a range N into N//arg + arg,
                       marking the inner axis with the opt type.
       LOCAL / GROUP   split a global / reduce axis into shared-mem
                       intermediates (GPU equivalent).
       SWAP            swap two global axis indices.

   The C-side codegen consumer is tracked in docs/bench/history.md
   as part of the kernel-variant/autotune arc; this demo exercises
   the WL surface today.

   Run from repo root:
       wolframscript wl/Examples/kernel-introspect.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

indent[s_String, prefix_String] := StringJoin @@ Riffle[
    Map[prefix <> # &, StringSplit[s, "\n", All]], "\n"]

SeedRandom[42];

(* ===========================================================
   Stage 0: build a TTerm expression
   =========================================================== *)

print["=== Stage 0: TTerm expression  ==="];

a = TGlorot[{8, 16}];
b = TGlorot[{8, 16}];
c = TGlorot[{8, 16}];

(* (a + b) * c via the WL UpValues -- lowers to TUOpMul + TUOpAdd. *)
expr = (a + b) * c;
print["  expression: (a + b) * c   (a, b, c : {8, 16})"];
print["  expr term head: ", Head[expr]];
print["  expr UOP kind:  ", TUOpKind[expr]];
print[""];

(* ===========================================================
   Stage 1: UOP graph
   =========================================================== *)

print["=== Stage 1: UOP graph (TUOpKind / TUOpSrcs)  ==="];

walkUop[t_] := With[{kind = TUOpKind[t]},
    print["  ", Head[t], " ", kind];
    Module[{srcs = TUOpSrcs[t]},
        Do[ print["    src: ", Head[s], " ", TUOpKind[s]], {s, srcs}]]
];
walkUop[expr];
print[""];

(* ===========================================================
   Stage 2: KernelEntry (materialise without firing)
   =========================================================== *)

print["=== Stage 2: KernelEntry (TMaterialize + TKernel)  ==="];

mat = TMaterialize[expr];
k   = TKernel[mat];
kid = k["Kid"];

print["  kid:           ", kid];
print["  OutputShape:   ", k["OutputShape"]];
print["  OutputDtype:   ", k["OutputDtype"]];
print["  InputCount:    ", k["InputCount"]];
print["  OpCount:       ", k["OpCount"]];
print["  TKernelTable[[kid]]: ", TKernelTable[][[kid]]];
print["  TKernelInputs[kid]:  ", TKernelInputs[kid]];
print[""];

(* ===========================================================
   Stage 3: KProgOp[] program (linearised SSA)
   =========================================================== *)

print["=== Stage 3: KProgOp[] program (TKernelProgram)  ==="];

prog = TKernelProgram[k];
Do[
    With[{op = prog[[i]]},
        print["  [", i, "] ", op["Op"],
              "  src=", op["Sources"],
              "  arg=", op["Arg"],
              "  numel=", op["Numel"]]],
    {i, Length[prog]}];
print[""];

(* ===========================================================
   Stage 4: Axis structure (TKernelOpts + Opt[] rewrites)
   =========================================================== *)

print["=== Stage 4: Axis structure (TKernelOpts + Opt[...])  ==="];

reportOpts[label_String, kid_Integer] := Module[{o = TKernelOpts[kid]},
    print["  ", label, ":"];
    print["    AxisTypes: ", o["AxisTypes"]];
    print["    FullShape: ", o["FullShape"]];
    print["    Applied:   ", o["Applied"]]];

reportOpts["default", kid];

(* Apply UPCAST: split axis 1 (size 16) into outer 4 + inner UPCAST 4 *)
TKernelApplyOpt[kid, Opt["UPCAST", 1, 4]];
reportOpts["after Opt[\"UPCAST\", 1, 4]", kid];

(* Apply UNROLL: split axis 0 (size 8) into outer 4 + inner UNROLL 2 *)
TKernelApplyOpt[kid, Opt["UNROLL", 0, 2]];
reportOpts["after Opt[\"UNROLL\", 0, 2]", kid];

(* Reduce kernel for completeness *)
print[""];
red = TMaterialize @ TUOpReduce[a, 1, "SUM"];
kr  = TKernel[red];
reportOpts["reduce kernel", kr["Kid"]];
print[""];

(* ===========================================================
   Stage 5: Rendered source
   =========================================================== *)

print["=== Stage 5: Rendered source (TKernelSource)  ==="];

srcC = TKernelSource[kid, "C"];
print["  C source (length ", StringLength[srcC], "):"];
If[ StringLength[srcC] > 0,
    print[indent[srcC, "    | "]],
    print["    (interpreter fallback -- program contains REDUCE / movement)"]];

print[""];
srcM = TKernelSource[kid, "Metal"];
print["  Metal source (length ", StringLength[srcM], "):"];
If[ StringLength[srcM] > 0,
    print[indent[srcM, "    | "]],
    print["    (single-op shader fallback today)"]];
print[""];

(* ===========================================================
   Stage 6: Memory plan (per-buf alloc / last-use intervals)
   =========================================================== *)

print["=== Stage 6: Memory plan (TMemoryPlan + CPU buf table)  ==="];

mp = TMemoryPlan[];
mpData = First[mp];

print["  total live CPU bytes: ", TTotalBufBytes[]];
print["  TenDesc count:        ", TTensCount[]];
print["  CPU buf table rows:   ", Length[TCpuBufTable[]]];
print["  Bufs in TMemoryPlan:  ", Length[mpData["Bufs"]]];
print[""];
print["  per-buf snapshot (first 5 rows):"];
Module[{rows = Take[mpData["Bufs"], UpTo[5]]},
    Do[
        With[{r = rows[[i]]},
            print["    buf=", r["buf_id"],
                  "  nbytes=", r["nbytes"],
                  "  alloc_depth=", r["alloc_depth"],
                  "  last_use=", r["last_use_depth"],
                  "  status=", r["status"]]],
        {i, Length[rows]}]];
print[""];
print[indent[ToString[TMemoryPlanReport[mp]], "    "]];
print[""];

(* ===========================================================
   Stage 7: Dispatch profile (after firing)
   =========================================================== *)

print["=== Stage 7: Dispatch profile (after firing)  ==="];

(* Fire the materialised kernel + read its profile.  k[] dispatches
   via TWnf on the kernel term.  Per-kid counters tick on dispatch. *)
k[];   (* fire kid *)
prof = TKernelProfile[kid];

print["  fired kid:       ", kid];
print["  DispatchKind:    ", prof["DispatchKind"]];
print["  DispatchCount:   ", prof["DispatchCount"]];
print["  Flops:           ", prof["Flops"]];
print["  TotalUs:         ", prof["TotalUs"], " us"];
print["  AvgUs:           ", NumberForm[prof["AvgUs"], {6, 2}], " us"];
print["  GFlopsPerSec:    ", NumberForm[prof["GFlopsPerSec"], {6, 2}]];
print["  JitDylibPath:    ", prof["JitDylibPath"]];
print[""];

(* TProfileAll[] -- profile snapshot keyed by kid for every live kernel. *)
all = TProfileAll[];
print["  TProfileAll[] keys (live kids): ", Sort @ Keys[all]];
print[""];

print["PASS: every kernel-pipeline stage is observable from WL"];
