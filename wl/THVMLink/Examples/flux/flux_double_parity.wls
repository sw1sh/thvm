PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get["wl/THVMLink/Examples/flux/FluxForward.wl"]; TInit[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);
ref = TSafeTensorLoad["/tmp/flux_parity/ref_io.safetensors"];
wpath = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b/transformer/diffusion_pytorch_model.safetensors";
wt = TSafeTensorLoad[wpath];
f32[n_] := TUOpCast[wt[n], "f32"];
(* fxLinear now takes the pre-transposed {in,out} W^T, so 2-D matmul weights are
   transposed+realised once here; the rank check auto-leaves the 1-D norm gains
   (norm_q/norm_k/norm_added_q/norm_added_k) un-transposed. *)
f32T[n_] := Module[{w = f32[n]}, If[ Length[Dimensions[w]] == 2, TRealize[Transpose[w]], w]];
b = "transformer_blocks.0.";
W = <|
  "to_q"->f32T[b<>"attn.to_q.weight"], "to_k"->f32T[b<>"attn.to_k.weight"], "to_v"->f32T[b<>"attn.to_v.weight"],
  "add_q_proj"->f32T[b<>"attn.add_q_proj.weight"], "add_k_proj"->f32T[b<>"attn.add_k_proj.weight"], "add_v_proj"->f32T[b<>"attn.add_v_proj.weight"],
  "norm_q"->f32T[b<>"attn.norm_q.weight"], "norm_k"->f32T[b<>"attn.norm_k.weight"],
  "norm_added_q"->f32T[b<>"attn.norm_added_q.weight"], "norm_added_k"->f32T[b<>"attn.norm_added_k.weight"],
  "to_out_0"->f32T[b<>"attn.to_out.0.weight"], "to_add_out"->f32T[b<>"attn.to_add_out.weight"],
  "ff_linear_in"->f32T[b<>"ff.linear_in.weight"], "ff_linear_out"->f32T[b<>"ff.linear_out.weight"],
  "ffc_linear_in"->f32T[b<>"ff_context.linear_in.weight"], "ffc_linear_out"->f32T[b<>"ff_context.linear_out.weight"]|>;
md[s_] := <|"img_shift_"<>s->ref["mod_img_shift_"<>s], "img_scale_"<>s->ref["mod_img_scale_"<>s], "img_gate_"<>s->ref["mod_img_gate_"<>s],
            "txt_shift_"<>s->ref["mod_txt_shift_"<>s], "txt_scale_"<>s->ref["mod_txt_scale_"<>s], "txt_gate_"<>s->ref["mod_txt_gate_"<>s]|>;
mods = Join[md["msa"], md["mlp"]];
cfg = <|"heads"->24, "head_dim"->128, "eps"->1.*^-6|>;
pr["building double block..."];
{img, txt} = fxDoubleBlock[ref["double_img_in"], ref["double_txt_in"], mods, ref["rope_cos"], ref["rope_sin"], W, cfg];
io = Normal@TRealize@img; to = Normal@TRealize@txt;
ri = Normal@ref["double_img_out"]; rt = Normal@ref["double_txt_out"];
pr["img maxdiff = ", ToString[Max@Abs@Flatten[io-ri], InputForm], "   (ref std 2.10)"];
pr["txt maxdiff = ", ToString[Max@Abs@Flatten[to-rt], InputForm]];
pr[If[Max@Abs@Flatten[io-ri]<5.*^-3 && Max@Abs@Flatten[to-rt]<5.*^-3, "DOUBLE BLOCK PARITY OK", "MISMATCH"]];
