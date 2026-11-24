(* WhisperModel.wl -- OpenAI Whisper (MIT), speech to text, from the HuggingFace safetensors of
   openai/whisper-*.

   The port follows tinygrad's examples/whisper.py: MultiHeadAttention with its self and cross key/value
   caches (14-58), ResidualAttentionBlock (60-75), AudioEncoder (77-93), TextDecoder (95-118), prep_audio
   (136-169), get_encoding (184-208) and transcribe_waveform's greedy loop (240-285); and
   examples/audio_helpers.py: mel (51-79).  Two choices are the original model's, which tinygrad's example
   leaves: the exact GELU, and the reflected edges of the short-time Fourier transform.

   An encoder of two convolutions and n pre-norm transformer layers reads 30 seconds of log-mel spectrogram
   (3000 frames of 80 bands) into 1500 states; a decoder of n layers, each attending its own past and the
   encoder's states, gives the next token's logits.

   As in Phonon, every step has one shape and is captured once (TJit).  The encoder is three steps: the
   stem takes the samples of a segment, frames them and computes the spectrogram itself; one layer is
   captured with its weights as inputs and replayed for each layer, so the encoder keeps the
   intermediates of one layer (the attention matrices of 1500 positions are most of them), not of all;
   the last step leaves each decoder layer's cross keys and values in state buffers.  The decoder step
   takes the embedding of one token at its position, a row the host reads from the model file so that no
   step multiplies by the whole table, writes its keys and values into the cache row of its position
   through a one-hot column, and masks the rows after it.

   Every product is a kernel of its own that BLAS runs (whMat); the two convolutions are products too,
   over three shifted slices of their input laid side by side (whConv). *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

Begin["`Private`"];

(* === helpers ========================================================= *)

whT[a_] := TTensor[Dimensions[a], a]

whHost[t_] := Normal @ TTensorData @ TRealize @ t

whZeros[shape_] := whT[ConstantArray[0., shape]]

(* x . w as a kernel of its own, so that it runs through BLAS (see phMat). *)
whMat[x_, w_] := TRealize[TLinear[TRealize[x], w]]

(* A linear layer of weight w {out, in}, as the file has it: the transpose stays a view of the mapped
   weight, which BLAS multiplies by as it is, so the model keeps no copy of its weights. *)
whLinear[x_, w_] := whMat[x, Transpose[w]]

(* x {s, d} as {s, heads, d / heads}. *)
whHeads[x_, h_Integer] := With[{dims = Dimensions[x]}, ArrayReshape[x, {dims[[1]], h, dims[[2]] / h}]]

(* === audio front end ================================================== *)

$whRate = 16000;
$whSegment = 480000;

(* Slaney-scale mel filters {bands, 201} for 16 kHz and a 400-point transform (tinygrad
   examples/audio_helpers.py mel, itself librosa's). *)
whMelFilters[bands_Integer] := Module[{hzToMel, melToHz, fft = N @ Range[0, 200] * 16000 / 400, edges, diff, ramps, lower, upper},
    hzToMel[f_] := If[f >= 1000., 15. + Log[f / 1000.] / (Log[6.4] / 27.), f / (200. / 3)];
    melToHz[m_] := If[m >= 15., 1000. Exp[(Log[6.4] / 27.) (m - 15.)], (200. / 3) m];
    edges = melToHz /@ Subdivide[hzToMel[0.], hzToMel[8000.], bands + 1];
    diff = Differences[edges];
    ramps = Outer[Subtract, edges, fft];
    lower = -ramps[[;; bands]] / diff[[;; bands]];
    upper = ramps[[3 ;;]] / diff[[2 ;;]];
    Map[Max[#, 0.] &, MapThread[Min, {lower, upper}, 2], {2}] * (2. / (edges[[3 ;;]] - edges[[;; bands]]))
]

(* The real and imaginary parts of a 400-point Fourier transform of a Hann-windowed frame, as {400, 201}
   matrices to multiply frames by: the window is folded into them. *)
whFourier[] := Module[{hann = 0.5 - 0.5 Cos[2. Pi Range[0, 399] / 400], angle = 2. Pi Outer[Times, Range[0, 399], Range[0, 200]] / 400},
    {hann Cos[angle], hann Sin[angle]}
]

(* The 30-second segments 16 kHz samples make, the last one zero-padded (tinygrad prep_audio). *)
whSegmentCount[samples_List] := Max[1, Ceiling[Length[samples] / $whSegment]]

(* The samples of segment s as the stem takes them: the segment, zero-padded to 30 seconds, with its
   edges reflected by 200 samples (the original model's centered frames), cut where the last frame ends
   in whole hops: 3002 hops of 160. *)
whSegmentSamples[samples_List, s_Integer] := With[
    {seg = PadRight[N @ samples[[(s - 1) $whSegment + 1 ;; Min[s $whSegment, Length[samples]]]], $whSegment, 0.]},
    Take[Join[Reverse[seg[[2 ;; 201]]], seg, Reverse[seg[[-201 ;; -2]]]], 3002 * 160]
]

(* The frames {3000, 400} of the samples of a segment: a frame starts every 160 samples (tinygrad
   prep_audio), so it is two and a half consecutive hops. *)
whFrames[seg_] := With[{hops = ArrayReshape[seg, {3002, 160}]},
    TRealize @ Join[hops[[1 ;; 3000]], hops[[2 ;; 3001]], hops[[3 ;; 3002, 1 ;; 80]], 2]]

(* === weights ========================================================= *)

whAttention[W_, p_String] := <|
    "q" -> W[p <> "q_proj.weight"], "qb" -> W[p <> "q_proj.bias"],
    "k" -> W[p <> "k_proj.weight"],
    "v" -> W[p <> "v_proj.weight"], "vb" -> W[p <> "v_proj.bias"],
    "o" -> W[p <> "out_proj.weight"], "ob" -> W[p <> "out_proj.bias"]|>

whLayer[W_, p_String, cross_] := Join[
    <|"Self" -> whAttention[W, p <> "self_attn."],
      "ln1w" -> W[p <> "self_attn_layer_norm.weight"], "ln1b" -> W[p <> "self_attn_layer_norm.bias"],
      "f1" -> W[p <> "fc1.weight"], "f1b" -> W[p <> "fc1.bias"],
      "f2" -> W[p <> "fc2.weight"], "f2b" -> W[p <> "fc2.bias"],
      "ln2w" -> W[p <> "final_layer_norm.weight"], "ln2b" -> W[p <> "final_layer_norm.bias"]|>,
    If[cross,
        <|"Cross" -> whAttention[W, p <> "encoder_attn."],
          "lncw" -> W[p <> "encoder_attn_layer_norm.weight"], "lncb" -> W[p <> "encoder_attn_layer_norm.bias"]|>,
        <||>]
]

(* A convolution's kernel {out, in, 3} as the {3 in, out} matrix whConv multiplies by: row (tap, channel). *)
whConvMatrix[w_] := With[{dims = Dimensions[w]},
    TRealize @ ArrayReshape[Transpose[w, {3, 2, 1}], {3 dims[[2]], dims[[1]]}]]

(* Where a tensor's data starts in a safetensors file: the file opens with the length of its JSON header,
   8 bytes little-endian, and the header gives each tensor's offsets after it. *)
whTensorOffset[path_String, name_String] := Module[{strm = OpenRead[path, BinaryFormat -> True], n, entry},
    n = BinaryRead[strm, "UnsignedInteger64", ByteOrdering -> -1];
    entry = ImportByteArray[ByteArray[BinaryReadList[strm, "Byte", n]], "RawJSON"][name];
    Close[strm];
    8 + n + First[entry["data_offsets"]]
]

(* The model of a model.safetensors file: its sizes are read off the tensors. *)
whLoad[path_String] := Module[{W = TSafeTensorLoad[path], d, layers, e = "model.encoder.", dc = "model.decoder.", fourier = whFourier[]},
    d = First @ Dimensions[W[e <> "conv1.weight"]];
    layers[p_] := Count[Keys[W], k_ /; StringMatchQ[k, p <> "layers." ~~ DigitCharacter .. ~~ ".fc1.weight"]];
    <|
        "Width" -> d, "Heads" -> d / 64, "Vocabulary" -> First @ Dimensions[W[dc <> "embed_tokens.weight"]],
        "Context" -> First @ Dimensions[W[dc <> "embed_positions.weight"]],
        "FourierRe" -> whT[fourier[[1]]], "FourierIm" -> whT[fourier[[2]]], "Mel" -> whT[Transpose[whMelFilters[80]]],
        "Conv1W" -> whConvMatrix[W[e <> "conv1.weight"]], "Conv1B" -> W[e <> "conv1.bias"],
        "Conv2W" -> whConvMatrix[W[e <> "conv2.weight"]], "Conv2B" -> W[e <> "conv2.bias"],
        "MelPad" -> whZeros[{1, 80}], "ConvPad" -> whZeros[{1, d}],
        "EncoderPositions" -> W[e <> "embed_positions.weight"],
        "EncoderLayers" -> Table[whLayer[W, e <> "layers." <> ToString[i] <> ".", False], {i, 0, layers[e] - 1}],
        "EncoderNormW" -> W[e <> "layer_norm.weight"], "EncoderNormB" -> W[e <> "layer_norm.bias"],
        "Tokens" -> W[dc <> "embed_tokens.weight"], "Positions" -> whHost[W[dc <> "embed_positions.weight"]],
        "File" -> path, "TokensAt" -> whTensorOffset[path, dc <> "embed_tokens.weight"],
        "DecoderLayers" -> Table[whLayer[W, dc <> "layers." <> ToString[i] <> ".", True], {i, 0, layers[dc] - 1}],
        "DecoderNormW" -> W[dc <> "layer_norm.weight"], "DecoderNormB" -> W[dc <> "layer_norm.bias"]
    |>
]

(* === layers ========================================================== *)

whProject[A_, key_String, x_] := whLinear[x, A[key]] + A[key <> "b"]

whFeedForward[x_, L_] := whLinear[TGELU[whLinear[TLayerNormAffine[x, L["ln2w"], L["ln2b"]], L["f1"]] + L["f1b"], "Erf"], L["f2"]] + L["f2b"]

(* An encoder layer over x {1500, d}: every position attends every other. *)
whEncoderLayer[x_, L_, h_Integer] := Module[{A = L["Self"], y, q, k, v, x2},
    y = TRealize @ TLayerNormAffine[x, L["ln1w"], L["ln1b"]];
    q = TRealize @ whHeads[whProject[A, "q", y], h];
    k = TRealize @ whHeads[whLinear[y, A["k"]], h];
    v = TRealize @ whHeads[whProject[A, "v", y], h];
    x2 = TRealize[x + whProject[A, "o", THeadAttention[q, k, v, 0.125, None]]];
    TRealize[x2 + whFeedForward[x2, L]]
]

(* A decoder layer over the one new position x {1, d}: its keys and values replace row `oh` of the
   layer's self cache {context, heads, 64} (`keep` is 1 - oh), it attends the rows the mask leaves and
   then the encoder's states through the layer's cross keys and values. *)
whDecoderLayer[x_, L_, h_Integer, {selfK_, selfV_}, {crossK_, crossV_}, oh_, keep_, mask_] := Module[
    {A = L["Self"], X = L["Cross"], y, q, x2, x3},
    y = TRealize @ TLayerNormAffine[x, L["ln1w"], L["ln1b"]];
    q = TRealize @ whHeads[whProject[A, "q", y], h];
    TSet[selfK, selfK * keep + oh * whHeads[whLinear[y, A["k"]], h]];
    TSet[selfV, selfV * keep + oh * whHeads[whProject[A, "v", y], h]];
    x2 = TRealize[x + whProject[A, "o", THeadAttention[q, selfK, selfV, 0.125, mask]]];
    y = TRealize @ TLayerNormAffine[x2, L["lncw"], L["lncb"]];
    q = TRealize @ whHeads[whProject[X, "q", y], h];
    x3 = TRealize[x2 + whProject[X, "o", THeadAttention[q, crossK, crossV, 0.125, None]]];
    TRealize[x3 + whFeedForward[x3, L]]
]

(* === session ========================================================= *)

(* A convolution of kernel 3 and padding 1 along x {t, in}, as a product: output row i reads rows
   stride i .. stride i + 2 of the zero-padded input, laid side by side, times the kernel as a matrix
   (whConvMatrix).  With stride 2 the rows are the even and odd rows of the input in pairs. *)
whConv[x_, w_, b_, zero_, stride : 1 | 2] := Module[{t = First @ Dimensions[x], in = Last @ Dimensions[x], p, pairs},
    p = TRealize @ Join[zero, x, zero, 1];
    pairs[rows_, i_] := ArrayReshape[rows, {t / 2, 2, in}][[All, i]];
    whMat[
        If[stride == 1,
            Join[p[[1 ;; t]], p[[2 ;; t + 1]], p[[3 ;; t + 2]], 2],
            Join[pairs[p[[1 ;; t]], 1], pairs[p[[1 ;; t]], 2], pairs[p[[3 ;; t + 2]], 1], 2]],
        w] + b
]

(* The log-mel spectrogram {3000, 80} of frames {3000, 400}: power of the windowed Fourier transform,
   the mel filters, log10 floored 8 below its maximum, scaled (tinygrad prep_audio). *)
whLogMel[M_, frames_] := Module[{re = whMat[frames, M["FourierRe"]], im = whMat[frames, M["FourierIm"]], mel, top},
    mel = TRealize[Log[TMaximum[whMat[re * re + im * im, M["Mel"]], 1.*^-10]] / Log[10.]];
    top = ArrayReshape[TUOpReduce[TUOpReduce[mel, 1, "MAX"], 0, "MAX"], {1, 1}];
    TRealize[(TMaximum[mel, top - 8.] + 4.) / 4.]
]

(* The encoder's stem: the samples of a segment (whSegmentSamples) to the input {1500, d} of the first
   layer, through the spectrogram and the two convolutions. *)
whStemStep[M_, seg_] := Module[{x},
    x = TRealize @ TGELU[whConv[whLogMel[M, whFrames[seg]], M["Conv1W"], M["Conv1B"], M["MelPad"], 1], "Erf"];
    x = TGELU[whConv[x, M["Conv2W"], M["Conv2B"], M["ConvPad"], 2], "Erf"];
    TRealize[x + M["EncoderPositions"]]
]

(* An encoder layer's weights as the list the captured layer takes, and the layer of such a list. *)
$whLayerKeys = {"ln1w", "ln1b", "f1", "f1b", "f2", "f2b", "ln2w", "ln2b"};
$whAttentionKeys = {"q", "qb", "k", "v", "vb", "o", "ob"};

whLayerWeights[L_] := Join[Lookup[L, $whLayerKeys], Lookup[L["Self"], $whAttentionKeys]]

whLayerOf[weights_List] := Append[
    AssociationThread[$whLayerKeys, Take[weights, Length[$whLayerKeys]]],
    "Self" -> AssociationThread[$whAttentionKeys, Drop[weights, Length[$whLayerKeys]]]]

(* The encoder's last step: the output {1500, d} of the last layer to the encoder's states, each decoder
   layer's cross keys and values left in the state buffers. *)
whCrossStep[S_, last_] := Module[{M = S["M"], h = S["M"]["Heads"], x},
    x = TRealize @ TLayerNormAffine[last, M["EncoderNormW"], M["EncoderNormB"]];
    MapThread[
        Function[{L, kv},
            TSet[kv[[1]], whHeads[whLinear[x, L["Cross"]["k"]], h]];
            TSet[kv[[2]], whHeads[whProject[L["Cross"], "v", x], h]]],
        {M["DecoderLayers"], S["Cross"]}];
    x
]

(* The encoder over the samples of a segment, by the session's captured steps: gives its states. *)
whEncode[S_, seg_] := S["CrossStep"][
    Fold[S["Layer"][#1, Sequence @@ whLayerWeights[#2]] &, S["Stem"][seg], S["M"]["EncoderLayers"]]]

(* The decoder step: the embedding {1, d} of a token at its position (whEmbedding) to the logits
   {1, vocabulary} of the next token, the last hidden row times the token table. *)
whDecodeStep[S_, embedding_, oh_, keep_, mask_] := Module[{M = S["M"], h = S["M"]["Heads"], x = embedding},
    MapThread[
        Function[{L, self, cross}, x = whDecoderLayer[x, L, h, self, cross, oh, keep, mask]],
        {M["DecoderLayers"], S["Self"], S["Cross"]}];
    x = TRealize @ TLayerNormAffine[x, M["DecoderNormW"], M["DecoderNormB"]];
    whLinear[x, M["Tokens"]]
]

whStemJit[M_] := TJit[whStemStep[M, ##] &]
whLayerJit[h_Integer] := TJit[whEncoderLayer[#1, whLayerOf[{##2}], h] &]
whCrossJit[c_] := TJit[whCrossStep[c, ##] &]
whDecodeJit[c_] := TJit[whDecodeStep[c, ##] &]

(* Host-built step inputs by position, kept so that a session reuses their tensors. *)
$whInputs = <||>;
SetAttributes[whInput, HoldRest];
whInput[key_, body_] := If[KeyExistsQ[$whInputs, key], $whInputs[key], $whInputs[key] = body]

(* A decoder step at position pos of a context of n: the one-hot column of its cache row, its
   complement, and the additive mask of the rows after it. *)
whDecodeInputs[n_Integer, pos_Integer] := whInput[{n, pos}, With[{unit = N @ UnitVector[n, pos + 1]},
    {whT[ArrayReshape[unit, {n, 1, 1}]], whT[ArrayReshape[1. - unit, {n, 1, 1}]],
     whT[{(UnitStep[pos - Range[0, n - 1]] - 1) 1.*^9}]}]]

whSession[M_] := Module[{h = M["Heads"], n = M["Context"], layers = Length[M["DecoderLayers"]], core},
    core = <|"M" -> M, "Self" -> Table[whZeros[{n, h, 64}], layers, 2], "Cross" -> Table[whZeros[{1500, h, 64}], layers, 2]|>;
    Join[core, <|"Stem" -> whStemJit[M], "Layer" -> whLayerJit[h], "CrossStep" -> whCrossJit[core], "Decode" -> whDecodeJit[core]|>]
]

whSessionClose[S_] := Scan[TJitDrop, Lookup[S, {"Stem", "Layer", "CrossStep", "Decode"}]]

(* The decoder's input for a token (0-based id) at position pos: its row of the token table, read from
   the model file, plus the row of the position. *)
whEmbedding[M_, token_Integer, pos_Integer] := Module[{strm = OpenRead[M["File"], BinaryFormat -> True], row},
    SetStreamPosition[strm, M["TokensAt"] + 4 M["Width"] token];
    row = BinaryReadList[strm, "Real32", M["Width"], ByteOrdering -> -1];
    Close[strm];
    row + M["Positions"][[pos + 1]]
]

(* The logits after feeding token (0-based id) at position pos. *)
whStep[S_, token_Integer, pos_Integer] := Flatten @ whHost[
    S["Decode"][whT[{whEmbedding[S["M"], token, pos]}], Sequence @@ whDecodeInputs[S["M"]["Context"], pos]]]

(* === decoding ======================================================== *)

(* The special tokens of a vocabulary of n: the English-only one ends its text tokens at 50256, the
   multilingual one at 50257 (tinygrad get_encoding). *)
whSpecials[n_Integer] := With[{eot = If[n == 51864, 50256, 50257]},
    <|"EndOfText" -> eot, "Start" -> eot + 1, "Language" -> eot + 2, "Transcribe" -> eot + 102, "Previous" -> eot + 104,
      "NoSpeech" -> eot + 105, "NoTimestamps" -> eot + 106, "Multilingual" -> n != 51864|>]

$whLanguages = {"en", "zh", "de", "es", "ru", "ko", "fr", "ja", "pt", "tr", "pl", "ca", "nl", "ar", "sv", "it", "id", "hi", "fi", "vi",
    "he", "uk", "el", "ms", "cs", "ro", "da", "hu", "ta", "no", "th", "ur", "hr", "bg", "lt", "la", "mi", "ml", "cy", "sk", "te",
    "fa", "lv", "bn", "sr", "az", "sl", "kn", "et", "mk", "br", "eu", "is", "hy", "ne", "mn", "bs", "kk", "sq", "sw", "gl", "mr",
    "pa", "si", "km", "sn", "yo", "so", "af", "oc", "ka", "be", "tg", "sd", "gu", "am", "yi", "lo", "uz", "fo", "ht", "ps", "tk",
    "nn", "mt", "sa", "lb", "my", "bo", "tl", "mg", "as", "tt", "haw", "ln", "ha", "ba", "jw", "su"};

(* The tokens that open a transcription. *)
whStartTokens[sp_, language_String] := Join[{sp["Start"]},
    If[sp["Multilingual"], {sp["Language"] + First @ FirstPosition[$whLanguages, language, {1}] - 1, sp["Transcribe"]}, {}],
    {sp["NoTimestamps"]}]

(* Feeds the context tokens, then decodes greedily until the end-of-text token, half the context of new
   tokens, or the end of the context (tinygrad inferloop).  Gives the context with the new tokens, the
   probability of the no-speech token after the start token, and the mean log-probability of the new
   tokens, which is minus the log of the sum of the exponentials of the logits less their largest. *)
whGreedy[S_, context_List, sp_] := Module[
    {ctx = context, n = S["M"]["Context"], logits, next, noSpeech = 0., sum = 0., count = 0},
    Do[
        logits = whStep[S, ctx[[i]], i - 1];
        If[ctx[[i]] == sp["Start"],
            noSpeech = Exp[logits[[sp["NoSpeech"] + 1]] - Max[logits]] / Total[Exp[logits - Max[logits]]]],
        {i, Length[ctx]}];
    Do[
        next = First @ Ordering[logits, -1] - 1;
        sum -= Log[Total[Exp[logits - Max[logits]]]];
        count++;
        AppendTo[ctx, next];
        If[next == sp["EndOfText"] || Length[ctx] >= n, Break[]];
        logits = whStep[S, next, Length[ctx] - 1],
        {Quotient[n, 2]}];
    <|"Context" -> ctx, "NoSpeech" -> noSpeech, "LogProbability" -> sum / Max[count, 1]|>
]

(* The language of the segment just encoded, of a multilingual model: the likeliest language token after
   the start token (OpenAI whisper decoding.py detect_language). *)
whDetectLanguage[S_, sp_] := With[{logits = whStep[S, sp["Start"], 0]},
    $whLanguages[[First @ Ordering[logits[[sp["Language"] + 1 ;; sp["Language"] + Length[$whLanguages]]], -1]]]]

(* A segment whose samples stay within one 16-bit step is digital silence: there is nothing to decode,
   and the model would write a word for it all the same. *)
$whSilence = 2.^-15;

(* Whether the model itself takes a decoded segment (whGreedy) for no speech: a no-speech probability
   above 0.6 with a mean log-probability below -1 (OpenAI whisper transcribe.py, no_speech_threshold and
   logprob_threshold). *)
whNoSpeechQ[out_] := out["NoSpeech"] > 0.6 && out["LogProbability"] < -1.

(* The text tokens of 16 kHz samples and the language they were read in, segment by segment; each
   segment's decoding is prompted with the text of the one before (tinygrad transcribe_waveform).
   language is a code of $whLanguages or Automatic, which a multilingual model detects from the first
   segment with sound.  A silent segment is not decoded, and a decoded one that is no speech
   (whNoSpeechQ) is dropped.  report[s, n] is called before segment s of n. *)
whTranscribe[S_, samples_List, language_, report_ : (Null &)] := Module[
    {sp = whSpecials[S["M"]["Vocabulary"]], lang = language, half = Quotient[S["M"]["Context"], 2], n = whSegmentCount[samples],
     start, ctx = None, tokens = {}, seg, out, full, textQ},
    textQ = # < sp["EndOfText"] || # > sp["NoTimestamps"] &;
    Do[
        report[s, n];
        seg = whSegmentSamples[samples, s];
        If[Max[Abs[seg]] > $whSilence,
            whEncode[S, whT[seg]];
            If[ctx === None,
                If[lang === Automatic, lang = If[sp["Multilingual"], whDetectLanguage[S, sp], "en"]];
                start = whStartTokens[sp, lang];
                ctx = start];
            out = whGreedy[S, ctx, sp];
            If[! whNoSpeechQ[out],
                full = out["Context"];
                tokens = Join[tokens, TakeWhile[Drop[full, First @ FirstPosition[full, sp["NoTimestamps"]]], # =!= sp["EndOfText"] &]];
                ctx = With[{prev = Select[full, textQ]},
                    Join[{sp["Previous"]}, Take[prev, -Min[half - Length[start], Length[prev]]], start]]];
            TReclaim[]],
        {s, n}];
    <|"Tokens" -> Select[tokens, # < sp["EndOfText"] &], "Language" -> lang|>
]

(* === text ============================================================ *)

(* The token strings of a HuggingFace vocab.json (token -> id) by id. *)
whVocabulary[file_String] := Module[{ids = Import[file, "RawJSON"], out},
    out = ConstantArray["", Max[ids] + 1];
    out[[Values[ids] + 1]] = Keys[ids];
    out
]

(* The text of text tokens.  The vocabulary is byte-level as GPT-2's: a token is a string of characters
   that stand for bytes (dsvByteMaps has the table), and the bytes of the tokens together are UTF-8. *)
whText[vocabulary_List, ids_List] := StringTrim @ Quiet @ FromCharacterCode[
    Lookup[Last[dsvByteMaps[]], Characters[StringJoin[vocabulary[[ids + 1]]]]], "UTF-8"]

End[];

EndPackage[];
