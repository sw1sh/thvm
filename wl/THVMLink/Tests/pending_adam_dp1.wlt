(* pending_adam_dp1.wlt -- known-pending TAdam regression on the
   rank-1 single-param degenerate L2-loss case.

   TGradMany returns a TAG_DP1 (interaction-net duplicate projection,
   single fanout) for each gradient.  TAdam's bundled-TRealize then
   references that grad twice within one Module body
   (`(1-beta1)*g` for the m update and `(1-beta2)*g*g` for the v
   update), but DP1 silently no-ops on the second use -- so this
   degenerate case produces w/m/v all unchanged (delta = 0.19 =
   |0 - 0.19|).  Real training (full beautiful_mnist forward + Adam
   step) routes the chain rule through a richer DAG where the
   DP1 multi-use never bites; that path works end-to-end -- weights
   update, kernels capture (~57 per step), warm-step wallclock ~50ms
   on M3 Max at BS=128.  Whether loss converges is a separate
   question tracked in the parity benchmark.

   Fix candidates:
     (a) TGradMany returns TAG_TEN -- pre-buffer each gradient so
         multi-use is unbounded.  Naive `TRealize /@ TGradMany[...]`
         multiplies peak memory by Length[params] (each per-grad
         realize re-walks the forward intermediates without dedup).
     (b) Pre-buffer in TAdam itself via a bundled `MapThread[TAssign,
         {gradBufs, rawGrads}]` + TRealize.  Tried 2026-05-12; the
         bundled-assign path makes TJit replay leak ~14x peak memory
         (~4.9 GB at BS=128 vs ~230 MB normally) and produces NaN at
         step 2 because the captured kernel sequence ends up
         referencing freshly-allocated TZerosLike buffers instead of
         the JIT-captured ones.

   This file is run as INFORMATIONAL by the WL test runner -- failing
   cases here do NOT flip the overall exit code (see run.wls). *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

VerificationTest[
    TInit[];
    w   = TTensorCreate @ NumericArray[{1.0}, "Real32"];
    tgt = TTensorCreate @ NumericArray[{0.05}, "Real32"];
    m = TTensorCreate @ NumericArray[{0.0}, "Real32"];
    v = TTensorCreate @ NumericArray[{0.0}, "Real32"];
    loss = TL2Loss[w - tgt];
    TAdam[loss, {w}, {m}, {v}, 1];
    Max @ Abs @ Flatten @ {
        Normal @ TTensorData[w] - {0.999},
        Normal @ TTensorData[m] - {0.19},
        Normal @ TTensorData[v] - {0.00361}
    },
    _ ? (# < 1.0*^-5 &),
    SameTest -> MatchQ,
    TestID -> "pending/adam-rank1-single-param-dp1-fanout"
]
