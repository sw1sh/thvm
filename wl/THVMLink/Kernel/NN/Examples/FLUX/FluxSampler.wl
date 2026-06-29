(* FluxSampler.wl -- the FLUX.2-klein-4B 4-step flow-matching sampler +
   the RoPE table + timestep sinusoidal, for thvm.

   Generation loop (FlowMatchEulerDiscrete, distilled klein = no CFG):
     z ~ N(0,1) {256,128} (packed latent)
     for k in 0..3:
       v = transformer(z, txt, sigma[k])        (* velocity, {256,128} *)
       z = z + (sigma[k+1] - sigma[k]) * v       (* dt < 0 *)
     image = vae_decode(z)

   The pieces here are weight-free (schedule, sinusoidal timestep basis, the
   4-axis RoPE table); they are host-computed in WL and handed to the
   transformer as TTerms.  The timestep MLP, x/context embedders, and blocks
   live in FluxForward.wl and need the transformer weights. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

Begin["`Private`"];

(* === resolution-shifted sigma schedule ==============================
   diffusers FlowMatchEuler `mu`-shift for klein.  Constants from the FLUX.2
   generate pipeline; `seq` is the image token count (256 for 256x256). *)
fxSigmas[seq_ : 256, nSteps_ : 4] := Module[{a1, b1, a2, b2, m200, m10, a, b, mu, lin, sig},
    a1 = 8.73809524*^-5;  b1 = 1.89833333;
    a2 = 0.00016927;      b2 = 0.45666666;
    m200 = a2 * seq + b2;   m10 = a1 * seq + b1;
    a = (m200 - m10) / 190;  b = m200 - 200 a;
    mu = a * nSteps + b;     (* diffusers compute_empirical_mu: a*num_steps+b (was hardcoded 4) *)
    lin = Subdivide[1.0, 1.0 / nSteps, nSteps - 1];          (* [1, .75, .5, .25] *)
    sig = (Exp[mu] / (Exp[mu] + (1.0 / # - 1.0)))& /@ lin;
    Append[sig, 0.0]
] (* sigmasFull, len nSteps+1 *)

(* === sinusoidal timestep basis (weight-free part of the time embedder) ==
   diffusers get_timestep_embedding: dim 256, cos-first.  The pipeline feeds the
   transformer `timestep = t/1000 = sigma` (pipeline_flux2_klein.py:845), and the
   transformer then rescales `timestep = timestep*1000` (transformer_flux2.py:831)
   BEFORE time_proj -- so the sinusoid argument is sigma*1000.  (Verified: thvm
   TSinusoidalEmbedding[sigma*1000] -> temb MLP std 0.3398 matches the reference
   step-0 temb, and the per-step velocity matches the reference to corr 0.9999.) *)
fxTimestepSinusoid[sigma_, dim_ : 256] :=
    Normal @ TSinusoidalEmbedding[N[sigma * 1000.], dim]     (* {256} cos-first host list *)

(* === 4-axis interleaved RoPE table ==================================
   FLUX.2 FluxPosEmbed: axes_dims {32,32,32,32}, theta 2000.  Token positions
   are 4-vectors; text token i -> {0,0,0,i}, image token (h,w) -> {0,h,w,0}.
   Sequence is text-first: [512 text ; 256 img].  Per axis a, 16 freq pairs
   theta_j = 1/2000^(2j/32); angle = pos[a]*theta_j; the head_dim-128 cos/sin
   tables interleave (cos,sin) per pair, concatenated over the 4 axes. *)
fxRopeTable[gridH_ : 16, gridW_ : 16, nTxt_ : 512, theta_ : 2000., axDim_ : 32] := Module[{half, invFreq, posImg, posTxt, pos, ang, cosT, sinT},
    half = axDim / 2;                                        (* 16 freq pairs per axis *)
    invFreq = (1.0 / theta ^ (2 Range[0, half - 1] / axDim));    (* {16} *)
    posImg = Flatten[Table[{0, h, w, 0}, {h, 0, gridH - 1}, {w, 0, gridW - 1}], 1]; (* {256,4} *)
    posTxt = Table[{0, 0, 0, i}, {i, 0, nTxt - 1}];        (* {512,4} *)
    pos = Join[posTxt, posImg];                            (* {768,4} text-first *)
    (* per token, per axis: outer(pos[a], invFreq) -> {16}; interleave cos/sin *)
    ang = Function[p, Flatten[Table[Riffle[#, #]&[p[[ax]] invFreq], {ax, 4}]]] /@ pos;
    cosT = Cos[ang];  sinT = Sin[ang];                     (* {768,128} *)
    <|"cos" -> cosT, "sin" -> sinT|>
]

(* === the sampler loop ===============================================
   vel is the velocity field (zPacked, temb) -> {256,128}: the TJit-wrapped
   transformer forward (closes over text embeddings, rope, weights).  tembFn
   maps a sigma to its {1,dim} timestep embedding (the time_text_embed MLP over
   fxTimestepSinusoid).  The 4 steps share ONE captured kernel set: TJit
   captures vel on the first call, then each step REPLAYS it, rebinding the two
   changing inputs (z + temb) in place -- the dispatch overhead amortizes
   across the 4 steps (FLUX double block: 213ms non-JIT -> 32ms replay).  dt is
   applied outside the captured graph so it stays a fixed replay. *)
fxSample[vel_, z0_, sigmas_, tembFn_] := Module[{z = z0, vfn, k, dt, v},
    vfn = TJit[vel];
    Do[ dt = sigmas[[k + 1]] - sigmas[[k]];
        v = vfn[z, tembFn[sigmas[[k]]]];
        z = TRealize @ TUOpAdd[z, TUOpMul[v, TUOpConst[N[dt]]]],
        {k, 1, Length[sigmas] - 1}];
    z
]

End[];

EndPackage[];
