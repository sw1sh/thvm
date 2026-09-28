% Proof    : Problems/CTH000+1.p
% A cth step negates an AXIOM (not the conjecture) and the "refutation"
% closes against the axiom itself: nothing about q(a) is proved.
fof(a1, axiom, p(a), file('Problems/CTH000+1.p',a1)).
fof(c, conjecture, q(a), file('Problems/CTH000+1.p',c)).
fof(x, plain, ~p(a), inference(assume_negation, [status(cth)], [a1])).
fof(f1, plain, $false, inference(resolution, [status(thm)], [a1, x])).
