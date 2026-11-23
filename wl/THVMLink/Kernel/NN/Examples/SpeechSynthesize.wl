(* SpeechSynthesize.wl - TSpeechSynthesize, the text-to-speech entry point for the thvm example models.
   It dispatches on the Method option to a per-model synthesizer: "Phonon" runs Gradium's Phonon 31M
   (PhononSynthesize).  Shaped after the built-in SpeechSynthesize and laid out like TImageSynthesize.
   Part of the WolframInstitute`THVMLink`Examples` package. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

TSpeechSynthesize::usage = "TSpeechSynthesize[text$] synthesizes speech for the string text$ with a thvm text-to-speech model chosen by the Method option and gives an Audio object. Method -> \"Phonon\" (the default) is the English Phonon 31M model; the other options are the chosen synthesizer's (see PhononSynthesize).";

TSpeechSynthesize::badmethod = "`1` is not a known Method; use one of `2`.";

Begin["`Private`"];

(* Method -> the synthesizer symbol, context-qualified so the registry binds the public symbol whatever
   order Examples.wl loads the model files in. *)
$speechSynthesizers = <|
    "Phonon" -> WolframInstitute`THVMLink`Examples`PhononSynthesize
|>;

Options[TSpeechSynthesize] = {Method -> "Phonon"};

TSpeechSynthesize[text_String, opts : OptionsPattern[]] := With[{method = Lookup[<|opts|>, Method, "Phonon"]},
    With[{gen = Lookup[$speechSynthesizers, method, Missing[]]},
        If[MissingQ[gen],
            Message[TSpeechSynthesize::badmethod, method, Keys[$speechSynthesizers]]; $Failed,
            gen[text, FilterRules[{opts}, Options[gen]]]]]
]

End[];

EndPackage[];
