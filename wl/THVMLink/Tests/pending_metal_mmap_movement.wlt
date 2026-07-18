(* pending_metal_mmap_movement.wlt - OPEN core bug: a movement realize (e.g.
   Transpose) of a disk-mmap-backed tensor on the Metal backend produces NaN.

   TToDevice of an mmap tensor zero-copy WRAPS the file pages as a borrowed
   MTLBuffer; a movement kernel reading that wrapped buffer returns garbage, in
   BOTH orders (Transpose then move, and move then Transpose).  The CPU backend
   realizes the same graphs correctly.  Custom kernels reading a wrapped buffer
   LINEARLY are fine (the DeepSeekV2 q8 expert path); only movement-realized
   wraps are hit.  Found via the DSV2 GGUF import: the transposed f32 gate
   weight was the only movement-realized mmap in the model and NaN'd the whole
   MoE layer (route read garbage logits -> Inf/Inf probs).  The loader now
   materializes small F32 tensors host-side (Gguf.wl); this test documents the
   core bug for a backend fix.

   Runs only when a Metal device initializes; passes trivially elsewhere. *)

Get["WolframInstitute`THVMLink`Examples`"]

VerificationTest[
    Block[{f = FileNameJoin[{$TemporaryDirectory, "mmapmv.bin"}], vals, cpuT, metT, metalUp},
        TReset[];
        SeedRandom[3];
        vals = RandomReal[{-1., 1.}, {64, 128}];
        BinaryWrite[f, Flatten[vals], "Real32"];
        Close[f];
        metalUp = Quiet @ Check[TRealize @ TToDevice[TTensorCreate[{0.}], "metal"], $Failed];
        If[ metalUp === $Failed,
            True,   (* no Metal device: vacuous pass *)
            cpuT = Normal @ TRealize @ Transpose @ TUOpReshape[TTensorMMap[f, 0, 64*128*4, "f32", {64*128}], {64, 128}];
            metT = Quiet @ Normal @ TRealize @ TToDevice[Transpose @ TUOpReshape[TTensorMMap[f, 0, 64*128*4, "f32", {64*128}], {64, 128}], "metal"];
            ListQ[metT] && Max @ Abs[Flatten[cpuT] - Flatten[metT]] < 1.*^-6
        ]
    ],
    True,
    TestID -> "metal-mmap/movement-realize-of-wrapped-buffer"
]
