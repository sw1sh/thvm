% synth ProoVer proof (gold: valid) -- family trans3
fof(ax1, axiom,
    alpha = beta, file('synth',ax1)).
fof(ax2, axiom,
    beta = gamma, file('synth',ax2)).
fof(c, conjecture,
    alpha = gamma, file('synth',c)).
fof(nc, negated_conjecture,
    alpha != gamma, inference(negated_conjecture, [status(cth)], [c])).
fof(ded, plain,
    alpha = gamma, inference(deduction, [status(thm)], [ax1, ax2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ded])).
