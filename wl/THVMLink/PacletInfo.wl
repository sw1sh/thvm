(* ::Package:: *)

PacletObject[<|
    "Name"           -> "WolframInstitute/THVMLink",
    "PublisherID"    -> "WolframInstitute",
    "Description"    -> "Wolfram Language bridge to thvm - observe and drive the interaction-net runtime.",
    "Creator"        -> "Wolfram Institute",
    "License"        -> "MIT",
    "Version"        -> "0.1.0",
    "WolframVersion" -> "13.0+",
    "PrimaryContext" -> "WolframInstitute`THVMLink`",
    "Extensions"     -> {
        {
            "Kernel",
            "Root"    -> "Kernel",
            "Context" -> {"WolframInstitute`THVMLink`", "WolframInstitute`THVMLink`ATP`"}
        },
        {"LibraryLink"},
        {
            "Documentation",
            "Language" -> "English",
            "MainPage" -> "Guides/THVMLink"
        },
        {
            "Asset",
            "Root"   -> "Assets",
            "Assets" -> {
                {"Hero", "hero.png"},
                {"GCNAtpScorer", "gcn_atp.safetensors"}
            }
        }
    }
|>]
