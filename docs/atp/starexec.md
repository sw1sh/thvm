# FindProof on StarExec: build and run, by hand

How to compile and run the FindProof submission the way the competition
infrastructure (StarExec) does, and how to reproduce every step manually so you
can test before uploading. One tarball serves two competitions from one binary:

- CASC (CADE ATP System Competition) unit-equality proving
- ProoVer first-order proof checking

Last verified 2026-07-08 on Ubuntu 22.04 / gcc 11.4 and macOS.

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
    starexec_run_proover         # CONFIG "proover": ProoVer proof checking
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

The script copies the engine sources plus the CLI, the two run scripts, the
build script, the Makefile, and the README into a staging `FindProof/` tree and
tars it. It does not compile anything; StarExec (or you) compiles from source.

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
wall-clock limit, and executes `bin/FindProof` in the appropriate mode. Standard
output is captured and the `% SZS status ...` line is parsed. Standard error is
kept separately (FindProof routes progress chatter there, so stdout stays clean).

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
STAREXEC_WALLCLOCK_LIMIT=30 sh bin/starexec_run_proover /path/to/proof.p
#    -> % SZS status Verified|FailedVerified|NotVerified for proof.p
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
and emits a CNFRefutation, and `starexec_run_proover` verifies a valid proof and
FailedVerifies a buggy one with the step localised.

---

## 7. Doing it manually, part B: upload and run on the StarExec web service

The web workflow changes over time, so confirm labels against the live site;
the shape is stable:

1. Sign in and open (or create) a SPACE for your work.
2. Upload the SOLVER: choose the `FindProof.tgz` archive. Select the "source /
   run build script" option so `starexec_build` compiles it on a build node.
   After the build finishes, the solver shows two configurations, `default` and
   `proover`.
3. Upload the BENCHMARKS as a benchmark archive into the space:
   - for CASC: TPTP unit-equality problems (`.p`);
   - for ProoVer: first-order TSTP proofs (`.p`).
   Pick the benchmark processor that matches the format if prompted.
4. Create a JOB. Pair the solver CONFIGURATION with the benchmark set:
   - use configuration `default` for the CASC problems,
   - use configuration `proover` for the ProoVer proofs.
   Set the wall-clock limit (CASC commonly 120s or the division's limit; ProoVer
   30s per proof) and the memory limit for the job.
5. Run the job and inspect results. StarExec shows the parsed SZS status per
   pairing and lets you download stdout/stderr for any run.

If you only want the compiled artifact (for example to run outside StarExec),
upload as source, let the build run, then download the built solver; it now
contains `bin/FindProof`.

---

## 8. The two configurations in detail

### default -- CASC unit-equality proving

```
bin/FindProof --schedule --szs --proof --wall=<limit> PROBLEM.p
```

- INPUT: a TPTP CNF unit-equality problem. `include('Axioms/*.ax')` directives
  resolve against the problem directory and the `TPTP` environment variable.
- `--schedule` runs a time-sliced portfolio of configurations, each in a child
  process under a slice of the wall limit, and stops at the first proof.
- OUTPUT: one `% SZS status` line using the CNF ontology
  (`Unsatisfiable` / `Satisfiable` / `GaveUp` / `ResourceOut` / `Timeout`) and,
  on success, an SZS `CNFRefutation` block.
- Problems the engine cannot represent (a functor of arity above eight, or a
  knowledge base of thousands of clauses) degrade to a clean `GaveUp` rather
  than crashing, so every problem gets a valid status line.

### proover -- first-order proof checking

```
bin/FindProof --check --szs PROOF.p
```

- INPUT: a first-order TSTP proof (`fof`/`cnf` records with `inference(...)`
  annotations), by refutation, closing with `$false`.
- Each derived step is validated by dispatching on its inference rule; every
  entailment obligation is discharged by the engine's own saturation prover.
- OUTPUT: one `% SZS status` verdict, `Verified` / `FailedVerified` /
  `NotVerified`, plus, when a step is rejected, a `% bad step:` comment naming
  it.
- The checker is conservative: it emits `FailedVerified` only when a step is
  certainly unsound, and abstains with `NotVerified` otherwise, because the
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
- Wall vs CPU. `--schedule` slices the WALL-clock limit. On a busy StarExec node
  the CPU limit can bite first; keep the schedule slices modest.
- Memory. Proof checking bounds its own term store and abstains (`NotVerified`)
  rather than exhausting memory, but still set a sane `STAREXEC_MAX_MEM` on the
  job.
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
STAREXEC_WALLCLOCK_LIMIT=30  sh bin/starexec_run_proover  proof.p     # ProoVer

# direct binary use (no run scripts)
bin/FindProof --schedule --szs --proof --wall=120 problem.p           # prove
bin/FindProof --check --szs proof.p                                   # check
bin/FindProof --list-knobs                                            # strategies

# local regression on the synthesized labelled proof corpus
python3 wl/Examples/proover/synth/score.py
```
