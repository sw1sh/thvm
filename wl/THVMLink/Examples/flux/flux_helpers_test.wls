PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxForward.wl"}]];
TInit[];
chk[lbl_, got_, oracle_] := WriteString["stdout", lbl, ": ",
   With[{md = Max@Abs@Flatten[got - oracle]}, ToString[md, InputForm] <> If[md < 2.*^-5, "  OK", "  FAIL"]], "\n"];
T[a_] := TTensorCreate[NumericArray[N[a], "Real32"]];
SeedRandom[3];
(* fxLinear: x{S,in} . W{out,in}^T -> {S,out} *)
xd = RandomReal[{-1,1},{5,16}]; wd = RandomReal[{-1,1},{12,16}];
chk["fxLinear", Normal@TRealize@fxLinear[T@xd, T@wd], xd . Transpose[wd]];
(* fxRMSNorm: x{S,H,D}, weight{D} *)
xd2 = RandomReal[{-1,1},{4,3,8}]; ww = RandomReal[{0.5,1.5},{8}]; eps=1.*^-6;
rms = Map[#/Sqrt[Mean[#^2]+eps]*ww &, xd2, {2}];
chk["fxRMSNorm", Normal@TRealize@fxRMSNorm[T@xd2, T@ww, eps], rms];
(* fxLayerNorm affine-free: x{S,D} *)
xd3 = RandomReal[{-1,1},{6,10}];
ln = Map[With[{m=Mean[#], v=Variance[#]*(Length[#]-1)/Length[#]}, (#-m)/Sqrt[v+eps]] &, xd3];
chk["fxLayerNorm", Normal@TRealize@fxLayerNorm[T@xd3, eps], ln];
(* fxRoPE regression *)
s=4;h=2;dd=8; xr=RandomReal[{-1,1},{s,h,dd}]; cr=RandomReal[{-1,1},{s,1,dd}]; sr=RandomReal[{-1,1},{s,1,dd}];
orc = Table[With[{rot=Flatten@Table[{-xr[[i,j,2k]],xr[[i,j,2k-1]]},{k,dd/2}]}, xr[[i,j]]*cr[[i,1]]+rot*sr[[i,1]]],{i,s},{j,h}];
chk["fxRoPE", Normal@TRealize@fxRoPE[T@xr, T@cr, T@sr], orc];
