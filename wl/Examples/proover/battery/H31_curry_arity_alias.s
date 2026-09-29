% Proof    : Problems/WID002+1.p
% INVALID: f at arity 17 and f at arity 20 are distinct symbols.  An
% encoding naming a curried head after f alone makes f/20's chain
% extend f/17's, and the equation on f/17 then closes the goal.
fof(ax, axiom, f(a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a) = f(b,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a), file('Problems/WID002+1.p',ax)).
fof(c0, conjecture, f(a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,d,d,d) = f(b,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,d,d,d), file('Problems/WID002+1.p',c0)).
fof(nc, negated_conjecture, f(a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,d,d,d) != f(b,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,d,d,d), inference(negated_conjecture, [status(cth)], [c0])).
fof(f1, plain, $false, inference(superposition, [status(thm)], [nc, ax])).
