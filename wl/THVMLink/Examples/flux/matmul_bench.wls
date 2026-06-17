#!/usr/bin/env wolframscript
(* Focused bf16-output matmul microbench for the CUDA tiled-WMMA path.
   Exercises C = A . Transpose[W] with bf16 operands AND bf16 output -- the
   c_is_bf path that needs the per-warp f32 convert scratch, i.e. the one the
   128x256/32-warp tile can only use via DYNAMIC shared memory (extern
   __shared__ + cuFuncSetAttribute, > 48 KiB).  Reports rel-err vs an f32 host
   reference (correctness of the extern-shared kernel) and TFLOPS (timed with
   the graph disabled + per-launch sync so AbsoluteTiming reflects GPU time).

   Compare the dynamic-shared win on ONE binary:
     DEV=cuda M=1024 K=3072 N=12288 wolframscript matmul_bench.wls          (dynamic, 160 KiB cap -> 128x256)
     DEV=cuda THVM_TC_SMEM_CAP=49152 M=1024 K=3072 N=12288 wolframscript ... (static, 48 KiB cap -> 128x128)
   THVM_TC_DBG=1 prints the picked tile to stderr. *)
PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`"]; TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

dev = Environment["DEV"] /. x_ /; (x === $Failed || x === None) -> "cuda";
ev[name_, d_] := ToExpression[Environment[name] /. x_ /; (x === $Failed || x === None) -> d];
(* NB: do not use bare M/N/K as variables -- N is a Protected built-in. *)
dM = ev["M", "1024"]; dK = ev["K", "3072"]; dN = ev["N", "12288"]; nrep = ev["NREP", "20"];
pr["matmul bench: dev=", dev, "  C[", dM, ",", dN, "] = A[", dM, ",", dK, "] . W[", dN, ",", dK, "]^T  (bf16 in, bf16 out)"];

SeedRandom[7];
Ah = RandomReal[{-0.08, 0.08}, {dM, dK}];
Wh = RandomReal[{-0.08, 0.08}, {dN, dK}];
Ab = TRealize@TToDevice[TUOpCast[TTensorCreate@NumericArray[Ah, "Real32"], "bf16"], dev];
Wb = TRealize@TToDevice[TUOpCast[TTensorCreate@NumericArray[Wh, "Real32"], "bf16"], dev];
mm[a_] := TRealize@TMatMul[a, Transpose[Wb]];   (* bf16 operands -> bf16-output matmul *)

(* correctness: read the device result to host, compare to the f32 host ref *)
Cdev = Normal@mm[Ab];
Cref = Ah . Transpose[Wh];
absErr = Max@Abs@Flatten[Cdev - Cref];
relErr = absErr / Max@Abs@Flatten[Cref];
pr["  rel-err (bf16 vs f32 host ref) = ", ScientificForm[relErr, 3], "   abs-err = ", ScientificForm[absErr, 3]];

(* perf: graph off + per-launch sync (set via env on the wrapper) so each
   replay blocks until the GPU finishes; Min over reps is the steady GPU time *)
fj = TJit[mm]; fj[Ab];
t = Min@Table[AbsoluteTiming[fj[Ab]][[1]], {nrep}];
tflops = 2.0*dM*dN*dK/t/1.0*^12;
pr["  steady = ", Round[t*1000, 0.01], " ms/matmul    ", Round[tflops, 0.1], " TFLOPS"];
pr[If[relErr < 0.05, "OK", "FAIL(rel-err)"]];
