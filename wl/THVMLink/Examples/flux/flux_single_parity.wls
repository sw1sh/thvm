PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get["wl/THVMLink/Examples/flux/FluxForward.wl"]; TInit[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);
ref = TSafeTensorLoad["/tmp/flux_parity/ref_io.safetensors"];
t9 = "/Volumes/T9/hf-cache/hub/models--black-forest-labs--FLUX.2-klein-4B/snapshots/5e67da950fce4a097bc150c22958a05716994cea/transformer/diffusion_pytorch_model.safetensors";
wt = TSafeTensorLoad[t9];
f32[n_] := TUOpCast[wt[n], "f32"];
b = "single_transformer_blocks.0.";
W = <|"to_qkv_mlp_proj"->f32[b<>"attn.to_qkv_mlp_proj.weight"], "to_out"->f32[b<>"attn.to_out.weight"],
      "norm_q"->f32[b<>"attn.norm_q.weight"], "norm_k"->f32[b<>"attn.norm_k.weight"]|>;
mod = <|"shift"->ref["mod_single_shift"], "scale"->ref["mod_single_scale"], "gate"->ref["mod_single_gate"]|>;
cfg = <|"heads"->24, "head_dim"->128, "eps"->1.*^-6|>;
pr["building single block..."];
out = fxSingleBlock[ref["single_hidden_in"], mod, ref["rope_cos"], ref["rope_sin"], W, cfg];
o = Normal@TRealize@out; r = Normal@ref["single_hidden_out"];
pr["single maxdiff = ", ToString[Max@Abs@Flatten[o-r], InputForm], "   (ref std 21.1)"];
pr[If[Max@Abs@Flatten[o-r]<1.*^-2, "SINGLE BLOCK PARITY OK", "MISMATCH"]];
