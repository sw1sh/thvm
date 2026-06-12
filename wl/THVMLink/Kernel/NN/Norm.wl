(* ::Package:: *)
(* NN/Norm.wl - normalization layers: LayerNorm (+ affine), RMSNorm,
   GroupNorm, and BatchNorm (inference + training form). *)

BeginPackage["THVMLink`"];

GeneralUtilities`SetUsage[TLayerNorm, "TLayerNorm[x$] and TLayerNorm[x$, eps$] normalise along the last axis: y = (x$ - mean) / sqrt(var + eps$). Default eps$ = 1e-5. mean and var are per-row reductions broadcast back via the softmax-style reduce-broadcast pattern."];
GeneralUtilities`SetUsage[TLayerNormAffine, "TLayerNormAffine[x$, gamma$, beta$] applies TLayerNorm[x$] then multiplies by gamma$ and adds beta$ along the last axis. GPT-2's layer-norm carries learned gamma/beta; bare TLayerNorm normalises only."];
GeneralUtilities`SetUsage[TLayerNormAffine, "TLayerNormAffine[x$, gamma$, beta$] applies TLayerNorm[x$] then multiplies by gamma$ and adds beta$ along the last axis. GPT-2's layer-norm carries learned gamma/beta; bare TLayerNorm normalises only."];
GeneralUtilities`SetUsage[TRMSNorm, "TRMSNorm[x$, weight$, eps$] is RMS normalisation over the last axis: y = x$ * rsqrt(mean(x$^2) + eps$) * weight$, with weight$ a rank-1 {D} gain over the feature axis. The per-head q/k norm of FLUX / Qwen3 attention and the LLaMA-style pre-norm. No mean-subtraction (unlike LayerNorm)."];
GeneralUtilities`SetUsage[TGroupNorm, "TGroupNorm[x$, weight$, bias$, groups$, eps$] is GroupNorm over a {C, H, W} tensor: split C into groups$ groups, normalise each group over its (C/groups$, H, W) elements, then apply a per-channel affine weight$/bias$ ({C}). The VAE / U-Net spatial normalisation."];
GeneralUtilities`SetUsage[TBatchNorm, "TBatchNorm[x$, gamma$, beta$, mean$, var$] and TBatchNorm[x$, gamma$, beta$, mean$, var$, eps$] apply the inference-form batch-norm y = gamma$ * (x$ - mean$) / sqrt(var$ + eps$) + beta$ along the channel axis. x$ is rank-3 {C, H, W} or rank-4 {B, C, H, W}; gamma$, beta$, mean$, var$ are all rank-1 {C}. Default eps$ = 1e-5."];
GeneralUtilities`SetUsage[TBatchNormTrain, "TBatchNormTrain[x$, gamma$, beta$] and TBatchNormTrain[x$, gamma$, beta$, eps$] compute batch-norm using statistics from x$, then apply per-channel gamma$ and beta$. Supports rank-3 {C, H, W} and rank-4 {B, C, H, W}."];

Begin["`Private`"];

(* TLayerNorm[x, eps] -- normalize along the last axis.
       mean    = sum(x)         / N        scalar (per "row" if rank > 1)
       var     = sum((x-mean)^2) / N
       y       = (x - mean) / sqrt(var + eps)

   For a rank-1 input {N}, both reductions collapse to true scalars.
   For higher rank, reductions drop the last axis and we reshape +
   EXPAND back to broadcast across that axis (same shape pattern as
   TSoftmax).

   Two REDUCEs (mean + var) means the Phase-3 single-REDUCE relaxation
   doesn't fire; layernorm currently materializes mean as its own
   kernel.  The variance + normalise tail then fuses into one kernel
   via the same softmax-style relaxation when var's broadcast chain
   ends in an EXPAND (which it does -- (x - mean) feeds into a
   square-sum-recip-sqrt-broadcast-mul). *)
TLayerNorm[x_TTerm] := TLayerNorm[x, 1.0*^-5]
TLayerNorm[x_TTerm, eps_ ? NumericQ] := With[{shape = tUopShape[x]},
    Module[{rank, axis, n, sumShape, broadcast, mean, centered, var},
        rank      = Length[shape];
        axis      = rank - 1;
        n         = shape[[rank]];
        sumShape  = ReplacePart[shape, rank -> 1];
        broadcast = TUOpExpand[TUOpReshape[#, sumShape], shape] &;
        mean      = broadcast[TUOpReduce[x, axis, "SUM"] / n];
        centered  = x - mean;
        var       = broadcast[TUOpReduce[centered * centered, axis, "SUM"] / n];
        centered / Sqrt[var + eps]
    ]
]

TLayerNormAffine[x_TTerm, gamma_TTerm, beta_TTerm] := With[{
    shape = tUopShape[x],
    norm  = TLayerNorm[x]
},
    Module[{rank, gammaB, betaB, broadcastShape},
        rank = Length[shape];
        broadcastShape = ConstantArray[1, rank];
        broadcastShape[[rank]] = shape[[rank]];
        (* Reshape rank-1 {D} -> {1,...,1,D} then EXPAND to full
           shape.  Without the explicit EXPAND, the elementwise
           numel-cycle in MUL/ADD reads gamma/beta in the wrong
           stride pattern across the leading axes and silently
           produces denormals.  Tested at seq=4, dim=16: bare
           {seq,dim}*{1,dim} returns garbage; EXPAND[{1,dim} ->
           {seq,dim}] is correct. *)
        gammaB = TUOpExpand[TUOpReshape[gamma, broadcastShape], shape];
        betaB  = TUOpExpand[TUOpReshape[beta,  broadcastShape], shape];
        norm * gammaB + betaB
    ]
]

TLayerNormAffine[x_TTerm, gamma_TTerm, beta_TTerm] := With[{
    shape = tUopShape[x],
    norm  = TLayerNorm[x]
},
    Module[{rank, gammaB, betaB, broadcastShape},
        rank = Length[shape];
        broadcastShape = ConstantArray[1, rank];
        broadcastShape[[rank]] = shape[[rank]];
        (* Reshape rank-1 {D} -> {1,...,1,D} then EXPAND to full
           shape.  Without the explicit EXPAND, the elementwise
           numel-cycle in MUL/ADD reads gamma/beta in the wrong
           stride pattern across the leading axes and silently
           produces denormals.  Tested at seq=4, dim=16: bare
           {seq,dim}*{1,dim} returns garbage; EXPAND[{1,dim} ->
           {seq,dim}] is correct. *)
        gammaB = TUOpExpand[TUOpReshape[gamma, broadcastShape], shape];
        betaB  = TUOpExpand[TUOpReshape[beta,  broadcastShape], shape];
        norm * gammaB + betaB
    ]
]

(* TRMSNorm[x, weight, eps]: RMS norm over the last axis (the FLUX / Qwen3
   per-head q/k norm and the LLaMA pre-norm), weight a rank-1 {D} gain:
       y = x * rsqrt(mean(x^2) + eps) * weight
   No mean-subtraction.  mean(x^2) drops the last axis; reshape + EXPAND it
   (and the weight) back to broadcast over the feature axis. *)
TRMSNorm[x_TTerm, weight_TTerm, eps_ ? NumericQ] := With[{shape = tUopShape[x]},
    Module[{rank, d, sumShape, ms, inv, invB, wB},
        rank     = Length[shape];
        d        = Last[shape];
        sumShape = ReplacePart[shape, rank -> 1];
        ms       = TUOpReduce[x * x, rank - 1, "SUM"] / d;
        inv      = 1 / Sqrt[ms + eps];
        invB     = TUOpExpand[TUOpReshape[inv, sumShape], shape];
        wB       = TUOpExpand[TUOpReshape[weight, Append[ConstantArray[1, rank - 1], d]], shape];
        x * invB * wB
    ]
]

(* TGroupNorm[x, weight, bias, groups, eps]: GroupNorm over {C, H, W}:
   split C into `groups` groups, normalise each group over its
   (C/groups, H, W) elements, then per-channel affine.  The VAE / U-Net
   spatial normalisation. *)
TGroupNorm[x_TTerm, weight_TTerm, bias_TTerm, groups_Integer, eps_ ? NumericQ] :=
    With[{shape = tUopShape[x]},
    Module[{c, h, w, gsz, xg, mu, muB, xc, var, inv, invB, normed, wB, bB},
        {c, h, w} = shape;
        gsz    = c * h * w / groups;
        xg     = TUOpReshape[x, {groups, gsz}];
        mu     = TUOpReduce[xg, 1, "SUM"] / gsz;                  (* {groups} *)
        muB    = TUOpExpand[TUOpReshape[mu, {groups, 1}], {groups, gsz}];
        xc     = xg - muB;
        var    = TUOpReduce[xc * xc, 1, "SUM"] / gsz;
        inv    = 1 / Sqrt[var + eps];
        invB   = TUOpExpand[TUOpReshape[inv, {groups, 1}], {groups, gsz}];
        normed = TUOpReshape[xc * invB, shape];                   (* {C,H,W} *)
        wB     = TUOpExpand[TUOpReshape[weight, {c, 1, 1}], shape];
        bB     = TUOpExpand[TUOpReshape[bias,   {c, 1, 1}], shape];
        normed * wB + bB
    ]
]

(* TBatchNorm[x, gamma, beta, mean, var, eps] -- inference form.
       y[c, h, w] = gamma[c] * (x[c, h, w] - mean[c]) / sqrt(var[c] + eps)
                  + beta[c]
   Refactored so the per-channel scale/shift collapse to scalar
   computations once each: scale = gamma / sqrt(var + eps),
   shift = beta - mean * scale.  Then both broadcast across {H, W}
   as one MUL + one ADD on the rank-3 tensor.  Rank-1 ops on {C}
   stay tiny; the rank-3 ops fuse cleanly with the JIT path. *)
TBatchNorm[x_TTerm, gamma_TTerm, beta_TTerm, mean_TTerm, var_TTerm] :=
    TBatchNorm[x, gamma, beta, mean, var, 1.0*^-5]
TBatchNorm[x_TTerm, gamma_TTerm, beta_TTerm, mean_TTerm, var_TTerm,
           eps_ ? NumericQ] := With[{shape = tUopShape[x]},
    Module[{rank, b, c, h, w, scaleC, shiftC, bcastShape, scaleBcast, shiftBcast},
        rank = Length[shape];
        If[ rank === 3,
            c = shape[[1]];
            h = shape[[2]];
            w = shape[[3]];
            bcastShape = {c, 1, 1},
            If[ rank === 4,
                b = shape[[1]];
                c = shape[[2]];
                h = shape[[3]];
                w = shape[[4]];
                bcastShape = {1, c, 1, 1},
                Return @ Failure["NotImplemented",
                    <|"Message" -> "TBatchNorm expects rank-3 or rank-4 input",
                      "InputShape" -> shape|>]
            ]
        ];
        scaleC     = gamma / Sqrt[var + eps];
        shiftC     = beta  - mean * scaleC;
        scaleBcast = TUOpExpand[TUOpReshape[scaleC, bcastShape], shape];
        shiftBcast = TUOpExpand[TUOpReshape[shiftC, bcastShape], shape];
        x * scaleBcast + shiftBcast
    ]
]

TBatchNormTrain[x_TTerm, gamma_TTerm, beta_TTerm] :=
    TBatchNormTrain[x, gamma, beta, 1.0*^-5]
TBatchNormTrain[x_TTerm, gamma_TTerm, beta_TTerm, eps_ ? NumericQ] :=
    With[{shape = tUopShape[x]},
    Module[{rank, b, c, h, w, reduceCount, bcastShape, mean, centered, var,
            broadcast},
        rank = Length[shape];
        If[ rank === 3,
            c = shape[[1]];
            h = shape[[2]];
            w = shape[[3]];
            reduceCount = h * w;
            bcastShape = {c, 1, 1};
            mean = TUOpReduce[TUOpReduce[x, 2, "SUM"], 1, "SUM"] / reduceCount,
            If[ rank === 4,
                b = shape[[1]];
                c = shape[[2]];
                h = shape[[3]];
                w = shape[[4]];
                reduceCount = b * h * w;
                bcastShape = {1, c, 1, 1};
                mean = TUOpReduce[
                    TUOpReduce[
                        TUOpReduce[x, 3, "SUM"],
                        2, "SUM"],
                    0, "SUM"] / reduceCount,
                Return @ Failure["NotImplemented",
                    <|"Message" -> "TBatchNormTrain expects rank-3 or rank-4 input",
                      "InputShape" -> shape|>]
            ]
        ];
        broadcast[t_] := TUOpExpand[TUOpReshape[t, bcastShape], shape];
        centered = x - broadcast[mean];
        var = If[ rank === 3,
            TUOpReduce[TUOpReduce[centered * centered, 2, "SUM"], 1, "SUM"]
                / reduceCount,
            TUOpReduce[
                TUOpReduce[
                    TUOpReduce[centered * centered, 3, "SUM"],
                    2, "SUM"],
                0, "SUM"] / reduceCount
        ];
        centered / Sqrt[broadcast[var] + eps] * broadcast[gamma]
            + broadcast[beta]
    ]
]

End[];

EndPackage[];
