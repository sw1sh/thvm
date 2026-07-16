# Synthesized ProoVer corpus

The public ProoVer sample set is only 7 proofs. This is a larger labelled
corpus, generated so the `FindProof --check` checker can be regression-tested
across every bug class at scale.

```
python3 generate.py            # writes valid/*.p, buggy/*.p, manifest.tsv here
python3 score.py               # runs FindProof --check and scores vs the labels
```

## How it is built

`generate.py` has a set of first-order scenario **families**, each of which
builds a genuinely-sound refutation proof in the exact ProoVer annotation schema
(axioms, then `negated_conjecture`, `skolemize`, `instantiate`, and a closing
`consequence`/`$false`):

| family    | shape |
| --------- | ----- |
| `witness` | `![x] ?[y] ?[z] rel(y,z,x)`, conjecture `?[u,v,w] rel(u,v,w)` (two Skolem steps) |
| `horn`    | `p(c)`, `![x] (p(x) => q(x))`, conjecture `?[x] q(x)` |
| `trans`   | `a=b`, `b=c`, conjecture `a=c` (equality, `deduction` step) |
| `oneex`   | `![x] ?[y] rel(x,y)`, conjecture `?[y] rel(c,y)` (one Skolem step) |

Each family is instantiated over several lexical themes for diversity. For every
valid proof, one buggy variant is emitted per applicable bug class, mutating
exactly one step:

| bug class   | mutation                                                        | mirrors    |
| ----------- | --------------------------------------------------------------- | ---------- |
| `neg`       | wrong quantifier dual in the `negated_conjecture`               | example1_e |
| `sk_absent` | declared Skolem symbol absent from the resulting formula        | example4_e |
| `sk_reuse`  | a `skolemize` step reuses a prior Skolem symbol (not fresh)     | example3_e |
| `unentail`  | an inserted `deduction` step draws a conclusion the parents do not entail | example2_e |

Gold labels are ground truth by construction: valid proofs are sound; each
mutant injects one genuine unsoundness. `manifest.tsv` records `file`, `gold`,
`bug_class`.

## Result

On the current 80-proof corpus (22 valid + 58 buggy), `FindProof --check`
scores the maximum and makes **zero catastrophic errors** (never rejects a valid
proof, never accepts a buggy one):

```
valid  -> VerifiedGood         22
buggy  -> VerifiedBad   58     (neg 18, sk_absent 12, sk_reuse 6, unentail 22)
ProoVer score: 138 (max 138)   catastrophic errors: 0
```

`score.py` exits non-zero if any catastrophic error appears, so it doubles as a
regression gate. These synthesized bugs are close to the template shape and are
easier than the hidden competition set will be; the corpus proves the checker
handles every bug class soundly, not that the hidden set will also score 100%.
