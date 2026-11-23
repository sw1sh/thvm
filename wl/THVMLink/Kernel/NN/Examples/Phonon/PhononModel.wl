(* PhononModel.wl -- Gradium Phonon 31M (gradium/phonon-31m-en, Apache 2.0), English text-to-speech, read
   from the runtime's model.q8.gguf.

   A 6-layer, 512-wide flow transformer reads the voice frames, the text tokens and a start latent, then
   produces one 32-dim audio latent per 80 ms frame: its last hidden state conditions a one-step LSD flow
   head that maps temperature-scaled noise to the latent, and an end-of-speech logit above 4 stops it.
   The Mimi decoder turns the latents into 24 kHz audio: a 1x1 projection to 512 channels, a depthwise
   16x transposed convolution, 2 transformer layers attending a 250-step window, and a SEANet decoder
   (three transposed convolutions of 6x, 5x and 4x, each followed by a residual block).

   The spec is pocket_tts's modules (flow_lm.py, mlp.py, transformer.py, attention.py, rope.py, mimi.py,
   seanet.py, conv.py) with Phonon's differences from transformers' gradium_phonon_tts: exact GELU, an RMS
   norm closing the time embeddings, and the speaker/duration condition added to every audio-latent
   position.  Weights decode to f32 on the host at load (bf16, and Q8_0 for the flow transformer's
   projections); everything runs on f32 tensors. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

Begin["`Private`"];

(* === weights ========================================================= *)

(* f16 words to reals: normal and subnormal numbers (Q8_0 scales are never inf or NaN). *)
phF16[h_List] := Module[{s, e, m},
    s = 1 - 2 BitShiftRight[h, 15];
    e = BitAnd[BitShiftRight[h, 10], 31];
    m = BitAnd[h, 1023];
    N[s (UnitStep[e - 1] (1 + m / 1024.) 2.^(e - 15) + (1 - UnitStep[e - 1]) m 2.^-24)]
]

phBf16[u16_List] := ImportByteArray[ExportByteArray[65536 u16, "UnsignedInteger32", ByteOrdering -> -1], "Real32", ByteOrdering -> -1]

(* Q8_0: blocks of 34 bytes, an f16 scale then 32 int8. *)
phQ8[bytes_List] := Module[{blocks = Partition[bytes, 34], d, q},
    d = phF16[blocks[[All, 1]] + 256 blocks[[All, 2]]];
    q = blocks[[All, 3 ;;]];
    Flatten[(q - 256 UnitStep[q - 128]) d]
]

(* Every tensor of the GGUF as a host array in torch layout (the GGUF dims reversed). *)
phGguf[path_String] := Module[{hdr = dsvGgufHeader[path], strm, ds, read},
    ds = hdr["DataStart"];
    strm = OpenRead[path, BinaryFormat -> True];
    read[t_] := Module[{dims = t["Dims"], n},
        n = Times @@ dims;
        SetStreamPosition[strm, ds + t["Offset"]];
        ArrayReshape[
            Switch[t["Quant"],
                0, BinaryReadList[strm, "Real32", n, ByteOrdering -> -1],
                30, phBf16[BinaryReadList[strm, "UnsignedInteger16", n, ByteOrdering -> -1]],
                8, phQ8[BinaryReadList[strm, "Byte", n / 32 * 34]]],
            Reverse[dims]]
    ];
    With[{res = read /@ hdr["Tensors"]}, Close[strm]; res]
]

(* A host array as an f32 tensor.  TTensor writes the reals into the tensor and converts them in C, with
   no Real32 NumericArray built on the way: that conversion costs the v2 kernel seconds per megabyte. *)
phT[a_] := TTensor[Dimensions[a], a]
(* A linear layer's weight {out, in} as the {in, out} matrix TLinear multiplies by. *)
phLin[a_] := phT[Transpose[a]]

phLayer[A_, p_String, scaled_] := Join[
    <|"n1w" -> phT[A[p <> "norm1.weight"]], "n1b" -> phT[A[p <> "norm1.bias"]],
      "n2w" -> phT[A[p <> "norm2.weight"]], "n2b" -> phT[A[p <> "norm2.bias"]],
      "in" -> phLin[A[p <> "self_attn.in_proj.weight"]], "out" -> phLin[A[p <> "self_attn.out_proj.weight"]],
      "l1" -> phLin[A[p <> "linear1.weight"]], "l2" -> phLin[A[p <> "linear2.weight"]]|>,
    If[scaled, <|"ls1" -> phT[A[p <> "layer_scale_1.scale"]], "ls2" -> phT[A[p <> "layer_scale_2.scale"]]|>, <||>]
]

(* The flow head's two time embeddings at LSD's single step (s = 0, t = 1), averaged: a constant. *)
phTimeCondition[A_] := Module[{p = "flow_lm.flow_net.time_embed.", emb},
    emb[i_, t_] := Module[{q = p <> ToString[i] <> ".", args, e, h},
        args = t A[q <> "freqs"];
        e = Join[Cos[args], Sin[args]];
        h = A[q <> "mlp.0.weight"] . e + A[q <> "mlp.0.bias"];
        h = A[q <> "mlp.2.weight"] . (h / (1 + Exp[-h])) + A[q <> "mlp.2.bias"];
        A[q <> "mlp.3.alpha"] h / Sqrt[Mean[h^2] + 1.*^-5]
    ];
    (emb[0, 0.] + emb[1, 1.]) / 2
]

(* The speaker-count and duration condition every audio-latent position adds: num_speakers = 1 and
   duration_delta = 0 (config.json), so the duration embedding is cos 0 = 1 then sin 0 = 0. *)
phCondition[A_] := Module[{c = "flow_lm.condition_provider.conditioners."},
    A[c <> "num_speakers.output_proj.weight"] . A[c <> "num_speakers.embed.weight"][[2]] +
        A[c <> "duration_delta.output_proj.weight"] . Join[ConstantArray[1., 64], ConstantArray[0., 64]]
]

(* conv weight {Cout, Cin, K} as K matrices {Cin, Cout}; transposed-conv weight {Cin, Cout, K} as one
   {Cin, K * Cout} matrix whose product reshapes to {T, K, Cout}. *)
phConvW[w_] := Table[phT[Transpose[w[[All, All, k]]]], {k, Dimensions[w][[3]]}]
phConvTrW[w_] := phT[Flatten /@ (Transpose /@ w)]

phLoadGguf[path_String] := Module[{A = phGguf[path], f = "flow_lm.", fn = "flow_lm.flow_net.", d = "mimi.decoder.model."},
    <|
        "Speaker" -> A[f <> "condition_provider.conditioners.speaker_wavs.output_proj.weight"],
        "Embed" -> A[f <> "condition_provider.conditioners.transcript_in_segment.embed.weight"],
        "Condition" -> phCondition[A],
        "Bos" -> A[f <> "input_linear.weight"] . A[f <> "bos_emb"],
        "InputW" -> phLin[A[f <> "input_linear.weight"]],
        "ConditionT" -> phT[{phCondition[A]}],
        "Layers" -> Table[phLayer[A, f <> "transformer.layers." <> ToString[i] <> ".", False], {i, 0, 5}],
        "OutNormW" -> phT[A[f <> "out_norm.weight"]], "OutNormB" -> phT[A[f <> "out_norm.bias"]],
        "EosW" -> phLin[A[f <> "out_eos.weight"]], "EosB" -> phT[A[f <> "out_eos.bias"]],
        "Head" -> Join[
            Association @ Flatten @ Table[
                {k -> phLin[A[fn <> k <> ".weight"]], k <> "b" -> phT[A[fn <> k <> ".bias"]]},
                {k, Join[{"cond_embed", "input_proj", "final_layer.adaLN_modulation.1", "final_layer.linear"},
                    Flatten @ Table["res_blocks." <> ToString[i] <> "." <> s, {i, 0, 5},
                        {s, {"adaLN_modulation.1", "mlp.0", "mlp.2"}}]]}],
            Association @ Flatten @ Table[
                {"ln" <> ToString[i] <> "w" -> phT[A[fn <> "res_blocks." <> ToString[i] <> ".in_ln.weight"]],
                 "ln" <> ToString[i] <> "b" -> phT[A[fn <> "res_blocks." <> ToString[i] <> ".in_ln.bias"]]},
                {i, 0, 5}],
            <|"time" -> phT[{phTimeCondition[A]}]|>],
        "LatentStd" -> A[f <> "emb_std"], "LatentMean" -> A[f <> "emb_mean"],
        "Mimi" -> <|
            "Quantizer" -> phLin[A["mimi.quantizer.output_proj.weight"][[All, All, 1]]],
            "Upsample" -> phT[Transpose[A["mimi.upsample.convtr.convtr.weight"][[All, 1, All]]]],
            "Layers" -> Table[phLayer[A, "mimi.decoder_transformer.transformer.layers." <> ToString[i] <> ".", True], {i, 0, 1}],
            "Conv0" -> {phConvW[A[d <> "0.conv.weight"]], phT[A[d <> "0.conv.bias"]]},
            "Stages" -> Function[{up, res, s}, With[{u = d <> ToString[up], r = d <> ToString[res]},
                <|"Up" -> phConvTrW[A[u <> ".convtr.weight"]], "UpB" -> phT[A[u <> ".convtr.bias"]],
                  "Stride" -> s, "Channels" -> Dimensions[A[u <> ".convtr.weight"]][[2]],
                  "Res1" -> {phConvW[A[r <> ".block.1.conv.weight"]], phT[A[r <> ".block.1.conv.bias"]]},
                  "Res2" -> {phConvW[A[r <> ".block.3.conv.weight"]], phT[A[r <> ".block.3.conv.bias"]]}|>]] @@@
                {{2, 3, 6}, {5, 6, 5}, {8, 9, 4}},
            "ConvOut" -> {phConvW[A[d <> "11.conv.weight"]], phT[A[d <> "11.conv.bias"]]}|>
    |>
]

(* === helpers ========================================================= *)

phHost[t_] := Normal @ TTensorData @ TRealize @ t

phZeros[shape_] := phT[ConstantArray[0., shape]]

(* x . w as a kernel of its own, its input and its product realized: a matrix product alone in its kernel
   runs through BLAS, one fused with the normalisation or activation around it does not. *)
phMat[x_, w_] := TRealize[TLinear[TRealize[x], w]]

(* === transformer ===================================================== *)

(* Interleaved rotary tables for positions off .. off + s - 1, {s, 1, 64} each (pocket_tts rope.py). *)
phRope[off_Integer, s_Integer] := Module[{fr = Exp[-Log[10000.] 2 Range[0, 31] / 64], ang, c, sn},
    ang = Outer[Times, N @ Range[off, off + s - 1], fr];
    c = ArrayReshape[Transpose[{Cos[ang], Cos[ang]}, {3, 1, 2}], {s, 1, 64}];
    sn = ArrayReshape[Transpose[{Sin[ang], Sin[ang]}, {3, 1, 2}], {s, 1, 64}];
    {phT[c], phT[sn]}
]

(* One pre-norm layer over x {s, 512}, attention then the exact-GELU feed-forward, layer scale when L has
   it.  kv takes this step's keys and values {s, 8, 64}, updates the layer's cache and gives the realized
   keys and values attention reads. *)
phBlock[x_, L_, rope_, mask_, kv_] := Module[{s = First @ Dimensions[x], qkv, q, k, v, o, x2, f},
    qkv = phMat[TLayerNormAffine[x, L["n1w"], L["n1b"]], L["in"]];
    q = TRealize @ TRoPEInterleaved[ArrayReshape[qkv[[All, 1 ;; 512]], {s, 8, 64}], rope[[1]], rope[[2]]];
    {k, v} = kv[
        TRoPEInterleaved[ArrayReshape[qkv[[All, 513 ;; 1024]], {s, 8, 64}], rope[[1]], rope[[2]]],
        ArrayReshape[qkv[[All, 1025 ;; 1536]], {s, 8, 64}]];
    o = phMat[THeadAttention[q, k, v, 0.125, mask], L["out"]];
    If[KeyExistsQ[L, "ls1"], o = o * L["ls1"]];
    x2 = TRealize[x + o];
    f = phMat[TGELU[phMat[TLayerNormAffine[x2, L["n2w"], L["n2b"]], L["l1"]], "Erf"], L["l2"]];
    If[KeyExistsQ[L, "ls2"], f = f * L["ls2"]];
    TRealize[x2 + f]
]

phStack[layers_, x0_, rope_, mask_, kvs_List] := Module[{x = x0},
    Do[x = phBlock[x, layers[[i]], rope, mask, kvs[[i]]], {i, Length[layers]}];
    x
]

(* === flow head ======================================================= *)

phHeadLin[H_, k_String, x_] := phMat[x, H[k]] + H[k <> "b"]

(* LSD with one step: x0 + flow_net(c, s = 0, t = 1, x0) for the hidden state c {1, 512} and the noise x0
   {1, 32} (pocket_tts mlp.SimpleMLPAdaLN). *)
phFlowHead[H_, c_, x0_] := Module[{sy, x, m, h},
    sy = TRealize @ TSiLU[phHeadLin[H, "cond_embed", c] + H["time"]];
    x = TRealize @ phHeadLin[H, "input_proj", x0];
    Do[
        With[{b = "res_blocks." <> ToString[i] <> "."},
            m = TRealize @ phHeadLin[H, b <> "adaLN_modulation.1", sy];
            h = (TLayerNorm[x, 1.*^-6] * H["ln" <> ToString[i] <> "w"] + H["ln" <> ToString[i] <> "b"]) *
                (1 + m[[All, 513 ;; 1024]]) + m[[All, 1 ;; 512]];
            x = TRealize[x + m[[All, 1025 ;; 1536]] * phHeadLin[H, b <> "mlp.2", TSiLU[phHeadLin[H, b <> "mlp.0", h]]]]],
        {i, 0, 5}];
    m = TRealize @ phHeadLin[H, "final_layer.adaLN_modulation.1", sy];
    TRealize[x0 + phHeadLin[H, "final_layer.linear", TLayerNorm[x, 1.*^-6] * (1 + m[[All, 513 ;; 1024]]) + m[[All, 1 ;; 512]]]]
]

(* === Mimi decoder ==================================================== *)

phElu[x_] := TReLU[x] + Exp[TMinimum[x, 0.]] - 1

(* Causal conv over time-major x {T, Cin}: `tail` holds the last K - 1 input rows of the previous chunk
   (zeros at the start) and is updated in place; None for a kernel of 1.  Gives y {T, Cout}. *)
phConv[x_, {ws_, b_}, tail_] := Module[{kk = Length[ws], t = First @ Dimensions[x], xp},
    xp = TRealize[If[kk > 1, Join[tail, x, 1], x]];
    If[kk > 1, TSet[tail, xp[[t + 1 ;; t + kk - 1]]]];
    TRealize[Total[Table[phMat[xp[[k ;; k + t - 1]], ws[[k]]], {k, kk}]] + b]
]

(* Transposed conv with kernel 2 * stride over x {T, Cin}: frame t writes 2s samples, its first half at
   block t and its second half onto block t + 1.  `partial` {s, Cout} holds the previous chunk's last
   second half (zeros at the start) and is updated in place.  Gives y {T s, Cout}. *)
phConvTr[x_, w_, b_, s_Integer, cout_Integer, partial_] := With[{t = First @ Dimensions[x]},
    phOverlapAdd[ArrayReshape[phMat[x, w], {t, 2 s, cout}], b, s, cout, partial, t]
]

phOverlapAdd[c_, b_, s_, cout_, partial_, t_] := Module[{a = c[[All, 1 ;; s]], bb = c[[All, s + 1 ;; 2 s]], y},
    y = ArrayReshape[a + Join[ArrayReshape[partial, {1, s, cout}], bb[[1 ;; t - 1]], 1], {t s, cout}];
    y = TRealize[If[b === None, y, y + b]];
    TSet[partial, ArrayReshape[bb[[t ;; t]], {s, cout}]];
    y
]

(* The streaming state, zero at the start of an utterance: the upsampler's partial, each decoder layer's
   last 250 keys and values, then the conv tails and partials down the SEANet decoder. *)
phMimiState[W_] := {phZeros[{16, 512}], Table[phZeros[{250, 8, 64}], 2, 2], phZeros[{6, 512}],
    Table[{phZeros[{st["Stride"], st["Channels"]}], phZeros[{2, st["Channels"]}]}, {st, W["Mimi"]["Stages"]}],
    phZeros[{2, 64}]}

(* One chunk: $phMimiChunk latents z {n, 32} (denormalised) to {n * 1920, 1} samples, the state updated
   in place. *)
phMimiChunkStep[S_, z_, cos_, sin_, mask_] := Module[
    {mi = S["M"]["Mimi"], n = $phMimiChunk, t, up, caches, conv0, stages, out, x},
    {up, caches, conv0, stages, out} = S["Mimi"];
    t = 16 n;
    (* depthwise 16x transposed conv: frame t times the {32, 512} kernel *)
    x = phOverlapAdd[ArrayReshape[phMat[z, mi["Quantizer"]], {n, 1, 512}] * ArrayReshape[mi["Upsample"], {1, 32, 512}],
        None, 16, 512, up, n];
    x = phStack[mi["Layers"], x, {cos, sin}, mask,
        Function[{c}, With[{k = TRealize[Join[c[[1]], #1, 1]], v = TRealize[Join[c[[2]], #2, 1]]},
            TSet[c[[1]], k[[t + 1 ;; t + 250]]]; TSet[c[[2]], v[[t + 1 ;; t + 250]]]; {k, v}] &] /@ caches];
    x = phConv[x, mi["Conv0"], conv0];
    Do[
        With[{sg = mi["Stages"][[i]], ss = stages[[i]]},
            Module[{u, v},
                u = phConvTr[phElu[x], sg["Up"], sg["UpB"], sg["Stride"], sg["Channels"], ss[[1]]];
                v = phConv[phElu[u], sg["Res1"], ss[[2]]];
                v = phConv[phElu[v], sg["Res2"], None];
                x = TRealize[u + v]]],
        {i, 3}];
    phConv[phElu[x], mi["ConvOut"], out]
]

(* === session ========================================================= *)

(* A session holds the weights, the state buffers and three captured steps (TJit): the prompt prefill,
   one decode step and one Mimi chunk.  A captured step replays its kernels into the same buffers and
   updates the state buffers in place, so synthesis allocates nothing per frame.  Every step has one shape
   whatever the text:

   - the prompt (voice frames, text tokens, start latent) is zero-padded to $phPromptRows positions;
   - keys and values live in {$phContext, 8, 64} caches whose unused rows are zero, a decode step writes
     its row through a one-hot column and masks the rows after it;
   - Mimi decodes $phMimiChunk frames at a time over its last 250 keys.

   A step writes a state buffer only after realizing everything that reads its old contents. *)
$phPromptRows = 256;
$phContext = 512;
$phMimiChunk = 16;

(* Host-built step inputs by position, kept so that a session reuses their tensors. *)
$phInputs = <||>;
SetAttributes[phInput, HoldRest];
phInput[key_, body_] := If[KeyExistsQ[$phInputs, key], $phInputs[key], $phInputs[key] = body]

phRopeAt[off_Integer, s_Integer] := phInput[{"rope", off, s}, phRope[off, s]]

(* A decode step at position pos: the one-hot column of its cache row, and the additive mask of the rows
   after it. *)
phDecodeInputs[pos_Integer] := phInput[{"decode", pos}, With[{n = $phContext},
    {phT[ArrayReshape[UnitVector[n, pos + 1], {n, 1, 1}]], phT[{(UnitStep[pos - Range[0, n - 1]] - 1) 1.*^9}]}]]

(* A prompt of p positions: the column of its valid cache rows, and the one-hot row of its last one. *)
phPromptInputs[p_Integer] := phInput[{"prompt", p}, With[{n = $phContext},
    {phT[ArrayReshape[UnitStep[p - 1 - Range[0, n - 1]], {n, 1, 1}]], phT[{UnitVector[$phPromptRows, p]}]}]]

(* Additive mask for Mimi queries at off .. off + lq - 1 over the 250 cached keys then the lq new ones
   (keys at off - 250 .. off + lq - 1): a key counts when it is at most 249 steps back, not ahead and not
   before the start.  The same mask from off = 250 on. *)
phWindowMask[off_Integer, lq_Integer] := With[{o = Min[off, 250]}, phInput[{"window", o, lq},
    Module[{d = Outer[Subtract, Range[0, lq - 1] + 250, Range[0, lq + 249]]},
        phT[(UnitStep[d] UnitStep[249 - d] ConstantArray[UnitStep[Range[0, lq + 249] + o - 250], lq] - 1) 1.*^9]]]]

phEos[M_, h_] := phMat[h, M["EosW"]] + M["EosB"]

(* The prompt {$phPromptRows, 512}: fills the caches and the hidden state of the last position, and gives
   its end-of-speech logit {1, 1}. *)
phPrefillStep[S_, prompt_, valid_, sel_] := Module[{M = S["M"], pp = $phPromptRows, pad = S["CachePad"], x, h},
    x = phStack[M["Layers"], prompt, phRope[0, pp], TCausalMask[pp],
        Function[{c}, With[{k = TRealize[#1], v = TRealize[#2]},
            TSet[c[[1]], Join[k, pad, 1] * valid]; TSet[c[[2]], Join[v, pad, 1] * valid]; {k, v}] &] /@ S["Caches"]];
    h = TRealize @ TLayerNormAffine[phMat[sel, x], M["OutNormW"], M["OutNormB"]];
    TSet[S["H"], h];
    TRealize @ phEos[M, h]
]

(* One frame: the flow head draws the latent from the hidden state, and the latent enters the transformer
   as input_linear(latent) + condition, giving the next hidden state.  Gives {1, 33}: the latent, then
   the end-of-speech logit of the next hidden state. *)
phDecodeStep[S_, noise_, cos_, sin_, oh_, mask_] := Module[{M = S["M"], lat, x, h},
    lat = phFlowHead[M["Head"], S["H"], noise];
    x = phStack[M["Layers"], phMat[lat, M["InputW"]] + M["ConditionT"], {cos, sin}, mask,
        Function[{c}, (TSet[c[[1]], c[[1]] + oh #1]; TSet[c[[2]], c[[2]] + oh #2]; c) &] /@ S["Caches"]];
    h = TRealize @ TLayerNormAffine[x, M["OutNormW"], M["OutNormB"]];
    TSet[S["H"], h];
    TRealize @ Join[lat, phEos[M, h], 2]
]

phPrefillJit[c_] := TJit[phPrefillStep[c, ##] &]
phDecodeJit[c_] := TJit[phDecodeStep[c, ##] &]
phMimiJit[c_] := TJit[phMimiChunkStep[c, ##] &]

$phHostKeys = {"Speaker", "Embed", "Condition", "Bos", "LatentStd", "LatentMean"};

(* The session of loaded weights W.  The captured steps close over the device tensors only: a TJit
   closure is found by the hash of what it holds. *)
phSession[W_] := Module[{n = $phContext, core},
    $phInputs = <||>;
    core = <|"M" -> KeyDrop[W, $phHostKeys], "CachePad" -> phZeros[{n - $phPromptRows, 8, 64}],
        "H" -> phZeros[{1, 512}], "Caches" -> Table[phZeros[{n, 8, 64}], 6, 2], "Mimi" -> phMimiState[W]|>;
    Join[core, <|"Host" -> KeyTake[W, $phHostKeys], "MimiZero" -> phMimiState[W], "Noise0" -> phZeros[{1, 32}],
        "Prefill" -> phPrefillJit[core], "Decode" -> phDecodeJit[core], "MimiStep" -> phMimiJit[core]|>]
]

phSessionClose[S_] := Scan[TJitDrop, Lookup[S, {"Prefill", "Decode", "MimiStep"}]]

(* The prompt rows {p, 512}: the voice rows (voice frames {F, 512} through the speaker projection), the
   embeddings of the text token ids (0-based), then the start latent. *)
phPrompt[S_, ids_List, rows_] := With[{h = S["Host"]},
    Join[rows, h["Embed"][[ids + 1]], {h["Bos"] + h["Condition"]}]]

(* Latents {n, 32} (normalised) for a prompt, from noise of variance temperature (0: deterministic).
   Stops afterEos frames after the end-of-speech logit first passes 4, or at maxFrames. *)
phLatents[S_, prompt_, temperature_, maxFrames_Integer, afterEos_Integer] := Module[
    {p = Length[prompt], out, eos, lats = {}, eosAt = None},
    eos = First @ Flatten @ phHost[
        S["Prefill"][phT[PadRight[prompt, {$phPromptRows, 512}]], Sequence @@ phPromptInputs[p]]];
    Do[
        If[eosAt === None && eos > 4., eosAt = g];
        If[eosAt =!= None && g >= eosAt + afterEos, Break[]];
        out = First @ phHost[S["Decode"][
            If[temperature > 0, phT[RandomVariate[NormalDistribution[0, Sqrt[temperature]], {1, 32}]], S["Noise0"]],
            Sequence @@ phRopeAt[p + g, 1], Sequence @@ phDecodeInputs[p + g]]];
        AppendTo[lats, Most[out]];
        eos = Last[out],
        {g, 0, Min[maxFrames, $phContext - p] - 1}];
    lats
]

(* Latents {n, 32} (normalised) to {n * 1920} samples at 24 kHz: zero frames pad the last chunk, and
   causality keeps them out of the real samples. *)
phMimiDecode[S_, lats_] := Module[{c = $phMimiChunk, h = S["Host"], off = 0, audio = {}},
    MapThread[TSet, {Flatten[S["Mimi"]], Flatten[S["MimiZero"]]}];
    Do[
        AppendTo[audio, Flatten @ phHost[S["MimiStep"][phT[(# h["LatentStd"] + h["LatentMean"]) & /@ chunk],
            Sequence @@ phRopeAt[off, 16 c], phWindowMask[off, 16 c]]]];
        off += 16 c,
        {chunk, Partition[PadRight[lats, {c Ceiling[Length[lats] / c], 32}], c]}];
    Take[Flatten[audio], 1920 Length[lats]]
]

End[];

EndPackage[];
