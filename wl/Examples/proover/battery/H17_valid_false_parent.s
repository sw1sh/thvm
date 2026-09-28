% Proof    : Problems/TRU000+1.p
% VALID: the closure cites a derived $false (inconsistent axioms) and a
% premise-free tautology; $false/$true are logical constants.
fof(a1, axiom, p(a), file('Problems/TRU000+1.p',a1)).
fof(a2, axiom, ~p(a), file('Problems/TRU000+1.p',a2)).
fof(c, conjecture, q(b), file('Problems/TRU000+1.p',c)).
fof(t, plain, r(b) | ~r(b), inference(tautology, [status(thm)], [])).
fof(bot, plain, $false, inference(consequence, [status(thm)], [a1, a2])).
fof(negc, negated_conjecture, ~q(b), inference(negated_conjecture, [status(cth)], [c])).
fof(bot2, plain, $false, inference(consequence, [status(thm)], [negc, bot, t])).
