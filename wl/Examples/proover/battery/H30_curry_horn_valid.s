% Proof    : Problems/WID001+1.p
% VALID control: a first-order step over a 17-ary t whose clause set is
% Horn, where a refutation through the curried encoding confirms.
fof(ax, axiom, ![X] : (p(X) => q(t(X,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a))), file('Problems/WID001+1.p',ax)).
fof(pc, axiom, p(c), file('Problems/WID001+1.p',pc)).
fof(c0, conjecture, q(t(c,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a)), file('Problems/WID001+1.p',c0)).
fof(nc, negated_conjecture, ~q(t(c,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a)), inference(negated_conjecture, [status(cth)], [c0])).
fof(f1, plain, $false, inference(resolution, [status(thm)], [ax, pc, nc])).
