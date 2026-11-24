(* whisper_check.wls <fixtures dir>: the thvm Whisper port against the transformers fixtures
   (tools/whisper/whisper_fixtures.py writes them): max |thvm - reference| for the resampled samples, the
   log-mel spectrogram, the encoder's states and the first logits, whether the greedy tokens and the text
   are the same, and timings.

       python tools/whisper/whisper_fixtures.py speech.wav ~/.cache/thvm/whisper-fixtures
       wl -f tools/whisper/whisper_check.wls ~/.cache/thvm/whisper-fixtures

   The model is the one the fixtures were written for; its files are read from WHISPER_DIR (default
   ~/.cache/thvm/whisper- and the model's name, where WhisperRecognize fetches them).  THVMLink loads
   from this checkout by path: an installed release would win a context lookup. *)
With[{root = FileNameJoin[{DirectoryName[$InputFileName], "..", ".."}]},
    Get[FileNameJoin[{root, "wl", "THVMLink", "Kernel", "THVMLink.wl"}]];
    Get[FileNameJoin[{root, "wl", "THVMLink", "Kernel", "NN", "Examples", "Examples.wl"}]]];
Begin["WolframInstitute`THVMLink`Examples`Private`"];
fx = ExpandFileName[$ScriptCommandLine[[2]]];
meta = Import[FileNameJoin[{fx, "meta.json"}], "RawJSON"];
name = meta["model"];
dir = Replace[Environment["WHISPER_DIR"], $Failed -> whModelDirectory[name, Automatic]];
rd[key_] := ArrayReshape[BinaryReadList[FileNameJoin[{fx, key <> ".f32"}], "Real32", ByteOrdering -> -1], meta["shapes"][key]];
diff[a_, b_] := Max[Abs[Flatten[N[a]] - Flatten[N[b]]]];
say[s__] := WriteString["stdout", StringJoin[If[StringQ[#], #, ToString[#, InputForm]] & /@ {s}], "\n"];
Module[{t, M, S, wave = rd["wave16k"], seg, enc, logits, out, start = meta["start"]},
    say["model ", name, " from ", dir];
    With[{mine = speechResample[rd["wave_source"], meta["source_rate"], 16000]},
        say["resampled from ", meta["source_rate"], " Hz: ", Length[mine], " samples (reference ", Length[wave], "), max diff ",
            If[Length[mine] == Length[wave], diff[mine, wave], "length differs"], " of a peak ", Max[Abs[wave]]]];
    {t, M} = AbsoluteTiming[whLoad[FileNameJoin[{dir, "model.safetensors"}]]];
    say["load ", t, " s: width ", M["Width"], ", heads ", M["Heads"], ", layers ", Length[M["EncoderLayers"]], " + ", Length[M["DecoderLayers"]],
        ", vocabulary ", M["Vocabulary"]];
    S = whSession[M];
    seg = whSegmentSamples[wave, 1];
    say["log-mel: max diff ", diff[whHost[whLogMel[M, whFrames[whT[seg]]]], rd["mel"]]];
    Do[
        {t, enc} = AbsoluteTiming[whHost[whEncode[S, whT[seg]]]];
        say["encoder, run ", r, ": ", t, " s, max diff ", diff[enc, rd["encoded"]], " (largest state ", Max[Abs[rd["encoded"]]], ")"],
        {r, 2}];
    logits = Last @ Table[whStep[S, start[[i]], i - 1], {i, Length[start]}];
    say["logits after the start tokens: max diff ", diff[logits, rd["logits0"]]];
    Do[
        {t, out} = AbsoluteTiming[whTranscribe[S, wave, "en"]];
        say["transcribe, run ", r, ": ", t, " s, ", Length[out["Tokens"]], " tokens, the reference's: ", out["Tokens"] === meta["tokens"]],
        {r, 2}];
    say["text: ", whText[whVocabulary[FileNameJoin[{dir, "vocab.json"}]], out["Tokens"]]];
    say["reference: ", meta["text"]];
    say["captured kernels: stem ", TJitOpCount[S["Stem"]], ", layer ", TJitOpCount[S["Layer"]], ", cross step ", TJitOpCount[S["CrossStep"]],
        ", decode ", TJitOpCount[S["Decode"]], "; buffer MB ", Round[TTotalBufBytes[] / 2.^20, 0.1]]];
End[];
