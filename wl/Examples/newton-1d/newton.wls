#!/usr/bin/env wolframscript
(* newton.wls -- Newton's method using second-order TGrad.

   Minimize f(x) = 0.5*(x - 3)^2 + 0.25*x^4
       f'(x)  = (x - 3) + x^3
       f''(x) = 1 + 3*x^2

   Newton update: x_new = x - f'(x) / f''(x).  Quadratic convergence
   to a local minimum where f' = 0.

   Demonstrates:
     - first-order TGrad to compute the gradient
     - second-order TGrad applied to the gradient gives the Hessian
       (diagonal in 1-D = the full second derivative)
     - both run via the runtime's symbolic AD; no manual derivatives

   Run:
       wolframscript -f wl/Examples/newton-1d/newton.wls
*)

PacletDirectoryLoad["wl/THVMLink"]
Get["WolframInstitute`THVMLink`"]

(* Build f(x) symbolically as a UOp graph rooted at TEN handle x.
   x is mutated in place across iterations via TAssign so we can
   re-fire the same TGrad expressions each step. *)
makeF[x_TTerm] := Module[{three, half, quarter, diff, sqDiff, fourthPow},
    three   = TUOpExpand[TUOpConst[3.0,  "f32"], {1}];
    half    = TUOpExpand[TUOpConst[0.5,  "f32"], {1}];
    quarter = TUOpExpand[TUOpConst[0.25, "f32"], {1}];
    diff      = TUOpAdd[x, TUOpNeg[three]];                (* x - 3 *)
    sqDiff    = TUOpMul[diff, diff];                       (* (x-3)^2 *)
    fourthPow = TUOpMul[TUOpMul[TUOpMul[x, x], x], x];     (* x^4 *)
    TUOpAdd[ TUOpMul[half, sqDiff], TUOpMul[quarter, fourthPow]]
]

(* ===================================================================
   Step 1: scalar Newton minimization, x scalar shape {1}.
   =================================================================== *)
WriteString["stdout", "=== scalar Newton on f(x) = 0.5*(x-3)^2 + 0.25*x^4 ===\n"];

TInit[];
xInit = TTensorCreate[NumericArray[{0.5}, "Real32"]];
{x0} = Normal[TTensorData[xInit]];
WriteString["stdout", "x0 = " <> ToString[x0] <> "  f(x0) = " <>
    ToString[Normal[TTensorData[TRealize[makeF[xInit]]]][[1]]] <> "\n"];

(* Loop in the host: rebuild f, g, h each step (cheap; the TGrad
   structure is symbolic and gets compiled once per iteration).
   Could fuse with TPri/TLam for in-runtime stepping but keeping
   the host loop here makes the algorithm easy to read. *)
fmtNum[x_] := ToString[CForm[N[x]]]

xCurrent = xInit;
Do[
    Module[{f, g, h, gVal, hVal, fVal, xNext, xN},
        f = makeF[xCurrent];
        g = TGrad[f, xCurrent];
        (* Hessian = grad of grad.  TGrad's default seed (ones at
           target.shape) is correct here -- gives the full second
           derivative for scalar / vector targets with diagonal
           assumption. *)
        h = TGrad[g, xCurrent];
        gVal = Normal[TTensorData[TRealize[g]]][[1]];
        hVal = Normal[TTensorData[TRealize[h]]][[1]];
        fVal = Normal[TTensorData[TRealize[f]]][[1]];
        xN   = Normal[TTensorData[xCurrent]][[1]];
        WriteString["stdout",
            "step " <> StringPadLeft[ToString[step], 2] <>
            "  x=" <> StringPadRight[fmtNum[xN], 10] <>
            "  f=" <> StringPadRight[fmtNum[fVal], 10] <>
            "  g=" <> StringPadRight[fmtNum[gVal], 12] <>
            "  h=" <> StringPadRight[fmtNum[hVal], 10] <> "\n"];
        xNext = xN - gVal / hVal;
        xCurrent = TTensorCreate[NumericArray[{N[xNext]}, "Real32"]]
    ],
    {step, 0, 9}];

WriteString["stdout", "\nfinal x = " <> ToString[Normal[TTensorData[xCurrent]][[1]]] <>
    "  (expected: real root of x + x^3 - 3 = 0, ~1.213)\n"];

(* ===================================================================
   Step 2: vector Newton with diagonal Hessian.  Each component
   optimizes independently because f decomposes elementwise.
   =================================================================== *)
WriteString["stdout", "\n=== vector Newton (elementwise diagonal Hessian) ===\n"];

TInit[];
xInit = TTensorCreate[NumericArray[{0.5, -2.0, 1.0, 0.0}, "Real32"]];
seed  = TUOpExpand[TUOpConst[1.0, "f32"], {4}];
WriteString["stdout", "x0 = " <> ToString[Normal[TTensorData[xInit]]] <> "\n"];

xCurrent = xInit;
Do[
    Module[{f, g, h, gVals, hVals, fVal, xNs, xNew},
        f = makeF[xCurrent];
        g = TGrad[f, xCurrent, seed];
        h = TGrad[g, xCurrent, seed];
        gVals = Normal[TTensorData[TRealize[g]]];
        hVals = Normal[TTensorData[TRealize[h]]];
        fVal  = Normal[TTensorData[TRealize[TUOpReduce[f, 0, "SUM"]]]][[1]];
        xNs   = Normal[TTensorData[xCurrent]];
        WriteString["stdout",
            "step " <> StringPadLeft[ToString[step], 2] <>
            "  sum(f)=" <> fmtNum[fVal] <>
            "  ||g||=" <> fmtNum[Sqrt @ Total[gVals^2]] <> "\n"];
        xNew = MapThread[ #1 - #2 / #3 &, {xNs, gVals, hVals}];
        xCurrent = TTensorCreate[NumericArray[N[xNew], "Real32"]]
    ],
    {step, 0, 7}];

WriteString["stdout", "\nfinal x = " <> ToString[Normal[TTensorData[xCurrent]]] <>
    "  (each entry should converge to ~1.213)\n"];
