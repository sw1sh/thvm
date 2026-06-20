(* flux_block_kcount.wls -- kernel-count probe for ONE FLUX double/single block at
   velocity shapes (synthetic weights, no parity).  Reports TJitOpCount per block
   so the matmul-epilogue fusion can be measured by KERNEL COUNT (thermal-
   independent), not warm time.  DEV=metal.  Run from repo root:

     DEV=metal wolframscript -f tools/flux_block_kcount.wls
*)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`Examples`"];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

(* reach the FLUX-specific helpers in the Examples`Private` context *)
fxDoubleBlock = WolframInstitute`THVMLink`Examples`Private`fxDoubleBlock;
fxSingleBlock = WolframInstitute`THVMLink`Examples`Private`fxSingleBlock;

dev = Environment["DEV"] /. x_ /; (x === $Failed || x === None) -> "metal";
dt  = Environment["DT"]  /. x_ /; (x === $Failed || x === None) -> "bf16";
actDt = If[ dt === "f32", "f32", "bf16"];

(* velocity shapes: S_img=256, S_txt=512, dim=3072, heads=24, head_dim=128 *)
simg = 256; stxt = 512; dh = 128; h = 24; dim = h*dh; mlpD = 3*dim;
cfg = <|"heads" -> h, "head_dim" -> dh, "eps" -> 1.*^-6|>;

(* double-stream weight shapes (diffusers names, as-stored {out,in}) *)
dblShape = <|
  "to_q" -> {dim, dim}, "to_k" -> {dim, dim}, "to_v" -> {dim, dim},
  "add_q_proj" -> {dim, dim}, "add_k_proj" -> {dim, dim}, "add_v_proj" -> {dim, dim},
  "norm_q" -> {dh}, "norm_k" -> {dh}, "norm_added_q" -> {dh}, "norm_added_k" -> {dh},
  "to_out_0" -> {dim, dim}, "to_add_out" -> {dim, dim},
  "ff_linear_in" -> {2*mlpD, dim}, "ff_linear_out" -> {dim, mlpD},
  "ffc_linear_in" -> {2*mlpD, dim}, "ffc_linear_out" -> {dim, mlpD}|>;
(* single-stream: fused qkv+mlp {3*dim+2*mlp, dim}, to_out {dim, dim+mlp} *)
sglShape = <|
  "to_qkv_mlp_proj" -> {3*dim + 2*mlpD, dim}, "to_out" -> {dim, dim + mlpD},
  "norm_q" -> {dh}, "norm_k" -> {dh}|>;

synthW[sh_List] := If[ Length[sh] == 1,
    TUOpCast[TTensorCreate@NumericArray[RandomReal[{-0.05, 0.05}, sh], "Real32"], "bf16"],
    TUOpCast[TUOpExpand[TTensorCreate@NumericArray[RandomReal[{-0.05, 0.05}, {1, Last[sh]}], "Real32"], sh], "bf16"]];

ca[x_] := TRealize@TToDevice[If[ actDt === "f32", x, TUOpCast[x, actDt]], dev];
syn[s_, d__] := (SeedRandom[s]; TTensorCreate@NumericArray[RandomReal[{-0.05, 0.05}, {d}], "Real32"]);
modVec[s_] := ca@syn[s, 1, dim];

ctx = TContextNew[dev];
res = TInContext[ctx,
  Wd = TRealize@TToDevice[synthW[#], dev] & /@ dblShape;
  Sd = TRealize@TToDevice[synthW[#], dev] & /@ sglShape;
  img0 = ca@syn[1, simg, dim];
  txt0 = ca@syn[2, stxt, dim];
  x0   = ca@syn[6, stxt + simg, dim];       (* single block: [txt;img] concat *)
  rc = ca@syn[3, stxt + simg, dh]; rs = ca@syn[4, stxt + simg, dh];
  modKeys = {"img_shift_msa", "img_scale_msa", "img_gate_msa", "img_shift_mlp", "img_scale_mlp", "img_gate_mlp",
             "txt_shift_msa", "txt_scale_msa", "txt_gate_msa", "txt_shift_mlp", "txt_scale_mlp", "txt_gate_mlp"};
  mods = Association[(# -> modVec[10 + Hash[#, "CRC32"] ]) & /@ modKeys];
  smod = <|"shift" -> modVec[100], "scale" -> modVec[101], "gate" -> modVec[102]|>;

  dblFn = (im |-> First@fxDoubleBlock[im, txt0, mods, rc, rs, Wd, cfg]);
  sglFn = (x  |-> fxSingleBlock[x, smod, rc, rs, Sd, cfg]);

  vdbl = TJit[dblFn]; vdbl[img0];
  opsDbl = TJitOpCount[vdbl];
  capDbl = TJitCaptureOps[vdbl];
  vsgl = TJit[sglFn]; vsgl[x0];
  opsSgl = TJitOpCount[vsgl];
  capSgl = TJitCaptureOps[vsgl];
  {opsDbl, opsSgl, capDbl, capSgl}];
TContextDestroy[ctx];
{opsDbl, opsSgl, capDbl, capSgl} = res;

dumpCap[name_, cap_] := Module[{disp},
    disp = Select[cap, #["Kind"] === "DISPATCH" &];
    pr["=== ", name, " block: ", Length[disp], " dispatches ==="];
    pr["  DispatchKind counts: ", Counts[Lookup[disp, "DispatchKind"]]];
    pr["  per-kernel (OutputNumel, OpCount, ScalarUops, TileUops, kind):"];
    Do[ With[{o = disp[[i]]},
        pr["    [", i, "] numel=", o["OutputNumel"], " ops=", o["OpCount"],
           " scU=", o["ScalarUopCount"], " tlU=", o["TileUopCount"], "  ", o["DispatchKind"]]],
      {i, Length[disp]}];];

If[ Environment["KDUMP"] === "1", dumpCap["DOUBLE", capDbl]; dumpCap["SINGLE", capSgl]];

pr["FLUX block kernel counts (dev=", dev, ", dt=", dt, ")"];
pr["  DOUBLE block: ", opsDbl, " kernels"];
pr["  SINGLE block: ", opsSgl, " kernels"];
pr["  projected velocity (5 double + 20 single): ", 5*opsDbl + 20*opsSgl, " (+ embedders/modulation/norm_out/proj_out overhead)"];
