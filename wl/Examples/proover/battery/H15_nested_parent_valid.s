% Proof    : Problems/NST000+1.p
% VALID: s2 cites a nested (unnamed) inference.  A countermodel over
% the resolvable premises alone ([a2]) demonstrates nothing.
fof(a1, axiom, p(a), file('Problems/NST000+1.p',a1)).
fof(a2, axiom, p(a) => q(a), file('Problems/NST000+1.p',a2)).
fof(c, conjecture, q(a), file('Problems/NST000+1.p',c)).
fof(s1, negated_conjecture, ~q(a), inference(negated_conjecture, [status(cth)], [c])).
fof(s2, plain, q(a), inference(mp, [status(thm)], [a2, inference(copy, [status(thm)], [a1])])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, s2])).
