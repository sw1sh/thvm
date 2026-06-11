(* FluxForward.wl -- a compact FLUX.2-klein-4B forward for thvm.

   The tinygrad `examples/stable_diffusion.py` shape: read the named bf16
   tensors out of the diffusers safetensors and wire ops, looped over the
   block config -- NOT a hand-exported per-block NetGraph.  Get-loaded
   after THVMLink` (uses the public TUOp* surface + one alias to the lazy
   shape).  Architecture + tensor names: docs/flux_forward_spec.md.

   This file holds the verified primitive helpers; the double/single block
   factory + the full transformer forward build on top of them. *)

(* The lazy (graph-time) shape; public TUOp* builders below need it but it
   lives in THVMLink`Private`. *)
fxShape = THVMLink`Private`tUopShape;

(* --- linear: a diffusers weight is stored {out, in}, so y = x . W^T --- *)
fxLinear[x_, w_] := TMatMul[x, Transpose[w]]

(* --- RMSNorm over the last axis (FLUX per-head q/k norm, weight {D}):
       y = x * rsqrt(mean(x^2) + eps) * weight --- *)
fxRMSNorm[x_, weight_, eps_] := Module[{s, nd, d, ms, inv, invB, wB},
    s = fxShape[x];  nd = Length[s];  d = Last[s];
    ms   = TUOpReduce[TUOpMul[x, x], nd - 1, "SUM"];          (* drops last -> rank nd-1 *)
    inv  = TUOpRecip[TUOpSqrt[TUOpAdd[TUOpMul[ms, TUOpConst[N[1./d]]], TUOpConst[N[eps]]]]];
    invB = TUOpExpand[TUOpReshape[inv, Append[Most[s], 1]], s];
    wB   = TUOpExpand[TUOpReshape[weight, Join[ConstantArray[1, nd - 1], {d}]], s];
    TUOpMul[TUOpMul[x, invB], wB]]

(* --- affine-free LayerNorm over the last axis (FLUX block norms use
       elementwise_affine=False; the modulation supplies scale/shift):
       y = (x - mean) * rsqrt(var + eps) --- *)
fxLayerNorm[x_, eps_] := Module[{s, nd, d, mu, muB, xc, var, inv, invB},
    s = fxShape[x];  nd = Length[s];  d = Last[s];
    mu   = TUOpMul[TUOpReduce[x, nd - 1, "SUM"], TUOpConst[N[1./d]]];
    muB  = TUOpExpand[TUOpReshape[mu, Append[Most[s], 1]], s];
    xc   = TUOpAdd[x, TUOpNeg[muB]];
    var  = TUOpMul[TUOpReduce[TUOpMul[xc, xc], nd - 1, "SUM"], TUOpConst[N[1./d]]];
    inv  = TUOpRecip[TUOpSqrt[TUOpAdd[var, TUOpConst[N[eps]]]]];
    invB = TUOpExpand[TUOpReshape[inv, Append[Most[s], 1]], s];
    TUOpMul[xc, invB]]

(* --- concat a list of equal-rank tensors along a 1-indexed axis (thvm has
       no CAT op; place each in its slice via TUOpPad + sum -- the headStitch
       idiom).  Used for joint attention K/V, the RoPE interleave, and QKV
       splits. --- *)
fxConcat[xs_List, axis_] := Module[{rank, widths, offsets, total},
    rank    = Length @ fxShape[First[xs]];
    widths  = fxShape[#][[axis]] & /@ xs;
    offsets = Prepend[Accumulate[Most[widths]], 0];
    total   = Total[widths];
    Fold[TUOpAdd, MapThread[{t, off, w} |-> TUOpPad[t,
        Table[If[a === axis, {off, total - off - w}, {0, 0}], {a, rank}]],
        {xs, offsets, widths}]]]

(* --- interleaved rotary embedding (the FLUX.2 DiT convention,
       use_real_unbind_dim=-1): x{S,H,D}; cos,sin{S,1,D}; pairs (x[2i],x[2i+1])
       rotate to (-x[2i+1], x[2i]).  Validated to 8.5e-8 vs the oracle.
       The Qwen3 text encoder uses the DIFFERENT half-split convention -- do
       not reuse this there. --- *)
fxRoPE[x_, cos_, sin_] := Module[{s, h, d, xr, xe, xo, xrot},
    {s, h, d} = fxShape[x];
    xr   = TUOpReshape[x, {s, h, d/2, 2}];
    xe   = TUOpReshape[TUOpShrink[xr, {{0, s}, {0, h}, {0, d/2}, {0, 1}}], {s, h, d/2}];
    xo   = TUOpReshape[TUOpShrink[xr, {{0, s}, {0, h}, {0, d/2}, {1, 2}}], {s, h, d/2}];
    xrot = TUOpReshape[fxConcat[{TUOpReshape[TUOpNeg[xo], {s, h, d/2, 1}],
                                 TUOpReshape[xe,          {s, h, d/2, 1}]}, 4], {s, h, d}];
    TUOpAdd[TUOpMul[x, TUOpExpand[cos, {s, h, d}]], TUOpMul[xrot, TUOpExpand[sin, {s, h, d}]]]]
