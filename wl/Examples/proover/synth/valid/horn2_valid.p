% synth ProoVer proof (gold: valid) -- family horn2
fof(ax1, axiom,
    bird(tweety), file('synth',ax1)).
fof(ax2, axiom,
    ! [X] : (bird(X) => flies(X)), file('synth',ax2)).
fof(c, conjecture,
    ? [X] : flies(X), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [X] : flies(X)), inference(negated_conjecture, [status(cth)], [c])).
fof(rule_c, plain,
    bird(tweety) => flies(tweety), inference(instantiate, [status(thm)], [ax2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ax1, rule_c])).
