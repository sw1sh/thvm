(* SpeechRecognize.wl - TSpeechRecognize, the speech-to-text entry point for the thvm example models, and
   the reading of audio that its recognizers share.  It dispatches on the Method option to a per-model
   recognizer: "Whisper" runs OpenAI's Whisper (WhisperRecognize).  Shaped after the built-in
   SpeechRecognize and laid out like TSpeechSynthesize.

   The audio is read without the kernel's audio support: a WAV file is parsed here, another format goes
   through the kernel's Audio where that works and through the system's converter where it does not, and
   resampling is a windowed-sinc interpolation over plain lists.  Part of the
   WolframInstitute`THVMLink`Examples` package. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

TSpeechRecognize::usage = "TSpeechRecognize[audio$] recognizes the speech in audio$ with a thvm speech-to-text model chosen by the Method option and gives it as a string. audio$ is an Audio object, the path of an audio file or File[path$], or a list of samples. Method -> \"Whisper\" (the default) is OpenAI's Whisper; the other options are the chosen recognizer's (see WhisperRecognize).";

TSpeechRecognize::badmethod = "`1` is not a known Method; use one of `2`.";

Begin["`Private`"];

(* === reading audio ===================================================== *)

(* What a reader gives when it cannot read: speechBad[reason].  The recognizer turns it into its Failure. *)

(* The samples of a WAV file, its channels averaged, in [-1, 1], and its sample rate: {samples, rate}.
   Reads PCM of 8, 16, 24 and 32 bits and floats of 32 and 64. *)
speechWavRead[file_String] := Module[{strm = OpenRead[file, BinaryFormat -> True], result},
    result = speechWavParse[strm, FileByteCount[file]];
    Close[strm];
    result
]

speechWavParse[strm_, bytes_Integer] := Module[
    {u16, u32, pos = 12, id, size, format = None, data = None, tag, channels, rate, bits, count, x},
    u16[n_] := BinaryReadList[strm, "UnsignedInteger16", n, ByteOrdering -> -1];
    u32[n_] := BinaryReadList[strm, "UnsignedInteger32", n, ByteOrdering -> -1];
    If[bytes < 12 || FromCharacterCode[BinaryReadList[strm, "Byte", 4]] =!= "RIFF", Return[speechBad["it is not a RIFF file"]]];
    SetStreamPosition[strm, 8];
    If[FromCharacterCode[BinaryReadList[strm, "Byte", 4]] =!= "WAVE", Return[speechBad["it is not a WAVE file"]]];
    While[pos + 8 <= bytes && (format === None || data === None),
        SetStreamPosition[strm, pos];
        id = FromCharacterCode[BinaryReadList[strm, "Byte", 4]];
        size = First @ u32[1];
        Switch[id,
            "fmt ",
                {tag, channels} = u16[2];
                rate = First @ u32[2];
                bits = Last @ u16[2];
                (* WAVE_FORMAT_EXTENSIBLE: the format is the first word of the sub-format, 8 bytes on. *)
                If[tag == 65534 && size >= 26, u16[4]; tag = First @ u16[1]];
                format = {tag, channels, rate, bits},
            "data",
                (* A writer that streamed may have left the size unset: the data then runs to the end. *)
                data = {pos + 8, If[size == 0 || size > bytes - pos - 8, bytes - pos - 8, size]}];
        pos += 8 + size + Mod[size, 2]];
    If[format === None || data === None, Return[speechBad["it has no format or no data"]]];
    {tag, channels, rate, bits} = format;
    If[channels < 1 || rate < 1, Return[speechBad["its format is not valid"]]];
    SetStreamPosition[strm, First[data]];
    count = Quotient[Last[data], Quotient[bits, 8]];
    x = Switch[{tag, bits},
        {1, 8}, (BinaryReadList[strm, "UnsignedInteger8", count] - 128) / 128.,
        {1, 16}, BinaryReadList[strm, "Integer16", count, ByteOrdering -> -1] / 32768.,
        {1, 24}, (Mod[Partition[BinaryReadList[strm, "Byte", 3 count], 3] . {1, 256, 65536} + 8388608, 16777216] - 8388608) / 8388608.,
        {1, 32}, BinaryReadList[strm, "Integer32", count, ByteOrdering -> -1] / 2147483648.,
        {3, 32}, BinaryReadList[strm, "Real32", count, ByteOrdering -> -1],
        {3, 64}, BinaryReadList[strm, "Real64", count, ByteOrdering -> -1],
        _, None];
    If[x === None, Return[speechBad["its encoding (format " <> ToString[tag] <> ", " <> ToString[bits] <> " bits) is not PCM or float"]]];
    {If[channels > 1, Total[Partition[x, channels], {2}] / channels, x], rate}
]

(* Command lines that write the audio file `in` as the 16 kHz mono 16-bit WAV file `out`: the first whose
   program is installed is used for a file the kernel cannot read. *)
$speechConverters = {
    {"afconvert", Function[{in, out}, {"afconvert", "-f", "WAVE", "-d", "LEI16@16000", "-c", "1", in, out}]},
    {"ffmpeg", Function[{in, out}, {"ffmpeg", "-loglevel", "error", "-y", "-i", in, "-ac", "1", "-ar", "16000", out}]}
};

speechConverter[] := speechConverter[] = SelectFirst[$speechConverters,
    Quiet[RunProcess[{"which", First[#]}, "ExitCode"]] === 0 &, None]

(* An audio file: a WAV file read here, another through the kernel's Audio where it has one, and else
   through the system's converter. *)
speechFile[path_String] := Module[{read, audio, converter, wav},
    If[! FileExistsQ[path], Return[speechBad["there is no file " <> path]]];
    If[ToLowerCase[FileExtension[path]] === "wav",
        read = speechWavRead[path];
        If[ListQ[read], Return[read]]];
    audio = Quiet[Audio[path]];
    If[TrueQ[Quiet[AudioQ[audio]]], Return[speechAudio[audio]]];
    converter = speechConverter[];
    If[converter === None,
        Return[speechBad[path <> " is not a WAV file that can be read here, and there is no converter (" <>
            StringRiffle[$speechConverters[[All, 1]], ", "] <> ") on this system"]]];
    wav = FileNameJoin[{$TemporaryDirectory, "thvm-listen-" <> ToString[$ProcessID] <> "-" <> ToString[RandomInteger[10^9]] <> ".wav"}];
    RunProcess[Last[converter][path, wav]];
    read = If[FileExistsQ[wav], speechWavRead[wav], speechBad[First[converter] <> " could not convert " <> path]];
    If[FileExistsQ[wav], DeleteFile[wav]];
    read
]

speechRate[opts___] := FirstCase[Flatten[{opts}], (SampleRate -> r_) :> Round[If[NumericQ[r], r, QuantityMagnitude[r]]], 44100]

(* An Audio object: through the kernel's audio functions where it has them, and else, since Audio is then
   an inert head, from the samples or the file it was made of. *)
speechAudio[audio_] := If[TrueQ[Quiet[AudioQ[audio]]],
    {Mean[AudioData[audio]], Round @ QuantityMagnitude[AudioSampleRate[audio]]},
    Replace[audio, {
        Audio[data_ ? (VectorQ[#, NumericQ] &), opts___] :> {N @ data, speechRate[opts]},
        Audio[data_ ? (MatrixQ[#, NumericQ] &), opts___] :> {N @ Mean[data], speechRate[opts]},
        Audio[path_String | File[path_String], ___] :> speechFile[ExpandFileName[path]],
        _ :> speechBad["the samples of the Audio object cannot be read on this kernel"]}]
]

(* What the audio argument holds, as {samples, rate}; a list of samples is at `rate`. *)
speechRead[samples_List, rate_] := If[VectorQ[samples, NumericQ] && IntegerQ[rate] && rate > 0,
    {N @ samples, rate},
    speechBad["a list of samples must be a vector of numbers, at a positive integer sample rate"]]
speechRead[File[path_String], rate_] := speechRead[path, rate]
speechRead[path_String, _] := speechFile[ExpandFileName[path]]
speechRead[audio_Audio, _] := speechAudio[audio]
speechRead[_, _] := speechBad["an Audio object, the path of an audio file or a list of samples is expected"]

(* === resampling ======================================================== *)

(* Zero crossings of the interpolation kernel on each side. *)
$speechZeros = 16;

(* The taps that read the point `frac` of a sample after an input sample from the 2 half input samples
   around it: a sinc cut at c of the input's Nyquist frequency under a Blackman window, scaled to sum
   to 1. *)
speechKernel[frac_, c_, half_Integer] := With[{d = N[frac] - Range[-half + 1, half]},
    With[{u = Pi c d + 1.*^-12, v = Pi d / half},
        With[{h = Sin[u] / u (0.42 + 0.5 Cos[v] + 0.08 Cos[2 v])}, h / Total[h]]]]

(* Samples at `from` Hz as samples at `to` Hz, by band-limited interpolation.  With to / from = p / q in
   lowest terms, output r + i p sits at input sample Quotient[r q, p] + i q and Mod[r q, p] / p of a
   sample on: the outputs of one r share their taps, so each tap is one strided product. *)
speechResample[x_List, from_Integer, to_Integer] := Module[{p, q, c, half, m, padded, out},
    If[from == to, Return[N @ x]];
    {p, q} = {to, from} / GCD[from, to];
    c = Min[1, p / q];
    half = Ceiling[$speechZeros / c];
    m = Ceiling[Length[x] p / q];
    padded = Join[ConstantArray[0., half], N @ x, ConstantArray[0., half]];
    out = ConstantArray[0., m];
    Do[
        With[{n0 = Quotient[r q, p], count = Quotient[m - r + p - 1, p], h = speechKernel[Mod[r q, p] / p, c, half]},
            out[[r + 1 ;; r + 1 + (count - 1) p ;; p]] =
                Fold[#1 + h[[#2]] padded[[n0 + 1 + #2 ;; n0 + 1 + #2 + (count - 1) q ;; q]] &, 0., Range[2 half]]],
        {r, 0, Min[p, m] - 1}];
    out
]

(* The samples of the audio argument at `to` Hz, or speechBad[reason]. *)
speechSamples[audio_, rate_, to_Integer] :=
    Replace[speechRead[audio, rate], {samples_List, from_} :> speechResample[samples, from, to]]

(* === the entry point =================================================== *)

(* Method -> the recognizer symbol, context-qualified so the registry binds the public symbol whatever
   order Examples.wl loads the model files in. *)
$speechRecognizers = <|
    "Whisper" -> <|"Recognize" -> WolframInstitute`THVMLink`Examples`WhisperRecognize|>
|>;

speechRecognizer[opts_List] := With[{method = FirstCase[opts, (Method -> m_) :> m, "Whisper"]},
    If[KeyExistsQ[$speechRecognizers, method],
        $speechRecognizers[method],
        WolframInstitute`THVMLink`Private`thvmFailure[TSpeechRecognize::badmethod, method, Keys[$speechRecognizers]]]]

Options[TSpeechRecognize] = {Method -> "Whisper"};

TSpeechRecognize[audio_, opts : OptionsPattern[]] := With[{m = speechRecognizer[{opts}]},
    If[FailureQ[m], m,
        m["Recognize"][audio, FilterRules[{opts}, Options[m["Recognize"]]]]]]

End[];

EndPackage[];
