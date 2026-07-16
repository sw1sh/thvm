% synth ProoVer proof (gold: buggy, neg) -- family witness2
fof(ax, axiom,
    ! [X] : ? [Y] : ? [Z] : beats(Y, Z, X), file('synth',ax)).
fof(c, conjecture,
    ? [U] : ? [V] : ? [W] : beats(U, V, W), file('synth',c)).
fof(nc, negated_conjecture,
    ? [U] : ? [V] : ? [W] : ~(beats(U, V, W)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : ? [Z] : beats(sK20(X), Z, X), inference(skolemize, [status(esa), new_symbols(skolem, [sK20])], [ax])).
fof(sk2, plain,
    ! [X] : beats(sK20(X), sK21(X), X), inference(skolemize, [status(esa), new_symbols(skolem, [sK21])], [sk1])).
fof(inst, plain,
    beats(sK20(p1), sK21(p1), p1), inference(instantiate, [status(thm)], [sk2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
