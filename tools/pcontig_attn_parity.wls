(* pcontig_attn_parity.wls -- CPU bit-exact parity probe for THVM_PCONTIG flash
   fusion.  Builds an attention (Q@K^T -> *scale -> softmax over keys -> @V) over
   already-realized q,k,v and realizes on DEV=cpu.  Run twice (THVM_PCONTIG unset
   vs THVM_PCONTIG=3) and diff the outputs -- they MUST be byte-identical.

     DEV=cpu wolframscript -f tools/pcontig_attn_parity.wls
*)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`Examples`"];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

dev = Environment["DEV"] /. x_ /; (x === $Failed || x === None) -> "cpu";

(* attention shapes: seq*headdim, small so cpu is fast *)
sq = 16; sk = 16; dh = 8; scale = 1./Sqrt[dh];

SeedRandom[12345];
qa = TRealize@TToDevice[TTensorCreate@NumericArray[RandomReal[{-1, 1}, {sq, dh}], "Real32"], dev];
ka = TRealize@TToDevice[TTensorCreate@NumericArray[RandomReal[{-1, 1}, {sk, dh}], "Real32"], dev];
va = TRealize@TToDevice[TTensorCreate@NumericArray[RandomReal[{-1, 1}, {sk, dh}], "Real32"], dev];

attn[q_, k_, v_] := Module[{scores, sm},
    scores = scale * TMatMul[q, Transpose[k]];   (* {sq, sk} *)
    sm     = TSoftmax[scores, 1];                 (* softmax over keys axis *)
    TMatMul[sm, v]                                (* {sq, dh} *)
];

out = TRealize@TToDevice[attn[qa, ka, va], dev];
res = Normal[out];

pr["PCONTIG=", Environment["THVM_PCONTIG"], "  dev=", dev];
pr["out shape: ", Dimensions[res]];
(* print a stable hash + the flattened values so the caller can diff two runs *)
pr["HASH: ", Hash[res, "SHA256"]];
pr["FLAT: ", N[Flatten[res], 17]];
