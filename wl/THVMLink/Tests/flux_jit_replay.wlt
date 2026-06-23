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

(* A multi-input capture Function[{img, txt}, ...] must REBIND a changed txt on
   warm replay (the second JIT input, feeding the block matmuls like the FLUX
   context embedding).  Capture on txtA, replay on a very different txtB: the
   replay must match a direct forward on txtB AND differ from the txtA capture.
   This isolates that single-shot multi-input rebind is correct -- the residual
   warm-prompt-collapse below is the per-step-loop interaction, not this.
   Model-free; skipped on non-Metal. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], cfg, rnd, mkW, Ws, fwd, imgA, txtA, txtB,
            directB, jc, cap, rep, repVsDirectB, repVsCap},
        If[ ctx === 0, Return[True]];
        cfg = <|"heads" -> 4, "head_dim" -> 24, "eps" -> 1.*^-6|>;
        rnd[lo_, hi_, d__] := TToDevice[
            TTensorCreate @ NumericArray[RandomReal[{lo, hi}, {d}], "Real32"], "metal"];
        mkW[] := <|
            "wq" -> rnd[-0.05, 0.05, 96, 96],  "wk" -> rnd[-0.05, 0.05, 96, 96],
            "wv" -> rnd[-0.05, 0.05, 96, 96],  "wqt" -> rnd[-0.05, 0.05, 96, 96],
            "wkt" -> rnd[-0.05, 0.05, 96, 96], "wvt" -> rnd[-0.05, 0.05, 96, 96],
            "wo" -> rnd[-0.05, 0.05, 96, 96],  "wot" -> rnd[-0.05, 0.05, 96, 96]|>;
        {repVsDirectB, repVsCap} = TInContext[ctx,
            SeedRandom[11];
            Ws = Table[mkW[], {3}];
            fwd = Function[{img, txt}, Module[{hh = img, tt = txt, r},
                Do[ r = TRealize @ fxBlock[hh, tt, Ws[[i]], cfg];
                    hh = r[[1]];  tt = r[[2]], {i, 3}];
                hh]];
            imgA = rnd[-0.05, 0.05, 8, 96];
            txtA = rnd[-0.05, 0.05, 12, 96];
            txtB = rnd[4.95, 5.05, 12, 96];
            directB = N @ Normal @ TRealize @ fwd[imgA, txtB];
            jc = TJit[fwd];
            cap = N @ Normal @ jc[imgA, txtA];
            rep = N @ Normal @ jc[imgA, txtB];
            {Max @ Abs @ Flatten[rep - directB], Max @ Abs @ Flatten[rep - cap]}];
        TContextDestroy[ctx];
        repVsDirectB < 1.*^-4 && repVsCap > 1.*^-3 || ctx === 0
    ],
    True,
    TestID -> "flux/jit-replay-rebinds-changed-text-encoding-single-shot"
]

(* REGRESSION (KNOWN-FAILING, weights-gated) for the FLUX.2-klein warm-prompt-
   collapse.  A warm FluxGenerate -- a second prompt on a cached session, OR any
   prompt after the first in a batch -- decodes the FIRST prompt's image content:
   "a blue bird" generated after "a red apple on a table" (same session) comes out
   an APPLE.  Each stage's JIT rebinds correctly in isolation (verified: the qwen
   encodings differ, a single velJit call rebinds enc, the vaeJit rebinds the
   latent), so the collapse is the per-step Euler loop's multi-input replay
   interaction in the velocity capture -- a C-level capture/input-replace issue
   (src/jit/capture.c), NOT WL-fixable.  Left failing per "write a normal failing
   test" so whoever fixes capture.c has a check.  Skipped without the weights. *)
VerificationTest[
    TInit[]; TReset[];
    Needs["WolframInstitute`THVMLink`Examples`"];
    Module[{md = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b", apple, bird},
        If[ ! FileExistsQ @ FileNameJoin[{md, "transformer",
                "diffusion_pytorch_model.safetensors"}], Return[True]];   (* skip: no weights *)
        (* COLD captures the velocity on the apple prompt; the WARM bird replay must
           NOT collapse to the apple -- a clearly different image (different prompt). *)
        apple = FluxGenerate["a red apple on a table",
            "ImageSize" -> {128, 128}, RandomSeeding -> 0, ProgressReporting -> False];
        bird = FluxGenerate["a blue bird",
            "ImageSize" -> {128, 128}, RandomSeeding -> 0, ProgressReporting -> False];
        (* a real apple vs a real bird differ across most of the frame; the collapse
           makes them near-identical (only seed-noise apart, max|d| ~ the bf16 floor). *)
        Max @ Abs @ Flatten[ImageData[apple] - ImageData[bird]] > 0.4
    ],
    True,
    TestID -> "flux/warm-prompt-rebinds-text-encoding"
]
