% Unsatisfiable: 'p' and p are the same atomic word (TPTP BNF).  The
% unquoting pass renames 'p' apart from p.
fof(a1,axiom,'p').
fof(a2,axiom,~ p).
