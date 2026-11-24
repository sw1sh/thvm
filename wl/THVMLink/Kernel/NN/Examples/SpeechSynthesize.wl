(* SpeechSynthesize.wl - TSpeechSynthesize and TSpeak, the text-to-speech entry points for the thvm example
   models.  They dispatch on the Method option to a per-model synthesizer: "Phonon" runs Gradium's Phonon
   31M (PhononSynthesize).  Shaped after the built-in SpeechSynthesize and Speak and laid out like
   TImageSynthesize.  TSpeak needs nothing of the kernel's audio support: it writes the samples to a WAV
   file itself and hands the file to the system's audio player.  Part of the
   WolframInstitute`THVMLink`Examples` package. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

TSpeechSynthesize::usage = "TSpeechSynthesize[text$] synthesizes speech for the string text$ with a thvm text-to-speech model chosen by the Method option and gives an Audio object. TSpeechSynthesize[text$, \"Samples\"] gives the list of samples. Method -> \"Phonon\" (the default) is the English Phonon 31M model; the other options are the chosen synthesizer's (see PhononSynthesize).";
TSpeak::usage = "TSpeak[text$] speaks the string text$: it synthesizes the speech as TSpeechSynthesize does and plays it through the system's audio player, returning when the speech has ended. \"Volume\" -> v$ scales the loudness (1 by default); the other options are those of TSpeechSynthesize.";

TVoiceStyleData::usage = "TVoiceStyleData[] gives the voices of a thvm text-to-speech model, each an association of its name, language, gender and description. TVoiceStyleData[voice$] gives the association of the voice named voice$, and TVoiceStyleData[voice$, prop$] its property prop$. A voice's name is what the \"Voice\" option of TSpeechSynthesize and TSpeak takes. Method -> \"Phonon\" (the default) gives the English voices of Gradium's catalogue for Phonon.";

TSpeechSynthesize::badmethod = "`1` is not a known Method; use one of `2`.";
TSpeak::player = "No audio player was found on this system (looked for `1`).";
TVoiceStyleData::voice = "`1` is not a voice; TVoiceStyleData[] gives the voices.";
TVoiceStyleData::prop = "`1` is not a property of a voice; use one of `2`.";

Begin["`Private`"];

(* Method -> the synthesizer symbol and the sample rate of what it gives.  The symbols are
   context-qualified so the registry binds the public symbol whatever order Examples.wl loads the model
   files in. *)
$speechSynthesizers = <|
    "Phonon" -> <|"Synthesize" -> WolframInstitute`THVMLink`Examples`PhononSynthesize, "SampleRate" -> 24000,
        "Voices" -> phVoiceStyles|>
|>;

(* The registry entry of the Method among opts, or a Failure for an unknown one. *)
speechMethod[opts_List] := With[{method = FirstCase[opts, (Method -> m_) :> m, "Phonon"]},
    If[KeyExistsQ[$speechSynthesizers, method],
        $speechSynthesizers[method],
        WolframInstitute`THVMLink`Private`thvmFailure[TSpeechSynthesize::badmethod, method, Keys[$speechSynthesizers]]]]

Options[TSpeechSynthesize] = {Method -> "Phonon"};

TSpeechSynthesize[text_String, spec : ("Audio" | "Samples") : "Audio", opts : OptionsPattern[]] :=
    With[{m = speechMethod[{opts}]},
        If[FailureQ[m], m,
            m["Synthesize"][text, spec, FilterRules[{opts}, Options[m["Synthesize"]]]]]]

(* The voices of the Method's synthesizer, shaped after the built-in VoiceStyleData: all of them, one by
   its name in any case, or one property of one. *)
Options[TVoiceStyleData] = {Method -> "Phonon"};

TVoiceStyleData[opts : OptionsPattern[]] := With[{m = speechMethod[{opts}]},
    If[FailureQ[m], m, m["Voices"][]]]

TVoiceStyleData[voice_String, opts : OptionsPattern[]] := With[{voices = TVoiceStyleData[opts]},
    If[FailureQ[voices], voices,
        SelectFirst[voices, ToLowerCase[#Name] === ToLowerCase[voice] &,
            WolframInstitute`THVMLink`Private`thvmFailure[TVoiceStyleData::voice, voice]]]]

TVoiceStyleData[voice_String, prop_String, opts : OptionsPattern[]] := With[{data = TVoiceStyleData[voice, opts]},
    Which[
        FailureQ[data], data,
        KeyExistsQ[data, prop], data[prop],
        True, WolframInstitute`THVMLink`Private`thvmFailure[TVoiceStyleData::prop, prop, Keys[data]]]]

(* Samples in [-1, 1] at `rate` Hz as a mono 16-bit WAV file. *)
speechWavWrite[file_String, samples_List, rate_Integer] := Module[
    {pcm = Round[32767 Clip[samples, {-1., 1.}]], strm = OpenWrite[file, BinaryFormat -> True], n},
    n = 2 Length[pcm];
    BinaryWrite[strm, ToCharacterCode["RIFF"], "Byte"];
    BinaryWrite[strm, 36 + n, "UnsignedInteger32", ByteOrdering -> -1];
    BinaryWrite[strm, ToCharacterCode["WAVEfmt "], "Byte"];
    BinaryWrite[strm, {16}, "UnsignedInteger32", ByteOrdering -> -1];
    BinaryWrite[strm, {1, 1}, "UnsignedInteger16", ByteOrdering -> -1];
    BinaryWrite[strm, {rate, 2 rate}, "UnsignedInteger32", ByteOrdering -> -1];
    BinaryWrite[strm, {2, 16}, "UnsignedInteger16", ByteOrdering -> -1];
    BinaryWrite[strm, ToCharacterCode["data"], "Byte"];
    BinaryWrite[strm, n, "UnsignedInteger32", ByteOrdering -> -1];
    BinaryWrite[strm, pcm, "Integer16", ByteOrdering -> -1];
    Close[strm]
]

(* The command line of the system's player for a WAV file: the first of these that is installed. *)
$speechPlayers = Switch[$OperatingSystem,
    "MacOSX", {{"afplay"}},
    "Unix", {{"paplay"}, {"aplay", "-q"}, {"ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet"}},
    _, {}];

speechPlayer[] := speechPlayer[] = SelectFirst[$speechPlayers,
    Quiet[RunProcess[{"which", First[#]}, "ExitCode"]] === 0 &, None]

Options[TSpeak] = {Method -> "Phonon", "Volume" -> 1};

TSpeak[text_String, opts : OptionsPattern[]] := Module[{m = speechMethod[{opts}], player = speechPlayer[], samples, file},
    If[FailureQ[m], Return[m]];
    If[player === None, Return[WolframInstitute`THVMLink`Private`thvmFailure[TSpeak::player, $speechPlayers[[All, 1]]]]];
    samples = m["Synthesize"][text, "Samples", FilterRules[{opts}, Options[m["Synthesize"]]]];
    If[! ListQ[samples], Return[samples]];
    file = FileNameJoin[{$TemporaryDirectory, "thvm-speak-" <> ToString[$ProcessID] <> "-" <> ToString[RandomInteger[10^9]] <> ".wav"}];
    speechWavWrite[file, FirstCase[{opts}, ("Volume" -> v_) :> v, 1] samples, m["SampleRate"]];
    RunProcess[Append[player, file]];
    DeleteFile[file];
]

End[];

EndPackage[];
