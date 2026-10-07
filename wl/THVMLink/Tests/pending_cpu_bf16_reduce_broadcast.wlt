(* pending_cpu_bf16_reduce_broadcast.wlt - OPEN CPU-backend correctness bug:
   a fused kernel of the shape EXPAND(broadcast) over a REDUCE chain whose
   leaf is CAST(bf16 -> f32) computes a wrong reduction (likely the same
   chain-fuse misrender family as the open sum-of-matmul bug).

   Repro facts (2026-07, CPU backend):
     - {1,64} bf16 leaf, c = CAST(c, f32), inv = 1/Sqrt[SUM(c*c)/64 + eps]:
       realizing `inv` alone is CORRECT; realizing EXPAND(inv -> {1,64}) in
       one fused kernel returns a uniform WRONG scalar (got 3.66868, expect
       1.71002 at SeedRandom[3]).
     - The pure-f32 version of the same graph is correct; inserting a no-op
       f32 CAST is correct; only the bf16->f32 CAST leaf triggers it.
     - A rank-1 {32} variant with the elementwise consumer in the kernel is
       correct, but casting THAT kernel's root to bf16 is wrong too
       (second shape of the same family).
     - Both dispatch routes agree on the wrong value (THVM_CPU_JIT_WARMUP
       forced-walker matches the JIT), so the bug is in the shared kernel
       lift, not a renderer.  THVM_CPU_BLAS_DISABLE=1 does not change the
       result.
     - Pre-realizing the reduce scalar before the EXPAND is correct, which
       is the workaround NN/Examples/DeepSeekV2/Decode.wl (dsvRmsNorm) uses.

   These stage the expected-correct behaviour for the future lift fix. *)

VerificationTest[
    TInit[]; TReset[];
    SeedRandom[3];
    x = RandomReal[{-1., 1.}, {1, 64}];
    xb = TRealize @ TUOpCast[TTensorCreate @ NumericArray[x, "Real32"], "bf16"];
    xh = First @ Normal @ xb;
    c = TUOpCast[xb, "f32"];
    inv = 1 / Sqrt[TUOpReduce[c * c, 1, "SUM"] / 64 + 1.*^-6];
    got = First @ Normal @ TRealize @ TUOpExpand[TUOpReshape[inv, {1, 1}], {1, 64}];
    Max @ Abs[got - 1 / Sqrt[Mean[xh^2] + 1.*^-6]] < 1.*^-3,
    True,
    TestID -> "pending-cpu-bf16/expand-of-reduce-over-bf16-cast"
]

VerificationTest[
    (* bf16 root over the rank-1 expand-of-reduce elementwise chain *)
    SeedRandom[3];
    v = RandomReal[{-2., 2.}, 32];
    vb = TRealize @ TUOpCast[TTensorCreate @ NumericArray[v, "Real32"], "bf16"];
    vh = Normal @ vb;
    cf = TUOpCast[vb, "f32"];
    i1 = 1 / Sqrt[TUOpReduce[cf * cf, 0, "SUM"] / 32 + 1.*^-6];
    y = cf * TUOpExpand[TUOpReshape[i1, {1}], {32}];
    got = Normal @ TRealize @ TUOpCast[y, "bf16"];
    Max @ Abs[got - vh / Sqrt[Mean[vh^2] + 1.*^-6]] < 3.*^-2,
    True,
    TestID -> "pending-cpu-bf16/bf16-root-over-reduce-broadcast-chain"
]

VerificationTest[
    (* IN-CONTEXT stale-kernel adoption (likely the same open family as the
       (x@w).sum() chain-fuse misrender): after leaf -> RMS-norm -> matmul
       realizes, a FRESH reduce over CAST(RESHAPE(SHRINK(matmul))) (the rank-1
       slice kva[[1, 1;;32]]) emits its reduce kernel but never fires the
       cast producer -- the dispatch trace shows ONE dispatch -- and the
       reduce reads a stale buffer (got 25.2872, expect 5.31702,
       deterministic).  The rank-2 slice kva[[All, 1;;32]] fuses the view
       chain into the reduce and is correct; NN/Examples/DeepSeekV2/Decode.wl
       keeps every slice rank-2 for this reason. *)
    SeedRandom[7];
    d = 64; kv = 32;
    emb = RandomReal[{-0.05, 0.05}, d];
    wkv = RandomReal[{-0.05, 0.05}, {d, kv + 8}];
    x = TRealize @ TUOpCast[TTensorCreate @ NumericArray[{emb}, "Real32"], "bf16"];
    winT = TTensorCreate @ NumericArray[ConstantArray[1., d], "Real32"];
    c0 = TUOpCast[x, "f32"];
    inv0 = TRealize[1 / Sqrt[TUOpReduce[c0 * c0, 1, "SUM"] / d + 1.*^-6]];
    xn = TRealize @ TUOpCast[c0 * TUOpExpand[TUOpReshape[inv0, {1, 1}], {1, d}] * TUOpExpand[TUOpReshape[winT, {1, d}], {1, d}], "bf16"];
    wkvT = TRealize @ TUOpCast[TTensorCreate @ NumericArray[wkv, "Real32"], "bf16"];
    kva = TRealize[xn . wkvT];
    kvaH = First @ Normal @ kva;
    c2 = TUOpCast[kva[[1, 1 ;; kv]], "f32"];
    got = First @ Flatten @ {Normal @ TRealize[1 / Sqrt[TUOpReduce[c2 * c2, 0, "SUM"] / kv + 1.*^-6]]};
    Abs[got - 1 / Sqrt[Mean[kvaH[[1 ;; kv]]^2] + 1.*^-6]] < 1.*^-2,
    True,
    TestID -> "pending-cpu-bf16/rank1-slice-reduce-stale-kernel-adoption"
]

VerificationTest[
    TReset[];
    True,
    True,
    TestID -> "pending-cpu-bf16/cleanup"
]
