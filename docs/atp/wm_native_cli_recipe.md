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
