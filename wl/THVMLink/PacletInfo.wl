(* ::Package:: *)

PacletObject[<|
    "Name" -> "WolframInstitute/THVMLink",
    "PublisherID" -> "WolframInstitute",
    "Description" -> "Wolfram Language bridge to thvm - observe and drive the interaction-net runtime.",
    "Creator" -> "Wolfram Institute",
    "License" -> "MIT",
    "Version" -> "0.1.0",
    "WolframVersion" -> "13.0+",
    "PrimaryContext" -> "WolframInstitute`THVMLink`",
    "Extensions" -> {
        {
            "Kernel",
            "Root" -> "Kernel",
            (* Examples` is opt-in: its loader (NN/Examples/Examples.wl) is mapped
               explicitly so Get["WolframInstitute`THVMLink`Examples`"] pulls in the
               main runtime + the example models, while the base paclet load skips
               everything under Kernel/.../Examples/ (see THVMLink.wl scanner). *)
            "Context" -> {
                "WolframInstitute`THVMLink`",
                "WolframInstitute`THVMLink`ATP`",
                {"WolframInstitute`THVMLink`Examples`", "NN/Examples/Examples.wl"}
            }
        },
        {"LibraryLink"},
        {"Documentation", "Language" -> "English", "MainPage" -> "Guides/THVMLink"},
        {
            "Asset",
            "Root" -> "Assets",
            "Assets" -> {{"Hero", "hero.png"}, {"GCNAtpScorer", "gcn_atp.safetensors"}}
        }
    }
|>]
