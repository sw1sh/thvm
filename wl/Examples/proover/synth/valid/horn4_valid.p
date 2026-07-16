% synth ProoVer proof (gold: valid) -- family horn4
fof(ax1, axiom,
    even(four), file('synth',ax1)).
fof(ax2, axiom,
    ! [X] : (even(X) => integer(X)), file('synth',ax2)).
fof(c, conjecture,
    ? [X] : integer(X), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [X] : integer(X)), inference(negated_conjecture, [status(cth)], [c])).
fof(rule_c, plain,
    even(four) => integer(four), inference(instantiate, [status(thm)], [ax2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ax1, rule_c])).
