# 6. Classical foundations: the GOFAI floor

(GOFAI = good old-fashioned AI, the classical symbolic tradition. AI =
artificial intelligence. PSSH = physical symbol system hypothesis. GPS =
General Problem Solver. KRR = knowledge representation and reasoning. KR =
knowledge representation. FOL = first-order logic. DL = description logic.
RDF = Resource Description Framework. OWL = Web Ontology Language. W3C =
World Wide Web Consortium. SLD = selective linear definite-clause
resolution. ASP = answer set programming. CLP = constraint logic
programming. NP = nondeterministic polynomial time. LLM = large language
model. ARC = Abstraction and Reasoning Corpus. Abbreviations are
page-local; the directory cheat sheet is in [README.md](README.md).)

This doc set opens with the symbolist/connectionist divide
([01-the-divide.md](01-the-divide.md)) and jumps straight to modern
neuro-symbolic systems ([02-integration.md](02-integration.md)) and rule
induction ([05-reasoning-and-rule-induction.md](05-reasoning-and-rule-induction.md)),
assuming a GOFAI floor it never lays. This page lays it: the founding
hypothesis, the systems that made and broke the field's first reputation,
the architectures and representations that survived, and the
compositionality argument that is the actual intellectual reason "symbols
matter."

## The Physical Symbol System Hypothesis

The founding claim of symbolic AI is Allen Newell and Herbert Simon's
**physical symbol system hypothesis**: "A physical symbol system has the
necessary and sufficient means for general intelligent action." It and its
companion **heuristic search hypothesis** ("a physical symbol system
exercises its intelligence in problem solving by search") are stated in
their 1975 ACM Turing Award lecture, *Computer Science as Empirical
Inquiry: Symbols and Search* (Communications of the ACM 19(3):113-126,
March 1976,
[doi:10.1145/360018.360022](https://dl.acm.org/doi/10.1145/360018.360022)).
A symbol system is just a machine that builds, copies, and rearranges
symbol structures over time; the hypothesis says that is *all you need* for
intelligence, and the only mystery left is the search through symbol-structure
space.

What aged well: the framing of intelligence as search over structured
representations is still exactly right for the program-synthesis and
theorem-proving systems that win hard reasoning today
([02-integration.md](02-integration.md)). What aged badly: "necessary and
sufficient" was too strong in both directions. The connectionist
counter-claim, that intelligence can be carried by sub-symbolic distributed
representations with no explicit symbols, is the entire other tradition; and
the systematicity debate below is the sharpest form of the surviving
argument over whether the connectionist side can deliver general
intelligence *without* implementing a symbol system after all. Note Newell
himself never abandoned the symbolist program: his last work was the
cognitive architecture SOAR (below) as a "unified theory of cognition."

## GOFAI history in brief: from GPS to the knowledge-acquisition bottleneck

The **General Problem Solver** (Newell, J. C. Shaw, Simon; demonstrated
1959, fully exposited as "GPS, a Program that Simulates Human Thought" in
Feigenbaum and Feldman's *Computers and Thought*, 1963;
[history](https://en.wikipedia.org/wiki/General_Problem_Solver)) was the
first program meant to be a *general* reasoner. Its engine was **means-ends
analysis**: measure the difference between current and goal state, pick an
operator that reduces that difference, recurse. It solved toy problems
(logic puzzles, the Tower of Hanoi) and modeled human protocols, but it was
general only over problems you could hand-encode into its difference tables.
The generality was in the search method, not the knowledge.

The lesson the field drew in the 1970s was the opposite: **knowledge, not
search, is the bottleneck.** This produced the **expert systems** era.
**DENDRAL** (Edward Feigenbaum, Joshua Lederberg, Carl Djerassi at Stanford,
from 1965;
[Britannica](https://www.britannica.com/technology/DENDRAL)) inferred
molecular structure from mass spectrometry data and is generally called the
first expert system. **MYCIN** (Edward Shortliffe's dissertation under Bruce
Buchanan, Stanford, early 1970s; written in Lisp, published as Buchanan and
Shortliffe, *Rule-Based Expert Systems*, 1984;
[overview](https://en.wikipedia.org/wiki/Mycin)) diagnosed bloodstream
infections and recommended antibiotics by **backward chaining** over a few
hundred IF-THEN rules with certainty factors. MYCIN reportedly matched
specialists on its narrow task, yet was never deployed clinically
(liability and integration, not accuracy).

The era ended in the **knowledge-acquisition bottleneck**: every expert
system needed its rules elicited by hand from human experts, one brittle
rule at a time, and the rules did not transfer between domains or degrade
gracefully off-distribution. That brittleness, plus overpromising, drove the
funding collapses now called the **AI winters**. The honest takeaway for
this repo: hand-authored symbolic rules are the thing modern systems try to
*learn* rather than elicit. That is precisely the move from MYCIN's
hand-coded rules to DreamCoder-style learned program libraries
([02-integration.md](02-integration.md)) and inductive logic programming
([05-reasoning-and-rule-induction.md](05-reasoning-and-rule-induction.md)).

## Cognitive architectures: SOAR and ACT-R

If GPS was a problem-solver, the **cognitive architectures** were attempts
to specify a fixed machine that could do *all* of cognition, with task
knowledge added as content.

- **SOAR** (John Laird, Allen Newell, Paul Rosenbloom; "SOAR: An
  Architecture for General Intelligence," Artificial Intelligence
  33(1):1-64, 1987;
  [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/0004370287900506))
  represents all long-term knowledge as **production rules** and all
  problem-solving as search in **problem spaces**. When it hits an impasse,
  it sets up a subgoal, solves it, and **chunks** the result into a new rule
  -- a single, uniform learning mechanism. Newell built SOAR as the concrete
  carrier of his *Unified Theories of Cognition* (1990) program. It is still
  developed (John Laird's group, Michigan;
  [intro, arXiv:2205.03854](https://arxiv.org/abs/2205.03854)).

- **ACT-R** (John Anderson, Carnegie Mellon; "An Integrated Theory of the
  Mind," Anderson, Bothell, Byrne, Douglass, Lebiere, Qin, Psychological
  Review 111(4):1036-1060, 2004;
  [PubMed](https://pubmed.ncbi.nlm.nih.gov/15482072/); book *How Can the
  Human Mind Occur in the Physical Universe?*, Oxford, 2007) is a modular
  production system: a procedural module of rules arbitrated by
  **subsymbolic** utilities, plus a declarative memory of chunks with
  activation dynamics, with modules mapped onto brain regions and validated
  against reaction times and fMRI. It is the dominant architecture in
  cognitive psychology.

Both are symbolic agent architectures with subsymbolic control statistics,
which makes them the original neuro-symbolic systems in spirit. Their
relevance now is that the ARC-AGI-3 agent loop in this repo's `brain/` arc
([04-implications-for-thvm.md](04-implications-for-thvm.md)) is structurally
a production-system agent: perceive a frame, select an action, learn from
the outcome. SOAR's impasse-then-chunk and ACT-R's rule-utility learning are
the classical reference designs for "an agent that improves its own rule set
online," which is exactly the missing rule-induction capability that arc
diagnosed.

## Knowledge representation and reasoning (KRR)

The KRR program is about how to *write down* what an agent knows so a
reasoner can use it.

- **Semantic networks** (M. Ross Quillian, PhD thesis 1968, in Minsky's
  *Semantic Information Processing*;
  [history](https://cse.buffalo.edu/~rapaport/semantic-networks.html))
  model concepts as nodes and relations as labeled edges, with inheritance
  along is-a links. They are the direct ancestor of today's knowledge
  graphs.
- **Frames** (Marvin Minsky, "A Framework for Representing Knowledge,"
  MIT-AI Lab Memo 306, June 1974;
  [PDF](https://courses.media.mit.edu/2004spring/mas966/Minsky%201974%20Framework%20for%20knowledge.pdf))
  package a stereotyped situation as a record of slots with default values
  and procedural attachments -- the object-with-defaults idea that became
  object-oriented and ontological modeling.
- **Description logics** are the formalization that made this rigorous:
  decidable fragments of first-order logic with concepts, roles, and
  reasoners that compute subsumption and consistency. They are the
  mathematical core of the **Semantic Web** vision (Tim Berners-Lee, James
  Hendler, Ora Lassila, "The Semantic Web," Scientific American 284(5),
  May 2001;
  [PDF](https://www-sop.inria.fr/acacia/cours/essi2006/Scientific%20American_%20Feature%20Article_%20The%20Semantic%20Web_%20May%202001.pdf)):
  data as triples in **RDF** (subject-predicate-object), schemas and
  ontologies in **OWL**, a description-logic-based standard that became a
  W3C Recommendation on 10 February 2004
  ([W3C](https://www.w3.org/TR/owl-features/)).

Honest verdict: the grand Semantic Web of universally interoperable machine
ontologies never arrived; hand-curating consistent global ontologies is the
knowledge-acquisition bottleneck again. What did arrive is the **knowledge
graph**: large, messy, mostly-RDF-shaped entity-relation stores
(Google's, Wikidata, enterprise graphs) used pragmatically for retrieval and
grounding. The live form of KRR today is knowledge graphs as a retrieval and
verification substrate around LLMs, not OWL reasoners proving theorems about
the world.

## Logic programming: Prolog, Datalog, ASP, CLP

Logic programming is the idea that a program *is* a logical theory and
running it *is* proof search.

- **Prolog** (Alain Colmerauer and Philippe Roussel, Marseille, 1972, with
  Robert Kowalski's procedural reading of Horn clauses;
  [Kowalski history PDF](https://www.doc.ic.ac.uk/~rak/papers/History.pdf))
  computes by **SLD resolution** -- linear resolution for definite clauses
  with a selection function, the inference rule named by Maarten van Emden
  ([SLD resolution](https://en.wikipedia.org/wiki/SLD_resolution)) -- with
  backtracking and unification. You state facts and rules; the engine
  searches for proofs.
- **Datalog** is Prolog's function-symbol-free, guaranteed-terminating core,
  developed in the 1980s at the database/logic-programming boundary
  ([overview](https://en.wikipedia.org/wiki/Datalog)). Its contribution is
  **recursive queries** over relations (transitive closure, reachability)
  that relational algebra cannot express; it is having a genuine revival as
  the query layer for static analysis, networking, and graph databases. This
  repo's `02-integration.md` already touches differentiable Datalog
  (Scallop).
- **Answer set programming** moves from "find a proof" to "find a model."
  Its semantics is the **stable model** of Michael Gelfond and Vladimir
  Lifschitz ("The Stable Model Semantics for Logic Programming," ICLP 1988,
  pp. 1070-1080;
  [dblp](https://dblp.org/rec/conf/iclp/GelfondL88.html)), which handles
  negation-as-failure soundly. You encode an NP-hard search or combinatorial
  problem as a logic program; the **answer sets** are the solutions. The
  standard toolchain is **clingo** from the Potassco collection (grounder
  gringo + solver clasp; [Potassco](https://potassco.org/)). ASP is the live,
  practical descendant of classical logic programming for combinatorial
  search and planning.
- **Constraint logic programming** generalizes unification to constraint
  solving over a domain (reals, finite domains), letting the engine call a
  constraint solver instead of pure resolution -- the bridge from logic
  programming to modern constraint and SAT/SMT solving.

For this repo's ARC work, ASP and CLP are the credible classical tools for
the *symbolic-verify* half of neural-proposes/symbolic-verifies: a grid
transformation rule, once induced, is naturally checked or even searched as
a constraint or stable-model problem.

## The systematicity / compositionality debate (the actual reason symbols matter)

This is the intellectual core. The argument is not "symbols are nicer";
it is a specific empirical claim about generalization.

**Fodor and Pylyshyn** ("Connectionism and Cognitive Architecture: A
Critical Analysis," Cognition 28:3-71, 1988;
[PDF](https://ruccs.rutgers.edu/images/personal-zenon-pylyshyn/proseminars/Proseminar13/ConnectionistArchitecture.pdf))
argued that thought is **systematic** and **compositional**: anyone who can
think "John loves Mary" can think "Mary loves John," because the capacities
share constituent structure. A classical symbol system gets this for free
(the constituents are literally there to be recombined); a connectionist
net, they argued, can only exhibit systematicity by *implementing* a symbol
system, in which case it is an implementation detail, not an alternative
theory. This is the canonical "why symbols matter" argument and it has
never been cleanly refuted; it has been turned into experiments.

The modern empirical version replaces philosophy with benchmarks:

- **SCAN** (Brenden Lake, Marco Baroni, "Generalization without
  Systematicity," ICML 2018, arXiv:1711.00350;
  [arXiv](https://arxiv.org/abs/1711.00350)): map compositional navigation
  commands ("jump twice") to action sequences. Sequence-to-sequence RNNs
  generalize when train/test overlap is high and **fail catastrophically**
  when a held-out command requires recombining known primitives (e.g.
  generalizing "jump" after seeing it only in isolation). Fodor and
  Pylyshyn's prediction, measured.
- **COGS** (Najoung Kim, Tal Linzen, EMNLP 2020, arXiv:2010.05465;
  [arXiv](https://arxiv.org/abs/2010.05465)): a semantic-parsing benchmark
  with systematic structural gaps (new combinations of familiar words and
  syntax). Same failure mode in a language-understanding setting.
- **gSCAN** (Laura Ruis, Jacob Andreas, Marco Baroni, Diane Bouchacourt,
  Brenden Lake, "A Benchmark for Systematic Generalization in Grounded
  Language Understanding," NeurIPS 2020;
  [proceedings](https://papers.nips.cc/paper/2020/hash/e5a90182cc81e12ab5e72d66e0b46fe3-Abstract.html))
  grounds SCAN in a grid world, so generalization must combine language with
  perception and navigation -- much closer to the ARC-AGI-3 setting this
  repo targets, and still hard.
- **Partial resolution** (Brenden Lake, Marco Baroni, "Human-like
  systematic generalization through a meta-learning neural network," Nature
  623:115-121, 2023;
  [Nature](https://www.nature.com/articles/s41586-023-06668-3)). Their
  **meta-learning for compositionality** trains a standard transformer over
  a stream of *episodes*, each with its own freshly-permuted
  word-to-meaning mapping, so the optimization target is the compositional
  skill itself rather than any fixed vocabulary. It matches human
  systematic generalization on SCAN-style tasks and on behavioral
  comparisons with people. This is the strongest evidence to date that a
  pure neural net can be *coaxed* into Fodor-Pylyshyn systematicity, though
  by paying for it with a curriculum that bakes compositional structure into
  training, which is arguably handing the network the symbolic prior rather
  than refuting the need for one.

The standing position: classical architectures get systematicity by
construction; neural nets do not get it by default and fail the diagnostic
benchmarks; with the right meta-training they can approximate it. For this
repo, that is the precise reason the stuck ARC-AGI-3 games resist a plain
convolutional world model
([04-implications-for-thvm.md](04-implications-for-thvm.md)): the games are
governed by a *systematic, recombinable* rule, and a pattern-fitter that
never represents the rule cannot recombine it.

## What is still live, what is history

Live: the search-over-structured-representations framing of the PSSH;
production-system *agent* designs (SOAR, ACT-R) as references for online
rule learning; knowledge graphs (the surviving, pragmatic form of KRR);
Datalog's recursion revival; answer set programming and constraint solving
as practical combinatorial reasoners and verifiers; and above all the
systematicity argument, which is the empirical reason this repo's neural
world model stalls and the conceptual case for adding rule induction. Mostly
history: hand-elicited expert-system rule bases and the certainty-factor
machinery (the knowledge-acquisition bottleneck that triggered the AI
winters), and the maximalist Semantic Web of universal hand-built
ontologies. The throughline this set keeps returning to is the same one
GOFAI learned the hard way: do not hand-author the rules, *learn to induce
them*, and keep a sound symbolic engine to check them.
