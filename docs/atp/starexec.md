# FindProof on StarExec: build and run, by hand

How to compile and run the FindProof submission the way the competition
infrastructure (StarExec) does, and how to reproduce every step manually so you
can test before uploading. Two tarballs, one code base: FindProof.tgz (CASC proving) and CheckProof.tgz (ProoVer checking; binary CheckProof, built by tools/proover/mk_tarball.sh with the same layout). This doc walks the FindProof one; the CheckProof tarball is identical in shape with bin/CheckProof and a single checking configuration:

- CASC (CADE ATP System Competition) unit-equality proving
- ProoVer first-order proof checking

Last verified 2026-07-08 on Ubuntu 22.04 / gcc 11.4; re-verified 2026-07-12 on
macOS (tarball -> extract -> `starexec_build` -> `starexec_run_default` proves
LCL192-10 and LAT159-1 at a 30 s wall). The direct-binary command paths below
were re-verified 2026-07-16 (LAT171-1 proves + `--check` returns VerifiedGood).

---

## 1. What StarExec expects

StarExec is the shared execution service the competitions run on. A "solver" is
an archive (`.tar.gz`, `.tgz`, `.tar`, or `.zip`) with a fixed shape:

- A directory named `bin/` somewhere inside. The directory that CONTAINS `bin/`
  is treated as the solver root.
- Inside `bin/`, every executable file whose name begins with `starexec_run_`
  is a CONFIGURATION. The suffix is the configuration name (`default`,
  `proover`, ...). StarExec invokes it as

      starexec_run_<config>  <absolute-path-to-benchmark>

  and captures the script's standard output.
- Optionally, a script named `starexec_build` at the solver root. If the solver
  is uploaded as "source" (build on upload), StarExec runs `starexec_build` once
  on a build node, with the solver root as the working directory, then
  re-packages the built tree for the run nodes.

Resource limits are passed to each run as environment variables:

| variable                   | meaning                                    |
| -------------------------- | ------------------------------------------ |
| `STAREXEC_WALLCLOCK_LIMIT` | wall-clock limit for this pairing, seconds |
| `STAREXEC_CPU_LIMIT`       | CPU-time limit, seconds                    |
| `STAREXEC_MAX_MEM`         | memory limit (confirm the unit on the live service; historically megabytes) |

The result is classified by a post-processor that reads the SZS (Solution Status
Scheme) status line from standard output, so the run script must print exactly
one `% SZS status ...` line.

---

## 2. Anatomy of the FindProof tarball

`tools/casc/mk_tarball.sh` assembles `FindProof.tgz`. Unpacked:

```
FindProof/
  starexec_build                 # build script (compiles bin/FindProof)
  Makefile                       # the actual build rule
  README                         # short usage note (both roles)
  bin/
    starexec_run_default         # CONFIG "default": CASC UEQ proving
    FindProof                    # the compiled binary (created by the build)
  tools/
    thvm_atp_cli.c               # the CLI translation unit (the whole build)
    proof_check.c                # included by thvm_atp_cli.c (--check mode)
  tests/
    test_atp_wolfram_bench.c     # included by thvm_atp_cli.c (the engine)
  src/                           # the thvm ATP engine sources (unity build)
```

The whole engine compiles as ONE translation unit: `tools/thvm_atp_cli.c`
`#include`s `tests/test_atp_wolfram_bench.c`, which `#include`s `src/thvm.c`,
which pulls in the rest of `src/`. There are no object files and no link step
beyond the one compiler invocation. The only external dependency is the C
standard library and the math library (`-lm`); on macOS the Makefile links
Apple's Accelerate framework instead, but on StarExec (Linux) it is `-lm` only.

The Makefile rule:

```make
bin/FindProof: tools/thvm_atp_cli.c tools/proof_check.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $(DEFS) -o bin/FindProof tools/thvm_atp_cli.c $(LDFLAGS)
```

`DEFS` is the set of engine feature flags (the ATP flatterm, ground-join,
free-variable index, minimal-normal-form front, and so on). They are passed as
SEPARATE `-D` arguments; do not collapse them into one shell variable, or the
preprocessor treats the first as a macro whose value is the rest, and the build
fails with confusing "unknown type" errors.

---

## 3. Building the tarball from the repository

From the repository root:

```
sh tools/casc/mk_tarball.sh                       # writes ./FindProof.tgz
sh tools/casc/mk_tarball.sh /path/to/FindProof.tgz  # or a chosen path
```

The script copies the engine sources plus the CLI, the run script, the
build script, the Makefile, and the README into a staging `FindProof/` tree and
tars it. It does not compile anything; StarExec (or you) compiles from source.
(`tools/casc/mk_bin_tarball.sh` is the variant that ships a prebuilt binary;
`tools/proover/mk_tarball.sh` builds the CheckProof tarball the same way.)

Keep the output off the repository tree (it is a generated artifact, not
version-controlled). A durable choice is `~/.cache/thvm/casc/FindProof.tgz`.

---

## 4. How StarExec builds it

When the solver is uploaded as source with the build step enabled, StarExec, on
a build node, does the equivalent of:

```
cd FindProof
sh starexec_build          # which runs:  make bin/FindProof
```

`starexec_build` is intentionally trivial:

```sh
#!/bin/sh
set -e
make bin/FindProof
```

Notes that matter on the build node:

- The build node has NO network access. The build must be self-contained; it is
  (one compiler invocation over the vendored sources).
- The default compiler is the platform `cc`. The Makefile uses `CC ?= cc`, so
  StarExec's toolchain is honoured. It builds clean under gcc and clang at C11.
- One portability point already handled: a few counters were declared `extern
  unsigned long long` but defined as `u64`, which on LP64 Linux (`u64` =
  `unsigned long`) is a conflicting type. That is fixed; the monolith compiles
  clean on non-Apple targets.

After the build, StarExec re-archives the tree (now containing
`bin/FindProof`) for the run nodes.

---

## 5. How StarExec runs it

For each (configuration, benchmark) pairing, on a run node, StarExec sets the
resource-limit environment variables and calls the configuration script with the
benchmark path:

```
STAREXEC_WALLCLOCK_LIMIT=120 STAREXEC_CPU_LIMIT=120 STAREXEC_MAX_MEM=... \
  bin/starexec_run_default  /path/to/benchmark.p
```

Each run script resolves the solver root relative to its own location, reads the
wall-clock limit, pins `THVM_ATP_SCHED_MAX_PAR` to the node's core count (so the
whole portfolio races in ONE wave -- under a hard wall, later waves never run;
exit-fast slices free their slots immediately on inapplicable problems), and
executes `bin/FindProof --schedule --szs --proof`. Standard output is captured
and the `% SZS status ...` line is parsed. Output is quiet by default on the
competition path -- stdout carries exactly the SZS line(s) + proof, and the
engine's progress chatter is suppressed; `--verbose` (or `THVM_ATP_VERBOSE=1`)
restores it for development.

---

## 6. Doing it manually, part A: reproduce StarExec locally

This runs exactly what StarExec would, so you can test before uploading. From a
scratch directory (state your working directory; do not rely on a fixed path):

```sh
# 1. build the tarball from the repo
sh tools/casc/mk_tarball.sh ~/.cache/thvm/casc/FindProof.tgz

# 2. unpack into a clean directory (as a StarExec run node would)
mkdir -p /tmp/se && tar xzf ~/.cache/thvm/casc/FindProof.tgz -C /tmp/se
cd /tmp/se/FindProof

# 3. build, exactly as StarExec's build node does
sh starexec_build
#    -> compiles bin/FindProof ; exit status 0 on success

# 4a. run the CASC configuration on a real UEQ problem
STAREXEC_WALLCLOCK_LIMIT=120 sh bin/starexec_run_default /path/to/GRP014-1.p
#    -> % SZS status Unsatisfiable for GRP014-1.p
#       % SZS output start CNFRefutation ... % SZS output end CNFRefutation

# 4b. run the ProoVer configuration on a real TSTP proof
STAREXEC_WALLCLOCK_LIMIT=30 sh CheckProof/bin/starexec_run_default /path/to/proof.p
#    -> % SZS status VerifiedGood|VerifiedBad|Unknown for proof.p
#       (a localised faulty step is printed as a % comment)
```

To batch-check locally against the synthesized labelled corpus and confirm no
regressions:

```sh
python3 wl/Examples/proover/synth/generate.py    # (re)build the corpus
python3 wl/Examples/proover/synth/score.py        # scores --check vs gold labels
#    exits non-zero if any valid proof is rejected or any buggy proof accepted
```

The manual build and run were verified end to end on an Ubuntu 22.04 / gcc 11.4
machine: `starexec_build` produces an ELF binary, `starexec_run_default` proves
and emits a CNFRefutation, and the CheckProof tarball's own
`starexec_run_default` verifies a valid proof and rejects a buggy one with the
step localised.

---

## 7. Doing it manually, part B: upload and run on the StarExec web service

The web workflow changes over time, so confirm labels against the live site;
the shape is stable:

1. Sign in and open (or create) a SPACE for your work.
2. Upload the SOLVER: choose the `FindProof.tgz` archive. Select the "source /
   run build script" option so `starexec_build` compiles it on a build node.
   After the build finishes, each solver shows its single configuration
   `default` (FindProof.tgz proves; CheckProof.tgz checks -- upload them as
   two separate solvers).
3. Upload the BENCHMARKS as a benchmark archive into the space:
   - for CASC: TPTP unit-equality problems (`.p`);
   - for ProoVer: first-order TSTP proofs (`.p`).
   Pick the benchmark processor that matches the format if prompted.
4. Create a JOB. Pair the solver CONFIGURATION with the benchmark set:
   - the FindProof solver's `default` for the CASC problems,
   - the CheckProof solver's `default` for the ProoVer proofs.
   Set the wall-clock limit (CASC commonly 120s or the division's limit; ProoVer
   30s per proof) and the memory limit for the job.
5. Run the job and inspect results. StarExec shows the parsed SZS status per
   pairing and lets you download stdout/stderr for any run.

If you only want the compiled artifact (for example to run outside StarExec),
upload as source, let the build run, then download the built solver; it now
contains `bin/FindProof`.

---

## 8. The two entries in detail

### default -- CASC unit-equality proving

```
bin/FindProof --schedule --szs --proof --wall=<limit> PROBLEM.p
```

- INPUT: a TPTP CNF unit-equality problem, or FOF / non-unit CNF via the
  equationalization front end. `include('Axioms/*.ax')` directives resolve
  against the problem directory and the `TPTP` environment variable.
- `--schedule` runs a CONCURRENT portfolio: the configuration slices launch as
  child processes in a bounded pool (`THVM_ATP_SCHED_MAX_PAR` at once --
  default 5 for the bare binary, pinned to the core count by the run script --
  each with the FULL wall limit and a per-child RSS cap), and the first proof
  wins; the losers are killed and reaped. The slice set (29 slices, the
  `SCHEDULE[]` table in `tools/thvm_atp_cli.c`) is documented in
  `docs/atp/casc_strategy.md` section 3: the Waldmeister preset, the
  existential-goal lanes (equals encoding + narrowing queue), the forward and
  backward condensed-detachment lanes, the 7-slice FOF pack, AC completion,
  definition unfolding (alone and composed with goal naming), goal flattening
  (dressed and bare), the Twee-scored completion lane, the MNF goal-directed
  front, encompassment demodulation, bare completion, the ifeq-guard and tgt
  lanes, three raw-presentation recovery lanes, and three ground-join backups.
  Exit-fast slices (`=2` gates) give up instantly on inapplicable problems,
  freeing their pool slot.
- OUTPUT: one `% SZS status` line using the CNF ontology
  (`Unsatisfiable` / `Satisfiable` / `GaveUp` / `ResourceOut` / `Timeout`;
  `Theorem` for a `fof(` conjecture) and, on success, an SZS `CNFRefutation`
  (or `Proof`) block.
- Problem transforms widen what the engine represents: a functor of arity
  above eight is curried into nested binary `ap2` applications (`--curry`,
  default on), and a knowledge base of thousands of clauses is cut to the
  goal-relevant slice by SInE selection before the capacity checks
  (`THVM_ATP_SINE=0` restores the old clean give-up). A problem that still
  cannot be represented degrades to a clean `GaveUp` rather than crashing, so
  every problem gets a valid status line.

### CheckProof (the ProoVer entry) -- first-order proof checking

```
bin/CheckProof --szs PROOF.p        # the CheckProof tarball's binary
bin/FindProof --check --szs PROOF.p # same checker inside FindProof
```

(The CheckProof run script exports `THVM_PROOFCHECK_WALL` from the StarExec
wall limit.)

- INPUT: a first-order TSTP proof (`fof`/`cnf` records with `inference(...)`
  annotations), by refutation, closing with `$false`.
- Each derived step is validated by dispatching on its inference rule; every
  entailment obligation is discharged by the engine's own saturation prover.
- OUTPUT: one `% SZS status` verdict, `VerifiedGood` / `VerifiedBad` /
  `Unknown`, plus, when a step is rejected, a `% bad step:` comment naming
  it.
- The checker is conservative: it emits `VerifiedBad` only when a step is
  certainly unsound, and abstains with `Unknown` otherwise, because the
  competition scoring penalises a wrong verdict far more than abstaining.

---

## 9. Gotchas

- One SZS line only. The run scripts route the engine's progress output to
  standard error and, in `--schedule`, suppress the SZS line of every child
  except the winner, so standard output carries exactly one status line. Do not
  add extra prints to stdout.
- Self-contained build. The build node has no network; everything needed is in
  the tarball. Do not add a dependency that fetches at build time.
- Separate `-D` flags. Pass each engine feature define as its own argument (the
  Makefile already does). Collapsing them into one unquoted shell variable
  silently breaks the build.
- Wall vs CPU. `--schedule` runs up to `THVM_ATP_SCHED_MAX_PAR` slices
  CONCURRENTLY (the run script pins it to the core count), each against the
  full WALL-clock limit, so total CPU consumption can approach MAX_PAR x the
  wall. If the job's `STAREXEC_CPU_LIMIT` equals the wall limit, the CPU limit
  bites first on a saturating problem -- give the job a CPU limit of at least
  MAX_PAR x the wall (CASC's standard config does).
- Memory. Each schedule child inherits a per-slice RSS abort cap
  (`SCHED_SLICE_RSS_MB`, 4500 MB) unless the caller exported a lower
  `THVM_ATP_RSS_ABORT_MB`; measured practical peak was ~7 GB for 4 concurrent
  slices, and scales with `THVM_ATP_SCHED_MAX_PAR` (exit-fast slices cost
  nothing on inapplicable problems). Proof checking bounds its own term store and abstains
  (`Unknown`) rather than exhausting memory. Still set a sane
  `STAREXEC_MAX_MEM` on the job.
- Regenerate the tarball after any source change. `mk_tarball.sh` copies the
  live sources; the Makefile depends on both `tools/thvm_atp_cli.c` and the
  `#include`d `tools/proof_check.c`, so a checker-only edit does trigger a
  rebuild.

---

## 10. Quick reference

```sh
# build the submission archive
sh tools/casc/mk_tarball.sh ~/.cache/thvm/casc/FindProof.tgz

# reproduce StarExec build + run locally
tar xzf ~/.cache/thvm/casc/FindProof.tgz -C /tmp/se && cd /tmp/se/FindProof
sh starexec_build
STAREXEC_WALLCLOCK_LIMIT=120 sh bin/starexec_run_default  problem.p   # CASC
STAREXEC_WALLCLOCK_LIMIT=30  sh CheckProof/bin/starexec_run_default  proof.p     # ProoVer

# direct binary use (no run scripts)
bin/FindProof --schedule --szs --proof --wall=120 problem.p           # prove
bin/FindProof --check --szs proof.p                                   # check
bin/FindProof --list-knobs                                            # strategies

# local regression on the synthesized labelled proof corpus
python3 wl/Examples/proover/synth/score.py
```
