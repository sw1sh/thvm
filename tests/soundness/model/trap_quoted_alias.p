% Unsatisfiable: 'p' and p are the same atomic word (TPTP BNF); read as
% two symbols (an unquoting pass that renames 'p' apart from p), both
% records hold in one element.
fof(a1,axiom,'p').
fof(a2,axiom,~ p).
