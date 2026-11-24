(* Models.wl -- what the example models share: fetching a model's files from its HuggingFace repository,
   and the engine reset that unloading a model is.

   A model names its source as <|"Repository" -> "owner/name", "Revision" -> commit, "Files" ->
   <|name -> <|"Bytes" -> size, "SHA256" -> hex|>, ...|>|>: the revision is the one its port was checked
   against, and a download is kept only when it has the pinned size and SHA-256.

   Unloading any model resets the engine, which invalidates every tensor, the other models' too.  So each
   model registers how to forget what it has loaded (modelOnReset), and every unload goes through
   modelEngineReset, after which the others load again on their next call. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

Begin["`Private`"];

(* === fetching ========================================================== *)

hubUrl[source_Association, name_String] :=
    "https://huggingface.co/" <> source["Repository"] <> "/resolve/" <> source["Revision"] <> "/" <> name

(* A file of the model directory counts when it has its pinned size. *)
hubHaveQ[source_Association, dir_String, name_String] := With[{file = FileNameJoin[{dir, name}]},
    FileExistsQ[file] && FileByteCount[file] == source["Files"][name]["Bytes"]]

(* hubReport[text, fraction] sets what the progress display shows; nothing outside hubWithProgress. *)
hubReport[_, _] := Null

(* Evaluates body under a progress display, titled at first, that follows hubReport, as fxWithProgress
   does for FLUX. *)
SetAttributes[hubWithProgress, HoldRest];
hubWithProgress[{reportQ_, title_String}, body_] := If[TrueQ[reportQ],
    Module[{text = title, progress = Indeterminate},
        Block[{hubReport = Function[{t, p}, text = t; progress = p]},
            Progress`EvaluateWithProgress[body, <|"Text" :> text, "Progress" :> progress|>]]],
    body
]

hubCurlQ[] := hubCurlQ[] = Quiet[RunProcess[{"curl", "--version"}, "ExitCode"]] === 0

(* Downloads url to file, calling report[bytes so far] while it runs.  By curl in the background where the
   system has it: the size of its growing file is read five times a second, since not every kernel can
   download asynchronously, and a file already there is a download that stopped, which curl continues
   where it stopped (-C -).  Else by URLDownload in one step, from the start. *)
hubDownload[url_String, file_String, report_] := (
    If[hubCurlQ[],
        With[{proc = StartProcess[{"curl", "-fsSL", "--retry", "2", "-C", "-", "-o", file, url}]},
            While[ProcessStatus[proc] === "Running",
                report[If[FileExistsQ[file], FileByteCount[file], 0]];
                Pause[0.2]]],
        If[FileExistsQ[file], DeleteFile[file]];
        Quiet[URLDownload[url, file]]];
)

(* The bytes free on the disk that holds dir (df -Pk: blocks, used, available, capacity), or Infinity
   where the system does not say. *)
hubFreeBytes[dir_String] := With[{out = Quiet[RunProcess[{"df", "-Pk", dir}, "StandardOutput"]]},
    With[{free = If[StringQ[out],
            StringCases[out, DigitCharacter .. ~~ Whitespace .. ~~ DigitCharacter .. ~~ Whitespace .. ~~ (a : DigitCharacter ..) ~~
                Whitespace .. ~~ DigitCharacter .. ~~ "%" :> a],
            {}]},
        If[free === {}, Infinity, 1024 FromDigits[Last[free]]]]]

(* A byte count in megabytes with one decimal, "64.5". *)
hubMegabytes[bytes_] := With[{tenths = Round[bytes / 100000]},
    IntegerString[Quotient[tenths, 10]] <> "." <> IntegerString[Mod[tenths, 10]]]

(* True when dir holds the source's files, the missing ones fetched and checked first; else a Failure of
   sym, by its message sym::fetch (file, URL, reason), that says which file could not be had and why.
   A download goes to a .part file.  One that stopped short is left for the next call to continue; one of
   the full size is kept only with the pinned SHA-256. *)
hubFetch[sym_Symbol, source_Association, dir_String] := Module[
    {files = source["Files"], missing, total, done = 0, fail},
    missing = Select[Keys[files], ! hubHaveQ[source, dir, #] &];
    If[missing === {}, Return[True]];
    fail[name_, why_] := Throw[WolframInstitute`THVMLink`Private`thvmFailure[MessageName[sym, "fetch"], name, hubUrl[source, name], why]];
    (* A file's name may have directories, as it has in its repository. *)
    Scan[
        With[{parent = DirectoryName[FileNameJoin[{dir, #}]]},
            If[! DirectoryQ[parent], CreateDirectory[parent, CreateIntermediateDirectories -> True]]] &,
        missing];
    total = Total[files[#]["Bytes"] & /@ missing];
    Catch[
        (* What is still to arrive must fit on the disk: said before a long download, not at its end. *)
        With[{need = total - Total[With[{part = FileNameJoin[{dir, # <> ".part"}]}, If[FileExistsQ[part], FileByteCount[part], 0]] & /@ missing],
              free = hubFreeBytes[dir]},
            If[need > free,
                fail[First[missing], "the files still to fetch need " <> hubMegabytes[need] <> " MB and the disk has " <> hubMegabytes[free] <> " MB free"]]];
        Do[
            With[{file = FileNameJoin[{dir, name}], part = FileNameJoin[{dir, name <> ".part"}], want = files[name]},
                hubDownload[hubUrl[source, name], part, Function[bytes, hubReport[
                    "Fetching " <> name <> " from huggingface.co/" <> source["Repository"] <> " (" <> hubMegabytes[done + bytes] <>
                        " of " <> hubMegabytes[total] <> " MB)",
                    (done + bytes) / total]]];
                Which[
                    ! FileExistsQ[part],
                        fail[name, "the download failed"],
                    FileByteCount[part] < want["Bytes"],
                        fail[name, "the download stopped at " <> hubMegabytes[FileByteCount[part]] <> " of " <>
                            hubMegabytes[want["Bytes"]] <> " MB; the next call continues it"],
                    (hubReport["Checking " <> name, (done + want["Bytes"]) / total];
                     FileByteCount[part] != want["Bytes"] || FileHash[part, "SHA256", All, "HexString"] =!= want["SHA256"]),
                        DeleteFile[part];
                        fail[name, "what arrived is not the file expected, of " <> ToString[want["Bytes"]] <> " bytes and SHA-256 " <> want["SHA256"]],
                    True,
                        If[FileExistsQ[file], DeleteFile[file]];
                        RenameFile[part, file];
                        done += want["Bytes"]]],
            {name, missing}];
        True]
]

(* === unloading ========================================================= *)

(* Model name -> a function that drops what the model has loaded: its captured steps and its state. *)
$modelForget = <||>;

modelOnReset[name_String, forget_] := ($modelForget[name] = forget;)

(* Every model forgets what it has loaded, and the engine is reset: the kernel table keeps every tensor a
   captured step read, so nothing short of a reset frees a model.  Terms made before it are no longer
   valid. *)
modelEngineReset[] := (Scan[#[] &, $modelForget]; Quiet[TReset[]];)

End[];

EndPackage[];
