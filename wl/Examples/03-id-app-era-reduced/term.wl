(* Reduce the (id ERA) APP and return the original APP term so the
   graph still shows the now-orphan APP triangle alongside the
   substituted body cell. *)
With[{app = TApp[TLam[var |-> var], TEra[]]},
    TWnf[app];
    app
]
