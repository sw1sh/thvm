(* kernel_custom.wlt - raw custom-kernel injection via
   TKernel[<|"Program" -> $$|>][buffers$$]: a hand-written C kernel runs
   as a first-class op on the CPU backend, eagerly and under TJit
   capture/replay (the tinygrad raw-PROGRAM outs bug class: replays
   must re-fire the injected kernels and keep their scratch alive). *)

$addOneSrc = "
void addone(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)block;
  const float *x = (const float *)bufs[0];
  float       *y = (float *)bufs[1];
  for (unsigned i = 0; i < grid[0]; i++) y[i] = x[i] + 1.0f;
}
";

$doubleSrc = "
void dbl(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)block;
  const float *x = (const float *)bufs[0];
  float       *s = (float *)bufs[1];
  for (unsigned i = 0; i < grid[0]; i++) s[i] = x[i] * 2.0f;
}
";

$addThreeSrc = "
void add3(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)block;
  const float *s = (const float *)bufs[0];
  float       *y = (float *)bufs[1];
  for (unsigned i = 0; i < grid[0]; i++) y[i] = s[i] + 3.0f;
}
";

VerificationTest[
    TInit[]; TReset[];
    n = 8;
    x = TTensor[{n}, N @ Range[n]];
    y = TTensor[{n}];
    k = TKernel[<|"Program" -> $addOneSrc, "Name" -> "addone",
        "Grid" -> {n}, "Outs" -> {2}|>];
    out = k[x, y];
    Normal @ TTensorData @ out,
    N @ Range[2, 9],
    TestID -> "kernel-custom/eager-addone"
]

VerificationTest[
    (* the injected kernel is the newest KernelEntry and records the
       "custom" dispatch route *)
    TKernelDispatchKind[TKernelCount[] - 1],
    "custom",
    TestID -> "kernel-custom/dispatch-kind"
]

VerificationTest[
    (* repeat fire over mutated input bytes: an explicit custom fire
       always executes and reuses the cached kid *)
    TSetData[x, N @ Range[11, 18]];
    {Normal @ TTensorData @ k[x, y], TKernelCount[]},
    {N @ Range[12, 19], TKernelCount[]},
    TestID -> "kernel-custom/eager-refire-fresh-input"
]

VerificationTest[
    (* bad spec: out position past the argument list *)
    FailureQ @ TKernel[<|"Program" -> $addOneSrc, "Name" -> "addone",
        "Grid" -> {n}, "Outs" -> {3}|>][x, y],
    True,
    TestID -> "kernel-custom/out-slot-validation"
]

VerificationTest[
    (* 2-kernel chain through a scratch buffer under TJit: capture once,
       then 3 replays with fresh inputs must recompute BOTH kernels off
       the fresh bytes (scratch stays held, dispatches never skipped) *)
    cx = TTensor[{n}, N @ Range[n]];
    scratch = TTensor[{n}];
    cy = TTensor[{n}];
    kDbl = TKernel[<|"Program" -> $doubleSrc, "Name" -> "dbl",
        "Grid" -> {n}, "Outs" -> {2}|>];
    kAdd3 = TKernel[<|"Program" -> $addThreeSrc, "Name" -> "add3",
        "Grid" -> {n}, "Outs" -> {2}|>];
    chain = TJit[{a, b} |-> (kDbl[a, scratch]; kAdd3[scratch, b])];
    chain[cx, cy];
    Table[
        TSetData[cx, N @ Range[10 rep + 1, 10 rep + n]];
        chain[cx, cy];
        Normal @ TTensorData @ cy === 2 N @ Range[10 rep + 1, 10 rep + n] + 3.,
        {rep, 3}
    ],
    {True, True, True},
    TestID -> "kernel-custom/tjit-chain-replay-fresh-inputs"
]

VerificationTest[
    TReset[];
    True,
    True,
    TestID -> "kernel-custom/cleanup"
]
