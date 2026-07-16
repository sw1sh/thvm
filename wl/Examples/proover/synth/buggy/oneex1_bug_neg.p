% synth ProoVer proof (gold: buggy, neg) -- family oneex1
fof(ax, axiom,
    ! [X] : ? [Y] : owns(X, Y), file('synth',ax)).
fof(c, conjecture,
    ? [Y] : owns(bob, Y), file('synth',c)).
fof(nc, negated_conjecture,
    ? [Y] : ~(owns(bob, Y)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : owns(X, sK310(X)), inference(skolemize, [status(esa), new_symbols(skolem, [sK310])], [ax])).
fof(inst, plain,
    owns(bob, sK310(bob)), inference(instantiate, [status(thm)], [sk1])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
