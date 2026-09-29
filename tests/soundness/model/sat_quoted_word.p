% Satisfiable in one element: 'p' IS the atom p (TPTP BNF), so a model
% makes p true once and prints it as p.
fof(a1,axiom,'p').
fof(a2,axiom,(p | q)).
