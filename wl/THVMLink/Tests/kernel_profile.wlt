(* kernel_profile.wlt -- TKernelProfile snapshot, delta, and
   per-program-shape grouping helpers. *)

VerificationTest[
    TInit[];
    snap = TProfileAll[];
    TProfileDelta[snap, snap],
    <||>,
    TestID -> "kernel-profile/delta-empty-for-identical-snapshot"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1., 2., 3., 4.}, "Real32"];
    before = TProfileAll[];
    TRealize @ TUOpMul[a, a];
    delta = TProfileDelta[before, TProfileAll[]];
    rows = Values[delta];
    {Length[rows] > 0,
     Total[Lookup[#, "DispatchCount", 0] & /@ rows] > 0,
     AllTrue[rows, KeyExistsQ[#, "ProgramKey"] &],
     AllTrue[rows, KeyExistsQ[#, "AvgUs"] &]},
    {True, True, True, True},
    TestID -> "kernel-profile/delta-captures-fired-kernels"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1., 2., 3., 4.}, "Real32"];
    before = TProfileAll[];
    TRealize @ TUOpMul[a, a];
    groups = TProfileProgramGroups @ TProfileDelta[before, TProfileAll[]];
    {Length[groups] > 0,
     AllTrue[groups, KeyExistsQ[#, "Key"] &],
     AllTrue[groups, KeyExistsQ[#, "RepKid"] &],
     AllTrue[groups, KeyExistsQ[#, "DispatchKinds"] &],
     AllTrue[groups, KeyExistsQ[#, "Ops"] &]},
    {True, True, True, True, True},
    TestID -> "kernel-profile/program-groups-summarize-delta"
]

VerificationTest[
    TInit[];
    TProfileFusionGaps[<||>],
    {},
    TestID -> "kernel-profile/fusion-gaps-empty-profile"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1., 2., 3., 4.}, "Real32"];
    before = TProfileAll[];
    TRealize @ TUOpMul[a, a];
    gaps = TProfileFusionGaps @ TProfileDelta[before, TProfileAll[]];
    AllTrue[gaps,
        And[
            KeyExistsQ[#, "FusionStatus"],
            KeyExistsQ[#, "ScalarLowered"],
            KeyExistsQ[#, "TileLowered"],
            KeyExistsQ[#, "ProposalCount"]
        ] &],
    True,
    TestID -> "kernel-profile/fusion-gaps-schema"
]
