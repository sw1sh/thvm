(* flux_jit_replay.wlt -- TJit capture/replay correctness on a chained
   multi-root forward (the FLUX.2-klein transformer shape) on Metal.

   Regression for the Metal indirect-command-buffer (ICB) replay bug: a
   chained forward whose blocks realize TWO roots together
   (TRealize[{img, txt}], the FLUX double block) produces a captured replay
   stream in which the scheduler recycles an output buf_id across kernels.
   Batching such a stream into one ICB is unsafe -- two ICB commands writing
   the same physical MTLBuffer have undefined order on Apple GPUs even with
   [cmd setBarrier], and no encoder / command-buffer barrier recovers it.
   The replay then read stale bytes and diverged from the direct forward
   (correlation collapsed; max|d| up to ~0.2 on a 5-block synthetic, far
   worse on the full 25-block transformer).  Capture was wrong too once the
   chain was deep enough.

   Fix (src/jit/capture.c): jit_capture_finalize sets `metal_graph_unsafe` when
   any output buf_id is written by two live captured dispatches;
   jit_replay_try_metal_graph_run then declines the ICB path for that capture
   and replays per-op (still scheduler-free, and Metal hazard-tracks per-op
   dispatches correctly).  Hazard-free streams (distinct output buffers,
   tinygrad's MetalGraph invariant) keep the ICB.

   Model-free: random real-dim-shaped weights, no safetensors.  Skipped on
   non-Metal platforms (TContextNew returns 0).  Multi-trial: the bug's
   manifestation is buffer-layout-dependent, so a single trial can pass by
   luck -- the loop over seeds reliably triggered the pre-fix divergence. *)

(* --- minimal FLUX double-stream block, inlined (the Examples/flux glue is
       not a paclet file).  img0/txt0 {S*, dim}; the joint attention sub-DAG
       (concat q/k/v -> attention -> split) is SHARED by both returned roots,
       which is what makes the bundled TRealize[{img,txt}] recycle buffers. --- *)
ClearAll[fxBlock];
fxBlock[img0_, txt0_, w_, cfg_] := Module[
    {h, dh, eps, scale, simg, stxt, dim, qi, ki, vi, qt, kt, vt,
     q, k, v, ctx, ctxI, ctxT, img, txt},
    h = cfg["heads"];  dh = cfg["head_dim"];  eps = cfg["eps"];
    scale = 1/Sqrt[N[dh]];
    simg = Dimensions[img0][[1]];  stxt = Dimensions[txt0][[1]];  dim = h dh;
    qi = ArrayReshape[TMatMul[TLayerNorm[img0, eps], Transpose[w["wq"]]], {simg, h, dh}];
    ki = ArrayReshape[TMatMul[TLayerNorm[img0, eps], Transpose[w["wk"]]], {simg, h, dh}];
    vi = ArrayReshape[TMatMul[TLayerNorm[img0, eps], Transpose[w["wv"]]], {simg, h, dh}];
    qt = ArrayReshape[TMatMul[TLayerNorm[txt0, eps], Transpose[w["wqt"]]], {stxt, h, dh}];
    kt = ArrayReshape[TMatMul[TLayerNorm[txt0, eps], Transpose[w["wkt"]]], {stxt, h, dh}];
    vt = ArrayReshape[TMatMul[TLayerNorm[txt0, eps], Transpose[w["wvt"]]], {stxt, h, dh}];
    q = Join[qt, qi, 1];  k = Join[kt, ki, 1];  v = Join[vt, vi, 1];
    ctx = THeadAttention[q, k, v, scale];     (* {stxt+simg, dim}, shared by both roots *)
    ctxT = ctx[[1 ;; stxt]];
    ctxI = ctx[[stxt + 1 ;; stxt + simg]];
    img = img0 + TMatMul[ctxI, Transpose[w["wo"]]];
    txt = txt0 + TMatMul[ctxT, Transpose[w["wot"]]];
    {img, txt}];

(* run a chained nB-block forward, then compare TJit replay to the direct
   forward.  Returns {maxdCapture, maxdReplay} (0/0 when correct). *)
ClearAll[fxJitTrial];
fxJitTrial[seed_, nB_, dim_, heads_, hd_, simg_, stxt_] := Module[
    {cfg, rnd, mkW, txt0, fwd, img1, direct, jc, cap, rep},
    cfg = <|"heads" -> heads, "head_dim" -> hd, "eps" -> 1.*^-6|>;
    rnd[d__] := TToDevice[
        TTensorCreate @ NumericArray[RandomReal[{-0.05, 0.05}, {d}], "Real32"],
        "metal"];
    mkW[] := <|
        "wq" -> rnd[dim, dim],  "wk" -> rnd[dim, dim],  "wv" -> rnd[dim, dim],
        "wqt" -> rnd[dim, dim], "wkt" -> rnd[dim, dim], "wvt" -> rnd[dim, dim],
        "wo" -> rnd[dim, dim],  "wot" -> rnd[dim, dim]|>;
    SeedRandom[seed];
    txt0 = rnd[stxt, dim];
    With[{Ws = Table[mkW[], {nB}]},
        fwd = Function[img, Module[{hh = img, tt = txt0, r},
            Do[ r = TRealize @ fxBlock[hh, tt, Ws[[i]], cfg];
                hh = r[[1]];  tt = r[[2]], {i, nB}];
            hh]]];
    img1 = rnd[simg, dim];
    direct = N @ Normal @ TRealize @ fwd[img1];
    jc = TJit[fwd];
    cap = N @ Normal @ jc[img1];   (* capture *)
    rep = N @ Normal @ jc[img1];   (* replay  *)
    {Max[Abs[Flatten[direct - cap]]], Max[Abs[Flatten[direct - rep]]]}];

(* chained 5-block double-stream forward: TJit capture AND replay must match
   the direct forward to float noise across several seeds.  Pre-fix this
   diverged (max|d| up to ~0.2 on replay) for most seeds. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], worst},
        If[ ctx === 0, Return[True]];   (* skip on no-Metal platforms *)
        worst = TInContext[ctx,
            Max @ Flatten @ Table[fxJitTrial[s, 5, 96, 4, 24, 8, 12], {s, {3, 4, 17, 23, 42}}]];
        TContextDestroy[ctx];
        worst < 1.*^-4 || ctx === 0
    ],
    True,
    TestID -> "flux/jit-replay-matches-direct-chained-double-block"
]

(* a hazard-FREE chained forward (each step a single root, distinct output
   buffers) must STILL match -- the fix must not regress the safe ICB path. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], rnd, w, fwd, x0, direct, jc, rep},
        If[ ctx === 0, Return[True]];
        rnd[d__] := TToDevice[
            TTensorCreate @ NumericArray[RandomReal[{0.5, 1.5}, {d}], "Real32"], "metal"];
        rep = TInContext[ctx,
            SeedRandom[1];
            w = Table[rnd[256], {40}];
            fwd = Function[x, Module[{hh = x}, Do[hh = TRealize[(hh w[[i]]) + w[[i]]], {i, 40}]; hh]];
            x0 = rnd[256];
            direct = N @ Normal @ TRealize @ fwd[x0];
            jc = TJit[fwd];  jc[x0];
            Max @ Abs @ Flatten[direct - N @ Normal @ jc[x0]]];
        TContextDestroy[ctx];
        rep < 1.*^-4 || ctx === 0
    ],
    True,
    TestID -> "flux/jit-replay-matches-direct-hazard-free-chain"
]
