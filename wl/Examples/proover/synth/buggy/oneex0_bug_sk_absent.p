% synth ProoVer proof (gold: buggy, sk_absent) -- family oneex0
fof(ax, axiom,
    ! [X] : ? [Y] : likes(X, Y), file('synth',ax)).
fof(c, conjecture,
    ? [Y] : likes(alice, Y), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [Y] : likes(alice, Y)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : likes(X, sK300(X)), inference(skolemize, [status(esa), new_symbols(skolem, [sKmissing])], [ax])).
fof(inst, plain,
    likes(alice, sK300(alice)), inference(instantiate, [status(thm)], [sk1])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
