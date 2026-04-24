(* uop-add: plain elementwise tensor addition.  Simplest binary
   compute UOP -- no grad rewrite, no movement.  The IC diagram
   shows TEN#1 and TEN#2 as cyan apex-down leaves feeding the
   ADD@0 compute triangle. *)
With[{a = TTensor[{3}], b = TTensor[{3}]},
    TUOpAdd[a, b]
]
