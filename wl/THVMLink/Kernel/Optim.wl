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

TAdamHostInit::usage = "TAdamHostInit[weightsHosts] returns {mHosts, vHosts} -- per-tensor zero-like NumericArrays matching the shapes of the input weight NumericArrays.  Used to seed the m and v running buffers for TAdamHostStep.";

TAdamHostStep::usage = "TAdamHostStep[weightsHosts, gradsHosts, mHosts, vHosts, lr, beta1, beta2, eps, t] applies one Adam update step to a LIST of weight NumericArrays in pure WL host code.  No TTerm involvement -- intended for multi-tensor networks where TOptim[\"Adam\"]'s single-weight recursion isn't enough.  Returns {newWeights, newM, newV} as lists of NumericArrays.";

(* Forward-declare symbols owned by later-loading siblings (Ref.wl,
   Switch.wl).  Without this, bare references to TDef / TIfZero /
   TRef / TOp2 / TNum below would resolve to phantom symbols in
   THVMLink`Private` (since those symbols don't yet exist on the
   $ContextPath when this file is Get'd), leading to silently
   broken term construction at call time. *)
{TDef, TRef, TIfZero, TOp2, TNum, TMatNum};

Begin["`Private`"];

$optimDefCounter = 0;

freshOptimDefName[algo_String] := (
    $optimDefCounter += 1;
    algo <> "_loop_" <> ToString[$optimDefCounter]
)

(* === Adam helpers ===

   tZerosLike[wTen]  -- given a TTerm wrapping TAG_TEN, returns a
                        fresh f32 zero TTensor with the same shape.
                        Used to seed Adam's m and v running buffers.
   tF32[x]           -- shorthand for TUOpConst[x, "f32"].  Adam's
                        body needs many scalar-CONST operands
                        (1-beta1, eps, etc.) and the wrapper keeps
                        them readable. *)

tZerosLike[wTen_TTerm] := With[{shape = TTensorShape[wTen]},
    TTensorCreate @ NumericArray[ConstantArray[0., shape], "Real32"]
]

tF32[x_] := TUOpConst[N[x], "f32"]

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
        b1c  = tF32[beta1];        b2c  = tF32[beta2];
        omb1 = tF32[1.0 - beta1];  omb2 = tF32[1.0 - beta2];
        epsC = tF32[eps];          oneC = tF32[1.0];
        m0   = tZerosLike[w0];     v0   = tZerosLike[w0];

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

(* === Multi-tensor Adam (host-side) ============================
   Pure WL host computation: no TTerm involvement.  Mirrors the
   per-weight Adam math from adamRecursiveTerm above but applied
   to a LIST of NumericArray weights so multi-tensor networks
   (LeNet has 8 weight tensors) can step all parameters in one
   call.  Bias-correction uses 1 - beta^t with the explicit step
   index t (the recursive form threads beta^t through the
   recursion; this version computes it directly per call). === *)

TAdamHostInit[weightsHosts_List] :=
    Module[{zerosLike},
        zerosLike[w_] := NumericArray[
            ConstantArray[0., Dimensions @ Normal @ w], "Real32"];
        {Map[zerosLike, weightsHosts], Map[zerosLike, weightsHosts]}
    ]

TAdamHostStep[
    weightsHosts_List, gradsHosts_List,
    mHosts_List, vHosts_List,
    lr_, beta1_, beta2_, eps_, t_Integer
] := Module[{
    n = Length[weightsHosts],
    mNew, vNew, mHat, vHat, wNew,
    b1corr = 1.0 - beta1^t,
    b2corr = 1.0 - beta2^t
},
    mNew = Table[
        NumericArray[
            beta1 * Normal[mHosts[[i]]] +
            (1.0 - beta1) * Normal[gradsHosts[[i]]],
            "Real32"], {i, n}];
    vNew = Table[
        NumericArray[
            beta2 * Normal[vHosts[[i]]] +
            (1.0 - beta2) * Normal[gradsHosts[[i]]]^2,
            "Real32"], {i, n}];
    mHat = Table[Normal[mNew[[i]]] / b1corr, {i, n}];
    vHat = Table[Normal[vNew[[i]]] / b2corr, {i, n}];
    wNew = Table[
        NumericArray[
            Normal[weightsHosts[[i]]] -
            lr * mHat[[i]] / (Sqrt[vHat[[i]]] + eps),
            "Real32"], {i, n}];
    {wNew, mNew, vNew}
]

End[];
EndPackage[];
