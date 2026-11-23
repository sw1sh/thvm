(* phonon.wlt - Phonon 31M text to speech (NN/Examples/Phonon).  The text normaliser and the prompt
   preparation need no model.  With the model's files in place (~/.cache/thvm/phonon-31m-en, where the
   first PhononSynthesize fetches them): the tokenizer against the HuggingFace tokenizer's ids, the
   chunker's token limit, and a deterministic synthesis against the NumPy reference
   (tools/phonon/ref_phonon.py), by its frame count and signal level. *)

Get["WolframInstitute`THVMLink`Examples`"]

$phononDir = FileNameJoin[{$HomeDirectory, ".cache", "thvm", "phonon-31m-en"}];
$phononFiles = AllTrue[{"model.q8.gguf", "tokenizer.json", "default-voice.safetensors"},
    FileExistsQ[FileNameJoin[{$phononDir, #}]] &];

VerificationTest[
    WolframInstitute`THVMLink`Examples`Private`phNormalize /@ {
        "It costs $3.50, or about 50 cents more than last year.",
        "The total is $1,234.56 and the tax is 8.5%.",
        "Visit https://www.wolfram.com/language or email info@example.com today.",
        "Dr. Smith arrived at 3:45 PM on Jan. 5, 2026.",
        "Temperatures hit -5\[Degree]C; x^2 + y^2 = 1.",
        "Pi is 3.14159, in release 1.2.3."},
    {"It costs 3 dollars and 50 cents, or about 50 cents more than last year.",
     "The total is 1 thousand 234 dollars and 56 cents and the tax is 8.5%.",
     "Visit H-T-T-P-S colon slash slash W-W-W dot wolfram dot com slash language or email info at example dot com today.",
     "Dr. Smith arrived at 3:45 PM on January 5, 2026.",
     "Temperatures hit minus 5 degrees Celsius, x^2 plus y^2 equals 1.",
     "Pi is 3 point 14159, in release 1.2.3."},
    TestID -> "phonon/normalize-symbols-to-words"
]

VerificationTest[
    WolframInstitute`THVMLink`Examples`Private`phPrepare /@ {"hello world", "  wait, what:  ", "He said \"go\"",
        "A sentence of more than four words"},
    {{"Hello world.", 3}, {"Wait, what.", 3}, {"He said \"go\".", 3}, {"A sentence of more than four words.", 1}},
    TestID -> "phonon/prepare-capitalizes-terminates-and-counts-words"
]

If[$phononFiles,

    $phononTok = WolframInstitute`THVMLink`Examples`Private`phTokenizer[FileNameJoin[{$phononDir, "tokenizer.json"}]];

    VerificationTest[
        WolframInstitute`THVMLink`Examples`Private`phEncode[$phononTok, #] & /@ {
            "Hello! This is a test of speech synthesis.",
            "Numbers: 3.14159, 2026, 1,234.56 and 50%.",
            "Unusual: na\[IDoubleDot]ve caf\[EAcute] \[LongDash] \[UDoubleDot]ber \:65e5\:672c\:8a9e \|01f600 end."},
        {{3248, 1367, 919, 677, 668, 1606, 672, 660, 3777, 3695, 2639, 662, 983, 663},
         {660, 3130, 662, 462, 660, 873, 663, 748, 924, 748, 856, 905, 661, 660, 769, 720, 769, 978, 661, 660, 748, 661,
          769, 873, 924, 663, 856, 978, 670, 660, 856, 720, 1240, 663},
         {1970, 1010, 887, 734, 462, 1280, 599, 579, 717, 723, 2758, 1053, 660, 630, 532, 552, 660, 2987, 1433, 660, 634,
          555, 569, 634, 560, 576, 636, 574, 562, 660, 644, 563, 556, 532, 971, 663}},
        TestID -> "phonon/tokenizer-matches-huggingface-ids"
    ];

    VerificationTest[
        With[{ids = WolframInstitute`THVMLink`Examples`Private`phEncode[$phononTok, "Pi is 3.14, roughly. Is that clear?"]},
            WolframInstitute`THVMLink`Examples`Private`phDecode[$phononTok, ids]],
        "Pi is 3.14, roughly. Is that clear?",
        TestID -> "phonon/tokenizer-decode-round-trip"
    ];

    VerificationTest[
        With[{chunks = WolframInstitute`THVMLink`Examples`Private`phChunks[$phononTok,
                StringRiffle[ConstantArray["The quick brown fox jumps over the lazy dog, twice. Pi is about 3.14159!", 6], " "]]},
            {Length[chunks] > 1, Max[Length[WolframInstitute`THVMLink`Examples`Private`phEncode[$phononTok, #]] & /@ chunks] <= 50,
             AllTrue[chunks, StringEndsQ[#, "." | "!"] &]}],
        {True, True, True},
        TestID -> "phonon/chunks-are-whole-sentences-within-the-token-limit"
    ];

    (* The reference speaks this sentence in 42 frames (80640 samples) at an rms of 0.0594. *)
    VerificationTest[
        With[{a = PhononSynthesize["Hello! This is a test of speech synthesis.", "Temperature" -> 0]},
            With[{x = Flatten @ Normal @ AudioData[a]},
                {Head[a], QuantityMagnitude[AudioSampleRate[a]], Length[x], Abs[Sqrt[Mean[x^2]] - 0.0594] < 0.002}]],
        {Audio, 24000, 80640, True},
        TestID -> "phonon/synthesis-matches-the-reference-length-and-level"
    ];

    VerificationTest[
        Normal[AudioData[TSpeechSynthesize["Twice the same.", "Temperature" -> 0]]] ===
            Normal[AudioData[TSpeechSynthesize["Twice the same.", "Temperature" -> 0]]],
        True,
        TestID -> "phonon/temperature-zero-is-deterministic"
    ];

    PhononUnload[]
]
