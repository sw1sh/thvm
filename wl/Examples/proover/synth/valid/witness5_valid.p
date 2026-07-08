% synth ProoVer proof (gold: valid) -- family witness5
fof(ax, axiom,
    ! [X] : ? [Y] : ? [Z] : cites(Y, Z, X), file('synth',ax)).
fof(c, conjecture,
    ? [U] : ? [V] : ? [W] : cites(U, V, W), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [U] : ? [V] : ? [W] : cites(U, V, W)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : ? [Z] : cites(sK50(X), Z, X), inference(skolemize, [status(esa), new_symbols(skolem, [sK50])], [ax])).
fof(sk2, plain,
    ! [X] : cites(sK50(X), sK51(X), X), inference(skolemize, [status(esa), new_symbols(skolem, [sK51])], [sk1])).
fof(inst, plain,
    cites(sK50(d0), sK51(d0), d0), inference(instantiate, [status(thm)], [sk2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
