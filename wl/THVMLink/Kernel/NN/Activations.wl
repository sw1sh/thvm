(* ::Package:: *)
(* NN/Activations.wl - elementwise activations and the mask-composed
   selection ops (tinygrad's where / max / min / clamp, plus relu /
   sigmoid / tanh / softmax / gelu / silu).

   thvm has no WHERE or MAX opcode; the same 0/1 comparison masks
   TReLU uses compose them. *)

BeginPackage["WolframInstitute`THVMLink`", {"GeneralUtilities`"}];

SetUsage[TReLU, "TReLU[x$] is elementwise max(x$, 0), lowered as x$ times a 0/1 less-than mask of x$ against 0."];
SetUsage[TWhere, "TWhere[cond$, a$, b$] selects a$ where the 0/1 mask cond$ is 1 and b$ where it is 0, lowered as cond$*a$ + (1 - cond$)*b$ (thvm has no WHERE opcode; cond$ is a comparison mask such as a$ < b$). The mask carries no gradient, so a$ gets cond$ and b$ gets 1 - cond$, matching a true ternary select. Keep both branches finite: the unselected branch is multiplied by 0, so an Inf/NaN there poisons the result."];
SetUsage[TMaximum, "TMaximum[a$, b$] is the elementwise maximum, lowered as TWhere[a$ < b$, b$, a$] over thvm's CMPLT mask; b$ may be a scalar (it broadcasts). Also installed as the binary Max UpValue on TTerms. Mirrors tinygrad's Tensor.maximum."];
SetUsage[TMinimum, "TMinimum[a$, b$] is the elementwise minimum, lowered as TWhere[a$ < b$, a$, b$]. Also installed as the binary Min UpValue on TTerms. Mirrors tinygrad's Tensor.minimum."];
SetUsage[TClip, "TClip[x$, lo$, hi$] clamps x$ into [lo$, hi$] elementwise, lowered as TWhere[x$ < lo$, lo$, x$] then TWhere[hi$ < x$, hi$, x$] (tinygrad's clamp: lower bound then upper). Either bound may be None or +/-Infinity to skip it; lo$/hi$ are scalars. Also installed as Clip[x$, {lo$, hi$}] and Clip[x$] (= TClip[x$, -1, 1])."];
SetUsage[TTanh, "TTanh[x$] is elementwise tanh, computed as 2*TSigmoid[2*x$] - 1. The negated-exponent sigmoid form stays finite and differentiable for all x$ (saturating to +-1), avoiding the NaN of the (e^(2x)-1)/(e^(2x)+1) form on large arguments. No tanh primitive is needed."];
SetUsage[TSigmoid, "TSigmoid[x$] = 1 / (1 + e^(-x$)), the numerically stable logistic sigmoid (negated exponent so large |x$| saturates to 0 or 1 instead of overflowing). Matches tinygrad's sigmoid lowering."];
SetUsage[TSiLU, "TSiLU[x$] = x$ * TSigmoid[x$], the SiLU / swish activation (the SwiGLU MLP gate). Mirrors tinygrad's Tensor.silu."];
SetUsage[TSwiGLU, "TSwiGLU[h$] is the SwiGLU activation on a {$$, 2*m} tensor: split the last axis into halves x1$ | x2$ and return TSiLU[x1$] * x2$, a {$$, m} tensor (the Flux2/LLaMA gated MLP activation).
TSwiGLU[x$, wIn$, wOut$] is the projected form linear_out(SwiGLU(linear_in(x$))) with diffusers {out, in} weights, i.e. TSwiGLU[x$ @ Transpose[wIn$]] @ Transpose[wOut$]."];
SetUsage[TSoftmax, "TSoftmax[x$] is exp(x$ - max(x$)) / sum(exp(x$ - max(x$))) over the last axis (the class / key axis).
TSoftmax[x$, axis$] reduces along axis$ (0-indexed).
Numerically stable (max-subtract) and grad-correct: every reduce is re-broadcast with an explicit expand so the chain rule keeps the softmax cross-coupling term."];
SetUsage[TLog, "TLog[x$] is the elementwise natural log, lowered as log2(x$) * ln(2) since the runtime has log2 but no native log. Kept as a public alias; new code should write Log[x$]."];
SetUsage[TGELU, "TGELU[x$] applies the tanh-form GELU approximation 0.5 * x$ * (1 + tanh(sqrt(2/pi) * (x$ + 0.044715 * x$^3))). Composes via TTanh; no new opcode required."];

Begin["`Private`"];

TReLU[x_TTerm] := x * (0 < x)

(* === elementwise selection (mask-composed; tinygrad's where/max/min/clamp) ===
   thvm has no WHERE or MAX opcode, but the same 0/1 comparison masks TReLU
   uses compose them: TWhere[c, a, b] = c*a + (1 - c)*b picks a where the
   mask c is 1.  The mask is a comparison (CMPLT/CMPEQ) so it carries no
   gradient, which is exactly a true WHERE's rule -- a gets c, b gets 1 - c.
   Caveat: the unselected branch is multiplied by 0, so an Inf/NaN there
   poisons the result (0*Inf = NaN); keep both branches finite (thvm's
   attention mask uses a large finite bias, not -Inf, for this reason). *)
TWhere[cond_TTerm, a_, b_] := cond * a + (1 - cond) * b

(* maximum via the where-from-mask form (tinygrad maximum is the MAX alu;
   minimum is -maximum(-a,-b) -- the mask form is equivalent and reuses
   CMPLT).  One operand may be a scalar; it broadcasts through the mask. *)
TMaximum[a_TTerm, b_] := TWhere[a < b, b, a]
TMaximum[a_, b_TTerm] := TWhere[a < b, b, a]
TMinimum[a_TTerm, b_] := TWhere[a < b, a, b]
TMinimum[a_, b_TTerm] := TWhere[a < b, a, b]

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

(* tanh(x) = 2 * sigmoid(2x) - 1 = 2 / (1 + e^(-2x)) - 1.
   Tinygrad's lowering (mixin/elementwise.py tanh -> sigmoid): sigmoid is
   1 / (1 + e^(-x)), whose exponent is negated so it never returns
   Inf/Inf = NaN.  For x -> +inf, e^(-2x) -> 0 and tanh -> +1; for
   x -> -inf, e^(-2x) -> +inf and 2/inf - 1 -> -1.  The naive
   (e^(2x)-1)/(e^(2x)+1) form overflows to NaN once 2x leaves the f32 exp
   range (~88), which GPT-2's tanh-form GELU hits because its argument
   grows cubically.  This form saturates cleanly AND is fully
   differentiable (no sign / abs), so it stays grad-correct for training. *)
TSigmoid[x_TTerm] := 1 / (1 + Exp[-x])
TTanh[x_TTerm] := 2 * TSigmoid[2 * x] - 1
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

(* softmax(x)_i = exp(x_i) / sum(exp(x)).  Every reduce result is
   re-broadcast to x's shape with an EXPLICIT TUOpExpand (the
   reduced axis re-introduced as a unit dim first), never the
   kernel-level numel-cycle broadcast in MUL/SUB: the chain rule
   for MUL/SUB has no notion of the implicit broadcast and drops
   the cross-coupling term in d(CE)/dz = probs - target.  The
   max-subtract is grad-transparent (softmax is shift-invariant and
   the EXPAND grad rule's REDUCE-along-broadcast-axes makes the
   subtracted term's contribution cancel) so we keep it for
   numerical stability -- random-init networks (LeNet) overflow exp
   without it. *)
TSoftmax[x_TTerm] := TSoftmax[x, Length[tUopShape[x]] - 1]

TSoftmax[x_TTerm, axis_Integer] := With[{shape = tUopShape[x]},
    Module[{m, xc, e, sumShape},
        sumShape = ReplacePart[shape, axis + 1 -> 1];
        m = TUOpExpand[TUOpReshape[TUOpReduce[x, axis, "MAX"], sumShape], shape];
        xc = x - m;
        e = Exp[xc];
        e * TUOpExpand[1 / TUOpReshape[TUOpReduce[e, axis, "SUM"], sumShape], shape]
    ]
]

TGELU[x_TTerm] := With[{sqrt2OverPi = Sqrt[2.0 / Pi] (* ~ 0.7978845608... *), halfCubeC = 0.044715},
    0.5 * x * (1 + TTanh[sqrt2OverPi * (x + halfCubeC * x^3)])
]

End[];

EndPackage[];
