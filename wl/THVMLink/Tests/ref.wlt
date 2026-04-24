(* ref.wlt -- lazy named definitions via TDef / TRef. *)

VerificationTest[
    TInit[];
    (* @id := lambda x. x *)
    TDef["id", TLam[Function[x, x]]];
    out = TWnf @ TApp[TRef["id"], TEra[]];
    TTagName[TTermTag[out]],
    "ERA",
    TestID -> "ref/identity-applied-to-era"
]

VerificationTest[
    TInit[];
    (* Two distinct calls to the same def must allocate fresh dyn cells
       per fire (no aliasing of the static template). *)
    TDef["id", TLam[Function[x, x]]];
    before = THeapPos[];
    TWnf @ TApp[TRef["id"], TEra[]];
    mid    = THeapPos[];
    TWnf @ TApp[TRef["id"], TEra[]];
    after  = THeapPos[];
    {(mid - before) > 0, (after - mid) === (mid - before)},
    {True, True},
    TestID -> "ref/two-calls-allocate-fresh-cells"
]

VerificationTest[
    TInit[];
    (* @loop := lambda x. @loop  -- self-referential.  Reducing once
       returns the body lambda; lazy unfolding means we never blow up
       at construction time. *)
    TDef["loop", TLam[Function[x, TRef["loop"]]]];
    out = TWnf @ TApp[TRef["loop"], TEra[]];
    TTagName[TTermTag[out]],
    "LAM",
    TestID -> "ref/self-reference-unfolds-lazily"
]

VerificationTest[
    TInit[];
    TDef["foo", TLam[Function[x, x]]];
    TDef["bar", TLam[Function[x, TEra[]]]];
    fooSlot = TDefName["foo"];
    barSlot = TDefName["bar"];
    (* Slots are distinct + each repeated lookup is stable. *)
    {fooSlot =!= barSlot,
     TDefName["foo"] === fooSlot,
     TDefName["bar"] === barSlot},
    {True, True, True},
    TestID -> "ref/string-name-interning-stable"
]
