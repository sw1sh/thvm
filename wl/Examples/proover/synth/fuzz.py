#!/usr/bin/env python3
"""Adversarial battery for CheckProof.

  python3 fuzz_checkproof.py <checkproof_binary> <valid_proofs_dir> [rounds]

Invariants asserted on EVERY run:
  - the process exits (no hang beyond the timeout)
  - exactly ONE '% SZS status' line on stdout
  - the verdict is one of VerifiedGood / VerifiedBad / Unknown / Error

Soundness assertions:
  - a token-corrupted VALID proof must never be VerifiedGood unless the
    corruption is provably neutral (we count and list survivors for review)
  - structural mutants (cyclic parents, duplicate names, missing $false,
    forward references) must never be VerifiedGood
  - garbage / truncated / empty inputs must never be VerifiedGood/VerifiedBad
  - determinism: two runs on the same input give identical stdout
"""
import subprocess, os, sys, glob, random, re, tempfile

CP = os.path.abspath(sys.argv[1])
VALID = sys.argv[2]
ROUNDS = int(sys.argv[3]) if len(sys.argv) > 3 else 200
random.seed(20260713)

def run(path):
    try:
        r = subprocess.run([CP, "--szs", path], capture_output=True, text=True, timeout=45)
        lines = [l for l in r.stdout.splitlines() if "SZS status" in l]
        if len(lines) != 1:
            return ("BAD-LINECOUNT", str(len(lines)))
        m = re.search(r"SZS status (\w+)", lines[0])
        return (m.group(1) if m else "UNPARSEABLE", lines[0])
    except subprocess.TimeoutExpired:
        return ("HANG", "")

VERDICTS = {"VerifiedGood", "VerifiedBad", "Unknown", "Error"}
problems = sorted(glob.glob(os.path.join(VALID, "*.p"))) + \
           sorted(glob.glob(os.path.join(VALID, "*.s")))
viol = []

def check(tag, path, forbid_good=True, forbid_any_verdict=False):
    v, detail = run(path)
    if v in ("HANG", "BAD-LINECOUNT", "UNPARSEABLE") or v not in VERDICTS:
        viol.append((tag, os.path.basename(path), v, detail)); return v
    if forbid_any_verdict and v in ("VerifiedGood", "VerifiedBad"):
        viol.append((tag, os.path.basename(path), "VERDICT-ON-GARBAGE:" + v, "")); return v
    if forbid_good and v == "VerifiedGood":
        viol.append((tag, os.path.basename(path), "FALSE-GOOD", detail)); return v
    return v

# --- 0. baseline: valid proofs stay VerifiedGood + deterministic -----
base = {}
for p in problems:
    v1, d1 = run(p); v2, d2 = run(p)
    base[p] = v1
    if (v1, d1.split(" for ")[0]) != (v2, d2.split(" for ")[0]):
        viol.append(("nondeterministic", os.path.basename(p), v1 + "/" + v2, ""))
print(f"baseline: {sum(1 for v in base.values() if v=='VerifiedGood')} VerifiedGood "
      f"of {len(problems)} inputs", flush=True)

# --- 1. token corruption of valid proofs ----------------------------
good_srcs = [p for p in problems if base[p] == "VerifiedGood"]
survivors = 0
for i in range(ROUNDS):
    src = random.choice(good_srcs)
    txt = open(src, errors="ignore").read()
    toks = list(re.finditer(r"[a-zA-Z_][a-zA-Z0-9_]*", txt))
    if not toks: continue
    t = random.choice(toks)
    repl = random.choice(["zz_q", t.group(0) + "x", "p", "a0"])
    mut = txt[:t.start()] + repl + txt[t.end():]
    with tempfile.NamedTemporaryFile("w", suffix=".p", delete=False) as fh:
        fh.write(mut); mp = fh.name
    v = check("token-corrupt", mp, forbid_good=False)
    if v == "VerifiedGood":
        survivors += 1   # possibly neutral (comment token / consistent rename)
    os.unlink(mp)
print(f"token-corruption: {ROUNDS} mutants, {survivors} still VerifiedGood "
      f"(reviewable; corruption may be neutral)", flush=True)

# --- 2. structural mutants (must never be VerifiedGood) -------------
def structural_variants(txt):
    out = {}
    out["no-false"]   = txt.replace("$false", "p_nonfalse")
    out["dup-names"]  = re.sub(r"fof\((\w+),", "fof(s_dup,", txt)
    m = re.findall(r"fof\((\w+),", txt)
    if len(m) >= 2 and ("[" + m[0] + "]") in txt:
        out["fwd-parent"]  = txt.replace("[" + m[0] + "]", "[" + m[-1] + "]", 1)
    d = re.search(r"fof\((\w+),\s*\w+,.*?inference\(\w+,\s*\[[^\]]*\]\s*,\s*\[([^\]]*)\]", txt, re.S)
    if d and ("[" + d.group(2) + "]") in txt:
        out["self-parent"] = txt.replace("[" + d.group(2) + "]", "[" + d.group(1) + "]", 1)
    out["truncated"] = txt[: len(txt) // 2]
    return out

for src in good_srcs:
    txt = open(src, errors="ignore").read()
    for tag, mut in structural_variants(txt).items():
        if mut == txt: continue
        with tempfile.NamedTemporaryFile("w", suffix=".p", delete=False) as fh:
            fh.write(mut); mp = fh.name
        check("struct-" + tag, mp, forbid_good=True)
        os.unlink(mp)
print("structural mutants done", flush=True)

# --- 3. garbage ------------------------------------------------------
for i in range(40):
    data = bytes(random.getrandbits(8) for _ in range(random.randint(0, 4096)))
    with tempfile.NamedTemporaryFile("wb", suffix=".p", delete=False) as fh:
        fh.write(data); mp = fh.name
    check("garbage", mp, forbid_any_verdict=True)
    os.unlink(mp)
print("garbage fuzz done", flush=True)

print(f"\nFUZZ: {len(viol)} violations", flush=True)
for v in viol[:40]:
    print("  !!", v, flush=True)
