% synth ProoVer proof (gold: buggy, neg) -- family horn0
fof(ax1, axiom,
    human(socrates), file('synth',ax1)).
fof(ax2, axiom,
    ! [X] : (human(X) => mortal(X)), file('synth',ax2)).
fof(c, conjecture,
    ? [X] : mortal(X), file('synth',c)).
fof(nc, negated_conjecture,
    ? [X] : ~(mortal(X)), inference(negated_conjecture, [status(cth)], [c])).
fof(rule_c, plain,
    human(socrates) => mortal(socrates), inference(instantiate, [status(thm)], [ax2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ax1, rule_c])).
