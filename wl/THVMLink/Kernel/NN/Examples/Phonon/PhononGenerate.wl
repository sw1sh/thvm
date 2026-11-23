(* PhononGenerate.wl -- PhononSynthesize: English text to speech with Gradium's Phonon 31M
   (gradium/phonon-31m-en, Apache 2.0) on thvm.

   The text is normalised (PhononText.wl), split into chunks of sentences, and each chunk is spoken on
   its own: its prompt (the voice frames, the chunk's tokens, a start latent) goes through the flow
   transformer, which then generates audio latents frame by frame until its end-of-speech logit fires, and
   the Mimi decoder turns the latents into 24 kHz samples (PhononModel.wl).  The model, its tokenizer and
   the captured steps stay loaded between calls.

   The weights are not shipped with the paclet.  The first call fetches the model's three files from its
   HuggingFace repository, at the revision the port was checked against, under a progress display, and
   keeps a download only when its size and SHA-256 are the pinned ones. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

PhononSynthesize::usage = "PhononSynthesize[text$] synthesizes speech for the English string text$ with the Phonon 31M text-to-speech model and gives an Audio object (24 kHz, mono). PhononSynthesize[text$, \"Samples\"] gives the list of samples instead. The first call fetches the model's files (65 MB) from its HuggingFace repository, gradium/phonon-31m-en, to the model directory, showing its progress and checking each file against its SHA-256; the model then stays loaded between calls. Options: \"Voice\" (Automatic for the model's default voice, the path of a .safetensors voice file, or a {frames$, 512} array of voice frames), \"Temperature\" (the noise variance of the latents, 0.3; 0 gives the same audio every time), \"Normalize\" (True writes money, addresses and other symbols as words first), \"ModelDirectory\" (Automatic is ~/.cache/thvm/phonon-31m-en; a directory that already holds the files is used as it is), ProgressReporting (False hides the progress of the fetch and the load).";
PhononUnload::usage = "PhononUnload[] unloads the Phonon model and frees its buffers by resetting the thvm engine, so terms made before it are no longer valid; the next PhononSynthesize loads the model again.";

PhononSynthesize::fetch = "Could not fetch `1` from `2`: `3`.";
PhononSynthesize::voice = "`1` is not a voice: give Automatic, a .safetensors voice file or a {frames, 512} array.";
PhononSynthesize::empty = "There is no text to speak.";

Begin["`Private`"];

(* === the model's files ================================================= *)

(* Where the files come from and what each must be.  The revision is the one the port was checked
   against.  A file in the model directory counts when it has its size; a download is kept when it also
   has its SHA-256. *)
$phRepository = "gradium/phonon-31m-en";
$phRevision = "8f7a37c94a4c58b04c8287300c53f11d2e208eae";
$phFiles = <|
    "model.q8.gguf" -> <|"Bytes" -> 64512352, "SHA256" -> "a7d9aad2cd6dc66473f24a08bc6d2939030ef4d751ef82349361fbef3272dbae"|>,
    "tokenizer.json" -> <|"Bytes" -> 277967, "SHA256" -> "691a00068fa7b989f2f3e5ae5daa2d1a5bb319f2d5360deafcfb19a8bb6e7936"|>,
    "default-voice.safetensors" -> <|"Bytes" -> 256136, "SHA256" -> "dc537478eaadc1a5257fcda2e3501ab81e14c111a5d95a5838f350db2afd0f1a"|>
|>;

phUrl[name_String] := "https://huggingface.co/" <> $phRepository <> "/resolve/" <> $phRevision <> "/" <> name

phModelDirectory[Automatic] := FileNameJoin[{$HomeDirectory, ".cache", "thvm", "phonon-31m-en"}]
phModelDirectory[dir_String] := ExpandFileName[dir]

phHaveQ[dir_String, name_String] := With[{file = FileNameJoin[{dir, name}]},
    FileExistsQ[file] && FileByteCount[file] == $phFiles[name]["Bytes"]]

(* phReport[text, fraction] sets what the progress display shows; nothing outside phWithProgress. *)
phReport[_, _] := Null

(* Evaluates body under a progress display that follows phReport, as fxWithProgress does for FLUX. *)
SetAttributes[phWithProgress, HoldRest];
phWithProgress[reportQ_, body_] := If[TrueQ[reportQ],
    Module[{text = "Loading Phonon 31M", progress = Indeterminate},
        Block[{phReport = Function[{t, p}, text = t; progress = p]},
            Progress`EvaluateWithProgress[body, <|"Text" :> text, "Progress" :> progress|>]]],
    body
]

phCurlQ[] := phCurlQ[] = Quiet[RunProcess[{"curl", "--version"}, "ExitCode"]] === 0

(* Downloads url to file, calling report[bytes so far] while it runs: by curl in the background where the
   system has it, the size of its growing file read five times a second, since not every kernel can
   download asynchronously; else by URLDownload in one step. *)
phDownload[url_String, file_String, report_] := (
    If[FileExistsQ[file], DeleteFile[file]];
    If[phCurlQ[],
        With[{proc = StartProcess[{"curl", "-fsSL", "--retry", "2", "-o", file, url}]},
            While[ProcessStatus[proc] === "Running",
                report[If[FileExistsQ[file], FileByteCount[file], 0]];
                Pause[0.2]]],
        Quiet[URLDownload[url, file]]];
)

(* A byte count in megabytes with one decimal, "64.5". *)
phMegabytes[bytes_] := With[{tenths = Round[bytes / 100000]},
    IntegerString[Quotient[tenths, 10]] <> "." <> IntegerString[Mod[tenths, 10]]]

(* True when dir holds the model's files, the missing ones fetched and checked first; a Failure that says
   which file could not be had and why. *)
phFetch[dir_String] := Module[{missing = Select[Keys[$phFiles], ! phHaveQ[dir, #] &], total, done = 0},
    If[missing === {}, Return[True]];
    If[! DirectoryQ[dir], CreateDirectory[dir, CreateIntermediateDirectories -> True]];
    total = Total[$phFiles[#]["Bytes"] & /@ missing];
    Catch[
        Do[
            With[{file = FileNameJoin[{dir, name}], part = FileNameJoin[{dir, name <> ".part"}], want = $phFiles[name]},
                phDownload[phUrl[name], part, Function[bytes, phReport[
                    "Fetching " <> name <> " from huggingface.co/" <> $phRepository <> " (" <> phMegabytes[done + bytes] <>
                        " of " <> phMegabytes[total] <> " MB)",
                    (done + bytes) / total]]];
                Which[
                    ! FileExistsQ[part],
                        Throw[WolframInstitute`THVMLink`Private`thvmFailure[PhononSynthesize::fetch, name, phUrl[name], "the download failed"]],
                    FileByteCount[part] != want["Bytes"] || FileHash[part, "SHA256", All, "HexString"] =!= want["SHA256"],
                        DeleteFile[part];
                        Throw[WolframInstitute`THVMLink`Private`thvmFailure[PhononSynthesize::fetch, name, phUrl[name],
                            "what arrived is not the file expected, of " <> ToString[want["Bytes"]] <> " bytes and SHA-256 " <> want["SHA256"]]],
                    True,
                        If[FileExistsQ[file], DeleteFile[file]];
                        RenameFile[part, file];
                        done += want["Bytes"]]],
            {name, missing}];
        True]
]

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

(* The loaded model of dir.  When it is not loaded yet, its files are fetched and its weights decoded
   under the progress display; a Failure when a file cannot be had. *)
phLoad[dir_String, reportQ_] := If[AssociationQ[$phLoaded] && $phLoaded["Directory"] === dir,
    $phLoaded,
    phWithProgress[reportQ,
        With[{fetched = phFetch[dir]},
            If[fetched =!= True,
                fetched,
                phReport["Loading the Phonon 31M weights", Indeterminate];
                PhononUnload[];
                $phLoaded = <|"Directory" -> dir, "Session" -> phSession[phLoadGguf[FileNameJoin[{dir, "model.q8.gguf"}]]],
                    "Tokenizer" -> phTokenizer[FileNameJoin[{dir, "tokenizer.json"}]], "Voices" -> <||>|>]]]
]

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
        True, Return[WolframInstitute`THVMLink`Private`thvmFailure[PhononSynthesize::voice, Short[voice]]]];
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

Options[PhononSynthesize] = {"Voice" -> Automatic, "Temperature" -> 0.3, "Normalize" -> True, "ModelDirectory" -> Automatic,
    ProgressReporting -> Automatic};

PhononSynthesize[text_String, opts : OptionsPattern[]] := PhononSynthesize[text, "Audio", opts]

PhononSynthesize[text_String, spec : "Audio" | "Samples", opts : OptionsPattern[]] := Module[{loaded, rows, chunks, tok},
    loaded = phLoad[phModelDirectory[OptionValue["ModelDirectory"]], OptionValue[ProgressReporting] =!= False];
    If[FailureQ[loaded], Return[loaded]];
    rows = phVoiceRows[OptionValue["Voice"]];
    If[FailureQ[rows], Return[rows]];
    tok = $phLoaded["Tokenizer"];
    chunks = Join @@ (phFit[tok, #] & /@ phChunks[tok, If[TrueQ[OptionValue["Normalize"]], phNormalize[text], text]]);
    If[chunks === {}, Return[WolframInstitute`THVMLink`Private`thvmFailure[PhononSynthesize::empty]]];
    With[{samples = Join @@ (phSpeak[rows, #, OptionValue["Temperature"]] & /@ chunks)},
        phReclaim[64 * 2^20];
        If[spec === "Samples", samples, Audio[samples, SampleRate -> 24000]]]
]

End[];

EndPackage[];
