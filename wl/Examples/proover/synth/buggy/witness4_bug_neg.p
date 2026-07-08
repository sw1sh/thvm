% synth ProoVer proof (gold: buggy, neg) -- family witness4
fof(ax, axiom,
    ! [X] : ? [Y] : ? [Z] : sends(Y, Z, X), file('synth',ax)).
fof(c, conjecture,
    ? [U] : ? [V] : ? [W] : sends(U, V, W), file('synth',c)).
fof(nc, negated_conjecture,
    ? [U] : ? [V] : ? [W] : ~(sends(U, V, W)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : ? [Z] : sends(sK40(X), Z, X), inference(skolemize, [status(esa), new_symbols(skolem, [sK40])], [ax])).
fof(sk2, plain,
    ! [X] : sends(sK40(X), sK41(X), X), inference(skolemize, [status(esa), new_symbols(skolem, [sK41])], [sk1])).
fof(inst, plain,
    sends(sK40(m0), sK41(m0), m0), inference(instantiate, [status(thm)], [sk2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
