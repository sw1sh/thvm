(* PhononGenerate.wl -- PhononSynthesize: English text to speech with Gradium's Phonon 31M
   (gradium/phonon-31m-en, Apache 2.0) on thvm.

   The text is normalised (PhononText.wl), split into chunks of sentences, and each chunk is spoken on
   its own: its prompt (the voice frames, the chunk's tokens, a start latent) goes through the flow
   transformer, which then generates audio latents frame by frame until its end-of-speech logit fires, and
   the Mimi decoder turns the latents into 24 kHz samples (PhononModel.wl).  The model, its tokenizer and
   the captured steps stay loaded between calls. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

PhononSynthesize::usage = "PhononSynthesize[text$] synthesizes speech for the English string text$ with the Phonon 31M text-to-speech model and gives an Audio object (24 kHz, mono). PhononSynthesize[text$, \"Samples\"] gives the list of samples instead. The model files are fetched to the model directory on first use and stay loaded between calls. Options: \"Voice\" (Automatic for the model's default voice, the path of a .safetensors voice file, or a {frames$, 512} array of voice frames), \"Temperature\" (the noise variance of the latents, 0.3; 0 gives the same audio every time), \"Normalize\" (True writes money, addresses and other symbols as words first), \"ModelDirectory\" (Automatic is ~/.cache/thvm/phonon-31m-en).";
PhononUnload::usage = "PhononUnload[] unloads the Phonon model and frees its buffers by resetting the thvm engine, so terms made before it are no longer valid; the next PhononSynthesize loads the model again.";

PhononSynthesize::fetch = "Could not fetch `1` to `2`.";
PhononSynthesize::voice = "`1` is not a voice: give Automatic, a .safetensors voice file or a {frames, 512} array.";
PhononSynthesize::empty = "There is no text to speak.";

Begin["`Private`"];

(* The revision the port was checked against. *)
$phRepository = "https://huggingface.co/gradium/phonon-31m-en/resolve/8f7a37c94a4c58b04c8287300c53f11d2e208eae/";
$phFiles = {"model.q8.gguf", "tokenizer.json", "default-voice.safetensors"};

phModelDirectory[Automatic] := FileNameJoin[{$HomeDirectory, ".cache", "thvm", "phonon-31m-en"}]
phModelDirectory[dir_String] := ExpandFileName[dir]

(* True when dir holds the model's files, fetching the missing ones from its HuggingFace repository. *)
phFetch[dir_String] := (
    If[! DirectoryQ[dir], CreateDirectory[dir, CreateIntermediateDirectories -> True]];
    AllTrue[$phFiles, Function[name, With[{file = FileNameJoin[{dir, name}]},
        FileExistsQ[file] || (
            Quiet @ URLDownload[$phRepository <> name, file <> ".part"];
            If[FileExistsQ[file <> ".part"] && FileByteCount[file <> ".part"] > 0,
                RenameFile[file <> ".part", file]; True,
                Message[PhononSynthesize::fetch, $phRepository <> name, file]; False])]]]
)

(* The frames {F, 512} of a voice file: its one tensor, {1, 512, F} of f32 (or {F, 512}). *)
phVoiceFile[path_String] := Module[{strm = OpenRead[path, BinaryFormat -> True], n, entry, data, shape},
    n = BinaryRead[strm, "UnsignedInteger64", ByteOrdering -> -1];
    entry = First @ KeyDrop[ImportByteArray[ByteArray[BinaryReadList[strm, "Byte", n]], "RawJSON"], "__metadata__"];
    shape = entry["shape"];
    SetStreamPosition[strm, 8 + n + First[entry["data_offsets"]]];
    data = BinaryReadList[strm, "Real32", Times @@ shape, ByteOrdering -> -1];
    Close[strm];
    If[shape[[-2]] == 512, Transpose[ArrayReshape[data, Take[shape, -2]]], ArrayReshape[data, {Automatic, 512}]]
]

(* === the loaded model ================================================== *)

$phLoaded = None;

phLoad[dir_String] := Which[
    AssociationQ[$phLoaded] && $phLoaded["Directory"] === dir, $phLoaded,
    ! phFetch[dir], $Failed,
    True, PhononUnload[];
        $phLoaded = <|"Directory" -> dir, "Session" -> phSession[phLoadGguf[FileNameJoin[{dir, "model.q8.gguf"}]]],
            "Tokenizer" -> phTokenizer[FileNameJoin[{dir, "tokenizer.json"}]], "Voices" -> <||>|>]

(* Unloading resets the engine, as FluxUnload does: the kernel table keeps every tensor a session's
   kernels read, so nothing short of a reset frees them.  Terms made before it are no longer valid. *)
PhononUnload[] := (
    If[AssociationQ[$phLoaded],
        phSessionClose[$phLoaded["Session"]];
        $phLoaded = None;
        $phInputs = <||>;
        Quiet[TReset[]];
        $phReclaimed = 0];
)

(* Each chunk leaves its prompt upload behind: reclaim once the buffers have grown by `slack` bytes since
   the last time. *)
$phReclaimed = 0;
phReclaim[slack_] := If[TTotalBufBytes[] > $phReclaimed + slack, TReclaim[]; $phReclaimed = TTotalBufBytes[]]

(* The prompt rows of a voice: its frames through the speaker projection, kept per voice.  The frames
   leave room in the prompt for a chunk's tokens and the start latent. *)
phVoiceRows[voice_] := Module[{frames, key = voice},
    If[KeyExistsQ[$phLoaded["Voices"], key], Return[$phLoaded["Voices"][key]]];
    frames = Which[
        voice === Automatic, phVoiceFile[FileNameJoin[{$phLoaded["Directory"], "default-voice.safetensors"}]],
        StringQ[voice] && FileExistsQ[voice], phVoiceFile[voice],
        MatrixQ[voice, NumericQ] && Last[Dimensions[voice]] == 512, N[voice],
        True, Message[PhononSynthesize::voice, Short[voice]]; Return[$Failed]];
    frames = Take[frames, UpTo[$phPromptRows - $phChunkTokens - 1]];
    $phLoaded["Voices"][key] = frames . Transpose[$phLoaded["Session"]["Host"]["Speaker"]]
]

(* === synthesis ========================================================= *)

(* The most tokens a chunk may have: with them the prompt fits $phPromptRows and the frames they need
   (phFrames) fit the context after it. *)
$phChunkTokens = 60;

(* Frames a chunk of n tokens may take: about three tokens a second and two seconds to spare, at 12.5
   frames a second (pocket_tts _estimate_max_gen_len). *)
phFrames[n_Integer] := Ceiling[(n / 3. + 2) 12.5]

(* A chunk no longer than $phChunkTokens tokens, or its halves by words. *)
phFit[tok_, chunk_String] := With[{words = StringSplit[chunk]},
    If[Length[phEncode[tok, First @ phPrepare[chunk]]] <= $phChunkTokens || Length[words] < 2,
        {chunk},
        Join @@ (phFit[tok, StringRiffle[#, " "]] & /@ TakeDrop[words, Ceiling[Length[words] / 2]])]]

(* The samples of one chunk. *)
phSpeak[rows_, chunk_String, temperature_] := Module[
    {S = $phLoaded["Session"], prep = phPrepare[chunk], ids},
    ids = Take[phEncode[$phLoaded["Tokenizer"], First[prep]], UpTo[$phPromptRows - Length[rows] - 1]];
    phMimiDecode[S, phLatents[S, phPrompt[S, ids, rows], temperature, phFrames[Length[ids]], Last[prep]]]
]

Options[PhononSynthesize] = {"Voice" -> Automatic, "Temperature" -> 0.3, "Normalize" -> True, "ModelDirectory" -> Automatic};

PhononSynthesize[text_String, opts : OptionsPattern[]] := PhononSynthesize[text, "Audio", opts]

PhononSynthesize[text_String, spec : "Audio" | "Samples", opts : OptionsPattern[]] := Module[{rows, chunks, tok},
    If[phLoad[phModelDirectory[OptionValue["ModelDirectory"]]] === $Failed, Return[$Failed]];
    rows = phVoiceRows[OptionValue["Voice"]];
    If[rows === $Failed, Return[$Failed]];
    tok = $phLoaded["Tokenizer"];
    chunks = Join @@ (phFit[tok, #] & /@ phChunks[tok, If[TrueQ[OptionValue["Normalize"]], phNormalize[text], text]]);
    If[chunks === {}, Message[PhononSynthesize::empty]; Return[$Failed]];
    With[{samples = Join @@ (phSpeak[rows, #, OptionValue["Temperature"]] & /@ chunks)},
        phReclaim[64 * 2^20];
        If[spec === "Samples", samples, Audio[samples, SampleRate -> 24000]]]
]

End[];

EndPackage[];
