% synth ProoVer proof (gold: buggy, unentail) -- family horn1
fof(ax1, axiom,
    prime(two), file('synth',ax1)).
fof(ax2, axiom,
    ! [X] : (prime(X) => positive(X)), file('synth',ax2)).
fof(c, conjecture,
    ? [X] : positive(X), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [X] : positive(X)), inference(negated_conjecture, [status(cth)], [c])).
fof(rule_c, plain,
    prime(two) => positive(two), inference(instantiate, [status(thm)], [ax2])).
fof(bogus_step, plain,
    unrelated_pred(bogus_c), inference(deduction, [status(thm)], [ax1])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ax1, rule_c])).
