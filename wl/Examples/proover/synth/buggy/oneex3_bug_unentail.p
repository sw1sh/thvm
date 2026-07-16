% synth ProoVer proof (gold: buggy, unentail) -- family oneex3
fof(ax, axiom,
    ! [X] : ? [Y] : teaches(X, Y), file('synth',ax)).
fof(c, conjecture,
    ? [Y] : teaches(carol, Y), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [Y] : teaches(carol, Y)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : teaches(X, sK330(X)), inference(skolemize, [status(esa), new_symbols(skolem, [sK330])], [ax])).
fof(inst, plain,
    teaches(carol, sK330(carol)), inference(instantiate, [status(thm)], [sk1])).
fof(bogus_step, plain,
    unrelated_pred(bogus_c), inference(deduction, [status(thm)], [ax])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
