% synth ProoVer proof (gold: buggy, sk_reuse) -- family witness3
fof(ax, axiom,
    ! [X] : ? [Y] : ? [Z] : adj(Y, Z, X), file('synth',ax)).
fof(c, conjecture,
    ? [U] : ? [V] : ? [W] : adj(U, V, W), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [U] : ? [V] : ? [W] : adj(U, V, W)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : ? [Z] : adj(sK30(X), Z, X), inference(skolemize, [status(esa), new_symbols(skolem, [sK30])], [ax])).
fof(sk2, plain,
    ! [X] : adj(sK30(X), sK30(X), X), inference(skolemize, [status(esa), new_symbols(skolem, [sK30])], [sk1])).
fof(inst, plain,
    adj(sK30(v0), sK31(v0), v0), inference(instantiate, [status(thm)], [sk2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
