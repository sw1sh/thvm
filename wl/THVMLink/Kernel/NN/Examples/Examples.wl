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
If[ Environment["THVM_MAX_LIVE_BYTES"]   === $Failed, SetEnvironment["THVM_MAX_LIVE_BYTES"   -> "30000000000"]];

Needs["WolframInstitute`THVMLink`"];

(* Load every model package (FLUX/*.wl, Krea/*.wl, and any future model
   subdirectory) into the shared Examples`Private` context.  The generic pipeline
   framework (Pipeline.wl) loads FIRST: a model file references the framework's
   PUBLIC symbols (tisModelSpec / tisPipeline / tisRegisterComponent), and a bare
   reference resolves to a Private stub if the public symbol does not exist yet at
   the model file's parse time.  After Pipeline.wl the rest load in path order --
   every definition is SetDelayed and each file self-registers its context via its
   own BeginPackage, so among the model files order is irrelevant. *)
With[{base = DirectoryName[$InputFileName]},
    Module[{all, framework},
        all = Select[FileNames["*.wl", base, Infinity], FileBaseName[#] =!= "Examples" &];
        framework = Select[all, FileBaseName[#] === "Pipeline" &];
        Scan[Get, framework];
        Scan[Get, Sort @ Complement[all, framework]]
    ]
]
