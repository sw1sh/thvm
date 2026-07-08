% synth ProoVer proof (gold: valid) -- family trans0
fof(ax1, axiom,
    x1 = y1, file('synth',ax1)).
fof(ax2, axiom,
    y1 = z1, file('synth',ax2)).
fof(c, conjecture,
    x1 = z1, file('synth',c)).
fof(nc, negated_conjecture,
    x1 != z1, inference(negated_conjecture, [status(cth)], [c])).
fof(ded, plain,
    x1 = z1, inference(deduction, [status(thm)], [ax1, ax2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ded])).
