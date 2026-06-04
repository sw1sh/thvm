---
Template: TechNote
Name: Disproof
Title: Disproving Conjectures with Countermodels
Context: THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Disproof
Keywords: [counterexample, countermodel, disproof, refutation, congruence closure, saturation, term rewriting, finite model, equational]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, SMT, Overview]
---

## Proving is only half the question

A theorem prover answers "does the goal follow from the axioms?" with *yes* and
a proof. The complementary answer is *no* and a witness: a structure where the
axioms all hold but the goal fails. That structure is a **countermodel**, and
producing one is **disproof** (or **refutation**). The two are dual: a goal is a
theorem exactly when no countermodel exists, so a complete reasoner runs both
searches and returns whichever finishes - a proof, or a counterexample.

THVMLink exposes both through the one entry point
[TFindProof](paclet:WolframInstitute/THVMLink/ref/TFindProof), selected by its
output-kind argument: `"ProofObject"` (the default) asks "is the goal
derivable?", and `"Counterexample"` asks "is the goal refutable?". The latter
returns a [CounterexampleObject](), the equational dual of `ProofObject`.
[TFindProof]() picks the engine by problem shape - congruence closure for a
ground (variable-free) goal, the saturating completion engine for a quantified
one - so a single call covers both.

```wl
Needs["THVMLink`ATP`"];
```

## The duality, already in the kernel

The Wolfram Language built-in [FindEquationalProof]() is itself a
prove-or-disprove engine. Ask it for a non-theorem - commutativity does not
follow from associativity alone - and instead of running forever it returns a
definitive `Failure`: the goal could not be reduced to True.

```wl
FindEquationalProof[
    ForAll[{x, y}, f[x, y] == f[y, x]],
    {ForAll[{x, y, z}, f[f[x, y], z] == f[x, f[y, z]]]}]
```

That verdict is itself a disproof. The only way to *know* a goal is underivable
is to exhibit a model where the axioms hold but the goal fails, and a finite
model finder (the Mace-style dual of a saturating prover) found one internally:
it searches finite algebras over a domain $\{1, \dots, n\}$ for one satisfying
`axioms` together with the negated goal. The same backend is exposed standalone
as the resource functions `FindEquationalCounterexample` (refute a goal,
returning a `CounterexampleObject`) and [FindFiniteModels]() (enumerate the
satisfying operation tables). THVMLink gives the same duality for its own
engines, and shapes its countermodel after these: a `CounterexampleObject` whose
model follows the `FindFiniteModels` structure.

## The CounterexampleObject

A [CounterexampleObject]() is the disproof artifact. Like a `ProofObject` it
renders as a summary box and answers a property interface: `co["Method"]` (the
engine that produced it), `co["Goal"]` and `co["Hypotheses"]`, `co["Model"]`
(the refuting model), `co["Counterexample"]` (the falsifying assignment),
`co["Domain"]`, and `co["NormalForms"]`. When the model is finite, `co["Model"]`
follows the [FindFiniteModels]() structure: an Association from each operator to
its Cayley table (a 0-indexed nested list) and each constant to its domain
element, over the domain $\{0, \dots, k-1\}$.

It is also **self-certifying**, mirroring the Wolfram Function Repository's
`FindEquationalCounterexample`. `co["FalsificationFunction"]` is a nullary
function that evaluates the goal in the model - a readable expression built
straight from the model's interpretation (the operator table is 1-indexed here
so `Part` lines up, versus the 0-indexed `co["Model"]`):

```wl
co = TFindProof[g[a] == a, {ForAll[{x}, g[g[x]] == x]}, "Counterexample"];
co["FalsificationFunction"]
```
<!-- => Function[{}, g[a] == a /. {g -> ({2, 1}[[##]]&), a -> 1}] -->

`co["VerificationFunction"]` does the same for the axioms, instantiated over the
finite domain:

```wl
co["VerificationFunction"]
```
<!-- => Function[{}, And @@ ({g[g[1]] == 1, g[g[2]] == 2} /. {g -> ({2, 1}[[##]]&), a -> 1})] -->

Calling them confirms the countermodel is real - the goal evaluates to `False`
there, the axioms to `True`:

```wl
{co["FalsificationFunction"][], co["VerificationFunction"][]}
```

## Ground disproof: a finite model from congruence closure

For a *ground* entailment - all terms variable-free - the engine is congruence
closure, the quantifier-free theory of equality with uninterpreted functions
(QF_UF) decided by [TSatEUF](paclet:WolframInstitute/THVMLink/ref/TSatEUF). It is
a complete decision procedure, and when the hypotheses together with the negated
goal are satisfiable the goal is *not* entailed - the quotient (one element per
equivalence class) is a finite refuting model.

Here `a == c` does not follow from `a == b` alone. `TFindProof` returns a
`CounterexampleObject` whose model merges `a` and `b` to element 0 and keeps `c`
apart as element 1:

```wl
TFindProof[a == c, {a == b}, "Counterexample"]
```

The model is a finite algebra in `FindFiniteModels` form, read off the property:

```wl
TFindProof[a == c, {a == b}, "Counterexample"]["Model"]
```

For the decision itself - does the entailment hold? - `Method -> "SMT"` returns
a verdict Association on a proved entailment (and the same
`CounterexampleObject` on a refuted one):

```wl
TFindProof[a == c, {a == b, b == c}, Method -> "SMT"]
```

The lazy DPLL(T) lift in
[TSmtDecide](paclet:WolframInstitute/THVMLink/ref/TSmtDecide) extends this to
Boolean combinations of equality atoms; a non-entailment there is refuted with
the certified satisfying assignment as the model:

```wl
TFindProof[Implies[a == b, a == c], {}, "Counterexample"]
```

## Quantified disproof: a finite model from saturation

Congruence closure cannot touch axioms with free variables, e.g.
`g[g[x]] == x`. Those are the province of the completion engine inside
[TFindProof](), an unfailing Knuth-Bendix completion that saturates critical
pairs into a convergent term-rewriting system (TRS): two ground terms are equal
in the theory exactly when they reduce to the same normal form.

So when a run **saturates** (`"Status" -> "Saturated"`) without proving the
goal, normalizing the goal's two sides decides it: distinct normal forms mean
the goal is *not* a consequence. Involution `g(g(x)) = x` is a one-rule
convergent system whose initial term algebra is finite - two elements, `a` and
`g(a)` - so `TFindProof` returns a `CounterexampleObject` with a 2-element
`FindFiniteModels`-style model:

```wl
TFindProof[g[a] == a, {ForAll[{x}, g[g[x]] == x]}, "Counterexample"]
```

The model `g -> {1, 0}` reads as $g(0) = 1, g(1) = 0$ with `a -> 0`, so
$g(a) = 1 \ne 0 = a$ - the goal fails. The separating normal forms are recorded
too:

```wl
TFindProof[g[a] == a, {ForAll[{x}, g[g[x]] == x]},
    "Counterexample"]["NormalForms"]
```

A genuine theorem has no countermodel - the spec is `$Failed` while the ordinary
proof still goes through:

```wl
TFindProof[g[g[g[g[a]]]] == a, {ForAll[{x}, g[g[x]] == x]}, "Counterexample"]
```

## Soundness: declining the unorientable case

Unfailing completion can saturate even when some equation is **unorientable** -
neither side is bigger in the reduction order, so it cannot become a terminating
rule. Commutativity `x * y == y * x` is the canonical case: the run saturates,
but the equation is stored with an arbitrary one-way orientation and is meant to
be applied only via *ordered* rewriting (in whichever direction is decreasing at
a given ground instance).

Naively rewriting with that one orientation would either loop or, worse, decide
a weaker theory and report an unsound "refutation". So the extractor is gated: it
acts only when *every* completed rule is strictly size-reducing under
substitution, which guarantees a genuine terminating TRS with no unorientable
equations. A commutative-saturated theory fails the gate, and the extractor
soundly **declines** (returns `$Failed`) rather than guess:

```wl
TFindProof[CircleTimes[a, b] == b,
    {ForAll[{x, y}, CircleTimes[x, y] == CircleTimes[y, x]]}, "Counterexample"]
```

This is conservative by design: it refutes cleanly-convergent equational
theories and steps back from associative-commutative ones, where the standalone
finite model finders (`FindEquationalCounterexample`, [FindFiniteModels]())
remain the tool of choice.

## Prove or disprove in one call

Because `"Counterexample"` rides on the same run as `"ProofObject"`, a single
call can ask for both outcomes. Request a list of specs and read whichever is
populated - here the goal is refuted, so `"Status"` is `"Saturated"` and the
`CounterexampleObject` is present:

```wl
TFindProof[g[a] == a, {ForAll[{x}, g[g[x]] == x]},
    {"Status", "Counterexample"}]
```

`All` returns every spec at once, including both `"ProofObject"` and
`"Counterexample"` - exactly one of which is non-`$Failed` for a decided goal.

## Which surface to use

| Goal shape | Engine [TFindProof]() picks | `co["Model"]` |
|---|---|---|
| Ground equality / disequality | congruence closure (complete) | finite algebra (`FindFiniteModels` tables) |
| Ground Boolean combination of atoms | DPLL(T) + congruence closure | satisfying truth assignment |
| Quantified, finite initial algebra | saturated completion | finite algebra (`FindFiniteModels` tables) |
| Quantified, infinite initial algebra | saturated completion | the convergent rules + normal forms |
| Quantified, associative-commutative | declines (`$Failed`) | use [FindFiniteModels]() / `FindEquationalCounterexample` |

The ground path is a sound and complete decision procedure: it always returns a
proof or a counterexample. The quantified `"Counterexample"` is a sound disproof
that fires whenever the completion saturates into a convergent rewrite system,
and declines where ordered rewriting would be needed.

## See also

- [SMT](paclet:WolframInstitute/THVMLink/tutorial/SMT) - the congruence-closure and DPLL(T) decision procedures behind the ground path (and the `Method -> "SMT"` decision surface).
- [ATP](paclet:WolframInstitute/THVMLink/tutorial/ATP) - the unfailing-completion prover whose saturated runs the `"Counterexample"` spec mines.
- [TFindProof](paclet:WolframInstitute/THVMLink/ref/TFindProof), [TSatEUF](paclet:WolframInstitute/THVMLink/ref/TSatEUF), [TSmtDecide](paclet:WolframInstitute/THVMLink/ref/TSmtDecide) - the symbol reference pages.
