% Proof    : Problems/EQA000+1.p
% As H19, with the under-cited step a plain CNF equation.
cnf(a1, axiom, f(X) = g(X), file('Problems/EQA000+1.p',a1)).
cnf(a2, axiom, g(X) = h(X), file('Problems/EQA000+1.p',a2)).
cnf(c, negated_conjecture, f(a) != h(a), file('Problems/EQA000+1.p',c)).
cnf(s1, plain, f(X) = h(X), inference(superposition, [status(thm)], [a1])).
cnf(s2, plain, $false, inference(superposition, [status(thm)], [c, s1])).
