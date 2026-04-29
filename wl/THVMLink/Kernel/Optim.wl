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
                       -- (Phase-1 next item; stub raises Failure.)

   gradFn is a Wolfram Function that takes a TTerm w and returns the
   loss gradient w.r.t. w as a TTerm UOp graph.
*)

BeginPackage["THVMLink`"];

TOptim::usage = "TOptim[\"SGD\", lr] returns a function {gradFn, w0, n} -> TTerm that, when realised, performs n SGD steps from w0 with learning rate `lr`.  TOptim[\"Adam\", lr, beta1, beta2, eps] likewise for Adam.  gradFn is a host-side function w_TTerm -> TTerm carrying the gradient w.r.t. w.";

TAdam::usage = "TAdam[loss, params, m, v, t, opts] applies one Adam step in tensor-land.\n\nArguments:\n    loss    -- TTerm scalar; the value being minimised.  Grads w.r.t.\n               every entry of `params` are computed internally via\n               TGradMany (one realize, shared forward DAG, per-target\n               kernel-emit memo dedup).\n    params  -- List of TTerm tensor handles (TAG_TEN) to update.\n    m, v    -- Same-shape running first/second moment buffers (TTerm\n               tensors); seed with `TZerosLike /@ params`.\n    t       -- Integer step index (host-side).  Bias-correction\n               constants 1/(1-beta1^t) and 1/sqrt(1-beta2^t) are\n               precomputed at emit time so the kernel program stays\n               POW-free.\n    opts    -- Hyperparameters as Wolfram options.  Defaults:\n                   \"lr\"    -> 0.001\n                   \"beta1\" -> 0.9\n                   \"beta2\" -> 0.999\n                   \"eps\"   -> 1.0*^-8\n\nThe per-param body is the textbook Adam update built as a lazy\nUOP_ASSIGN chain:\n    m := beta1*m + (1-beta1)*grad\n    v := beta2*v + (1-beta2)*grad*grad\n    w := w - lrHat*m / (sqrt(v)*invSqrtB2cor + eps)\nReturns `params` for chainability.";

(* Forward-declare symbols owned by later-loading siblings (Ref.wl,
   Switch.wl, Tensor.wl).  Without this, bare references to TDef /
   TIfZero / TRef / TOp2 / TNum / TSet below would resolve to phantom
   symbols in THVMLink`Private` (since those symbols don't yet exist
   on the $ContextPath when this file is Get'd in alphabetical order),
   leading to silently broken term construction at call time. *)
{TDef, TRef, TIfZero, TOp2, TNum, TMatNum, TSet};

Begin["`Private`"];

$optimDefCounter = 0;

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
    Function[{gradFn, w0, n}, sgdRecursiveTerm[gradFn, lr, w0, n]]

TOptim["Adam", lr_TTerm, beta1_, beta2_, eps_] :=
    Function[{gradFn, w0, n}, adamRecursiveTerm[gradFn, lr, beta1, beta2, eps, w0, n]]

(* === TAdam: TAssign-form Adam (graph-resident) ===

   Each call returns a flat list of TAssign TTerms (3 per param: m, v,
   then w in that order so Realize'ing in sequence sees the freshly-
   computed m and v when computing m_hat / v_hat).  The math sits
   inside one TUOpAdd / TUOpMul chain per buffer; bias-correction
   terms (1 - beta1^t), (1 - beta2^t) are computed host-side at emit
   time and folded into the lr_hat scalar so the graph stays
   POW-free.  This keeps the Adam step inside the kernel surface
   that Phase 7's TJit will capture verbatim. *)
Options[TAdam] = {
    "lr"    -> 0.001,
    "beta1" -> 0.9,
    "beta2" -> 0.999,
    "eps"   -> 1.0*^-8
};

TAdam[loss_TTerm, params_List, mList_List, vList_List, t_Integer,
      opts : OptionsPattern[]] :=
    (* Bias-correction folded in at emit time so the kernel program
       stays POW-free.  Algebra:
           step = lr * m_hat / (sqrt(v_hat) + eps)
                = (lr / (1 - b1^t)) * m_new
                          / (sqrt(v_new) / sqrt(1 - b2^t) + eps)
                = lrHat * m_new / (sqrt(v_new) * invSqrtB2cor + eps)
       so lrHat and invSqrtB2cor are precomputed scalars.

       Grads come from TGradMany (one realize, shared forward DAG)
       so per-target kernels go through the same materialize-pass
       memo and forward intermediates dedup across targets. *)
    Block[{
        lr     = OptionValue["lr"],
        beta1  = OptionValue["beta1"],
        beta2  = OptionValue["beta2"],
        eps    = OptionValue["eps"],
        lrHat, invSqrtB2cor, grads
    },
        lrHat        = lr  / (1.0 - beta1^t);
        invSqrtB2cor = 1.0 / Sqrt[1.0 - beta2^t];
        grads        = TGradMany[loss, params];
        Do[
            Block[{
                wTen = params[[i]], gTen = grads[[i]],
                mTen = mList[[i]],  vTen = vList[[i]],
                denom
            },
                (* Lazy nested-ASSIGN chain reduced in a single
                   TRealize.  Phase 14 wired materialize to walk the
                   UOP DAG and recursively materialize every
                   ASSIGN's src (src/schedule/materialize.c
                   `materialize_inner_assigns`) so the wnf dispatch
                   for the outer ASSIGN sees its src as a kernel
                   chain it can actually fire.  Each inner ASSIGN
                   (mAfter, vAfter) fires once during the kernel
                   chain expansion -- read-after-write through
                   mTen/vTen sees the post-update bytes via the
                   dependency edge from sqrt/mul to the assigned
                   tensors. *)
                Block[{
                    mAfter = TAssign[mTen, beta1 * mTen + (1.0 - beta1) * gTen],
                    vAfter = TAssign[vTen, beta2 * vTen + (1.0 - beta2) * (gTen * gTen)]
                },
                    denom = Sqrt[vAfter] * invSqrtB2cor + eps;
                    TRealize @ TAssign[wTen, wTen - lrHat * mAfter / denom]
                ];
            ],
            {i, Length[params]}];
        params
    ]

End[];
EndPackage[];
