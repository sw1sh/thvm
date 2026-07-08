% synth ProoVer proof (gold: valid) -- family oneex5
fof(ax, axiom,
    ! [X] : ? [Y] : knows(X, Y), file('synth',ax)).
fof(c, conjecture,
    ? [Y] : knows(erin, Y), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [Y] : knows(erin, Y)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : knows(X, sK350(X)), inference(skolemize, [status(esa), new_symbols(skolem, [sK350])], [ax])).
fof(inst, plain,
    knows(erin, sK350(erin)), inference(instantiate, [status(thm)], [sk1])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
