% Proof    : Problems/CAP000+1.p
% instantiate whose matrix binder captures a pattern variable:
% ![X]:?[Y]:r(X,Y) does NOT entail ?[Y]:r(Y,Y).
fof(a1, axiom, ![X]: ?[Y]: r(X,Y), file('Problems/CAP000+1.p',a1)).
fof(c, conjecture, ?[Y]: r(Y,Y), file('Problems/CAP000+1.p',c)).
fof(s1, negated_conjecture, ~ ?[Y]: r(Y,Y), inference(negated_conjecture, [status(cth)], [c])).
fof(s2, plain, ?[Y]: r(Y,Y), inference(instantiate, [status(thm)], [a1])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, s2])).
