% synth ProoVer proof (gold: buggy, neg) -- family oneex2
fof(ax, axiom,
    ! [X] : ? [Y] : near(X, Y), file('synth',ax)).
fof(c, conjecture,
    ? [Y] : near(site0, Y), file('synth',c)).
fof(nc, negated_conjecture,
    ? [Y] : ~(near(site0, Y)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : near(X, sK320(X)), inference(skolemize, [status(esa), new_symbols(skolem, [sK320])], [ax])).
fof(inst, plain,
    near(site0, sK320(site0)), inference(instantiate, [status(thm)], [sk1])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
