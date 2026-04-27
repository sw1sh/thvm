(* grad-add-xx: gradient of (x + x) with respect to x -- the simplest
   case where the chain rule's recursion meets a *repeated* target.
   The ADD case in interact_grad ([src/interact/uop_grad.c:118-126])
   emits two independent uop_grad calls, both reaching the same x leaf;
   the leaf rule (`y == target`) returns expand_to_target(gy, x) for
   each branch.  Two CONST(1)-EXPAND chains alias to the SAME TenDesc
   via the degenerate-kernel-skip in materialize, so structurally
   one CONST is allocated; the outer UOP_ADD then reads it twice
   and sums = 2.  Expected gradient = ones-shaped-like-x * 2.

   Run:
       wolframscript -f wl/Examples/run-steps.wls grad-add-xx
   to render one heap-graph + IC-grid-diagram per interaction step
   into wl/Examples/grad-add-xx/steps/. *)
With[{x = TTensor[{3}]},
    TGrad[TUOpAdd[x, x], x]
]
