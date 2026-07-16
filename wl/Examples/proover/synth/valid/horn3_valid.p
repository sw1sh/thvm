% synth ProoVer proof (gold: valid) -- family horn3
fof(ax1, axiom,
    metal(iron), file('synth',ax1)).
fof(ax2, axiom,
    ! [X] : (metal(X) => conducts(X)), file('synth',ax2)).
fof(c, conjecture,
    ? [X] : conducts(X), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [X] : conducts(X)), inference(negated_conjecture, [status(cth)], [c])).
fof(rule_c, plain,
    metal(iron) => conducts(iron), inference(instantiate, [status(thm)], [ax2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ax1, rule_c])).
