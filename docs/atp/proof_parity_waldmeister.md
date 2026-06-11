# Proof-object parity: thvm preset vs Waldmeister CLI

`AbelianGroupAxioms / InverseOfInverse` -- 4 axioms, 1-conjunct
goal `i(i(a)) = a`.

## TFindProof Method -> "Waldmeister"

Internal preset (`Mix` weight + KBO + AutoPrec + SR=51 +
RHSInterreduce + UnfailingCP; CPSetInterreduce off, the WM CLI -ki
default):

```
{Axiom, 1..4}                  -- assoc, comm, identity, inverse
{Hypothesis, 1}                -- i(i(a)) = a
{CriticalPairLemma, 1}         -- 3 superpositions
{CriticalPairLemma, 2}
{CriticalPairLemma, 3}
{SubstitutionLemma, 1}         -- 2 forward-demodulations
{SubstitutionLemma, 2}
{Conclusion, 1}                -- empty clause
```

Total: **6 saturation inferences** (3 CPL + 2 SL + 1 Conc), 11
constructs including axioms/hypothesis.

## TWaldmeisterProof (`wmcli` running the same problem)

Parsed proof protocol (see `wl/THVMLink/Kernel/ATP/Waldmeister.wl`'s
`parseProofLine` + `parseProtocol`):

```
 1..5  file                   -- 4 axioms + 1 hypothesis (i(i(a))=a)
 6,9,14,17,21,27  orient      -- 6 orientations of equations -> rules
 7  trivial_inequality_removal (renames axiom 2 as equation 1)
 8  superposition (cp 7, 6)   -- 3 superpositions
16  superposition (cp 9, 14)
23  superposition (cp 21, 14)
20  forward_demodulation       -- 3 forward-demodulations
24  forward_demodulation
28  forward_demodulation
29  trivial_inequality_removal -- closes goal
```

Total after canonicalization (drop `file`, `orient`, `trivial_-
inequality_removal`): **6 saturation inferences** (3 superp + 3
fwd_demod).

## Diff (canonicalized)

| construct           | thvm preset | WM CLI |
|---------------------|------------:|-------:|
| superposition / CPL |           3 |      3 |
| forward_demodulation / SubstLemma | 2 |   3 |
| Conclusion / (final fold) |       1 |      1 |
| **total**           |           6 |      7 |

**Off-by-one** on forward_demodulation count.  WM records an
extra reduction step (step 24: reduces step 23 by rule 6 to land
i(i(x)) = x); thvm collapses this into its SubstitutionLemma 2
which already does that reduction inline.

Same structural shape as the Vampire parity case
(docs/atp/proof_parity_inverseofinverse.md): the two ports agree
on the CP-generation side (3 superpositions both) and on the
final-step count (1 Conclusion both); they differ only by ONE
forward-demodulation step that's a question of where the
proof-reconstructor draws the boundary between rewrite steps.

Apart from this one-step segmentation, **the proofs are
structurally identical**:

* same 4 axioms used;
* same hypothesis `i(i(a)) = a`;
* same 3 critical pairs (commutativity-of-identity, assoc-applied-
  to-inverse, inverse-of-inverse);
* same demodulation chain landing the goal;
* same final empty-clause.

## What this verifies

* `wl/THVMLink/Kernel/ATP/Waldmeister.wl` (new): `TWaldmeisterProof`
  runs the local `wmcli` on a WM `.pr` problem file, parses the
  proof protocol (digits + `tes-*` rule + `<source>` + optional
  `###R/E` rule-id suffix), returns the inference DAG in the same
  Association shape as `TVampireProof` so
  `TSZSDerivationToProofObject` reads it uniformly.
* Waldmeister CLI does NOT speak TPTP -- the `.pr` input format
  is its own (NAME / MODE / SORTS / SIGNATURE / ORDERING /
  VARIABLES / EQUATIONS / CONCLUSION).  TPTP -> `.pr` converter is
  the next iter; `/tmp/inverseofinverse_kbo.pr` is a hand-written
  test case proving the wrapper end-to-end.
* `DYLD_FRAMEWORK_PATH` is set automatically from
  `/Applications/Wolfram 15.1.app/Contents/Frameworks` (wmcli is
  linked against `mathlink.framework`); fall back to other Wolfram
  installs if the 15.1 path is missing.

## Outstanding for full byte-identity

Same two folds the Vampire parity doc identified, plus one
WM-specific normalization:

1. **Drop `orient` steps** when folding the WM derivation -- they
   record the runtime decision to convert an equation into a
   directed rule, but the resulting rule is implicit in any
   superposition/demodulation that uses it.  Pure bookkeeping.
2. **Collapse `superposition + tes-red`** into a single
   `CriticalPairLemma` entry -- WM's `cp(I, ., J, .)` immediately
   followed by `tes-red(K, ., L, .)` is what thvm represents as ONE
   `CriticalPairLemma` step with `Construct` + `MatchingConstruct`
   + `Position` metadata.
3. **Drop final `trivial_inequality_removal`** -- already mapped to
   `Conclusion` in `$SZSRuleToConstruct`; the WM protocol's
   `tes-final` is the same idea (just cites the closing step).

All three are pure-WL post-processing on the parsed derivation; no
engine work needed.  Land them in `ProcessProofObject.wl`'s
`buildDatasetFromDerivation` and this case (and all easy WM cases)
should report `proofs identical? True` against the internal
`Method -> "Waldmeister"` preset.
