(* Examples.wl -- loader for the WolframInstitute`THVMLink`Examples` context: the
   example-model package.  Each model lives in its own subdirectory (FLUX/ for
   the FLUX.2-klein-4B text-to-image generator; room for more models alongside),
   and every model file is a package in the shared WolframInstitute`THVMLink`Examples`
   context with its FLUX-specific helpers in Examples`Private`.

   OPT-IN.  The base paclet scanner (Kernel/THVMLink.wl) skips everything under
   Kernel/NN/Examples/, so the heavy example code AND the FLUX-specific memory
   tuning below load ONLY when a caller asks for this context:

       Get["WolframInstitute`THVMLink`Examples`"]
       FluxGenerate[{"a cat", "a dog"}]

   The paclet maps the context to this file (PacletInfo.wl Kernel extension), so
   the Get pulls in the main runtime (Needs below) and every example model. *)

(* Memory tuning for the upload-everything-once FLUX forward, set BEFORE the
   runtime caches getenv (the first tensor touch is the first FluxGenerate call,
   well after this load).  These two are deliberately workload-specific, NOT
   global defaults:

   - THVM_MAX_LIVE_BYTES raises the live-buffer SAFETY backstop from the 8 GiB
     default: a full-forward JIT capture legitimately pins ~8.5 GB of block
     activations (over 8 GiB), and that is fine here -- but keeping the default
     low elsewhere still catches a genuine runaway allocation.
   - THVM_MMAP_NO_WILLNEED trades a little cold-start readahead for a much
     smaller peak RSS (the weights fault in one at a time instead of the whole
     multi-GB file up front).  Right for a memory-bound model on a shared box;
     a smaller model on a big-RAM box would rather keep the WILLNEED readahead,
     so it stays opt-in.

   (The realize-boundary forward reclaim is now a runtime default -- the faithful
   free-on-last-refcount GC -- so it no longer needs setting here.)  Guarded so a
   caller can override either. *)
If[ Environment["THVM_MMAP_NO_WILLNEED"] === $Failed, SetEnvironment["THVM_MMAP_NO_WILLNEED" -> "1"]];
If[ Environment["THVM_MAX_LIVE_BYTES"] === $Failed, SetEnvironment["THVM_MAX_LIVE_BYTES" -> "30000000000"]];

Needs["WolframInstitute`THVMLink`"];

(* Load every model package (FLUX/*.wl, Krea/*.wl, and any future model
   subdirectory) into the shared Examples`Private` context, in THREE waves so that
   cross-file PUBLIC symbol references bind the DEFINED symbol, not a Private stub:

   1. The generic pipeline framework (Pipeline.wl) FIRST -- a model file references
      its PUBLIC symbols (tisModelSpec / tisPipeline / tisRegisterComponent), and a
      bare reference would resolve to a Private stub if the public symbol did not
      exist at the model file's parse time.
   2. Implementation files (KreaForward / KreaVAE / FLUX layers, ...) next, sorted.
   3. Assembler files, those named with a Generate suffix, LAST.  An assembler's
      registered forward closures reference the implementation files' PUBLIC helpers
      (krVaeDecode,
      krTransformerPre, ...) by bare name at PARSE time; those publics must already
      exist so the closure binds the defined symbol.  (Pre-this-ordering KreaGenerate
      sorted before KreaVAE, so its VAE-forward bound a stub Examples`Private`krVaeDecode
      that stayed held at call time -- the decoded image was never evaluated.) *)
With[{base = DirectoryName[$InputFileName]},
    Module[{all, framework, assemblers, impl},
        all = Select[FileNames["*.wl", base, Infinity], FileBaseName[#] =!= "Examples" &];
        framework = Select[all, FileBaseName[#] === "Pipeline" &];
        assemblers = Select[all, StringEndsQ[FileBaseName[#], "Generate"] &];
        impl = Complement[all, framework, assemblers];
        Scan[Get, framework];
        Scan[Get, Sort @ impl];
        Scan[Get, Sort @ assemblers]
    ]
]
