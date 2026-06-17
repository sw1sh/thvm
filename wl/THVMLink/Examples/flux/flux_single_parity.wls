PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`"];
Get["wl/THVMLink/Examples/flux/FluxForward.wl"]; TInit[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);
ref = TSafeTensorLoad["/tmp/flux_parity/ref_io.safetensors"];
wpath = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b/transformer/diffusion_pytorch_model.safetensors";
wt = TSafeTensorLoad[wpath];
f32[n_] := TUOpCast[wt[n], "f32"];
(* fxLinear now takes the pre-transposed {in,out} W^T, so 2-D matmul weights
   (to_qkv_mlp_proj, to_out) are transposed+realised once here; the rank check
   auto-leaves the 1-D norm gains (norm_q/norm_k) un-transposed. *)
f32T[n_] := Module[{w = f32[n]}, If[ Length[Dimensions[w]] == 2, TRealize[Transpose[w]], w]];
b = "single_transformer_blocks.0.";
W = <|"to_qkv_mlp_proj"->f32T[b<>"attn.to_qkv_mlp_proj.weight"], "to_out"->f32T[b<>"attn.to_out.weight"],
      "norm_q"->f32T[b<>"attn.norm_q.weight"], "norm_k"->f32T[b<>"attn.norm_k.weight"]|>;
mod = <|"shift"->ref["mod_single_shift"], "scale"->ref["mod_single_scale"], "gate"->ref["mod_single_gate"]|>;
cfg = <|"heads"->24, "head_dim"->128, "eps"->1.*^-6|>;
pr["building single block..."];
out = fxSingleBlock[ref["single_hidden_in"], mod, ref["rope_cos"], ref["rope_sin"], W, cfg];
o = Normal@TRealize@out; r = Normal@ref["single_hidden_out"];
pr["single maxdiff = ", ToString[Max@Abs@Flatten[o-r], InputForm], "   (ref std 21.1)"];
pr[If[Max@Abs@Flatten[o-r]<1.*^-2, "SINGLE BLOCK PARITY OK", "MISMATCH"]];
