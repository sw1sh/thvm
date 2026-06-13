#!/usr/bin/env wl
(* flux_bench.wls -- canonical FLUX.2-klein-4B transformer-forward perf
   benchmark on Metal, embodying the optimal inference path:

     (1) bf16-native: weights stay bf16 and activations are cast to bf16, so
         every matmul is bf16(act) x bf16(weight) -> Metal simdgroup_matrix
         (tensor cores).  bf16 is FLUX's native reference precision.
     (2) pre-transposed weights: each 2-D matmul weight is transposed+realised
         to a contiguous {in,out} W^T ONCE at load (fxLinear takes W^T), so the
         per-step forward and the JIT-captured replay stream carry NO transpose
         dispatch (1-D RMSNorm gains are left un-transposed).
     (3) keys pre-warm: every W^T is materialised by walking Keys[wt] (a pure
         transpose pass, no forward) BEFORE TJit, so the capture reads cached
         W^T and the cold path runs ONE forward, not two.

   COLD = weight upload (lazy mmap, paid on first touch by the pre-warm) +
   JIT capture (compile + first forward).  WARM = per-step replay (the 4-step
   sampler shares one captured kernel set, rebinding z + temb each step).

   Inputs are synthetic (the encoder/RoPE tables are a separate stage); this
   isolates the transformer forward, which dominates generation time.  Needs
   the transformer safetensors under ~/.cache/thvm/flux2-klein-4b/; skips
   cleanly if absent.

   Env: DT=bf16|f32 (default bf16), NREPLAY=N replays for the warm min
   (default 4).  Run with a raised live-bytes ceiling, e.g.
     THVM_MAX_LIVE_BYTES=45000000000 wl -f flux_bench.wls *)

PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxForward.wl"}]]; TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

wpath = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b/transformer/diffusion_pytorch_model.safetensors";
If[ !FileExistsQ[wpath],
    pr["SKIP: transformer weights not found at ", wpath]; Exit[0]];

dt = Environment["DT"] /. x_ /; (x === $Failed || x === None) -> "bf16";
nrep = ToExpression[Environment["NREPLAY"] /. x_ /; (x === $Failed || x === None) -> "4"];
cfg = <|"eps" -> 1.*^-6, "num_double" -> 5, "num_single" -> 20, "heads" -> 24, "head_dim" -> 128|>;
simg = 256; stxt = 512; dh = 128; inCh = 128; txtCh = 7680; tdim = 3072;

ctx = TContextNew["metal"];
res = TInContext[ctx,
  wt = TSafeTensorLoad[wpath];
  (* rank-aware loader: upload bf16 (or cast f32) to Metal, pre-transpose the
     2-D matmul weights to a contiguous {in,out} W^T, leave 1-D gains as-is. *)
  wcache = <||>;
  wf = (n |-> Lookup[wcache, n, wcache[n] = Module[{w0 = TToDevice[wt[n], "metal"], w, dims},
           w = If[ dt === "f32", TUOpCast[w0, "f32"], w0];
           dims = Dimensions[w];
           If[ Length[dims] == 2, TRealize[Transpose[w]], TRealize[w]]]]);
  (* activations in the working dtype (bf16 -> tensor-core matmuls) *)
  ca[x_] := If[ dt === "f32", TRealize[x], TRealize@TUOpCast[x, dt]];
  syn[s_, d__] := (SeedRandom[s]; TTensorCreate@NumericArray[RandomReal[{-0.05, 0.05}, {d}], "Real32"]);
  enc0 = ca@syn[2, stxt, txtCh];
  rc = ca@syn[3, stxt + simg, dh]; rs = ca@syn[4, stxt + simg, dh];
  z0 = ca@syn[1, simg, inCh]; temb0 = ca@syn[5, 1, tdim];
  velFn = (z |-> fxTransformer[z, enc0, temb0, rc, rs, wf, cfg]);

  tprewarm = AbsoluteTiming[Do[wf[k], {k, Keys[wt]}]][[1]];     (* transpose-only, no forward *)
  tcap = AbsoluteTiming[vfn = TJit[velFn]; vfn[z0];][[1]];      (* capture = compile + 1 forward *)
  trep = Min @ Table[AbsoluteTiming[vfn[z0]][[1]], {nrep}];     (* warm per-step replay *)
  {tprewarm, tcap, trep, TJitOpCount[vfn]}];
TContextDestroy[ctx];
{tprewarm, tcap, trep, ops} = res;

pr["FLUX.2-klein-4B transformer forward  (dt=", dt, ", ", ops, " captured ops)"];
pr["  COLD  = ", Round[tprewarm + tcap, 2], " s   (", Round[tprewarm, 2], " s weight pre-transpose + ", Round[tcap, 2], " s JIT capture)"];
pr["  WARM  = ", Round[trep*1000, 1], " ms/step   (4-step sampler ~ ", Round[4*trep, 2], " s)"];
