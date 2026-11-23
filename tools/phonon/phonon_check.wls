(* phonon_check.wls <fixtures dir> [runs]: the thvm Phonon port against the NumPy reference's fixtures
   (tools/phonon/ref_phonon.py writes them): max |thvm - reference| for the prompt, the latents and the
   audio, timings over repeated runs, and what grows per run.

       python tools/phonon/ref_phonon.py ~/.cache/thvm/phonon-fixtures
       wl -f tools/phonon/phonon_check.wls ~/.cache/thvm/phonon-fixtures

   The model files are read from PHONON_DIR (default ~/.cache/thvm/phonon-31m-en).  THVMLink loads from
   this checkout by path: an installed release would win a context lookup. *)
With[{root = FileNameJoin[{DirectoryName[$InputFileName], "..", ".."}]},
    Get[FileNameJoin[{root, "wl", "THVMLink", "Kernel", "THVMLink.wl"}]];
    Get[FileNameJoin[{root, "wl", "THVMLink", "Kernel", "NN", "Examples", "Examples.wl"}]]];
Begin["WolframInstitute`THVMLink`Examples`Private`"];
fx = ExpandFileName[$ScriptCommandLine[[2]]];
runs = If[Length[$ScriptCommandLine] > 2, ToExpression[$ScriptCommandLine[[3]]], 3];
model = Replace[Environment["PHONON_DIR"], $Failed -> phModelDirectory[Automatic]];
meta = Import[FileNameJoin[{fx, "meta.json"}], "RawJSON"];
rd[name_] := ArrayReshape[BinaryReadList[FileNameJoin[{fx, name <> ".f32"}], "Real32", ByteOrdering -> -1], meta["shapes"][name]];
diff[a_, b_] := Max[Abs[Flatten[N[a]] - Flatten[N[b]]]];
say[s__] := WriteString["stdout", StringJoin[If[StringQ[#], #, ToString[#, InputForm]] & /@ {s}], "\n"];
probe[tag_] := say[tag, ": pins ", TExternPinCount[], ", tensors ", TTensCount[], ", buffer MB ", Round[TTotalBufBytes[] / 2.^20, 0.1],
    ", kernels ", TKernelCount[], ", heap cells ", THeapPos[]];
Module[{t, W, S, prompt, lats, audio, ref = rd["latents"]},
    {t, W} = AbsoluteTiming[phLoadGguf[FileNameJoin[{model, "model.q8.gguf"}]]];
    say["load ", t, " s"];
    S = phSession[W];
    prompt = phPrompt[S, meta["ids"], rd["voice"] . Transpose[S["Host"]["Speaker"]]];
    say["prompt: max diff ", diff[prompt, rd["prompt_in"]]];
    probe["before"];
    Do[
        {t, lats} = AbsoluteTiming[phLatents[S, prompt, 0., 400, 1]];
        say["latents, run ", r, ": ", t, " s for ", Length[lats], " frames (reference ", Length[ref], "), max diff ",
            If[Length[lats] == Length[ref], diff[lats, ref], "length differs"]];
        probe["  after"],
        {r, runs}];
    Do[
        {t, audio} = AbsoluteTiming[phMimiDecode[S, ref]];
        say["audio from the reference latents, run ", r, ": ", t, " s for ", Length[audio], " samples, max diff ",
            diff[audio, rd["audio"]], " (reference rms ", Sqrt[Mean[rd["audio"]^2]], ")"];
        probe["  after"],
        {r, runs}];
    say["captured kernels: prefill ", TJitOpCount[S["Prefill"]], ", decode ", TJitOpCount[S["Decode"]], ", Mimi chunk ", TJitOpCount[S["MimiStep"]]]];
End[];
