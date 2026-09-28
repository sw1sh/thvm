% Proof    : Problems/SHD000+1.p
% VALID: a negated conjecture over SHADOWED binders (inner ?[X] under
% outer ![X]); the negation drops the double negation and renames.
fof(a1, axiom, ~(![X]: ?[Y]: (p(X,Y) | ?[X]: q(X,Y))), file('Problems/SHD000+1.p',a1)).
fof(c, conjecture, ~(![X]: ?[Y]: (p(X,Y) | ?[X]: q(X,Y))), file('Problems/SHD000+1.p',c)).
fof(neg, negated_conjecture, ![A]: ?[B]: (p(A,B) | ?[E]: q(E,B)), inference(negated_conjecture, [status(cth)], [c])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [neg, a1])).
