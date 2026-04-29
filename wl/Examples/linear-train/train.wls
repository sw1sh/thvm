#!/usr/bin/env wolframscript
(* train.wls -- one linear layer trained by SGD entirely on the IC
   runtime: a single TMaterialize compiles the whole step body
   (forward + backward + ASSIGNs + per-iter loss callback) into fused
   kernels; a recursive TDef/TRef loop fires those kernels N times
   via wnf, with per-iteration state mutated in place by UOP_ASSIGN.
   No host loop, no per-step re-materialize, no Compile dependency. *)

PacletDirectoryLoad["wl/THVMLink"]
Get["THVMLink`"]
SeedRandom[7]

(* === setup ============================================================ *)
{nF, nC} = {4, 3}
x      = TTensorCreate[{1.0, 0.2, -0.1, 0.0}]
target = TTensorCreate[{0.5, -0.3, 0.7}]
w      = TTensorCreate[RandomVariate[NormalDistribution[0., Sqrt[2.0 / nF]], {nC, nF}]]
b      = TTensorCreate[ConstantArray[0., nC]]
lr     = TUOpConst[0.1, "f32"]

(* === step body via UpValue arithmetic + MAT-CTR-destructured
       TGradMany.  The multi-GRAD lowers to an anonymous CTR of n
       unary GRADs sharing the y subgraph by reference; APP-MAT-CTR
       destructures the CTR into the body's bound names via N nested
       APP-LAM betas.  No projection opcode -- pure IC primitives. == *)
loss      = TMSELoss[TMatVec[w, x] + b, target]
{gW, gB}  = TGradMany[loss, {w, b}];
trainStep = TPri[
    Function[lt, WriteString["stdout",
        "loss=", CForm @ TTensorData[lt][[1]], "\n"]],
    loss,
    TPriForce[TAssign[w, w - lr * gW],
              TAssign[b, b - lr * gB]]]

(* === ONE TMaterialize over the whole APP-PRI tree, inlined into the
       recursive loop body (no separate "step" def needed).  k - 1 uses
       the numeric UpValue from Switch.wl. =============================== *)
TDef["loop",
    TLam[k, TIfZero[k, TUOpConst[0., "f32"],
        TPriForce[TMaterialize @ TNf @ trainStep,
                  TRef["loop"][k - 1]]]]]

TWnf @ TRef["loop"][TNum[20]]

(* === kernel info dump =============================================== *)
WriteString["stdout", "\nfused kernels: ", TKernelCount[] - 1, "\n"]
Do[
    With[{info = THVMLink`Private`decodeKernelInfo[k][[1]]},
        WriteString["stdout",
            "  k", k,
            " inputs=",   ToString @ TKernelInputs[k],
            " ops=",      info["OpCount"],
            " out_numel=", info["OutputNumel"], "\n"]],
    {k, TKernelCount[] - 1}]
