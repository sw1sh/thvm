% Proof    : Problems/INC002+1.p
fof(a1, axiom, p(a), file('Problems/INC002+1.p',a1)).
fof(a2, axiom, ![X]: (p(X) => q(X)), file('Problems/INC002+1.p',a2)).
fof(c, conjecture, q(a), file('Problems/INC002+1.p',c)).
fof(s1, negated_conjecture, ~q(a), inference(negated_conjecture, [status(cth)], [c])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, a1, a2])).
