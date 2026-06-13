PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get["wl/THVMLink/Examples/flux/FluxForward.wl"]; TInit[];
chk[lbl_,got_,orc_] := WriteString["stdout",lbl,": ",With[{md=Max@Abs@Flatten[got-orc]},ToString[md,InputForm]<>If[md<2.*^-4,"  OK","  FAIL"]],"\n"];
T[a_]:=TTensorCreate[NumericArray[N[a],"Real32"]];
SeedRandom[5];
(* TSiLU *)
xs=RandomReal[{-3,3},{20}]; chk["TSiLU", Normal@TRealize@TSiLU[T@xs], xs*LogisticSigmoid[xs]];
(* THeadAttention: q,k,v {S,H,Dh} *)
sq=6;sk=6;hh=4;dh=8; sc=1/Sqrt[N@dh];
qd=RandomReal[{-1,1},{sq,hh,dh}]; kd=RandomReal[{-1,1},{sk,hh,dh}]; vd=RandomReal[{-1,1},{sk,hh,dh}];
orc=Flatten[Table[With[{q=qd[[All,h]],k=kd[[All,h]],v=vd[[All,h]]},
   With[{sm=Map[#/Total[#]&, Exp[(q.Transpose[k])*sc]]}, sm.v]],{h,hh}]~Transpose~{2,1,3},2];
(* orc: {sq, hh*dh}; build per-head {sq,dh} then concat along dh *)
orc2=Table[Flatten@Table[With[{q=qd[[All,h]],k=kd[[All,h]],v=vd[[All,h]]},
   With[{sm=Map[#/Total[#]&, Exp[(q.Transpose[k])*sc]]}, (sm.v)[[i]]]],{h,hh}],{i,sq}];
chk["THeadAttention", Normal@TRealize@THeadAttention[T@qd,T@kd,T@vd,sc], orc2];
(* TSwiGLU projected form: linear_out(SwiGLU(linear_in(x))) with diffusers {out,in} weights *)
mlp=10; din=7; dout=5;
xin=RandomReal[{-1,1},{4,din}]; wIn=RandomReal[{-1,1},{2 mlp,din}]; wOut=RandomReal[{-1,1},{dout,mlp}];
hh2=xin.Transpose[wIn]; x1=hh2[[All,;;mlp]]; x2=hh2[[All,mlp+1;;]];
sw=(x1*LogisticSigmoid[x1]*x2).Transpose[wOut];
chk["TSwiGLU", Normal@TRealize@TSwiGLU[T@xin,T@wIn,T@wOut], sw];
