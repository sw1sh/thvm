% Proof    : Problems/INC000+1.p
fof(a9, axiom, ![X]: q(X), file('Problems/INC000+1.p',a9)).
fof(c, conjecture, q(a), file('Problems/INC000+1.p',c)).
fof(s1, negated_conjecture, ~q(a), inference(negated_conjecture, [status(cth)], [c])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, a9])).
