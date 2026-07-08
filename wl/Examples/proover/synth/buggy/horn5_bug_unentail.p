% synth ProoVer proof (gold: buggy, unentail) -- family horn5
fof(ax1, axiom,
    dog(rex), file('synth',ax1)).
fof(ax2, axiom,
    ! [X] : (dog(X) => animal(X)), file('synth',ax2)).
fof(c, conjecture,
    ? [X] : animal(X), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [X] : animal(X)), inference(negated_conjecture, [status(cth)], [c])).
fof(rule_c, plain,
    dog(rex) => animal(rex), inference(instantiate, [status(thm)], [ax2])).
fof(bogus_step, plain,
    unrelated_pred(bogus_c), inference(deduction, [status(thm)], [ax1])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ax1, rule_c])).
