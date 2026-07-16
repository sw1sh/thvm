% synth ProoVer proof (gold: valid) -- family witness1
fof(ax, axiom,
    ! [X] : ? [Y] : ? [Z] : connects(Y, Z, X), file('synth',ax)).
fof(c, conjecture,
    ? [U] : ? [V] : ? [W] : connects(U, V, W), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [U] : ? [V] : ? [W] : connects(U, V, W)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : ? [Z] : connects(sK10(X), Z, X), inference(skolemize, [status(esa), new_symbols(skolem, [sK10])], [ax])).
fof(sk2, plain,
    ! [X] : connects(sK10(X), sK11(X), X), inference(skolemize, [status(esa), new_symbols(skolem, [sK11])], [sk1])).
fof(inst, plain,
    connects(sK10(n0), sK11(n0), n0), inference(instantiate, [status(thm)], [sk2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
