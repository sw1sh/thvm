(* whisper.wlt - Whisper speech to text (NN/Examples/Whisper) and the audio reading it shares with
   TSpeechRecognize (NN/Examples/SpeechRecognize.wl).  The WAV reader, the resampler, the mel filters, the
   segments, the special tokens, the languages and the failures need no model.  With the tiny.en model's
   files in place (~/.cache/thvm/whisper-tiny.en, where WhisperRecognize[audio, "Model" -> "tiny.en"]
   fetches them): the text of token ids, and, when Phonon's files are in place too, the recognition of
   the sentence Phonon speaks, the tensors of which tools/whisper/whisper_check.wls compares with the
   transformers reference. *)

Get["WolframInstitute`THVMLink`Examples`"]

$whisperDir = FileNameJoin[{$HomeDirectory, ".cache", "thvm", "whisper-tiny.en"}];
$whisperFiles = AllTrue[{"model.safetensors", "vocab.json"}, FileExistsQ[FileNameJoin[{$whisperDir, #}]] &];
$whisperPhononFiles = AllTrue[{"model.q8.gguf", "tokenizer.json", "default-voice.safetensors"},
    FileExistsQ[FileNameJoin[{$HomeDirectory, ".cache", "thvm", "phonon-31m-en", #}]] &];

(* A WAV file of the given format tag, channels, rate and bits around the bytes of its data, as a
   temporary file: the test deletes it. *)
whisperWav[tag_, channels_, rate_, bits_, data_List] := Module[{file = FileNameJoin[{$TemporaryDirectory,
        "thvm-whisper-test-" <> ToString[$ProcessID] <> "-" <> ToString[RandomInteger[10^9]] <> ".wav"}], strm},
    strm = OpenWrite[file, BinaryFormat -> True];
    BinaryWrite[strm, ToCharacterCode["RIFF"], "Byte"];
    BinaryWrite[strm, 36 + Length[data], "UnsignedInteger32", ByteOrdering -> -1];
    BinaryWrite[strm, ToCharacterCode["WAVEfmt "], "Byte"];
    BinaryWrite[strm, 16, "UnsignedInteger32", ByteOrdering -> -1];
    BinaryWrite[strm, {tag, channels}, "UnsignedInteger16", ByteOrdering -> -1];
    BinaryWrite[strm, {rate, rate channels bits / 8}, "UnsignedInteger32", ByteOrdering -> -1];
    BinaryWrite[strm, {channels bits / 8, bits}, "UnsignedInteger16", ByteOrdering -> -1];
    BinaryWrite[strm, ToCharacterCode["data"], "Byte"];
    BinaryWrite[strm, Length[data], "UnsignedInteger32", ByteOrdering -> -1];
    BinaryWrite[strm, data, "Byte"];
    Close[strm];
    file
]

whisperRead[file_] := With[{r = WolframInstitute`THVMLink`Examples`Private`speechWavRead[file]}, DeleteFile[file]; r]

(* === audio ============================================================= *)

(* What TSpeak writes, the reader reads back: 16-bit samples, to half a step. *)
VerificationTest[
    With[{x = Sin[Range[0, 999] / 7.] / 2, file = FileNameJoin[{$TemporaryDirectory, "thvm-whisper-test-" <> ToString[$ProcessID] <> ".wav"}]},
        WolframInstitute`THVMLink`Examples`Private`speechWavWrite[file, x, 24000];
        With[{r = whisperRead[file]}, {Last[r], Length[First[r]], Max[Abs[First[r] - x]] < 1 / 32767.}]],
    {24000, 1000, True},
    TestID -> "whisper/wav-reads-back-what-speak-writes"
]

VerificationTest[
    {whisperRead[whisperWav[1, 1, 8000, 8, {128, 255, 0, 192}]],
     whisperRead[whisperWav[1, 2, 44100, 16, {0, 64, 0, 32, 0, 192, 0, 32}]],
     whisperRead[whisperWav[1, 1, 48000, 24, {0, 0, 64, 0, 0, 192, 255, 255, 127}]],
     whisperRead[whisperWav[1, 1, 16000, 32, {0, 0, 0, 64, 0, 0, 0, 128}]],
     whisperRead[whisperWav[3, 1, 16000, 32, {0, 0, 0, 63, 0, 0, 128, 191}]]},
    {{{0., 127 / 128., -1., 0.5}, 8000},
     {{0.375, -0.125}, 44100},
     {{0.5, -0.5, 8388607 / 8388608.}, 48000},
     {{0.5, -1.}, 16000},
     {{0.5, -1.}, 16000}},
    TestID -> "whisper/wav-pcm-and-float-formats-and-channels"
]

VerificationTest[
    {Head[whisperRead[whisperWav[7, 1, 8000, 8, {1, 2, 3, 4}]]],
     With[{file = FileNameJoin[{$TemporaryDirectory, "thvm-whisper-test-" <> ToString[$ProcessID] <> ".txt"}]},
         BinaryWrite[file, ToCharacterCode["not a sound at all"], "Byte"] // Close;
         Head[whisperRead[file]]]},
    {WolframInstitute`THVMLink`Examples`Private`speechBad, WolframInstitute`THVMLink`Examples`Private`speechBad},
    TestID -> "whisper/wav-other-encodings-and-files-are-refused"
]

(* A 440 Hz tone keeps its samples through a change of rate, away from the ends the window reaches. *)
VerificationTest[
    Table[
        With[{got = WolframInstitute`THVMLink`Examples`Private`speechResample[Sin[2. Pi 440. Range[0, rate - 1] / rate], rate, 16000]},
            {Length[got], Max[Abs[got - Sin[2. Pi 440. Range[0, 15999] / 16000]][[2000 ;; -2000]]] < 1.*^-4}],
        {rate, {8000, 22050, 24000, 44100, 48000}}],
    ConstantArray[{16000, True}, 5],
    TestID -> "whisper/resample-keeps-a-tone"
]

VerificationTest[
    {WolframInstitute`THVMLink`Examples`Private`speechResample[{1, 2, 3}, 16000, 16000],
     Length[WolframInstitute`THVMLink`Examples`Private`speechResample[ConstantArray[0., #1], #2, 16000]] & @@@
         {{100, 8000}, {7, 48000}, {0, 24000}},
     With[{k = WolframInstitute`THVMLink`Examples`Private`speechKernel[0.25, 1, 16]}, {Length[k], Abs[Total[k] - 1] < 1.*^-12, First @ Ordering[k, -1]}]},
    {{1., 2., 3.}, {200, 3, 0}, {32, True, 16}},
    TestID -> "whisper/resample-lengths-and-kernel"
]

(* An Audio of a file is read, by the kernel's audio functions or from the file itself. *)
VerificationTest[
    With[{x = Sin[Range[0, 999] / 7.] / 2, file = FileNameJoin[{$TemporaryDirectory, "thvm-whisper-test-" <> ToString[$ProcessID] <> ".wav"}]},
        WolframInstitute`THVMLink`Examples`Private`speechWavWrite[file, x, 8000];
        With[{r = WolframInstitute`THVMLink`Examples`Private`speechRead[Audio[file], 16000]},
            DeleteFile[file];
            {Last[r], Length[First[r]], Max[Abs[First[r] - x]] < 1.*^-4}]],
    {8000, 1000, True},
    TestID -> "whisper/read-an-audio-of-a-file"
]

(* An Audio expression that the kernel did not make into an audio object is read from its samples. *)
VerificationTest[
    {WolframInstitute`THVMLink`Examples`Private`speechRead[{0.5, -0.25}, 8000],
     WolframInstitute`THVMLink`Examples`Private`speechRead[Audio[{0.5, -0.25, 0.125}, SampleRate -> 8000], 16000],
     Head[WolframInstitute`THVMLink`Examples`Private`speechRead[{0.1, "x"}, 8000]],
     Head[WolframInstitute`THVMLink`Examples`Private`speechRead[42, 8000]],
     Head[WolframInstitute`THVMLink`Examples`Private`speechRead["/no/such/file.wav", 8000]]},
    {{{0.5, -0.25}, 8000}, {{0.5, -0.25, 0.125}, 8000},
     WolframInstitute`THVMLink`Examples`Private`speechBad, WolframInstitute`THVMLink`Examples`Private`speechBad,
     WolframInstitute`THVMLink`Examples`Private`speechBad},
    SameTest -> (#1 == #2 &),
    TestID -> "whisper/read-samples-audio-and-refusals"
]

(* === the front end ===================================================== *)

VerificationTest[
    With[{mel = WolframInstitute`THVMLink`Examples`Private`whMelFilters[80]},
        {Dimensions[mel], Min[mel] >= 0, First /@ (Ordering[#, -1] & /@ mel[[{1, 20, 40, 60, 80}]]),
         Max[Abs[Total[mel[[{1, 40, 80}]], {2}] - {0.024862593984176087, 0.023394937379861133, 0.024925339738858833}]] < 1.*^-12}],
    {{80, 201}, True, {2, 20, 42, 90, 193}, True},
    TestID -> "whisper/mel-filters"
]

VerificationTest[
    With[{x = N @ Range[1000]},
        With[{seg = WolframInstitute`THVMLink`Examples`Private`whSegmentSamples[x, 1]},
            {Length[seg], seg[[{1, 2, 200, 201, 202, 1200, 1201}]],
             WolframInstitute`THVMLink`Examples`Private`whSegmentCount /@ {{}, x, ConstantArray[0., 480000], ConstantArray[0., 480001]}}]],
    {480320, {201., 200., 2., 1., 2., 1000., 0.}, {1, 1, 1, 2}},
    TestID -> "whisper/segments-are-thirty-seconds-with-reflected-edges"
]

VerificationTest[
    With[{en = WolframInstitute`THVMLink`Examples`Private`whSpecials[51864], all = WolframInstitute`THVMLink`Examples`Private`whSpecials[51865]},
        {Lookup[en, {"EndOfText", "Start", "Previous", "NoTimestamps", "Multilingual"}],
         Lookup[all, {"EndOfText", "Start", "Transcribe", "Previous", "NoTimestamps", "Multilingual"}],
         WolframInstitute`THVMLink`Examples`Private`whStartTokens[en, "en"],
         WolframInstitute`THVMLink`Examples`Private`whStartTokens[all, "en"],
         WolframInstitute`THVMLink`Examples`Private`whStartTokens[all, "de"]}],
    {{50256, 50257, 50360, 50362, False}, {50257, 50258, 50359, 50361, 50363, True},
     {50257, 50362}, {50258, 50259, 50359, 50363}, {50258, 50261, 50359, 50363}},
    TestID -> "whisper/special-and-start-tokens"
]

VerificationTest[
    {WolframInstitute`THVMLink`Examples`Private`whLanguageCode /@ {Automatic, "de", "German", "german", "Haitian Creole", "Klingon", 5},
     Length /@ {WolframInstitute`THVMLink`Examples`Private`$whLanguages, WolframInstitute`THVMLink`Examples`Private`$whLanguageNames}},
    {{Automatic, "de", "de", "de", "ht", None, None}, {99, 99}},
    TestID -> "whisper/language-names-and-codes"
]

(* === failures ========================================================== *)

VerificationTest[
    Quiet[{WhisperRecognize[{0., 0.}, "Model" -> "huge"],
           WhisperRecognize[{0., 0.}, "Model" -> "tiny.en", Language -> "German"],
           WhisperRecognize[{0., 0.}, "Model" -> "tiny", Language -> "Klingon"],
           WhisperRecognize["/no/such/file.wav", "Model" -> "tiny.en"],
           WhisperRecognize[{0., "x"}, "Model" -> "tiny.en"],
           TSpeechRecognize[{0., 0.}, Method -> "Unknown"]}] /. f_Failure :> {FailureQ[f], f["Tag"]},
    {{True, "model"}, {True, "language"}, {True, "language"}, {True, "audio"}, {True, "audio"}, {True, "badmethod"}},
    TestID -> "whisper/bad-arguments-are-failures"
]

(* Every model names a repository, a revision and its two files with sizes and SHA-256. *)
VerificationTest[
    {Keys[WolframInstitute`THVMLink`Examples`Private`$whModels],
     AllTrue[WolframInstitute`THVMLink`Examples`Private`$whModels,
         StringStartsQ[#Repository, "openai/whisper-"] && StringLength[#Revision] == 40 &&
             Keys[#Files] === {"model.safetensors", "vocab.json"} &&
             AllTrue[#Files, IntegerQ[#Bytes] && StringLength[#SHA256] == 64 &] &],
     WolframInstitute`THVMLink`Examples`Private`hubUrl[WolframInstitute`THVMLink`Examples`Private`$whModels["tiny.en"], "vocab.json"],
     WolframInstitute`THVMLink`Examples`Private`hubMegabytes /@ {64512352, 151060136, 0}},
    {{"tiny", "tiny.en", "base", "base.en", "small", "small.en"}, True,
     "https://huggingface.co/openai/whisper-tiny.en/resolve/87c7102498dcde7456f24cfd30239ca606ed9063/vocab.json",
     {"64.5", "151.1", "0.0"}},
    TestID -> "whisper/models-are-pinned"
]

(* === with the model ==================================================== *)

If[$whisperFiles,

    VerificationTest[
        WolframInstitute`THVMLink`Examples`Private`whText[
            WolframInstitute`THVMLink`Examples`Private`whVocabulary[FileNameJoin[{$whisperDir, "vocab.json"}]],
            #] & /@ {{18435, 11, 428, 318, 257, 1332, 286, 4046, 21263, 13}, {}},
        {"Hello, this is a test of speech synthesis.", ""},
        TestID -> "whisper/text-of-tokens"
    ];

    (* The embedding row the host reads from the file is the row of the mapped table. *)
    VerificationTest[
        With[{M = WolframInstitute`THVMLink`Examples`Private`whLoad[FileNameJoin[{$whisperDir, "model.safetensors"}]]},
            {Lookup[M, {"Width", "Heads", "Vocabulary", "Context"}], Length /@ Lookup[M, {"EncoderLayers", "DecoderLayers"}],
             Max[Abs[WolframInstitute`THVMLink`Examples`Private`whEmbedding[M, 18435, 3] - M["Positions"][[4]] -
                 Flatten[Normal[TTensorData[TRealize[M["Tokens"][[18436 ;; 18436]]]]]]]] < 1.*^-6}],
        {{384, 6, 51864, 448}, {4, 4}, True},
        TestID -> "whisper/model-sizes-and-embedding-rows"
    ];

    (* Silence is no speech: whatever the model makes of it is a string. *)
    VerificationTest[
        StringQ[WhisperRecognize[ConstantArray[0., 16000], "Model" -> "tiny.en", ProgressReporting -> False]],
        True,
        TestID -> "whisper/silence-gives-a-string"
    ];

    If[$whisperPhononFiles,

        $whisperSpoken = TSpeechSynthesize["Hello, this is a test of speech synthesis.", "Samples", "Temperature" -> 0, ProgressReporting -> False];

        (* What Phonon says, Whisper hears: samples at 24 kHz, the same as an Audio expression and as a
           WAV file, through WhisperRecognize and TSpeechRecognize. *)
        VerificationTest[
            With[{file = FileNameJoin[{$TemporaryDirectory, "thvm-whisper-test-" <> ToString[$ProcessID] <> ".wav"}]},
                WolframInstitute`THVMLink`Examples`Private`speechWavWrite[file, $whisperSpoken, 24000];
                With[{texts = {
                        WhisperRecognize[$whisperSpoken, "Model" -> "tiny.en", "SampleRate" -> 24000, ProgressReporting -> False],
                        WhisperRecognize[Audio[$whisperSpoken, SampleRate -> 24000], "Model" -> "tiny.en", ProgressReporting -> False],
                        TSpeechRecognize[File[file], "Model" -> "tiny.en", ProgressReporting -> False]}},
                    DeleteFile[file];
                    texts]],
            ConstantArray["Hello, this is a test of speech synthesis.", 3],
            TestID -> "whisper/hears-what-phonon-says"
        ];

        (* Unloading one model resets the engine under the other, which loads again on its next call. *)
        VerificationTest[
            (WhisperUnload[];
             {Length[WolframInstitute`THVMLink`Examples`Private`$whLoaded], WolframInstitute`THVMLink`Examples`Private`$phLoaded,
              TSpeechSynthesize["Hello, this is a test of speech synthesis.", "Samples", "Temperature" -> 0, ProgressReporting -> False] == $whisperSpoken,
              WhisperRecognize[$whisperSpoken, "Model" -> "tiny.en", "SampleRate" -> 24000, ProgressReporting -> False]}),
            {0, None, True, "Hello, this is a test of speech synthesis."},
            TestID -> "whisper/unload-resets-the-other-models-which-load-again"
        ];

        PhononUnload[]
    ];

    WhisperUnload[]
]
