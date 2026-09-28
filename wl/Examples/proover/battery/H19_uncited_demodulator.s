% Proof    : Problems/EQA000+1.p
% s1 is TRUE (a1 then a2) but cites only a1: a step must follow from
% ITS premises -- no uncited record may serve as a demodulator.
cnf(a1, axiom, f(X) = g(X), file('Problems/EQA000+1.p',a1)).
cnf(a2, axiom, g(X) = h(X), file('Problems/EQA000+1.p',a2)).
cnf(c, negated_conjecture, f(a) != h(a), file('Problems/EQA000+1.p',c)).
fof(s1, plain, ![X]: f(X) = h(X), inference(superposition, [status(thm)], [a1])).
fof(s2, plain, $false, inference(superposition, [status(thm)], [c, s1])).
