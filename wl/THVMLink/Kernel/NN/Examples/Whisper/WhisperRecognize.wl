(* WhisperRecognize.wl -- WhisperRecognize: speech to text with OpenAI's Whisper (openai/whisper-*, MIT)
   on thvm.

   The audio is read and brought to 16 kHz mono (SpeechRecognize.wl), cut into 30-second segments, and
   each segment is encoded once and decoded greedily, token by token, prompted with the text of the
   segment before (WhisperModel.wl).  A model, its vocabulary and its captured steps stay loaded between
   calls.

   The weights are not shipped with the paclet.  The first call with a model fetches its two files from
   its HuggingFace repository, at the revision the port was checked against, under a progress display,
   and keeps a download only when its size and SHA-256 are the pinned ones (Models.wl). *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

WhisperRecognize::usage = "WhisperRecognize[audio$] recognizes the speech in audio$ with OpenAI's Whisper and gives it as a string. audio$ is an Audio object, the path of an audio file or File[path$], or a list of samples. The first call with a model fetches its files from its HuggingFace repository, openai/whisper-model$, to the model directory, showing its progress and checking each file against its SHA-256; the model then stays loaded between calls. Options: \"Model\" (\"base\" by default; \"tiny\", \"base\" and \"small\" know 99 languages, \"tiny.en\", \"base.en\" and \"small.en\" English only), Language (Automatic has a multilingual model detect it from the first 30 seconds; else a name such as \"German\" or a code such as \"de\"), \"SampleRate\" (of a list of samples, 16000), \"ModelDirectory\" (Automatic is ~/.cache/thvm/whisper-model$; a directory that already holds the files is used as it is), ProgressReporting (False hides the progress of the fetch, the load and the segments).";
WhisperUnload::usage = "WhisperUnload[] unloads the Whisper models and frees their buffers by resetting the thvm engine, so terms made before it are no longer valid; the next WhisperRecognize loads its model again.";

WhisperRecognize::fetch = "Could not fetch `1` from `2`: `3`.";
WhisperRecognize::audio = "Cannot read the audio: `1`.";
WhisperRecognize::model = "`1` is not a known model; use one of `2`.";
WhisperRecognize::language = "`1` is not a language the model `2` recognizes.";

Begin["`Private`"];

(* === the models' files ================================================= *)

(* Where a model's files come from and what each must be (Models.wl).  The revision is the one the port
   was checked against.  The English-only models share one vocabulary file and the multilingual ones
   another. *)
$whVocabularyFiles = <|
    True -> <|"Bytes" -> 798156, "SHA256" -> "3ba3c3109ff33976c4bd966589c11ee14fcaa1f4c9e5e154c2ed7f99d80709e7"|>,
    False -> <|"Bytes" -> 835550, "SHA256" -> "8f680bba319e01a653d2e8a5dbc17a9157179e0576e6ce74ce0c06356c6e24f9"|>
|>;

whSource[name_String, revision_String, bytes_Integer, sha_String] := name -> <|
    "Repository" -> "openai/whisper-" <> name, "Revision" -> revision,
    "Files" -> <|"model.safetensors" -> <|"Bytes" -> bytes, "SHA256" -> sha|>,
        "vocab.json" -> $whVocabularyFiles[StringEndsQ[name, ".en"]]|>|>

$whModels = Association[
    whSource["tiny", "169d4a4341b33bc18d8881c4b69c2e104e1cc0af", 151061672, "7ebd0e69e78190ffe1438491fa05cc1f5c1aa3a4c4db3bc1723adbb551ea2395"],
    whSource["tiny.en", "87c7102498dcde7456f24cfd30239ca606ed9063", 151060136, "db59695928ded6043adaef491a53ef4e12da9611184d77c53baa691a60b958ad"],
    whSource["base", "e37978b90ca9030d5170a5c07aadb050351a65bb", 290403936, "07cadb9f25677c8d50df603e66a98fbd842cce45047139baeb16e6219a1e807b"],
    whSource["base.en", "911407f4214e0e1d82085af863093ec0b66f9cd6", 290401888, "d4dd5542fd6a1d35639e21384238f3bfe6c557c849d392b5905d33ee29e71db5"],
    whSource["small", "973afd24965f72e36ca33b3055d56a652f456b4d", 966995080, "1d7734884874f1a1513ed9aa760a4f8e97aaa02fd6d93a3a85d27b2ae9ca596b"],
    whSource["small.en", "e8727524f962ee844a7319d92be39ac1bd25655a", 966992008, "6014ac49b506df900f66f4aca6b0801eed7245594ace97bcaf73e0ae5b863066"]
];

whModelDirectory[name_String, Automatic] := FileNameJoin[{$HomeDirectory, ".cache", "thvm", "whisper-" <> name}]
whModelDirectory[_String, dir_String] := ExpandFileName[dir]

(* === the loaded models ================================================= *)

(* {model, directory} -> its session and vocabulary. *)
$whLoaded = <||>;

(* The loaded model `name` of dir.  When it is not loaded yet its files are fetched and its weights
   mapped; a Failure when a file cannot be had. *)
whLoaded[name_String, dir_String] := If[KeyExistsQ[$whLoaded, {name, dir}],
    $whLoaded[{name, dir}],
    With[{fetched = hubFetch[WhisperRecognize, $whModels[name], dir]},
        If[fetched =!= True,
            fetched,
            hubReport["Loading the Whisper " <> name <> " weights", Indeterminate];
            $whLoaded[{name, dir}] = <|
                "Session" -> whSession[whLoad[FileNameJoin[{dir, "model.safetensors"}]]],
                "Vocabulary" -> whVocabulary[FileNameJoin[{dir, "vocab.json"}]]|>]]
]

(* Unloading resets the engine (modelEngineReset), so the other loaded models go with it; this is what
   Whisper drops when any of them unloads. *)
modelOnReset["Whisper", Function[
    Scan[whSessionClose[#["Session"]] &, $whLoaded];
    $whLoaded = <||>; $whInputs = <||>;]];

WhisperUnload[] := (If[Length[$whLoaded] > 0, modelEngineReset[]];)

(* === languages ========================================================= *)

(* The names of $whLanguages, in its order (OpenAI whisper tokenizer.py LANGUAGES). *)
$whLanguageNames = {"English", "Chinese", "German", "Spanish", "Russian", "Korean", "French", "Japanese", "Portuguese", "Turkish",
    "Polish", "Catalan", "Dutch", "Arabic", "Swedish", "Italian", "Indonesian", "Hindi", "Finnish", "Vietnamese", "Hebrew",
    "Ukrainian", "Greek", "Malay", "Czech", "Romanian", "Danish", "Hungarian", "Tamil", "Norwegian", "Thai", "Urdu", "Croatian",
    "Bulgarian", "Lithuanian", "Latin", "Maori", "Malayalam", "Welsh", "Slovak", "Telugu", "Persian", "Latvian", "Bengali",
    "Serbian", "Azerbaijani", "Slovenian", "Kannada", "Estonian", "Macedonian", "Breton", "Basque", "Icelandic", "Armenian",
    "Nepali", "Mongolian", "Bosnian", "Kazakh", "Albanian", "Swahili", "Galician", "Marathi", "Punjabi", "Sinhala", "Khmer",
    "Shona", "Yoruba", "Somali", "Afrikaans", "Occitan", "Georgian", "Belarusian", "Tajik", "Sindhi", "Gujarati", "Amharic",
    "Yiddish", "Lao", "Uzbek", "Faroese", "Haitian Creole", "Pashto", "Turkmen", "Nynorsk", "Maltese", "Sanskrit",
    "Luxembourgish", "Myanmar", "Tibetan", "Tagalog", "Malagasy", "Assamese", "Tatar", "Hawaiian", "Lingala", "Hausa", "Bashkir",
    "Javanese", "Sundanese"};

(* The code of a Language option value: Automatic, a code of $whLanguages or a name; None for another. *)
whLanguageCode[Automatic] := Automatic
whLanguageCode[language_String] := With[{l = ToLowerCase[language]},
    If[MemberQ[$whLanguages, l],
        l,
        With[{at = FirstPosition[ToLowerCase[$whLanguageNames], l, None]},
            If[at === None, None, $whLanguages[[First[at]]]]]]]
whLanguageCode[_] := None

(* === recognition ======================================================= *)

Options[WhisperRecognize] = {"Model" -> "base", Language -> Automatic, "SampleRate" -> 16000, "ModelDirectory" -> Automatic,
    ProgressReporting -> Automatic};

WhisperRecognize[audio_, opts : OptionsPattern[]] := Module[{name = OptionValue["Model"], language, samples},
    If[! KeyExistsQ[$whModels, name],
        Return[WolframInstitute`THVMLink`Private`thvmFailure[WhisperRecognize::model, name, Keys[$whModels]]]];
    language = whLanguageCode[OptionValue[Language]];
    If[language === None || (StringEndsQ[name, ".en"] && ! MatchQ[language, Automatic | "en"]),
        Return[WolframInstitute`THVMLink`Private`thvmFailure[WhisperRecognize::language, OptionValue[Language], name]]];
    samples = speechSamples[audio, OptionValue["SampleRate"], $whRate];
    If[! ListQ[samples],
        Return[WolframInstitute`THVMLink`Private`thvmFailure[WhisperRecognize::audio, First[samples]]]];
    hubWithProgress[{OptionValue[ProgressReporting] =!= False, "Loading Whisper " <> name},
        With[{loaded = whLoaded[name, whModelDirectory[name, OptionValue["ModelDirectory"]]]},
            If[FailureQ[loaded],
                loaded,
                whText[loaded["Vocabulary"], whTranscribe[loaded["Session"], samples, language,
                    Function[{segment, n}, hubReport[
                        "Recognizing speech" <> If[n > 1, " (segment " <> ToString[segment] <> " of " <> ToString[n] <> ")", ""],
                        If[n > 1, (segment - 1) / n, Indeterminate]]]]["Tokens"]]]]]
]

End[];

EndPackage[];
