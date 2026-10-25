% A selective include: only a1 belongs to the problem.  Without a2
% (g(X) = a), {f(X) = X, g(b) != g(c)} is satisfiable, so a refutation
% means the prover read the whole axiom file.
include('Axioms/edge_include_select.ax', [a1]).
cnf(goal, negated_conjecture, g(b) != g(c)).
