% synth ProoVer proof (gold: buggy, neg) -- family oneex4
fof(ax, axiom,
    ! [X] : ? [Y] : visits(X, Y), file('synth',ax)).
fof(c, conjecture,
    ? [Y] : visits(dave, Y), file('synth',c)).
fof(nc, negated_conjecture,
    ? [Y] : ~(visits(dave, Y)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : visits(X, sK340(X)), inference(skolemize, [status(esa), new_symbols(skolem, [sK340])], [ax])).
fof(inst, plain,
    visits(dave, sK340(dave)), inference(instantiate, [status(thm)], [sk1])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
