#!/usr/bin/env wl
(* flux_bench.wls -- canonical FLUX.2-klein-4B transformer-forward perf
   benchmark on Metal, embodying the optimal inference path:

     (1) bf16-native: weights stay bf16 and activations are cast to bf16, so
         every matmul is bf16(act) x bf16(weight) -> Metal simdgroup_matrix
         (tensor cores).  bf16 is FLUX's native reference precision.
     (2) contiguous weights: each weight uploads AS STORED ({out,in}) ONCE at
         load; fxLinear matmuls the Transpose[w] VIEW, which the tiled
         tensor-core matmul reads via the weight strides, so the per-step forward
         and the JIT-captured replay stream carry NO transpose dispatch and NO
         second resident weight buffer (~weights-on-disk, not ~2x).
     (3) keys pre-warm: every weight is uploaded by walking Keys[wt] (a pure
         upload pass, no forward) BEFORE TJit, so the capture reads cached
         device weights and the cold path runs ONE forward, not two.

   COLD = weight upload (lazy mmap, paid on first touch by the pre-warm) +
   JIT capture (compile + first forward).  WARM = per-step replay (the 4-step
   sampler shares one captured kernel set, rebinding z + temb each step).

   Inputs are synthetic (the encoder/RoPE tables are a separate stage); this
   isolates the transformer forward, which dominates generation time.  Needs
   the transformer safetensors under ~/.cache/thvm/flux2-klein-4b/; skips
   cleanly if absent.

   Env: DT=bf16|f32|q8 (default bf16), NREPLAY=N replays for the warm min
   (default 4).  DT=q8 stores the per-block QKV/out/MLP weights (2-D, min-dim
   >= 2048) as int8 + a per-output-channel scale (~half the resident weight
   bytes); activations stay bf16 and fxLinear reads the int8 weight on the
   simdgroup tensor-core tiled path then dequantises (see fxQuantizeWeight /
   fxLinear).  Metal has no int8 MMA, so q8 targets cold-load + memory, not warm.
   Run with a raised live-bytes ceiling, e.g.
     THVM_MAX_LIVE_BYTES=45000000000 wl -f flux_bench.wls

   SYNTH=1 synthesises every weight at its known FLUX.2-klein-4B shape instead
   of loading the safetensors, so the bench is fully self-contained (no 8 GB
   download) and isolates the transformer-forward compute -- the warm per-step
   time is identical to the real-weight run (same shapes, same kernels).  This
   is the path for a fresh GPU pod: provision, install the paclet, run with
   SYNTH=1 DEV=cuda. *)

PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`Examples`"]; TInit[]; TReset[];

synth = Environment["SYNTH"] === "1";
wpath = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b/transformer/diffusion_pytorch_model.safetensors";
If[ !synth && !FileExistsQ[wpath],
    Print["SKIP: transformer weights not found at ", wpath]; Exit[0]];

dt = Environment["DT"] /. x_ /; (x === $Failed || x === None) -> "bf16";
dev = Environment["DEV"] /. x_ /; (x === $Failed || x === None) -> "metal";   (* metal | cuda | cpu *)
nrep = ToExpression[Environment["NREPLAY"] /. x_ /; (x === $Failed || x === None) -> "4"];
ndbl = ToExpression[Environment["NDBL"] /. x_ /; (x === $Failed || x === None) -> "5"];
nsgl = ToExpression[Environment["NSGL"] /. x_ /; (x === $Failed || x === None) -> "20"];
cfg = <|"eps" -> 1.*^-6, "num_double" -> ndbl, "num_single" -> nsgl, "heads" -> 24, "head_dim" -> 128|>;
simg = 256; stxt = 512; dh = 128; inCh = 128; txtCh = 7680; tdim = 3072;

(* SYNTH: FLUX.2-klein-4B transformer weight shapes by name (dim=3072; gated MLP
   18432=6*dim in / 9216=3*dim hidden; single qkv_mlp 27648=9*dim, to_out 12288=4*dim). *)
synShape[n_String] := Which[
    StringMatchQ[n, "x_embedder.weight"], {3072, 128},
    StringMatchQ[n, "context_embedder.weight"], {3072, 7680},
    StringMatchQ[n, "proj_out.weight"], {128, 3072},
    StringMatchQ[n, "norm_out.linear.weight"], {6144, 3072},
    StringMatchQ[n, "double_stream_modulation_" ~~ ("img" | "txt") ~~ ".linear.weight"], {18432, 3072},
    StringMatchQ[n, "single_stream_modulation.linear.weight"], {9216, 3072},
    StringMatchQ[n, ___ ~~ ".norm_" ~~ ___ ~~ ".weight"], {128},
    StringMatchQ[n, ___ ~~ ".to_qkv_mlp_proj.weight"], {27648, 3072},
    StringMatchQ[n, "single_transformer_blocks." ~~ ___ ~~ ".attn.to_out.weight"], {3072, 12288},
    StringMatchQ[n, ___ ~~ ".ff" ~~ ("." | "_context.") ~~ "linear_in.weight"], {18432, 3072},
    StringMatchQ[n, ___ ~~ ".ff" ~~ ("." | "_context.") ~~ "linear_out.weight"], {3072, 9216},
    StringMatchQ[n, ___ ~~ (".to_q" | ".to_k" | ".to_v" | ".add_q_proj" | ".add_k_proj" | ".add_v_proj" | ".to_out.0" | ".to_add_out") ~~ ".weight"], {3072, 3072},
    True, Print["SYNTH: unknown key ", n]; {3072, 3072}];

dblKeys = {"attn.to_q.weight", "attn.to_k.weight", "attn.to_v.weight", "attn.add_q_proj.weight", "attn.add_k_proj.weight", "attn.add_v_proj.weight", "attn.norm_q.weight", "attn.norm_k.weight", "attn.norm_added_q.weight", "attn.norm_added_k.weight", "attn.to_out.0.weight", "attn.to_add_out.weight", "ff.linear_in.weight", "ff.linear_out.weight", "ff_context.linear_in.weight", "ff_context.linear_out.weight"};
sglKeys = {"attn.to_qkv_mlp_proj.weight", "attn.to_out.weight", "attn.norm_q.weight", "attn.norm_k.weight"};
synKeys = Join[
    {"x_embedder.weight", "context_embedder.weight", "proj_out.weight", "norm_out.linear.weight", "double_stream_modulation_img.linear.weight", "double_stream_modulation_txt.linear.weight", "single_stream_modulation.linear.weight"},
    Flatten@Table[(k |-> "transformer_blocks." <> ToString[i] <> "." <> k) /@ dblKeys, {i, 0, ndbl - 1}],
    Flatten@Table[(k |-> "single_transformer_blocks." <> ToString[i] <> "." <> k) /@ sglKeys, {i, 0, nsgl - 1}]];
(* synth weight: broadcast a tiny per-row f32 seed to the full shape, so the f32
   host footprint stays a few MB (not ~16 GB for the whole 4B-param model) while
   the realised bf16 buffer matches the real weight's resident size and layout.
   Values are irrelevant -- the kernels/shapes (hence warm timing) are identical. *)
synthW[n_String] := Module[{sh = synShape[n]},
    If[ Length[sh] == 1,
        TUOpCast[TTensorCreate@NumericArray[RandomReal[{-0.05, 0.05}, sh], "Real32"], "bf16"],
        TUOpCast[TUOpExpand[TTensorCreate@NumericArray[RandomReal[{-0.05, 0.05}, {1, Last[sh]}], "Real32"], sh], "bf16"]]];

ctx = TContextNew[dev];
res = TInContext[ctx,
  wt = If[ synth, Null, TSafeTensorLoad[wpath]];
  (* contiguous loader: upload each weight AS STORED ({out,in}) to Metal -- ONE
     buffer per weight, ~weights-on-disk resident.  fxLinear matmuls against the
     Transpose[w] VIEW, which the tiled tensor-core matmul reads via the weight's
     strides (no separate transpose dispatch or 2nd buffer).  The earlier
     pre-transpose held the weight TWICE (upload + realised W^T, ~2x). *)
  wcache = <||>;
  (* q8: quantise a 2-D weight whose dims are both >= 2048 (the per-block
     QKV/out/MLP projections -- the warm-hot bulk + ~all the resident bytes);
     everything else uploads bf16.  bf16/f32: upload AS STORED. *)
  wf = (n |-> Lookup[wcache, n, wcache[n] = Module[{w = If[ synth, synthW[n], wt[n]]},
           If[ dt === "q8" && fxQuantizableQ[n, Dimensions[w]],
               fxQuantizeWeight[TToDevice[w, dev]],
               w = If[ dt === "f32", TUOpCast[w, "f32"], w];
               TRealize@TToDevice[w, dev]]]]);
  (* activations in the working dtype (bf16 -> tensor-core matmuls; q8 also
     feeds bf16 activations, the int8 weight casts to bf16 in-kernel) *)
  actDt = If[ dt === "f32", "f32", "bf16"];
  (* place inputs on `dev` explicitly (like the weights' TToDevice): a host-leaf
     realize otherwise keeps the device in the graph tied to the CPU leaf, so on
     a non-default backend the forward's early ops dispatch to the CPU walk over
     device buffers.  On Metal this happened to resolve to the metal backend; on
     CUDA it routed reduces to cpu_uop_walk and crashed. *)
  ca[x_] := TRealize@TToDevice[If[ actDt === "f32", x, TUOpCast[x, actDt]], dev];
  syn[s_, d__] := (SeedRandom[s]; TTensorCreate@NumericArray[RandomReal[{-0.05, 0.05}, {d}], "Real32"]);
  enc0 = ca@syn[2, stxt, txtCh];
  rc = ca@syn[3, stxt + simg, dh]; rs = ca@syn[4, stxt + simg, dh];
  z0 = ca@syn[1, simg, inCh]; temb0 = ca@syn[5, 1, tdim];
  velFn = (z |-> fxTransformer[z, enc0, temb0, rc, rs, wf, cfg]);

  nojit = Environment["NOJIT"] === "1";   (* eager forward, no TJit/graph capture -- isolates a forward-op bug from a capture bug *)
  tprewarm = AbsoluteTiming[Do[wf[k], {k, If[ synth, synKeys, Keys[wt]]}]][[1]];     (* upload-only, no forward *)
  Print["  [prewarm done: ", Round[tprewarm, 2], " s; ", If[ nojit, "eager fwd ", "capturing "], ndbl, "+", nsgl, " blocks...]"];
  {tcap, opsN} = If[ nojit,
    {AbsoluteTiming[TRealize@velFn[z0];][[1]], 0},
    With[{t = AbsoluteTiming[vfn = TJit[velFn]; vfn[z0];][[1]]}, {t, TJitOpCount[vfn]}]];
  Print["  [", If[ nojit, "eager fwd", "capture"], " done: ", Round[tcap, 2], " s]"];
  trep = Min @ Table[AbsoluteTiming[If[ nojit, TRealize@velFn[z0], vfn[z0]]][[1]], {nrep}];     (* warm per-step *)
  (* THVM_FLUX_CHECK=1: print a deterministic output statistic for A/B
     correctness comparisons across kernel-path changes (same seed -> same
     stat within dtype tolerance). *)
  If[ Environment["THVM_FLUX_CHECK"] === "1",
      With[{o = Flatten@Normal@TRealize@velFn[z0]},
           Print["  OUTSTAT  sum|x|=", N@Total@Abs@o, "  rms=", N@Sqrt@Mean[o^2], "  finite=", AllTrue[o, NumberQ]]]];
  prof = If[ Environment["THVM_METAL_PROFILE_PEROP"] === "1", TMetalPerOpProfile[], <||>];
  {tprewarm, tcap, trep, opsN, prof}];
TContextDestroy[ctx];
{tprewarm, tcap, trep, ops, prof} = res;

Print["FLUX.2-klein-4B transformer forward  (dt=", dt, ", ", ops, " captured ops)"];
Print["  COLD  = ", Round[tprewarm + tcap, 2], " s   (", Round[tprewarm, 2], " s weight upload + ", Round[tcap, 2], " s JIT capture)"];
Print["  WARM  = ", Round[trep*1000, 1], " ms/step   (4-step sampler ~ ", Round[4*trep, 2], " s)"];
If[ Length[prof] > 0,
    Print["  --- top kernels by GPU time (per-op profile) ---"];
    Module[{rows},
      rows = Reverse @ SortBy[Normal[prof], #[[2]]["GpuUs"] &];
      Do[ With[{kid = r[[1]], v = r[[2]]},
          Print["    kid=", kid, "  gpu=", Round[v["GpuUs"]/Max[v["GpuSamples"],1]/1000., 0.01], " ms x", v["DispatchCount"],
             "  ", Round[v["GFlopsPerSec"]/1000., 0.1], " TFLOPS  ", v["DispatchKind"]]],
        {r, Take[rows, UpTo[14]]}]]];
