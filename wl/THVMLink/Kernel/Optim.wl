(* ::Package:: *)
(* Optim.wl - optimizer constructors that produce recursive lambda
   terms (TLam + TDef + TRef + TIfZero) suitable for TRealize.

   Mirrors the SGD-as-lambda pattern from wl/THVMLink/Tests/sgd.wlt.

   Public surface
     TOptim["SGD",  lr][gradFn, w0, n]
                       -- SGD recursion: w_{k+1} = w_k - lr * gradFn(w_k)
                          for n iterations.  Returns a TTerm; TRealize
                          to evaluate.
     TOptim["Adam", lr, beta1, beta2, eps][gradFn, w0, n]
                       -- Adam recursion over n iterations.  Returns a
                          TTerm; TRealize to evaluate.

   gradFn is a Wolfram Function that takes a TTerm w and returns the
   loss gradient w.r.t. w as a TTerm UOp graph.
*)

BeginPackage["THVMLink`"];

TOptim::usage = "TOptim[\"SGD\", lr] returns a function {gradFn, w0, n} -> TTerm that, when realised, performs n SGD steps from w0 with learning rate `lr`.  TOptim[\"Adam\", lr, beta1, beta2, eps] likewise for Adam.  gradFn is a host-side function w_TTerm -> TTerm carrying the gradient w.r.t. w.";

TAdam::usage = "TAdam[loss, params, m, v, b1pow, b2pow, opts] applies one Adam step in tensor-land.\n\nArguments:\n    loss    -- TTerm scalar; the value being minimised.  Grads w.r.t.\n               every entry of `params` are computed internally via\n               TGrad[loss, params] (one shared requires_grad backward\n               walk) and realized together via TRealize[grads].\n    params  -- List of TTerm tensor handles (TAG_TEN) to update.\n    m, v    -- Same-shape running first/second moment buffers (TTerm\n               tensors); seed with `TZerosLike /@ params`.\n    b1pow,  -- Shape-{1} scalar buffers carrying beta1^t / beta2^t.\n    b2pow      Seed each to 1.0 (e.g. `TOnes[{1}]`).  Each step advances\n               them in-graph (b1pow := beta1*b1pow), so bias correction\n               (1 - beta^t) is read from the LIVE buffer rather than\n               baked in as a compile-time constant.  This keeps the\n               step correct under TJit: the advance is a captured\n               UOP_ASSIGN that re-runs on every replay, so t is never\n               frozen at capture time.  Tinygrad analogue:\n               LAMB.b1_t / b2_t (tinygrad/nn/optim.py:152,158-159,163).\n    opts    -- Hyperparameters as Wolfram options.  Defaults:\n                   \"lr\"    -> 0.001\n                   \"beta1\" -> 0.9\n                   \"beta2\" -> 0.999\n                   \"eps\"   -> 1.0*^-8\n\nThe per-param body is the textbook Adam update built as a lazy\nUOP_ASSIGN chain:\n    b1pow := beta1*b1pow ;  b2pow := beta2*b2pow\n    m := beta1*m + (1-beta1)*grad\n    v := beta2*v + (1-beta2)*grad*grad\n    w := w - lr * (m/(1-b1pow)) / (sqrt(v/(1-b2pow)) + eps)\nReturns `params` for chainability.\n\nThe legacy form TAdam[loss, params, m, v, t_Integer, opts] folds the\nbias correction in at emit time -- correct only outside TJit; under\nTJit it freezes t at capture.  Use the b1pow/b2pow buffer form when\nthe step is JIT-captured.";

(* Forward-declare symbols owned by later-loading siblings (Ref.wl,
   Switch.wl, Tensor.wl).  Without this, bare references to TDef /
   TIfZero / TRef / TOp2 / TNum / TSet below would resolve to phantom
   symbols in THVMLink`Private` (since those symbols don't yet exist
   on the $ContextPath when this file is Get'd in alphabetical order),
   leading to silently broken term construction at call time. *)
{TDef, TRef, TIfZero, TOp2, TNum, TMatNum, TSet};

Begin["`Private`"];

$optimDefCounter = 0

freshOptimDefName[algo_String] := (
    $optimDefCounter += 1;
    algo <> "_loop_" <> ToString[$optimDefCounter]
)

(* SGD body: build the recursive lambda + invocation as one TTerm. *)
sgdRecursiveTerm[gradFn_, lr_, w0_, n_] :=
    Module[{defName = freshOptimDefName["sgd"], w, k},
        TDef[defName,
            TLam[w, TLam[k,
                TIfZero[k, w,
                    TApp[
                        TApp[TRef[defName],
                            TUOpAdd[w, TUOpNeg[TUOpMul[lr, gradFn[w]]]]],
                        TOp2["-", k, TNum[1]]]]]]];
        TApp[TApp[TRef[defName], w0], TNum[n]]
    ]

(* === Adam ===

   Threads (w, m, v, b1pow, b2pow, k) through the recursion.  By
   carrying b1pow = beta1^t and b2pow = beta2^t as state, bias
   correction (1 - beta^t) needs no POW UOP -- each iter just
   multiplies by the corresponding beta.

   On entry: w = w0; m, v = zerosLike(w0); b1pow0 = beta1, b2pow0 =
   beta2 (so the FIRST step uses (1 - beta^1) = (1 - beta) for bias
   correction, matching the textbook Adam definition). *)
adamRecursiveTerm[gradFn_, lr_, beta1_, beta2_, eps_, w0_, n_] :=
    Module[{
        defName = freshOptimDefName["adam"],
        w, m, v, b1pow, b2pow, k,
        b1c, b2c, omb1, omb2, epsC, oneC, m0, v0
    },
        b1c  = TUOpConst[N @ beta1];        b2c  = TUOpConst[N @ beta2];
        omb1 = TUOpConst[N[1.0 - beta1]];   omb2 = TUOpConst[N[1.0 - beta2]];
        epsC = TUOpConst[N @ eps];          oneC = TUOpConst[1.0];
        m0   = TZerosLike[w0];              v0   = TZerosLike[w0];

        TDef[defName,
            TLam[w, TLam[m, TLam[v, TLam[b1pow, TLam[b2pow, TLam[k,
                TIfZero[k, w,
                    With[{g = gradFn[w]},
                    With[{
                        mNew = TUOpAdd[TUOpMul[b1c, m], TUOpMul[omb1, g]],
                        vNew = TUOpAdd[TUOpMul[b2c, v], TUOpMul[omb2, TUOpMul[g, g]]]
                    },
                    With[{
                        mHat = TUOpMul[mNew, TUOpRecip[TUOpAdd[oneC, TUOpNeg[b1pow]]]],
                        vHat = TUOpMul[vNew, TUOpRecip[TUOpAdd[oneC, TUOpNeg[b2pow]]]]
                    },
                    With[{
                        update = TUOpMul[lr,
                                    TUOpMul[mHat,
                                        TUOpRecip[TUOpAdd[TUOpSqrt[vHat], epsC]]]]
                    },
                        TApp[TApp[TApp[TApp[TApp[TApp[TRef[defName],
                            TUOpAdd[w, TUOpNeg[update]]],
                            mNew], vNew],
                            TUOpMul[b1pow, b1c]],
                            TUOpMul[b2pow, b2c]],
                            TOp2["-", k, TNum[1]]]
                    ]]]]
                ]
            ]]]]]]];

        TApp[TApp[TApp[TApp[TApp[TApp[TRef[defName],
            w0], m0], v0],
            b1c], b2c],
            TNum[n]]
    ]

(* === Public dispatch.  Pattern: TOptim[name, hyperparams] returns a
   Function that the user invokes with (gradFn, w0, n). === *)

TOptim["SGD", lr_TTerm] :=
    {gradFn, w0, n} |-> sgdRecursiveTerm[gradFn, lr, w0, n]

TOptim["Adam", lr_TTerm, beta1_, beta2_, eps_] :=
    {gradFn, w0, n} |-> adamRecursiveTerm[gradFn, lr, beta1, beta2, eps, w0, n]

(* === TAdam: TAssign-form Adam (graph-resident) ===

   Each call returns a flat list of TAssign TTerms (m, v, then w per
   param, in that order so Realize'ing in sequence sees the freshly-
   computed m and v when computing m_hat / v_hat).  The math sits
   inside one TUOpAdd / TUOpMul chain per buffer.

   Bias correction (1 - beta^t).  The step counter t lives in two
   shape-{1} scalar buffers b1pow = beta1^t, b2pow = beta2^t that the
   step ADVANCES in-graph (b1pow := beta1*b1pow) and reads back as
   m_hat = m/(1 - b1pow).  Because the advance is a captured
   UOP_ASSIGN, it re-runs on every TJit replay -- so a JIT-captured
   Adam step self-corrects t at each replay rather than freezing the
   correction at capture-time t.  Tinygrad does the same: LAMB keeps
   b1_t / b2_t as runtime (1,) tensors, does `b1_t *= b1` per _step,
   and computes `m_hat = m / (1 - b1_t)`
   (tinygrad/nn/optim.py:152, 158-159, 163-164).

   Carrying beta^t in a buffer also keeps the kernel program POW-free
   (no exponent UOP): each step is one MUL by the host-side beta
   scalar. *)
Options[TAdam] = {
    "lr"    -> 0.001,
    "beta1" -> 0.9,
    "beta2" -> 0.999,
    "eps"   -> 1.0*^-8
}

(* Shared backward + per-param update.  mHatFn / vHatFn map an
   m/v buffer's post-assign term to its bias-corrected hat term;
   the two callers below supply either a committed-buffer divide
   (JIT-safe) or a host-folded scalar multiply (legacy). *)
adamStep[loss_, params_, mList_, vList_, lr_, beta1_, beta2_, eps_,
         mHatFn_, vHatFn_] :=
    Module[{grads, gradsRealized, mvAssigns, paramAssigns},
        (* Differentiate the loss graph directly.  Do NOT TMaterialize[loss]
           first: scheduling the forward into kernels before the backward
           walk severs the requires_grad provenance for non-trivial graphs,
           so TGrad returns ZERO gradients (the moment buffers then stay at
           their seed and Adam never converges).  SGD takes TGrad on the
           live loss directly and works; this matches it. *)
        grads        = TGrad[loss, params];
        (* Realize all gradients in ONE bundled materialize pass.
           TGrad[loss, params] does a single requires_grad backward walk, so the
           N grads share one backward DAG; bundling the realize lets
           bufferize_classify see the shared upstream cotangents (dL/dh1
           etc.) as multi-consumer and emit them ONCE.  The per-param
           `TRealize /@` form re-emits the shared chain for every param --
           each isolated pass sees its upstream as single-consumer and
           inlines it -- so it pays ~N x the kernels (beautiful_mnist BS=8
           backward: 1583 vs ~216 bundled).  The list form of TRealize
           returns the N realized roots as pinned TEN handles, so the three
           downstream Adam reads (m, g*g, w) all see the same concrete
           gradient buffer. *)
        gradsRealized = TRealize[grads];
        (* Commit the first/second moment buffers in their OWN bundled pass
           BEFORE the param update reads them, mirroring the b1pow / b2pow
           two-pass ordering above.  An m / v ASSIGN consumed INLINE by the
           w-update root (mHat / vHat folded into the w kernel) miscompiles
           the read-after-write under TJit replay -- the committed buffer is
           read stale / the store is dropped, which NaNs the step once real
           gradients flow.  Committing m / v as realize ROOTS makes each
           write a real dispatched store; the w-update then reads the
           committed buffers as plain TEN handles. *)
        mvAssigns = Table[
            {TAssign[mList[[i]], beta1 * mList[[i]] + (1.0 - beta1) * gradsRealized[[i]]],
             TAssign[vList[[i]], beta2 * vList[[i]] + (1.0 - beta2) * (gradsRealized[[i]] * gradsRealized[[i]])]},
            {i, Length[params]}];
        TRealize @ Flatten[mvAssigns];
        paramAssigns = Table[
            Block[{wTen = params[[i]], mHat, vHat},
                mHat = mHatFn[mList[[i]]];
                vHat = vHatFn[vList[[i]]];
                TAssign[wTen, wTen - lr * mHat / (Sqrt[vHat] + eps)]
            ],
            {i, Length[params]}];
        TRealize @ paramAssigns;
        params
    ]

(* Canonical JIT-safe form.  b1pow / b2pow are caller-owned shape-{1}
   scalar buffers seeded to 1.0; the step advances them in-graph and
   reads the committed value for bias correction.

   The advance (b1pow := beta1*b1pow) is realized in its OWN bundled
   pass BEFORE the param updates, then the param updates read the
   committed b1pow / b2pow buffer as plain TEN handles.  The two-pass
   ordering is load-bearing: a single realize that both overwrites a
   scalar buffer AND reads it (1 - b1pow) in another root miscompiles
   the read-after-write under TJit replay (the captured consumer
   dispatch reads a stale buffer slot).  Committing the advance first
   makes the read a clean buffer load -- the SSA shape tinygrad gets
   for free (b1_t *= b1 ; m_hat = m/(1 - b1_t),
   tinygrad/nn/optim.py:158-159,163-164).  The advance assign is itself
   a captured replayable op, so t tracks replays rather than freezing
   at capture. *)
TAdam[loss_TTerm, params_List, mList_List, vList_List,
      b1pow_TTerm, b2pow_TTerm, opts : OptionsPattern[]] :=
    Module[{
        lr    = OptionValue["lr"],
        beta1 = OptionValue["beta1"],
        beta2 = OptionValue["beta2"],
        eps   = OptionValue["eps"],
        oneMinusB1, oneMinusB2
    },
        TRealize @ {
            TAssign[b1pow, beta1 * b1pow],
            TAssign[b2pow, beta2 * b2pow]};
        oneMinusB1 = 1.0 - b1pow;
        oneMinusB2 = 1.0 - b2pow;
        adamStep[loss, params, mList, vList, lr, beta1, beta2, eps,
            (# / oneMinusB1) &,
            (# / oneMinusB2) &]
    ]

(* Legacy host-folded form: t is a host-side Integer and the
   bias correction is baked in at emit time.  Correct outside TJit;
   under TJit it freezes the correction at capture-time t (use the
   b1pow / b2pow buffer form there). *)
TAdam[loss_TTerm, params_List, mList_List, vList_List, t_Integer,
      opts : OptionsPattern[]] :=
    Module[{
        lr    = OptionValue["lr"],
        beta1 = OptionValue["beta1"],
        beta2 = OptionValue["beta2"],
        eps   = OptionValue["eps"],
        lrHat, invSqrtB2cor
    },
        (* step = lrHat * m_new / (sqrt(v_new) * invSqrtB2cor + eps),
           folding lr/(1-b1^t) and 1/sqrt(1-b2^t) into m_hat / v_hat. *)
        lrHat        = lr / (1.0 - beta1^t);
        invSqrtB2cor = 1.0 / Sqrt[1.0 - beta2^t];
        adamStep[loss, params, mList, vList, lrHat, beta1, beta2, eps,
            (# &),
            (# * invSqrtB2cor^2) &]
    ]

End[];
EndPackage[];
