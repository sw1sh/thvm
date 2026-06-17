(* jit_persistent_weight_lifetime.wlt -- a TJit capture's recorded weight
   buffer must survive the capture->finalize boundary even under recycle-
   freelist pressure that hard-frees a decref'd-to-zero buffer.

   Regression for the FLUX nil-binding bug (src/jit/capture.c
   jit_capture_finalize): recording incref+jit_pins every dispatch's input
   buffer, then finalize UNCONDITIONALLY released the whole recorded retain set
   (unpin + decref) before re-retaining the live ops' buffers.  A weight whose
   MTLBuffer was held ONLY by that recording incref (the TenDesc->buffer link is
   NOT refcounted, and the transient realize tensor that also named it rolled
   back at end-of-realize) dropped to refcount 0 during that window; with the
   recycle freelist tight/full, metal_buf_decref HARD-FREED the slot, and the
   subsequent re-retain's metal_buf_jit_pin no-op'd on the now-nil buffer.
   Replay then bound a nil MTLBuffer at the matmul's weight input -- aborting
   under Metal API validation, or silently reading stale/zero bytes otherwise.
   The fix (jit_capture_release_retained_except) keeps a still-needed buffer's
   recording incref + pin in place across finalize, so replay binds the live
   weight and matches the direct forward.

   THVM_METAL_FREELIST_BYTES (set tight here) is the freelist-pressure lever:
   it forces metal_buf_decref to hard-free a decref'd-to-zero buffer instead of
   parking it, reproducing the FLUX cold-capture condition.  The chained
   double-stream block (concat q/k/v -> attention -> split, shared by both
   returned roots) is the FLUX transformer shape whose bundled TRealize[{img,
   txt}] recycles buffers and drives the weight refcount to 0 at finalize.
   Pre-fix this diverged (replay read a freed weight); the fix keeps it bound.

   Model-free: random real-dim-shaped weights, no safetensors.  Skipped on
   non-Metal platforms (TContextNew returns 0). *)

(* minimal FLUX double-stream block: the joint-attention sub-DAG is SHARED by
   both returned roots, so the bundled TRealize[{img,txt}] recycles buffers. *)
ClearAll[pwBlock];
pwBlock[img0_, txt0_, w_, cfg_] := Module[
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
    ctx = THeadAttention[q, k, v, scale];     (* shared by both roots *)
    ctxT = ctx[[1 ;; stxt]];
    ctxI = ctx[[stxt + 1 ;; stxt + simg]];
    img = img0 + TMatMul[ctxI, Transpose[w["wo"]]];
    txt = txt0 + TMatMul[ctxT, Transpose[w["wot"]]];
    {img, txt}];

(* chained nB-block forward; TJit replay must match the direct forward.  Under
   tight freelist the pre-fix release hard-freed a weight the replay still
   needed -> divergence.  Returns max|direct - replay| (0 when correct). *)
ClearAll[pwTrial];
pwTrial[seed_, nB_, dim_, heads_, hd_, simg_, stxt_] := Module[
    {cfg, rnd, mkW, txt0, fwd, img1, direct, jc, rep},
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
            Do[ r = TRealize @ pwBlock[hh, tt, Ws[[i]], cfg];
                hh = r[[1]];  tt = r[[2]], {i, nB}];
            hh]]];
    img1 = rnd[simg, dim];
    direct = N @ Normal @ TRealize @ fwd[img1];
    jc = TJit[fwd];
    jc[img1];                                    (* capture + finalize *)
    rep = N @ Normal @ jc[img1];                 (* replay -- must not read a freed weight *)
    Max[Abs[Flatten[direct - rep]]]];

VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], worst},
        If[ ctx === 0, Return[True]];            (* skip on no-Metal platforms *)
        SetEnvironment["THVM_METAL_FREELIST_BYTES" -> "1000"];
        worst = TInContext[ctx,
            Max @ Flatten @ Table[pwTrial[s, 5, 96, 4, 24, 8, 12], {s, {3, 4, 17}}]];
        SetEnvironment["THVM_METAL_FREELIST_BYTES" -> None];
        TContextDestroy[ctx];
        NumberQ[worst] && worst < 1.*^-4
    ],
    True,
    TestID -> "jit/persistent-weight-survives-finalize-under-freelist-pressure"
]
