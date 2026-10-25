(* ::Package:: *)
(* NN/Activations.wl - elementwise activations and selections (tinygrad's
   where / maximum / minimum / clamp, relu / sigmoid / tanh / softmax / gelu /
   silu), built by the C ops of src/nn (docs/NN/nn.md) through nnCall. *)

BeginPackage["WolframInstitute`THVMLink`", {"GeneralUtilities`"}];

SetUsage[TReLU, "TReLU[x$] is elementwise max(x$, 0), tinygrad's relu: (x$ > 0).where(x$, 0)."];
SetUsage[TWhere, "TWhere[cond$, a$, b$] selects a$ where cond$ is nonzero and b$ elsewhere, a true select (tinygrad's Tensor.where): an Inf or NaN in the unselected branch never reaches the result. a$ and b$ may be scalars; all three broadcast. The mask carries no gradient."];
SetUsage[TMaximum, "TMaximum[a$, b$] is the elementwise maximum (tinygrad's Tensor.maximum), selects on a$ < b$; at a tie each side gets half the gradient. Either side may be a scalar. Also installed as the binary Max UpValue on TTerms."];
SetUsage[TMinimum, "TMinimum[a$, b$] is the elementwise minimum, -TMaximum[-a$, -b$] (tinygrad's Tensor.minimum). Also installed as the binary Min UpValue on TTerms."];
SetUsage[TClip, "TClip[x$, lo$, hi$] clamps x$ into [lo$, hi$] elementwise, lowered as TWhere[x$ < lo$, lo$, x$] then TWhere[hi$ < x$, hi$, x$] (tinygrad's clamp: lower bound then upper). Either bound may be None or +/-Infinity to skip it; lo$/hi$ are scalars. Also installed as Clip[x$, {lo$, hi$}] and Clip[x$] (= TClip[x$, -1, 1])."];
SetUsage[TTanh, "TTanh[x$] is elementwise tanh, computed as 2*TSigmoid[2*x$] - 1 (tinygrad's tanh), finite and differentiable for all x$."];
SetUsage[TSigmoid, "TSigmoid[x$] = 1 / (1 + e^(-x$)), the numerically stable logistic sigmoid (negated exponent so large |x$| saturates to 0 or 1 instead of overflowing). Matches tinygrad's sigmoid lowering."];
SetUsage[TSiLU, "TSiLU[x$] = x$ * TSigmoid[x$], the SiLU / swish activation (the SwiGLU MLP gate). Mirrors tinygrad's Tensor.silu."];
SetUsage[TSwiGLU, "TSwiGLU[h$] is the SwiGLU activation on a {$$, 2*m} tensor: split the last axis into halves x1$ | x2$ and return TSiLU[x1$] * x2$, a {$$, m} tensor (the Flux2/LLaMA gated MLP activation).
TSwiGLU[x$, wIn$, wOut$] is the projected form linear_out(SwiGLU(linear_in(x$))) with diffusers {out, in} weights, i.e. TSwiGLU[x$ @ Transpose[wIn$]] @ Transpose[wOut$]."];
SetUsage[TSoftmax, "TSoftmax[x$] is exp(x$ - max(x$)) / sum(exp(x$ - max(x$))) over the last axis (the class / key axis).
TSoftmax[x$, axis$] reduces along axis$ (0-indexed; a negative axis counts from the end)."];
SetUsage[TLog, "TLog[x$] is the elementwise natural log, lowered as log2(x$) * ln(2) since the runtime has log2 but no native log. Kept as a public alias; new code should write Log[x$]."];
SetUsage[TGELU, "TGELU[x$] applies the tanh-form GELU approximation 0.5 * x$ * (1 + tanh(sqrt(2/pi) * (x$ + 0.044715 * x$^3))), tinygrad's gelu."];

Begin["`Private`"];

TReLU[x_TTerm] := nnCall[TReLU, "relu", {x}]

(* A scalar operand as a constant in the dtype of the tensor beside it. *)
nnLift[v_TTerm, _] := v
nnLift[v_, ref_TTerm] := liftNumeric[v, inheritDType[ref]]

(* Tensor.where picks the scalars' dtype from x, else y, else cond. *)
TWhere[cond_TTerm, a_, b_] := With[{ref = Which[Head[a] === TTerm, a, Head[b] === TTerm, b, True, cond]},
    nnCall[TWhere, "where", {cond, nnLift[a, ref], nnLift[b, ref]}]
]

TMaximum[a_TTerm, b_] := nnCall[TMaximum, "maximum", {a, nnLift[b, a]}, {0}]
TMaximum[a_, b_TTerm] := nnCall[TMaximum, "maximum", {nnLift[a, b], b}, {0}]
TMinimum[a_TTerm, b_] := nnCall[TMinimum, "maximum", {a, nnLift[b, a]}, {1}]
TMinimum[a_, b_TTerm] := nnCall[TMinimum, "maximum", {nnLift[a, b], b}, {1}]

(* clamp(x, lo, hi): lower bound then upper, per tinygrad's clamp.  A None
   or +-Infinity bound is skipped so a one-sided clamp is TClip[x, lo, Infinity]. *)
clipLo[x_, lo_] := If[lo === None || lo === -Infinity, x, TWhere[x < lo, lo, x]]
clipHi[x_, hi_] := If[hi === None || hi === Infinity, x, TWhere[hi < x, hi, x]]
TClip[x_TTerm, lo_, hi_] := clipHi[clipLo[x, lo], hi]

(* Idiomatic UpValues: binary Max / Min and Clip over TTerms. *)
TTerm /: Max[a_TTerm, b_] := TMaximum[a, b]
TTerm /: Min[a_TTerm, b_] := TMinimum[a, b]
TTerm /: Clip[x_TTerm] := TClip[x, -1, 1]
TTerm /: Clip[x_TTerm, {lo_, hi_}] := TClip[x, lo, hi]

TSigmoid[x_TTerm] := nnCall[TSigmoid, "sigmoid", {x}]
TTanh[x_TTerm] := nnCall[TTanh, "tanh", {x}]
TSiLU[x_TTerm] := x * TSigmoid[x]

(* SwiGLU: split the last axis into halves and gate.  The seq-major {S, 2m}
   case uses idiomatic Part slicing (1-indexed inclusive spans). *)
TSwiGLU[h_TTerm] := With[{shape = tUopShape[h]},
    With[{m = Last[shape] / 2},
        TSiLU[h[[All, 1 ;; m]]] * h[[All, m + 1 ;; 2 m]]
    ]
]
TSwiGLU[x_TTerm, wIn_TTerm, wOut_TTerm] := TSwiGLU[x . Transpose[wIn]] . Transpose[wOut]

(* TLog is kept as a public alias; new code should write Log[x]. *)
TLog[x_TTerm] := Log[x]

TSoftmax[x_TTerm] := TSoftmax[x, -1]
TSoftmax[x_TTerm, axis_Integer] := nnCall[TSoftmax, "softmax", {x}, {axis, 0}]

TGELU[x_TTerm] := nnCall[TGELU, "gelu", {x}]

End[];

EndPackage[];
