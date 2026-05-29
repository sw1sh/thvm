# Building Waldmeister natively on macOS ARM64 (AndAssoc reference)

The shipped `waldmeister/bin/.std`/`.fast`/`.power`/`.comp` binaries are
linux x86-64 ELFs.  The MacOSX-ARM64 Makefile builds `libwaldmeister.a`
+ `ELProver` (MathLink protocol), not a standalone CLI.

Captured native AndAssoc wall on this hardware (iter 133):

| Path                                              | Wall  | Notes                                       |
|---------------------------------------------------|-------|---------------------------------------------|
| `FindEquationalProof` (Mathematica built-in)      | 7.4 s | private build; links Foundation+libc++      |
| `wmcli andassoc.pr` (my LTO+m1-tuned build)       | 10.0s | same WM source; -O3 -flto -mcpu=apple-m1    |
| thvm C-bench (KBO_FLAT=0 + others on)             | 12.1s | algorithm gap to native wmcli: ~2s          |
| thvm C-bench (KBO_FLAT=1 + others on)             | 14.6s | KBO_FLAT compare net-negative on this load  |
| thvm paclet (current stack, all flags default-on) | 25.2s | E-core scheduling +10s on top of C-bench    |

**The 2.6s gap between FindEquationalProof (7.4s) and my wmcli (10.0s)
is BUILD QUALITY, not algorithm.**  Both run the SAME Waldmeister
source.  Wolfram's `Contents/SystemFiles/Kernel/Binaries/MacOSX-ARM64/ELProver`
is 1.97 MB, dynamically links Foundation, libc++, CoreFoundation; mine
is 478 KB, libSystem + mathlink only.  Wolfram's build uses a more
aggressive compiler toolchain + global LTO (my `libwaldmeister.a`
archives non-LTO `.o` pieces).  This is not a reproducible delta
from the source tree alone.

So the honest thvm-engine reference is **wmcli at 10.0s**, not
FindEquationalProof's 7.4s.  The remaining algorithmic gap is C-bench
12.1s -> wmcli 10.0s = ~2s in `atp_rewrite_normalize` + KBO compare.

### thvm with LTO + M1 (matching wmcli's flags)

Rebuilt the thvm C-bench AND paclet dylib with the same flags my LTO
wmcli used (`-std=c11 -O3 -flto -mcpu=apple-m1`):

  C-bench (KBO_FLAT=0):  12.9s  (was 12.1s)  -- noise / slightly worse
  C-bench (KBO_FLAT=1):  15.1s  (was 14.6s)  -- noise / slightly worse
  paclet                : 26.7s  (was 25.2s)  -- noise / slightly worse

So `-O3 -flto -mcpu=apple-m1` does NOT measurably help thvm on this
workload at the bench level.  The 2.6s built-in advantage really does
come from Wolfram's private build chain, not from any flags I can set
through the public Makefile.

The remaining ~2s thvm-engine gap to wmcli has to be closed
algorithmically (atp_rewrite_normalize, atp_ri_find_redex / DT
descent, KBO compare inner loop).  No quick wins captured this iter
without finer profiling.

### Iter 134 sample-profile + env-flag sweep

Sample-profiled the C-bench during the AndAssociativity saturation:

  atp_push_cps_traced                 1153 samples
    atp_rewrite_normalize_flatterm_mixed   938
      atp_ft_unorient_step                 614
        thvm_kbo                           314
          kbo_vortest / kbo_lin_addto      ~280

So the dominant cost on the unorientable side is the KBO compare
chain (kbo_vortest -> kbo_lin_addto -> kbo_subtree_memo).  The
unorient discrimination tree already prunes 99.94% of positions
(0.06 candidates / query); the survivors all go through thvm_kbo.

Env-flag sweep:

  FLATTERM=1 + CP_INDEX=1 alone               12.2s  (best)
  FLATTERM=1 + CP_INDEX=1 + WMFPA=1            12.1s  (current default)
  FLATTERM=1 + CP_INDEX=1 + KBO_FLAT=1         14.6s
  FLATTERM=0 + CP_INDEX=1 + WMFPA=1            30s    TIMEOUT
  All four off                                 30s    TIMEOUT

Tried defaulting only FLATTERM + CP_INDEX in the paclet load to drop
WMFPA + KBO_FLAT: WolframKernel crashed mid-saturation.  Some
invariant on the WolframKernel side wants WMFPA or KBO_FLAT to be on.
Reverted -- the 4-flag default-on path stays the safe default.

So the remaining engine gap is genuinely in thvm_kbo's tree-walk
(kbo_vortest + kbo_subtree_memo + kbo_lin_addto).  Closing it wants
WM-style per-symbol-balance compact var-multiset + memo invalidation
strategy that survives splice-changed subjects -- real engine work,
not env tuning.

## Build recipe

Two edits unlock the in-tree `PowerMain.c` CLI driver (same one the
shipped `.std` uses, no MathLink):

1. `Makefile.MacOSX-ARM64`: drop `-DWALD_LIB=0` from CFLAGS.  The macro
   mismatch is real: `LeseSpezifikation` at `sources/RUN/WaldmeisterII.c:374`
   is guarded by `#ifndef WALD_LIB`, but `topfuns.c:10` uses `#if WALD_LIB`.
   With `-DWALD_LIB=0` the CLI parser body preprocesses to an empty stub
   and `WM_main` bails immediately with no spec.

2. `sources/RUN/Signale.c`: add `#include "Ausgaben.h"` after line 49.
   Once the `WALD_LIB=0` stub goes away, the signal handlers call
   `IO_DruckeFlex` which lives in `Ausgaben.h`.

Then build + link + run:

```sh
cd /Users/swish/src/wolfram/waldmeister
MLINKDIR=/Applications/Wolfram.app/Contents/SystemFiles/Links/MathLink/DeveloperKit/MacOSX-ARM64 \
    make -f Makefile.MacOSX-ARM64 libwaldmeister.a

# PowerMain.o sometimes lands in the archive as PowerMain.c -- repair:
ar d libwaldmeister.a PowerMain.c 2>/dev/null
make -f Makefile.MacOSX-ARM64 lib/mlwald/PowerMain.o
ar rs libwaldmeister.a lib/mlwald/PowerMain.o

cat > wmcli.c <<'CLI'
#include <stdio.h>
#include <time.h>
extern int WM_main(int, char *[]);
int main(int argc, char *argv[]) {
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  int rc = WM_main(argc, argv);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double wall = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
  fprintf(stderr, "wmcli wall: %.3fs rc=%d\n", wall, rc);
  return rc;
}
CLI

/usr/bin/clang -arch arm64 -mmacosx-version-min=10.12 -fPIC \
    -fgnu89-inline -O3 -I. -Iinclude \
    -DCOMP_OPTIMIERUNG=1 -DCOMP_POWER=1 -DCOMP_COMP=0 \
    -F/Applications/Wolfram.app/Contents/SystemFiles/Links/MathLink/DeveloperKit/MacOSX-ARM64/CompilerAdditions \
    -o wmcli wmcli.c unixfunctions.c \
    -L. -lwaldmeister -lm -lpthread -framework mathlink

DYLD_FRAMEWORK_PATH=/Applications/Wolfram.app/Contents/SystemFiles/Links/MathLink/DeveloperKit/MacOSX-ARM64/CompilerAdditions \
    ./wmcli andassoc.pr 2>&1 | tail -20
```

The `andassoc.pr` problem statement (already in the WM tree at
`waldmeister/andassoc.pr`) cracks under KBO + `p > q > r > nand`
precedence in ~9.7s, returning `1673 Rules / 102 Equations /
4574980 CPs`.

## Where this leaves the thvm comparison

- WM C-engine alone: 9.7s.  thvm C-bench: 14.7s.  ~5s engine-side gap.
- FindEquationalProof: 7.4s.  WM CLI: 9.7s.  ~2.3s = file-parse + WM-CLI
  startup overhead.  FindEquationalProof skips both by talking to WM via
  MathLink with terms already in Mathematica form.
- paclet 25.5s vs C-bench 14.7s.  ~10s in WL\<->C overhead localized in
  iter 132 to macOS scheduler parking the dylib on E-cores when called
  from WolframKernel.

### Iter 135: ORDERING + trajectory shape

Sample-profiled native wmcli — hot functions are LPO-vortest variants:

  MO_RegelGefunden               1352   (rule install)
  LV_VortestLPOGroesser           599
  LV_VortestLPO                   270
  LV_VortestLPOGroesserGleich     207
  CH_MixWeight + CF_Phi            55

WM uses **LPO** (`andassoc.pr` declares `ORDERING LPO`, precedence
`p > q > r > nand`).  thvm's C-bench uses **KBO** with the inverse-
direction precedence by default.

Tried switching the bench to LPO + WM-matching precedence + Mix
weight + Waldmeister knobs.  Results:

  thvm KBO + Gt + FLATTERM       12.1s, 338 rules  (CRACKED)
  thvm LPO + Mix + Wald-knobs    30s TIMEOUT, 196 rules
  thvm LPO + Gt + FLATTERM       30s TIMEOUT, 204 rules
  wmcli LPO (native)             10.0s, 1601 rules (CRACKED)

So thvm KBO finds a **5x SHORTER proof** (338 rules vs WM's 1601),
but the per-rule cost is higher (~36ms vs WM's ~6ms).  Net wall:
thvm 12.1s vs WM 10s = ~2s gap, all of it per-rule cost.

The proof-trajectory difference means a straight LPO port wouldn't
help -- thvm's LPO is also slower per-step than WM's, AND it doesn't
find the 338-rule KBO shortcut.  The real lever is closing the per-
rule cost on thvm's KBO+FLATTERM path: WM-style flatterm with cached
`Ende` sibling-skip pointers (per the workflow research finding) so
KBO compare's tree walk becomes linear pointer chasing.
