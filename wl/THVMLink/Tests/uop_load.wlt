(* uop_load.wlt -- UOP_LOAD opcode plumbing.

   Sub-item (a) of the UOP_LOAD arc: just verifies the opcode slot
   is reserved + reachable from WL.  Constructor / materializer /
   linearizer behaviour lands in (b)/(c)/(d). *)

VerificationTest[
    IntegerQ[$UopLoad],
    True,
    TestID -> "uop-load/constant-is-integer"
]

(* $UopLoad must not collide with any other reserved opcode. *)
VerificationTest[
    With[{
        others = {$UopMaterialize, $UopKernel, $UopConst,
                  $UopReshape,    $UopPermute, $UopExpand,
                  $UopPad,        $UopShrink,  $UopFlip,
                  $UopAdd,        $UopMul,     $UopNeg,
                  $UopRecip,      $UopExp2,    $UopLog2,
                  $UopSqrt,       $UopCmplt,   $UopReduce,
                  $UopGrad,       $UopCmpeq}
    },
        FreeQ[others, $UopLoad]
    ],
    True,
    TestID -> "uop-load/distinct-from-other-opcodes"
]

(* Round-trips through the $uopNames pretty-printer table.  The
   table itself is private; reach in by full context. *)
VerificationTest[
    Lookup[THVMLink`Private`$uopNames, $UopLoad, Missing["NotFound"]],
    "LOAD",
    TestID -> "uop-load/named-load"
]
