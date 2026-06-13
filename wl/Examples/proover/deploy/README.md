# Deploying the ProoVer checker (Docker / container)

Can the ProoVer checker run on the competition box? The box is **CentOS Linux
7.4.1708, kernel 3.10.0-693, glibc 2.17, x86_64** (Octa-core Xeon E5-2620 v4,
128 GB). The checker needs a Wolfram kernel + the `Wolfram/WolframParser`
paclet (`TPTPImport`); it does *not* need THVMLink.

## Finding: modern Wolfram cannot run natively on CentOS 7

Empirically (not from docs) -- `objdump` on the *installed* `libWolframEngine.so`,
the core library every evaluation runs through:

| Wolfram | `libWolframEngine.so` needs | evidence |
| ------- | --------------------------- | -------- |
| 14.0    | **glibc 2.27** (`logf`, `powf` @ `GLIBC_2.27`) + `GLIBCXX_3.4.22` | `objdump -T` |
| 15.0    | **glibc 2.28** + `GLIBCXX_3.4.22` | `objdump -p` version-needed |

CentOS 7.4 ships **glibc 2.17**. The kernel launcher binary itself only needs
`GLIBC_2.2.5` and limps to the license check via lazy binding, but real
evaluation resolves the `GLIBC_2.27`/`2.28` symbols in `libWolframEngine.so`,
which the system `libm` cannot provide.

- The **`libstdc++` gap** (`GLIBCXX_3.4.22` / `CXXABI_1.3.9`) *is* fixable on
  CentOS 7: devtoolset-11 ships a drop-in `libstdc++.so.6` built to run on
  glibc 2.17 (point `LD_LIBRARY_PATH` at it).
- The **glibc gap is not**: you cannot upgrade glibc on CentOS 7 without
  breaking the OS, and Wolfram's "glibc 2.17 minimum" applies only to the
  launcher, not the engine library.

So native Wolfram 14/15 on the CentOS 7 box is not a viable deployment.

## Solution: a container with a modern userspace

A Docker/Singularity/Apptainer image on a modern base (Ubuntu 22.04, glibc 2.35)
runs on the competition box's **own kernel 3.10** (fine for glibc 2.35, which
needs only kernel >= 3.2), sidestepping the host glibc entirely. This is why the
sibling PrimeIntellect Wolfram image is Ubuntu-based, not CentOS 7.

`Dockerfile.ubuntu` is the deployable artifact (Ubuntu 22.04 + Wolfram +
WolframParser + the checker). `Dockerfile.base` / `Dockerfile.centos7` reproduce
the CentOS-7 environment and the failed-native experiment that established the
finding above; keep them as the evidence trail.

### Open questions for the organizers (Cailler / Guilloud)

- Does the box have a **container runtime** (Singularity/Apptainer is near-
  universal on CentOS 7-era academic clusters)? If yes, ship the image.
- Is there **network access during runs**? The entitlement license polls the
  cloud; if runs are offline, a node-locked activation at install time is needed
  instead.

## Build and run

```bash
./stage.sh 15.0.0                 # copy checker + WolframParser paclet, download installer
docker build --platform linux/amd64 -t proover -f Dockerfile.ubuntu .
docker run --rm -e WOLFRAMSCRIPT_ENTITLEMENTID=O-XXXX proover corpus   # expect 11/11
docker run --rm -e WOLFRAMSCRIPT_ENTITLEMENTID=O-XXXX proover <proof.p> # SZS verdict
```

Build amd64 images on amd64 hardware (the PrimeIntellect remote builder / a pod);
emulated amd64 Wolfram on an Apple-Silicon host is too slow to start under qemu
for practical functional testing.

## Licensing

The image is unlicensed; activate at runtime with an on-demand entitlement
(`-e WOLFRAMSCRIPT_ENTITLEMENTID=...`). Mint one from any licensed Wolfram
install:

```wl
CreateLicenseEntitlement[<|"PolledEntitlement" -> Quantity[2, "Hours"], "LicenseCount" -> 2|>]["EntitlementID"]
```

The offline paclet install (`COPY wolframparser/ -> ~/.Wolfram/Paclets/Repository/`)
needs no license or network: a paclet directory under `Paclets/Repository` is
exactly what PacletManager scans on first kernel start (byte-identical to
`PacletInstall`).
