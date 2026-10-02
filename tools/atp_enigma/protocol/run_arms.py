#!/usr/bin/env python3
"""Pod-side runner for one ENIGMA arm comparison.

Implements the run protocol of docs/plans/enigma_portfolio.md section 1.3
and the readings registered in its section 5.4.  Python 3.8 or later,
standard library only, Linux only (it reads /proc and /sys/fs/cgroup).

Every run of an arm, one problem at a time on the whole pod:

  env -i TPTP=<root> THVM_ATP_SCHED_MAX_PAR=16 [THVM_ATP_RSS_ABORT_MB=3500]
         [THVM_ATP_SCHED_ERRDIR=<scratch>/errdir]
    timeout -k 5 <W+15> <binary> <arm flags> --szs --proof --wall=<W> <problem>
    < /dev/null

Pod rules (preflight refuses a pod that breaks one):
- 16 effective CPUs: the affinity mask, capped by a cgroup cpu quota.
- The memory cap is fixed per comparison (--rss-cap default|3500).
  "default" keeps the schedule's own 4500 MiB slice cap and needs an
  effective memory limit, min(MemTotal, the cgroup memory limit), of at
  least 16 x 4500 MiB, so 16 slices at the cap fit; "3500" sets
  THVM_ATP_RSS_ABORT_MB=3500 on every run.  Each run's row records the
  memory facts and the OOM kills the cgroup (else the kernel) counted
  during the run.
- One binary for every arm (--allow-binaries to override, logged).  Each
  arm's lane table comes from its own binary ("<flags> --list-lanes=flags":
  name TAB flags); two arms with different flags but the same binary and
  the same lane table are refused unless declared (--same-lanes A,B).
THVM_ATP_SCHED_ERRDIR only redirects each slice's stderr from /dev/null to
a file; the runner reads it for the end class of every lane and deletes it
(--no-errdir drops it, and with it the end classes).

Solution (all three): SZS status Unsatisfiable; a complete CNFRefutation
block, with the run ended within W; a strict CheckProof verdict of
VerifiedGood (CheckProof --szs <stdout file>, THVM_PROOFCHECK_WALL=60,
under env -i TPTP=<root> and timeout -k 5 75).

Order and repeats:
- The pod first runs the calibration problems of --calib-ref with the
  calibration binary, CALIB_PASSES passes; per problem the median time
  over the reference time.  A pod whose geometric mean of those ratios is
  outside [0.85, 1.15], or with one problem more than 30% off, or with a
  calibration problem unsolved, fails (calibration.verdict, run.done
  status=calibration-failed) and the runner stops.
- Problems run in sha256(seed, name) order.  For each problem, its pass-1
  jobs (every wall x arm) run in sha256(seed, problem, pass, wall, arm)
  order, so the arm order is a fixed-seed shuffle per problem.
- With --passes 1 (the default) and repeats on: at each wall, when the
  arms' pass-1 Solution flags disagree or any arm's pass-1 winning lane is
  clock-gated (its flags in that arm's own lane table leave one of
  GATED_RULES on: --lrs, --fol-lrs, --cp-set-ir-budget, --goal-narrow, or
  the default-on Horn release and clausal pre-pass, which only --horn=0
  and --fof-sat=0 turn off; amendment A19), every arm runs passes 2 and 3
  there.
  A winning lane missing from its arm's table stops the runner
  (run.failed).  The decision goes to decisions.tsv; the majority of three
  decides (stats.py).
- With --passes N > 1 (the L0 A/A design) every arm runs N independent
  passes and no repeats.

Outputs in --out:
  rows.tsv        one row per run, header line first (columns below)
  decisions.tsv   problem, wall, repeat (0/1), reason
  lanes.jsonl     per run: every launched lane's end class and detail
  runs/<problem>/w<W>_<arm>_p<pass>.{out,err,chk}.gz  stdout (the proof),
                  stderr, checker output
  superseded.tsv, superseded/  rows and runs of problems moved aside on a
                  resume (below)
  lanes_<arm>.tsv the arm binary's lane table
  calibration.tsv, calibration.verdict
  run.config      the configuration fingerprint of the first start
  run.started     one block per start (appended)
  run.done (status=complete|deadline|terminated|verifiedbad|
  calibration-failed), run.failed (preflight or crash), run.stop (a
  VerifiedBad: the line stops until its cause is fixed), run.log
Sealed runs (J13 problems, or --sealed on): every per-problem output
(rows, decisions, lanes.jsonl, runs/, superseded) is packed into
sealed.tgz (mode 0400) whenever the runner stops, and unpacked at the
next start; the log carries a heartbeat every 15 min (base-job progress
rounded down to 10%) and no per-job line; run.stop names no problem.

Resume: a rerun with the same configuration skips every run that has a
row.  A different configuration (arms with their sha and flags, walls,
passes, repeats, seed, problems, max_par, rss cap, checker and its wall,
errdir, sealed, protocol fingerprint) is refused (--force-config to
override, logged).  A problem whose runs are incomplete and include a row
from another pod is moved to superseded.tsv / superseded/ and rerun whole
here, so all arms of a problem run on one pod.

rows.tsv columns:
   1 problem   2 split   3 arm   4 wall   5 pass
   6 status    SZS status word on stdout (NoOutput when none)
   7 wall_s    elapsed seconds of the whole run (runner clock)
   8 lane      winning lane ("% FindProof schedule winner: NAME"), or empty
   9 proof     1 when a complete "% SZS output start/end CNFRefutation"
               block is on stdout
  10 verdict   CheckProof verdict (VerifiedGood / VerifiedBad / Unknown /
               NoOutput), "-" when not checked
  11 solution  1 when status Unsatisfiable, proof 1, in_wall 1 and verdict
               VerifiedGood
  12 lscpu     model name, threads per core, CPUs, cores per socket,
               sockets, affinity CPUs, cgroup CPU quota
  13 bin_sha   sha256 of the arm binary
  14 in_wall   1 when wall_s <= W
  15 rc        exit code of the timeout process (124/137 outer timeout,
               128+N signal N)
  16 cpu_s  17 maxrss_kb   rusage of the reaped process tree
  18 end_class proved | wall | memory-guard | step-cap | saturated |
               immediate-exit | alarm | crash | unknown
  19 lane_classes   class:count over the launched lanes
  20 proof_recs     cnf/fof records inside the SZS output block
  21 check_s   checker seconds  22 strays  processes killed after the run
  23 host
  24 cond      maxpar, rss cap, errdir, proof, check wall, protocol
               fingerprint (identical on every pod of one comparison)
  25 utc
  26 mem       MemTotal, cgroup limit, effective limit (kB)
  27 oom_kills OOM kills counted during the run ("?" when unreadable)

Subcommands:
  run        the comparison
  calib-ref  measure the calibration reference table on this pod
  plan       print the job plan, the first command and a worst-case time
             estimate; runs nothing
  hostfacts  print this pod's effective memory and CPUs (key=value)
Exit codes of run: 0 complete, 7 deadline, 5 terminated, 4 VerifiedBad,
3 calibration failed, 2 preflight error or crash.

Typical pod use (from a snapshot copy, detached; the Mac driver passes an
absolute --deadline-utc and polls the sentinels):
  cp run_arms.py ~/run_arms.snapshot.py
  nohup setsid python3 ~/run_arms.snapshot.py run --problems j13.tsv \\
    --arms arms.tsv --walls 180,56 --rss-cap default --root ~/j13 \\
    --checker ~/thvm/bin/CheckProof --calib-ref calib_ref.tsv \\
    --calib-bin ~/sub/FindProof/bin/FindProof --calib-root ~/tptp_pool \\
    --shard 1/4 --deadline-utc 2026-10-05T06:00:00Z --out ~/out_L0_1 \\
    </dev/null >~/run_arms_1.log 2>&1 &
"""
import argparse
import calendar
import collections
import fcntl
import glob
import gzip
import hashlib
import json
import math
import os
import re
import shutil
import signal
import socket
import statistics
import subprocess
import sys
import tarfile
import threading
import time

ROW_COLUMNS = [
    "problem", "split", "arm", "wall", "pass", "status", "wall_s", "lane",
    "proof", "verdict", "solution", "lscpu", "bin_sha",
    "in_wall", "rc", "cpu_s", "maxrss_kb", "end_class", "lane_classes",
    "proof_recs", "check_s", "strays", "host", "cond", "utc", "mem", "oom_kills",
]
DECISION_COLUMNS = ["problem", "wall", "repeat", "reason"]
CALIB_COLUMNS = ["name", "pass", "status", "wall_s", "ref_s", "ratio",
                 "host", "lscpu", "bin_sha", "utc"]

MAX_PAR = 16
SLICE_RSS_MB = 4500          # SCHED_SLICE_RSS_MB in tools/atp_cli/main.c
SMALL_POD_RSS_MB = 3500
RSS_CAPS = ("default", str(SMALL_POD_RSS_MB))
SPEED_TOLERANCE = 0.15
SINGLE_TOLERANCE = 0.30
CALIB_PASSES = 2
CHECK_WALL_S = 60
DEFAULT_SEED = "20261001"
REPEAT_PASSES = (2, 3)
# The lane flags whose disciplines read the work clock or the wall for a
# search decision (amendments A11, A19), each with the reading FindProof
# gives the flag's last value (None: the flag is absent; a bare boolean
# knob is "1").  --horn and --fof-sat are on unless disabled: the Horn
# release runs unless THVM_ATP_HORN starts with '0' (main.c), the clausal
# pre-pass unless atol(THVM_ATP_FOF_SAT) <= 0 (fof_split.c).
def _env_on(v):
    return v is not None and v != "" and v[0] != "0"


def _atoi(v):
    m = re.match(r"\s*([+-]?\d+)", v or "")
    return int(m.group(1)) if m else 0


GATED_RULES = (
    ("--lrs", _env_on),                                       # engine LRS
    ("--fol-lrs", lambda v: v is not None and _atoi(v) != 0),  # pre-pass LRS
    ("--cp-set-ir-budget", lambda v: v is not None and _atoi(v) > 0),
    ("--goal-narrow", _env_on),                               # its 85% endgame burst
    ("--horn", lambda v: v is None or v[:1] != "0"),          # the Horn release budget
    ("--fof-sat", lambda v: v is None or _atoi(v) > 0),       # the pre-pass wall share
)
GATED_FLAGS = tuple(name for name, _on in GATED_RULES)
GATED_DEFAULT_ON = tuple(name for name, on in GATED_RULES if on(None))
REPEAT_READING = ("disagree=pass-1 Solution flags differ at the wall;"
                  "gated=winning lane's flags in its own arm's lane table;per-wall")
HEARTBEAT_S = 900
PROBLEM_NAME = re.compile(r"^[A-Za-z0-9_.+=^-]+$")
ARM_NAME = re.compile(r"^[A-Za-z0-9_.+-]+$")
SEALED_MEMBERS = ("rows.tsv", "decisions.tsv", "lanes.jsonl", "superseded.tsv", "runs",
                  "superseded")
TMP_GLOBS = ("/tmp/thvm_sched_*", "/tmp/thvm_split_*", "/tmp/thvm_atp_*",
             "/tmp/pc_ob_*")
LOCK_PATH = "/tmp/thvm_run_arms.lock"

STOP = {"signal": 0}


# ----------------------------------------------------------------------------
# Small helpers


def fmt_num(x):
    x = float(x)
    if x == int(x):
        return str(int(x))
    return ("%.3f" % x).rstrip("0").rstrip(".")


def utc_now():
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def parse_utc(text):
    try:
        return calendar.timegm(time.strptime(text, "%Y-%m-%dT%H:%M:%SZ"))
    except ValueError:
        raise SystemExit("run_arms: --deadline-utc takes YYYY-MM-DDTHH:MM:SSZ, got %r" % text)


def clean_field(s):
    return str(s).replace("\t", " ").replace("\n", " ").replace("\r", " ")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fp:
        while True:
            b = fp.read(1 << 20)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def sha256_text(text):
    return hashlib.sha256(text.encode()).hexdigest()


def order_key(seed, *parts):
    text = "|".join([str(seed)] + [str(p) for p in parts])
    return hashlib.sha256(text.encode()).hexdigest()


def domain_of(name):
    m = re.match(r"[A-Z]+", name)
    return m.group(0) if m else name


def read_text(path, limit=None):
    try:
        with open(path, "rb") as fp:
            if limit is None:
                data = fp.read()
            else:
                size = os.fstat(fp.fileno()).st_size
                if size <= limit:
                    data = fp.read()
                else:
                    head = fp.read(limit // 4)
                    fp.seek(size - (limit - limit // 4))
                    data = head + b"\n...\n" + fp.read()
    except OSError:
        return ""
    return data.decode("utf-8", "replace")


def gzip_copy(src, dst):
    try:
        with open(src, "rb") as fi, gzip.open(dst, "wb", compresslevel=6) as fo:
            shutil.copyfileobj(fi, fo)
    except OSError:
        pass


def append_line(path, fields):
    line = "\t".join(clean_field(f) for f in fields) + "\n"
    with open(path, "a") as fp:
        fp.write(line)
        fp.flush()
        os.fsync(fp.fileno())


def write_file(path, text):
    tmp = path + ".tmp"
    with open(tmp, "w") as fp:
        fp.write(text)
        fp.flush()
        os.fsync(fp.fileno())
    os.replace(tmp, path)


def append_file(path, text):
    with open(path, "a") as fp:
        fp.write(text)
        fp.flush()
        os.fsync(fp.fileno())


def read_tsv(path):
    """Rows of a TSV written by this runner: (header, list of dicts)."""
    header = None
    rows = []
    if not os.path.exists(path):
        return header, rows
    with open(path) as fp:
        for line in fp:
            line = line.rstrip("\n")
            if not line:
                continue
            if line.startswith("#"):
                if header is None:
                    header = line[1:].split("\t")
                continue
            f = line.split("\t")
            if header is None or len(f) != len(header):
                continue
            rows.append(dict(zip(header, f)))
    return header, rows


def ensure_header(path, columns):
    if not os.path.exists(path) or os.path.getsize(path) == 0:
        with open(path, "w") as fp:
            fp.write("#" + "\t".join(columns) + "\n")
        return
    with open(path) as fp:
        first = fp.readline().rstrip("\n")
    if first != "#" + "\t".join(columns):
        raise SystemExit("run_arms: %s has a different header; refusing to mix formats" % path)
    # A line cut by a crash must not run into the next row.
    with open(path, "rb+") as fp:
        fp.seek(-1, os.SEEK_END)
        if fp.read(1) != b"\n":
            fp.write(b"\n")


def protocol_fingerprint(check_wall, seed):
    """(sha12, text) of the protocol parameters that change outcomes."""
    text = ("check_wall=%s;seed=%s;gated=%s;default-on=%s;repeat=%s;calib=gm%.2f/single%.2f/passes%d;"
            "caps=%dx%dMiB|%d" % (check_wall, seed, ",".join(GATED_FLAGS),
                                  ",".join(GATED_DEFAULT_ON), REPEAT_READING,
                                  SPEED_TOLERANCE, SINGLE_TOLERANCE, CALIB_PASSES, MAX_PAR,
                                  SLICE_RSS_MB, SMALL_POD_RSS_MB))
    return sha256_text(text)[:12], text


# ----------------------------------------------------------------------------
# Output parsing (pure functions; unit-tested in test_stats.py)


def parse_szs_status(stdout_text):
    m = re.search(r"^% SZS status (\S+)", stdout_text, re.M)
    return m.group(1) if m else "NoOutput"


def proof_block(stdout_text):
    """(present, form): a complete SZS output block on stdout."""
    start = re.search(r"^% SZS output start (\S+)", stdout_text, re.M)
    if start is None:
        return False, ""
    form = start.group(1)
    end = re.compile(r"^% SZS output end " + re.escape(form) + r"\b", re.M)
    if end.search(stdout_text, start.end()) is None:
        return False, form
    return True, form


def proof_records(stdout_text):
    """Number of cnf/fof records inside the SZS output block (0 for an
    empty block, as in J13's "no single-NF chain extractable" class)."""
    m = re.search(r"^% SZS output start.*?$(.*?)^% SZS output end", stdout_text, re.M | re.S)
    if m is None:
        return 0
    return len(re.findall(r"^(?:cnf|fof|tff)\(", m.group(1), re.M))


def parse_winner(stderr_text):
    m = re.search(r"^% FindProof schedule winner: (\S+)", stderr_text, re.M)
    return m.group(1) if m else ""


def stopped_at_wall(stderr_text):
    return re.search(r"^FindProof: schedule stopped at the wall", stderr_text, re.M) is not None


def _int_or_none(s):
    try:
        return int(s)
    except (TypeError, ValueError):
        return None


def classify_lane(text, stopped, has_winner):
    """End class of one schedule slice from its stderr (THVM_ATP_SCHED_ERRDIR).

    Markers as in the lane-matrix runner: the engine's "=> STATUS" line,
    "thvm fatal:", FindProof give-up reasons, the "[wall cap" line, the
    step cap.  A slice with no end marker was still running when the
    schedule stopped it (at the wall, or after another slice won).
    Returns (class, detail, steps)."""
    est_m = re.search(r"^=> (.*)$", text, re.M)
    est = est_m.group(1).strip() if est_m else ""
    steps = None
    stats = re.search(r"^   goal=.*?steps=(\d+)\s+rules=(\d+)\s+cps=(\d+)", text, re.M)
    if stats:
        steps = int(stats.group(1))
    else:
        prog = re.findall(r"^  step +(\d+) +rules=(\d+) +cps=(\d+)", text, re.M)
        if prog:
            steps = int(prog[-1][0])
    cap_m = re.search(r"  step_cap=(\d+)", text)
    stepcap = _int_or_none(cap_m.group(1)) if cap_m else None
    fatal_m = re.search(r"^thvm fatal: (.*)$", text, re.M)
    reasons = []
    for line in text.splitlines():
        if not line.startswith("FindProof: "):
            continue
        if line.startswith("FindProof: schedule stopped at the wall"):
            continue
        if line.startswith("FindProof: all schedule slices"):
            continue
        if line.startswith("FindProof: proof withheld"):
            continue
        reasons.append(line[len("FindProof: "):].strip())
    if est.startswith("PROVED"):
        return "proved", est, steps
    if fatal_m:
        msg = re.sub(r" \[phase=.*", "", fatal_m.group(1)).strip()
        if "from-space exhausted" in msg:
            return "memory-guard", "term heap exhausted (thvm fatal)", steps
        return "crash", "thvm fatal: " + msg, steps
    if est.startswith("ABORTED"):
        return "memory-guard", est, steps
    if est.startswith("QUEUE_EMPTY") or est.startswith("REFUTED"):
        return "saturated", est, steps
    if est.startswith("RUNNING"):
        if re.search(r"^  \[wall cap ", text, re.M):
            return "wall", est, steps
        return "step-cap", est, steps
    if est.startswith("TIMEOUT"):
        if stepcap is not None and steps is not None and steps >= stepcap - 1:
            return "step-cap", est, steps
        return "wall", est, steps
    if reasons:
        r = reasons[-1]
        if "no proof within budget" in r:
            return "saturated", r, steps
        if "spent the wall" in r or "reached the wall" in r:
            return "wall", r, steps
        if "OOM" in r or "exhausted the term heap" in r:
            return "memory-guard", r, steps
        return "immediate-exit", r, steps
    if stopped:
        return "wall", "running at the wall stop", steps
    if has_winner:
        return "stopped-by-winner", "", steps
    last = ""
    for line in text.splitlines():
        if line.strip():
            last = line.strip()
    return "unknown", last[:160], steps


MISS_PRECEDENCE = ("memory-guard", "step-cap", "saturated", "crash", "immediate-exit")


def classify_run(rc, stderr_text, lane_counts, winner, status=""):
    """End class of a schedule run.  A run with a winner (or, from a build
    that names none, an Unsatisfiable status) is "proved"; a miss
    takes the first that applies: alarm (the outer timeout, rc 124/137, or
    the parent's own wall alarm, rc 142), crash (input error, other
    signal), wall (the schedule stopped at the wall), then the lane classes
    by precedence memory-guard, step-cap, saturated, crash, immediate-exit."""
    if winner or status == "Unsatisfiable":
        return "proved"
    if rc in (124, 137, 128 + signal.SIGALRM):
        return "alarm"
    if rc == 2 or "all schedule slices reported an input error" in stderr_text:
        return "crash"
    if rc >= 128:
        return "crash"
    if stopped_at_wall(stderr_text):
        return "wall"
    for cls in MISS_PRECEDENCE:
        if lane_counts.get(cls):
            return cls
    return "unknown"


def lane_histogram(lane_classes):
    counts = {}
    for cls, _detail, _steps in lane_classes.values():
        counts[cls] = counts.get(cls, 0) + 1
    return counts


def histogram_text(counts):
    return ",".join("%s:%d" % (k, counts[k]) for k in sorted(counts))


def clock_gated_flags(flags):
    """The GATED_RULES flags a lane's flags leave on, the last value of a
    repeated flag winning as in FindProof's setenv."""
    last = {}
    for tok in flags.split():
        name, eq, val = tok.partition("=")
        last[name] = val if eq else "1"
    return [name for name, on in GATED_RULES if on(last.get(name))]


def is_clock_gated(flags):
    return bool(clock_gated_flags(flags))


def parse_lane_listing(text):
    """[(lane, flags)] from FindProof --list-lanes=flags ("name TAB flags" per
    line); None when the listing has no flags column or is empty."""
    lanes = []
    for line in text.splitlines():
        if not line.strip():
            continue
        name, tab, flags = line.partition("\t")
        if not tab:
            return None
        lanes.append((name.strip(), flags.strip()))
    return lanes or None


class UnknownLane(Exception):
    pass


def repeat_decision(pass1, tables):
    """pass1: arm -> (solution 0/1, winning lane); tables: arm -> {lane:
    flags}, each arm's own lane table.  Returns (repeat, reason); raises
    UnknownLane for a winning lane its arm's table lacks."""
    sols = {v[0] for v in pass1.values()}
    reasons = []
    if len(sols) > 1:
        reasons.append("disagree")
    gated = set()
    for arm in sorted(pass1):
        lane = pass1[arm][1]
        if not lane:
            continue
        table = tables.get(arm)
        if table is None or lane not in table:
            raise UnknownLane("arm %s won with lane %r, which its lane table lacks" % (arm, lane))
        if is_clock_gated(table[lane]):
            gated.add(lane)
    if gated:
        reasons.append("clock-gated:" + "+".join(sorted(gated)))
    return (1 if reasons else 0), (";".join(reasons) if reasons else "-")


def solution_flag(status, proof_present, form, in_wall, verdict):
    return int(status == "Unsatisfiable" and proof_present and form == "CNFRefutation"
               and in_wall and verdict == "VerifiedGood")


def shard_select(names, k, n):
    ordered = sorted(names, key=lambda x: (domain_of(x), x))
    return [x for i, x in enumerate(ordered) if i % n == k - 1]


def problem_complete(rows, decisions, problem, arms, walls, passes, repeats):
    """rows: this problem's row dicts; walls as fmt_num strings."""
    have = {(r["arm"], r["wall"], r["pass"]) for r in rows}
    for w in walls:
        for a in arms:
            for k in range(1, passes + 1):
                if (a, w, str(k)) not in have:
                    return False
        if repeats:
            dec = decisions.get((problem, w))
            if dec is None:
                return False
            if dec[0]:
                for a in arms:
                    for k in REPEAT_PASSES:
                        if (a, w, str(k)) not in have:
                            return False
    return True


def problems_to_supersede(rows, decisions, arms, walls, passes, repeats, host):
    """Problems whose runs are incomplete and include a row from another
    pod: they are rerun whole on this pod (all arms of a problem on one)."""
    by = collections.defaultdict(list)
    for r in rows:
        by[r["problem"]].append(r)
    out = []
    for p, rs in by.items():
        if all(r["host"] == host for r in rs):
            continue
        if problem_complete(rs, decisions, p, arms, walls, passes, repeats):
            continue
        out.append(p)
    return sorted(out)


def calibration_verdict(results):
    """Per problem the median time ratio over its passes; the pod passes
    when every problem is solved in every pass, the geometric mean of the
    ratios is within SPEED_TOLERANCE of 1 and no ratio is more than
    SINGLE_TOLERANCE off."""
    by = collections.OrderedDict()
    for r in results:
        by.setdefault(r["name"], []).append(r)
    ratios = {}
    unsolved = 0
    for name, rs in by.items():
        if not all(x["ok"] for x in rs) or not rs[0].get("ref_s"):
            unsolved += 1
            continue
        ratios[name] = statistics.median(x["wall_s"] for x in rs) / rs[0]["ref_s"]
    vals = list(ratios.values())
    gm = math.exp(sum(math.log(x) for x in vals) / len(vals)) if vals else float("nan")
    worst_name = max(ratios, key=lambda k: abs(ratios[k] - 1.0)) if ratios else "-"
    worst = abs(ratios[worst_name] - 1.0) if ratios else float("nan")
    ok = bool(unsolved == 0 and vals and abs(gm - 1.0) <= SPEED_TOLERANCE
              and worst <= SINGLE_TOLERANCE)
    return {"ok": ok, "geomean": gm, "n": len(vals), "unsolved": unsolved,
            "worst": worst, "worst_name": worst_name}


# ----------------------------------------------------------------------------
# Host facts and process control


def _cgroup_dirs(controller):
    """[(version, dir)] for `controller`, from this process's own cgroup
    up to the hierarchy root, the existing ones only."""
    v2 = None
    v1 = {}
    for line in read_text("/proc/self/cgroup").splitlines():
        parts = line.split(":", 2)
        if len(parts) != 3:
            continue
        hid, ctrls, path = parts
        if hid == "0" and ctrls == "":
            v2 = path
        else:
            for c in ctrls.split(","):
                v1[c] = path

    def up(base, path):
        out = []
        p = (path or "").strip("/")
        while p:
            out.append(os.path.join(base, p))
            p = os.path.dirname(p)
        out.append(base)
        return [d for d in out if os.path.isdir(d)]

    dirs = []
    if v2 is not None and os.path.exists("/sys/fs/cgroup/cgroup.controllers"):
        dirs += [("v2", d) for d in up("/sys/fs/cgroup", v2)]
    if controller in v1:
        try:
            mounts = [os.path.join("/sys/fs/cgroup", n) for n in os.listdir("/sys/fs/cgroup")
                      if controller in n.split(",")]
        except OSError:
            mounts = []
        for m in mounts:
            dirs += [("v1", d) for d in up(m, v1[controller])]
    return dirs


def mem_facts():
    """MemTotal, the tightest cgroup memory limit, and their minimum (kB)."""
    m = re.search(r"^MemTotal:\s+(\d+)\s+kB", read_text("/proc/meminfo"), re.M)
    total = int(m.group(1)) if m else 0
    limits = []
    for ver, d in _cgroup_dirs("memory"):
        f = "memory.max" if ver == "v2" else "memory.limit_in_bytes"
        v = read_text(os.path.join(d, f)).strip()
        if v.isdigit() and int(v) < (1 << 60):
            limits.append(int(v) // 1024)
    cg = min(limits) if limits else None
    eff = min([total] + ([cg] if cg is not None else []))
    return {"memtotal_kb": total, "cgroup_kb": cg, "eff_kb": eff}


def mem_text(f):
    return "memtotal=%d;cgroup=%s;eff=%d" % (f["memtotal_kb"], f["cgroup_kb"] or "none",
                                            f["eff_kb"])


def caps_need_kb(max_par):
    return max_par * SLICE_RSS_MB * 1024


def oom_count():
    """OOM kills counted by this cgroup (memory.events / oom_control), else
    by the kernel (/proc/vmstat); None when neither is readable."""
    for ver, d in _cgroup_dirs("memory"):
        f = "memory.events" if ver == "v2" else "memory.oom_control"
        m = re.search(r"^oom_kill (\d+)", read_text(os.path.join(d, f)), re.M)
        if m:
            return int(m.group(1))
    m = re.search(r"^oom_kill (\d+)", read_text("/proc/vmstat"), re.M)
    return int(m.group(1)) if m else None


def cpu_facts():
    """Affinity CPUs, the tightest cgroup CPU quota (CPUs), and the
    effective count: the affinity capped by the floor of the quota."""
    try:
        aff = len(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        aff = os.cpu_count() or 0
    quotas = []
    for ver, d in _cgroup_dirs("cpu"):
        if ver == "v2":
            v = read_text(os.path.join(d, "cpu.max")).split()
            if len(v) == 2 and v[0] != "max" and v[0].isdigit() and v[1].isdigit():
                quotas.append(int(v[0]) / float(int(v[1])))
        else:
            q = read_text(os.path.join(d, "cpu.cfs_quota_us")).strip()
            per = read_text(os.path.join(d, "cpu.cfs_period_us")).strip()
            if q.lstrip("-").isdigit() and int(q) > 0 and per.isdigit() and int(per) > 0:
                quotas.append(int(q) / float(int(per)))
    quota = min(quotas) if quotas else None
    eff = aff if quota is None else min(aff, int(math.floor(quota + 1e-9)))
    return {"affinity": aff, "quota": quota, "eff": eff}


def lscpu_summary(cpu=None):
    cpu = cpu or cpu_facts()
    info = {}
    try:
        out = subprocess.run(["lscpu"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             universal_newlines=True, timeout=30, stdin=subprocess.DEVNULL,
                             env={"LC_ALL": "C", "PATH": "/usr/bin:/bin"}).stdout
        for line in out.splitlines():
            k, _, v = line.partition(":")
            info[k.strip()] = v.strip()
    except (OSError, subprocess.SubprocessError):
        pass
    model = info.get("Model name", "")
    if not model:
        txt = read_text("/proc/cpuinfo")
        m = re.search(r"^model name\s*:\s*(.*)$", txt, re.M)
        model = m.group(1).strip() if m else "unknown"
    return clean_field("%s|tpc=%s|cpus=%s|cps=%s|sockets=%s|aff=%d|quota=%s" % (
        model, info.get("Thread(s) per core", "?"), info.get("CPU(s)", "?"),
        info.get("Core(s) per socket", "?"), info.get("Socket(s)", "?"), cpu["affinity"],
        "%g" % cpu["quota"] if cpu["quota"] is not None else "none"))


def proc_entries():
    for d in os.listdir("/proc"):
        if d.isdigit():
            yield int(d)


def proc_session(pid):
    try:
        with open("/proc/%d/stat" % pid) as fp:
            stat = fp.read()
    except OSError:
        return None
    rest = stat[stat.rfind(")") + 2:].split()
    if len(rest) < 4:
        return None
    return int(rest[3])


def kill_session(sid):
    n = 0
    me = os.getpid()
    for pid in proc_entries():
        if pid == me:
            continue
        if proc_session(pid) == sid:
            try:
                os.kill(pid, signal.SIGKILL)
                n += 1
            except OSError:
                pass
    return n


def kill_binaries(real_paths):
    n = 0
    me = os.getpid()
    for pid in proc_entries():
        if pid == me:
            continue
        try:
            exe = os.readlink("/proc/%d/exe" % pid)
        except OSError:
            continue
        if exe.endswith(" (deleted)"):
            exe = exe[:-len(" (deleted)")]
        if exe in real_paths:
            try:
                os.kill(pid, signal.SIGKILL)
                n += 1
            except OSError:
                pass
    return n


def running_binaries(real_paths):
    found = []
    for pid in proc_entries():
        try:
            exe = os.readlink("/proc/%d/exe" % pid)
        except OSError:
            continue
        if exe.replace(" (deleted)", "") in real_paths:
            found.append(pid)
    return found


def clean_tmp():
    for pat in TMP_GLOBS:
        for f in glob.glob(pat):
            try:
                if os.path.isdir(f):
                    shutil.rmtree(f, ignore_errors=True)
                else:
                    os.unlink(f)
            except OSError:
                pass


def exit_code(status):
    if os.WIFEXITED(status):
        return os.WEXITSTATUS(status)
    if os.WIFSIGNALED(status):
        return 128 + os.WTERMSIG(status)
    return 255


def spawn(argv, cwd, out_path, err_path, hard_s):
    """Run argv in its own session; return (rc, elapsed, cpu_s, maxrss_kb,
    sid, killed).  The runner kills the session past hard_s (a stuck run)."""
    with open(out_path, "wb") as fo, open(err_path, "wb") as fe:
        t0 = time.monotonic()
        p = subprocess.Popen(argv, cwd=cwd, stdin=subprocess.DEVNULL, stdout=fo,
                             stderr=fe, start_new_session=True, close_fds=True)
        sid = p.pid
        killed = False
        while True:
            try:
                pid, status, ru = os.wait4(p.pid, os.WNOHANG)
            except ChildProcessError:
                pid, status, ru = p.pid, 255 << 8, None
            if pid != 0:
                break
            if STOP["signal"] or time.monotonic() - t0 > hard_s:
                killed = True
                kill_session(sid)
                try:
                    os.killpg(sid, signal.SIGKILL)
                except OSError:
                    pass
                pid, status, ru = os.wait4(p.pid, 0)
                break
            time.sleep(0.02)
        elapsed = time.monotonic() - t0
    rc = exit_code(status)
    p.returncode = rc
    cpu = (ru.ru_utime + ru.ru_stime) if ru is not None else 0.0
    rss = ru.ru_maxrss if ru is not None else 0
    return rc, elapsed, cpu, rss, sid, killed


def wall_of_flags(flags):
    m = re.search(r"(?:^|\s)--wall=([0-9.]+)(?:\s|$)", flags)
    return float(m.group(1)) if m else None


def read_lane_table(timeout_bin, binary, flags):
    """The arm binary's own lane table under the arm's flags: ([(lane,
    flags)], "") or (None, reason)."""
    argv = (["/usr/bin/env", "-i", timeout_bin, "-k", "5", "30", binary] + flags.split()
            + ["--list-lanes=flags"])
    try:
        r = subprocess.run(argv, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, universal_newlines=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as e:
        return None, clean_field(e)
    if r.returncode != 0:
        first = (r.stderr.strip().splitlines() or [""])[0]
        return None, "exit %d: %s" % (r.returncode, clean_field(first))
    lanes = parse_lane_listing(r.stdout)
    if lanes is None:
        return None, "no 'name TAB flags' listing (the binary predates it)"
    return lanes, ""


# ----------------------------------------------------------------------------
# Inputs


class Problem(object):
    def __init__(self, name, path, split):
        self.name = name
        self.path = path
        self.split = split


class Arm(object):
    def __init__(self, name, binary, flags):
        self.name = name
        self.binary = binary
        self.flags = flags
        self.sha = ""
        self.lanes = None
        self.lanes_note = ""


def load_problems(path):
    probs = []
    seen = set()
    with open(path) as fp:
        for line in fp:
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            f = line.split("\t")
            if len(f) < 3:
                raise SystemExit("run_arms: problem line needs name, path, split: %r" % line)
            name, ppath, split = f[0].strip(), f[1].strip(), f[2].strip()
            # The name is a path component (runs/<name>/).
            if not PROBLEM_NAME.match(name) or name in (".", ".."):
                raise SystemExit("run_arms: problem name %r: use letters, digits, _ . + = ^ -"
                                 % name)
            if name in seen:
                raise SystemExit("run_arms: duplicate problem %s" % name)
            seen.add(name)
            probs.append(Problem(name, ppath, split))
    if not probs:
        raise SystemExit("run_arms: no problems in %s" % path)
    return probs


def load_arms(path):
    arms = []
    seen = set()
    with open(path) as fp:
        for line in fp:
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            f = line.split("\t")
            if len(f) < 2:
                raise SystemExit("run_arms: arm line needs name, binary[, flags]: %r" % line)
            name, binary = f[0].strip(), f[1].strip()
            flags = f[2].strip() if len(f) > 2 else ""
            if not ARM_NAME.match(name) or name in (".", ".."):
                raise SystemExit("run_arms: arm name %r: use letters, digits, _ . + -" % name)
            if name in seen:
                raise SystemExit("run_arms: duplicate arm %s" % name)
            if re.search(r"(?:^|\s)--(?:wall|szs|proof|list-lanes)(?:=|\s|$)", flags):
                raise SystemExit("run_arms: arm %s: --szs, --proof and --wall are added by the "
                                 "runner (and --list-lanes is not a run)" % name)
            seen.add(name)
            arms.append(Arm(name, os.path.abspath(os.path.expanduser(binary)), flags))
    if not arms:
        raise SystemExit("run_arms: no arms in %s" % path)
    return arms


def load_calib_ref(path):
    meta = {}
    rows = []
    with open(path) as fp:
        for line in fp:
            line = line.rstrip("\n")
            if not line.strip():
                continue
            if line.startswith("#"):
                k, eq, v = line[1:].partition("=")
                if eq:
                    meta[k.strip()] = v.strip()
                continue
            f = line.split("\t")
            if len(f) < 3:
                continue
            rows.append({"name": f[0], "path": f[1], "ref_s": float(f[2])})
    if not rows:
        raise SystemExit("run_arms: no calibration rows in %s" % path)
    for k in ("bin_sha", "flags", "rss_cap", "max_par"):
        if k not in meta:
            raise SystemExit("run_arms: calibration reference %s lacks #%s=" % (path, k))
    return meta, rows


def parse_walls(text):
    walls = []
    for w in text.split(","):
        w = w.strip()
        if not w:
            continue
        v = float(w)
        if v <= 0:
            raise SystemExit("run_arms: wall must be positive: %s" % w)
        walls.append(v)
    if len(set(walls)) != len(walls) or not walls:
        raise SystemExit("run_arms: walls must be distinct and non-empty")
    return walls


def parse_split_roots(items):
    roots = {}
    for it in items or []:
        k, eq, v = it.partition("=")
        if not eq:
            raise SystemExit("run_arms: --split-root takes SPLIT=DIR, got %r" % it)
        roots[k] = os.path.abspath(os.path.expanduser(v))
    return roots


def parse_same_lanes(items):
    out = set()
    for it in items or []:
        a, _, b = it.partition(",")
        if not a or not b:
            raise SystemExit("run_arms: --same-lanes takes A,B")
        out.add(frozenset((a, b)))
    return out


def job_worst_s(wall, check_wall):
    """The runner's bound on one job: the hard kill at W + 50, plus the
    checker's at its wall + 50 (check_wall None: no checker)."""
    return wall + 50 + ((check_wall + 50) if check_wall is not None else 0)


# ----------------------------------------------------------------------------
# The runner


class Runner(object):
    def __init__(self, args):
        self.args = args
        self.out = os.path.abspath(os.path.expanduser(args.out))
        self.rows_path = os.path.join(self.out, "rows.tsv")
        self.dec_path = os.path.join(self.out, "decisions.tsv")
        self.lanes_path = os.path.join(self.out, "lanes.jsonl")
        self.log_path = os.path.join(self.out, "run.log")
        self.scratch = os.path.join(self.out, "scratch")
        self.seed = args.seed
        self.walls = parse_walls(args.walls)
        self.passes = args.passes
        self.repeats = (args.passes == 1) and not args.no_repeats
        self.root = os.path.abspath(os.path.expanduser(args.root))
        self.split_roots = parse_split_roots(args.split_root)
        all_probs = load_problems(args.problems)
        self.n_all_problems = len(all_probs)
        if args.shard:
            k, _, n = args.shard.partition("/")
            k, n = int(k), int(n)
            if not (1 <= k <= n):
                raise SystemExit("run_arms: --shard K/N with 1 <= K <= N")
            keep = set(shard_select([p.name for p in all_probs], k, n))
            all_probs = [p for p in all_probs if p.name in keep]
        self.problems = sorted(all_probs, key=lambda p: order_key(self.seed, "problem", p.name))
        self.arms = load_arms(args.arms)
        self.checker = None
        if not args.no_check:
            if not args.checker:
                raise SystemExit("run_arms: --checker <CheckProof> is required (or --no-check)")
            self.checker = os.path.abspath(os.path.expanduser(args.checker))
            if not os.path.basename(self.checker).startswith("CheckProof"):
                raise SystemExit("run_arms: the checker's file name must start with CheckProof "
                                 "(the name selects the strict profile)")
        self.check_wall = args.check_wall
        sealed = args.sealed
        if sealed == "auto":
            sealed = "on" if any(p.split == "j13" for p in self.problems) else "off"
        self.sealed = sealed == "on"
        self.errdir_on = not args.no_errdir
        self.rss_label = args.rss_cap
        self.rss_cap = None if args.rss_cap == "default" else int(args.rss_cap)
        self.same_lanes = parse_same_lanes(args.same_lanes)
        self.protocol_sha, self.protocol_text = protocol_fingerprint(self.check_wall, self.seed)
        self.deadline = None
        self.max_par = args.max_par
        self.lscpu = ""
        self.host = socket.gethostname()
        self.mem = {"memtotal_kb": 0, "cgroup_kb": None, "eff_kb": 0}
        self.cpu = {"affinity": 0, "quota": None, "eff": 0}
        self.done_keys = set()
        self.rows_by_key = {}
        self.decisions = {}
        self.jobs_run = 0
        self.base_total = len(self.problems) * len(self.walls) * len(self.arms) * self.passes
        self.base_done = 0
        self.real_bins = set()
        self.locked = False
        self.deviations = []
        self.hb_stop = threading.Event()

    # -- logging

    def log(self, msg):
        line = "%s %s" % (utc_now(), msg)
        print(line, flush=True)
        try:
            with open(self.log_path, "a") as fp:
                fp.write(line + "\n")
        except OSError:
            pass

    def deviation(self, msg):
        self.deviations.append(msg)
        self.log("DEVIATION: " + msg)

    def root_for(self, split):
        return self.split_roots.get(split, self.root)

    def problem_abs(self, prob):
        return os.path.join(self.root_for(prob.split), prob.path)

    # -- preflight

    def check_pod(self):
        """Effective CPUs and memory against the protocol (also calib-ref)."""
        self.cpu = cpu_facts()
        if self.cpu["eff"] != self.max_par:
            if not self.args.allow_nproc:
                raise SystemExit("run_arms: %d effective CPUs (affinity %d, cgroup quota %s); "
                                 "the protocol is %d (--allow-nproc to override, logged)" % (
                                     self.cpu["eff"], self.cpu["affinity"],
                                     self.cpu["quota"], self.max_par))
            self.deviation("%d effective CPUs (protocol: %d)" % (self.cpu["eff"], self.max_par))
        self.mem = mem_facts()
        need = caps_need_kb(self.max_par)
        if self.rss_cap is None and self.mem["eff_kb"] < need:
            raise SystemExit("run_arms: --rss-cap default needs an effective memory limit of "
                             "%d x %d MiB = %d kB; this pod has %d kB (%s); use a larger pod or "
                             "run the whole comparison with --rss-cap %d" % (
                                 self.max_par, SLICE_RSS_MB, need, self.mem["eff_kb"],
                                 mem_text(self.mem), SMALL_POD_RSS_MB))
        self.lscpu = lscpu_summary(self.cpu)

    def preflight(self):
        if not os.access("/usr/bin/env", os.X_OK):
            raise SystemExit("run_arms: missing /usr/bin/env")
        self.timeout_bin = shutil.which("timeout", path="/usr/bin:/bin")
        if not self.timeout_bin:
            raise SystemExit("run_arms: missing coreutils timeout")
        if not os.path.isdir("/proc/self"):
            raise SystemExit("run_arms: needs Linux /proc")
        self.check_pod()
        for arm in self.arms:
            if not os.access(arm.binary, os.X_OK):
                raise SystemExit("run_arms: arm %s: no executable %s" % (arm.name, arm.binary))
            arm.sha = sha256_file(arm.binary)
            self.real_bins.add(os.path.realpath(arm.binary))
        shas = {a.sha for a in self.arms}
        if len(shas) > 1:
            if not self.args.allow_binaries:
                raise SystemExit("run_arms: the arms use %d different binaries; section 1.1 "
                                 "runs every arm on one binary (--allow-binaries to override, "
                                 "logged)" % len(shas))
            self.deviation("arms on %d binaries: %s" % (len(shas), " ".join(
                "%s:%s" % (a.name, a.sha[:12]) for a in self.arms)))
        self.read_lane_tables()
        if self.checker:
            if not os.access(self.checker, os.X_OK):
                raise SystemExit("run_arms: no executable checker %s" % self.checker)
            self.checker_sha = sha256_file(self.checker)
            self.real_bins.add(os.path.realpath(self.checker))
        missing = [self.problem_abs(p) for p in self.problems
                   if not os.path.isfile(self.problem_abs(p))]
        if missing:
            raise SystemExit("run_arms: %d problem files missing, e.g. %s" % (len(missing), missing[0]))
        for split in {p.split for p in self.problems}:
            if not os.path.isdir(self.root_for(split)):
                raise SystemExit("run_arms: no root directory for split %s" % split)
        if self.args.calib_bin:
            self.real_bins.add(os.path.realpath(os.path.abspath(os.path.expanduser(self.args.calib_bin))))
        busy = running_binaries(self.real_bins)
        if busy:
            raise SystemExit("run_arms: %d prover/checker processes already running; "
                             "the protocol needs an idle pod" % len(busy))
        self.wait_idle()

    def read_lane_tables(self):
        for arm in self.arms:
            lanes, why = read_lane_table(self.timeout_bin, arm.binary, arm.flags)
            if lanes is None:
                if self.repeats or len(self.arms) > 1:
                    raise SystemExit("run_arms: arm %s: no lane table from '%s --list-lanes=flags' "
                                     "(%s); the repeat rule and the arm check need it" % (
                                         arm.name, arm.flags, why))
                arm.lanes_note = why
                continue
            names = [n for n, _f in lanes]
            if len(set(names)) != len(names):
                raise SystemExit("run_arms: arm %s: duplicate lane names in its table" % arm.name)
            arm.lanes = collections.OrderedDict(lanes)
            write_file(os.path.join(self.out, "lanes_%s.tsv" % arm.name),
                       "".join("%s\t%s\n" % kv for kv in lanes))
        for i, a in enumerate(self.arms):
            for b in self.arms[i + 1:]:
                if a.lanes is None or b.lanes is None:
                    continue
                if (a.flags != b.flags and a.sha == b.sha and list(a.lanes.items())
                        == list(b.lanes.items())
                        and frozenset((a.name, b.name)) not in self.same_lanes):
                    raise SystemExit(
                        "run_arms: arms %s and %s differ in flags but list the same lanes on "
                        "one binary: a schedule mode may be ignored (declare an intended pair "
                        "with --same-lanes %s,%s)" % (a.name, b.name, a.name, b.name))

    def wait_idle(self):
        t0 = time.monotonic()
        while True:
            try:
                load1 = float(read_text("/proc/loadavg").split()[0])
            except (IndexError, ValueError):
                return
            if load1 < 1.5:
                return
            if time.monotonic() - t0 > 600:
                raise SystemExit("run_arms: 1-minute load %.2f after 10 min: the pod is not idle" % load1)
            if STOP["signal"]:
                raise SystemExit("run_arms: terminated while waiting for an idle pod")
            time.sleep(15)

    def cond(self):
        return "maxpar=%d;rss=%s;errdir=%d;proof=1;check_wall=%s;protocol=%s" % (
            self.max_par, self.rss_label, 1 if self.errdir_on else 0,
            self.check_wall if self.checker else "none", self.protocol_sha)

    def job_env(self, root, errdir):
        env = ["TPTP=" + root, "THVM_ATP_SCHED_MAX_PAR=%d" % self.max_par]
        if self.rss_cap:
            env.append("THVM_ATP_RSS_ABORT_MB=%d" % self.rss_cap)
        if errdir:
            env.append("THVM_ATP_SCHED_ERRDIR=" + errdir)
        return env

    def argv_for(self, arm, prob, wall, errdir):
        root = self.root_for(prob.split)
        return (["/usr/bin/env", "-i"] + self.job_env(root, errdir)
                + [self.timeout_bin, "-k", "5", fmt_num(wall + 15), arm.binary]
                + arm.flags.split()
                + ["--szs", "--proof", "--wall=" + fmt_num(wall), self.problem_abs(prob)])

    # -- configuration and state

    def config(self):
        prob_text = "".join("%s\t%s\t%s\n" % (p.name, p.path, p.split)
                            for p in sorted(self.problems, key=lambda p: p.name))
        cfg = collections.OrderedDict([
            ("arms", [[a.name, a.sha, a.flags] for a in self.arms]),
            ("walls", [fmt_num(w) for w in self.walls]),
            ("passes", self.passes), ("repeats", int(self.repeats)), ("seed", str(self.seed)),
            ("problems_sha", sha256_text(prob_text)), ("problems", len(self.problems)),
            ("max_par", self.max_par), ("rss_cap", self.rss_label),
            ("checker_sha", getattr(self, "checker_sha", "none")),
            ("check_wall", self.check_wall if self.checker else "none"),
            ("errdir", int(self.errdir_on)), ("sealed", int(self.sealed)),
            ("protocol", self.protocol_sha),
        ])
        text = json.dumps(cfg, sort_keys=True)
        return sha256_text(text)[:16], text

    def check_config(self):
        fp, text = self.config()
        path = os.path.join(self.out, "run.config")
        old = read_text(path)
        if not old:
            write_file(path, "fingerprint=%s\n%s\n" % (fp, text))
            return fp
        m = re.match(r"fingerprint=(\S+)\n(.*)", old, re.S)
        if m is None or m.group(1) != fp:
            old_fp = m.group(1) if m else "?"
            if not self.args.force_config:
                raise SystemExit("run_arms: %s holds configuration %s; this launch is %s.  "
                                 "Rows of different configurations do not mix (compare %s with "
                                 "this run's); --force-config to override, logged" % (
                                     path, old_fp, fp, path))
            self.deviation("resume under configuration %s over %s (--force-config)" % (fp, old_fp))
        return fp

    def load_state(self):
        _h, rows = read_tsv(self.rows_path)
        _h, decs = read_tsv(self.dec_path)
        decisions = {(d["problem"], d["wall"]): (int(d["repeat"]), d["reason"]) for d in decs}
        names = {p.name for p in self.problems}
        sup = problems_to_supersede([r for r in rows if r["problem"] in names], decisions,
                                    [a.name for a in self.arms],
                                    [fmt_num(w) for w in self.walls], self.passes,
                                    self.repeats, self.host)
        if sup:
            self.supersede(set(sup), rows, decs)
            rows = [r for r in rows if r["problem"] not in sup]
            decisions = {k: v for k, v in decisions.items() if k[0] not in sup}
            self.log("resume: %d problems had partial runs from another pod; they rerun whole "
                     "here (rows in superseded.tsv)" % len(sup))
        for r in rows:
            key = (r["problem"], r["arm"], r["wall"], r["pass"])
            self.done_keys.add(key)
            self.rows_by_key[key] = r
            if r["problem"] in names and int(r["pass"]) <= self.passes:
                self.base_done += 1
        self.decisions = decisions

    def supersede(self, probs, rows, decs):
        sup_path = os.path.join(self.out, "superseded.tsv")
        ensure_header(sup_path, ROW_COLUMNS)
        with open(sup_path, "a") as fp:
            for r in rows:
                if r["problem"] in probs:
                    fp.write("\t".join(r.get(c, "") for c in ROW_COLUMNS) + "\n")
        keep = [r for r in rows if r["problem"] not in probs]
        write_file(self.rows_path, "#" + "\t".join(ROW_COLUMNS) + "\n" + "".join(
            "\t".join(r.get(c, "") for c in ROW_COLUMNS) + "\n" for r in keep))
        write_file(self.dec_path, "#" + "\t".join(DECISION_COLUMNS) + "\n" + "".join(
            "\t".join(d[c] for c in DECISION_COLUMNS) + "\n" for d in decs
            if d["problem"] not in probs))
        if os.path.exists(self.lanes_path):
            kept = []
            with open(self.lanes_path) as fp:
                for line in fp:
                    try:
                        if json.loads(line).get("problem") in probs:
                            continue
                    except ValueError:
                        continue
                    kept.append(line)
            write_file(self.lanes_path, "".join(kept))
        stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
        for p in probs:
            src = os.path.join(self.out, "runs", p)
            if os.path.isdir(src):
                dst = os.path.join(self.out, "superseded", "%s.%s" % (p, stamp))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.move(src, dst)

    # -- the seal

    def unpack_sealed(self):
        path = os.path.join(self.out, "sealed.tgz")
        if not os.path.exists(path):
            return
        os.chmod(path, 0o600)
        with tarfile.open(path, "r:gz") as tf:
            for m in tf.getmembers():
                if m.name.startswith("/") or ".." in m.name.split("/"):
                    raise SystemExit("run_arms: %s holds an unsafe path %r" % (path, m.name))
            try:
                tf.extractall(self.out, filter="data")
            except TypeError:
                tf.extractall(self.out)
        os.unlink(path)

    def pack_sealed(self):
        """Every per-problem output into sealed.tgz (mode 0400); the scratch
        files of the last job are deleted."""
        shutil.rmtree(self.scratch, ignore_errors=True)
        members = [m for m in SEALED_MEMBERS if os.path.exists(os.path.join(self.out, m))]
        if not members:
            return
        path = os.path.join(self.out, "sealed.tgz")
        if os.path.exists(path):
            raise SystemExit("run_arms: %s exists next to unpacked outputs" % path)
        tmp = path + ".tmp"
        with tarfile.open(tmp, "w:gz") as tf:
            for m in members:
                tf.add(os.path.join(self.out, m), arcname=m)
        os.chmod(tmp, 0o400)
        os.replace(tmp, path)
        for m in members:
            p = os.path.join(self.out, m)
            if os.path.isdir(p):
                shutil.rmtree(p, ignore_errors=True)
            else:
                os.unlink(p)

    def heartbeat(self):
        pct = (100 * self.base_done // self.base_total) // 10 * 10 if self.base_total else 100
        self.log("alive; base jobs done: %d%% (rounded down to 10%%; outcomes sealed)" % pct)

    def heartbeat_loop(self):
        while not self.hb_stop.wait(HEARTBEAT_S):
            self.heartbeat()

    # -- one run

    def reap(self, sid):
        n = kill_session(sid) + kill_binaries(self.real_bins)
        if n:
            time.sleep(1.0)
            n += kill_session(sid) + kill_binaries(self.real_bins)
        clean_tmp()
        return n

    def run_checker(self, proof_path, root, tag, jobdir):
        out = os.path.join(self.scratch, "check.out")
        err = os.path.join(self.scratch, "check.err")
        argv = (["/usr/bin/env", "-i", "TPTP=" + root,
                 "THVM_PROOFCHECK_WALL=%d" % self.check_wall,
                 self.timeout_bin, "-k", "5", str(self.check_wall + 15),
                 self.checker, "--szs", proof_path])
        rc, elapsed, _cpu, _rss, sid, _killed = spawn(argv, root, out, err,
                                                     self.check_wall + 50)
        self.reap(sid)
        txt = read_text(out)
        m = re.search(r"^% SZS status (\S+)", txt, re.M)
        verdict = m.group(1) if m else "NoOutput"
        with gzip.open(os.path.join(jobdir, tag + ".chk.gz"), "wt") as fp:
            fp.write("# rc=%d elapsed=%.2f\n" % (rc, elapsed))
            fp.write(txt)
            fp.write("\n# stderr\n")
            fp.write(read_text(err, limit=1 << 20))
        return verdict, elapsed

    def run_one(self, arm, prob, wall, pass_no):
        wall_s = fmt_num(wall)
        root = self.root_for(prob.split)
        jobdir = os.path.join(self.out, "runs", prob.name)
        os.makedirs(jobdir, exist_ok=True)
        tag = "w%s_%s_p%d" % (wall_s, arm.name, pass_no)
        errdir = os.path.join(self.scratch, "errdir") if self.errdir_on else None
        if errdir:
            shutil.rmtree(errdir, ignore_errors=True)
            os.makedirs(errdir)
        out = os.path.join(self.scratch, "stdout.p")
        err = os.path.join(self.scratch, "stderr.txt")
        argv = self.argv_for(arm, prob, wall, errdir)
        oom0 = oom_count()
        rc, elapsed, cpu, rss, sid, killed = spawn(argv, self.scratch, out, err, wall + 50)
        strays = self.reap(sid)
        if STOP["signal"]:
            # The runner itself was told to stop: this run is not a result.
            return "terminated"
        stdout = read_text(out)
        stderr = read_text(err, limit=4 << 20)
        status = parse_szs_status(stdout)
        present, form = proof_block(stdout)
        winner = parse_winner(stderr)
        stopped = stopped_at_wall(stderr)
        lanes = {}
        if errdir:
            for f in sorted(glob.glob(os.path.join(errdir, "*.err"))):
                lane = os.path.basename(f)[:-len(".err")]
                lanes[lane] = classify_lane(read_text(f, limit=2 << 20), stopped, bool(winner))
            shutil.rmtree(errdir, ignore_errors=True)
        counts = lane_histogram(lanes)
        end_class = classify_run(rc, stderr, counts, winner, status)
        if killed:
            end_class = "alarm"
        in_wall = int(elapsed <= wall)
        verdict = "-"
        check_s = ""
        if self.checker and status == "Unsatisfiable" and present:
            verdict, ce = self.run_checker(out, root, tag, jobdir)
            check_s = "%.2f" % ce
            if STOP["signal"]:
                return "terminated"
        oom1 = oom_count()
        ooms = (oom1 - oom0) if (oom0 is not None and oom1 is not None) else "?"
        sol = solution_flag(status, present, form, in_wall, verdict)
        gzip_copy(out, os.path.join(jobdir, tag + ".out.gz"))
        gzip_copy(err, os.path.join(jobdir, tag + ".err.gz"))
        with open(self.lanes_path, "a") as fp:
            fp.write(json.dumps({
                "problem": prob.name, "arm": arm.name, "wall": wall_s, "pass": pass_no,
                "rc": rc, "winner": winner, "stopped": stopped, "killed_by_runner": killed,
                "lanes": {k: {"class": v[0], "detail": v[1], "steps": v[2]}
                          for k, v in sorted(lanes.items())},
            }, sort_keys=True) + "\n")
        row = [prob.name, prob.split, arm.name, wall_s, pass_no, status, "%.2f" % elapsed,
               winner, int(present and form == "CNFRefutation"), verdict, sol, self.lscpu,
               arm.sha, in_wall, rc, "%.2f" % cpu, rss, end_class, histogram_text(counts),
               proof_records(stdout), check_s, strays, self.host, self.cond(), utc_now(),
               mem_text(self.mem), ooms]
        append_line(self.rows_path, row)
        key = (prob.name, arm.name, wall_s, str(pass_no))
        self.done_keys.add(key)
        self.rows_by_key[key] = dict(zip(ROW_COLUMNS, [str(x) for x in row]))
        self.jobs_run += 1
        if pass_no <= self.passes:
            self.base_done += 1
        if not self.sealed:
            self.log("%s %s w%s p%d %s %.1fs %s proof=%d %s sol=%d %s oom=%s" % (
                prob.name, arm.name, wall_s, pass_no, status, elapsed, winner or "-",
                int(present), verdict, sol, end_class, ooms))
        if verdict == "VerifiedBad":
            if self.sealed:
                evidence = ("sealed.tgz (the row with verdict VerifiedBad); opening it for "
                            "this cause is the section 1.3 exception")
            else:
                evidence = "runs/%s/%s.*" % (prob.name, tag)
            write_file(os.path.join(self.out, "run.stop"),
                       "stopped=%s reason=VerifiedBad evidence=%s\n" % (utc_now(), evidence))
            self.log("VerifiedBad: the line stops (see run.stop)")
            return "verifiedbad"
        return "ok"

    # -- calibration

    def calibrate(self):
        a = self.args
        verdict_path = os.path.join(self.out, "calibration.verdict")
        if a.no_calib:
            write_file(verdict_path, "status=skipped host=%s utc=%s\n" % (self.host, utc_now()))
            self.deviation("calibration skipped (--no-calib)")
            return "pass"
        if not (a.calib_ref and a.calib_bin and a.calib_root):
            raise SystemExit("run_arms: the pod calibration needs --calib-ref, --calib-bin "
                             "and --calib-root (or --no-calib)")
        old = read_text(verdict_path)
        ref_sha = sha256_file(a.calib_ref)
        if ("status=pass" in old and ("host=%s " % self.host) in old
                and ("ref_sha=%s " % ref_sha) in old and ("rss=%s\n" % self.rss_label) in old):
            self.log("calibration already passed on this host; skipping")
            return "pass"
        meta, ref_rows = load_calib_ref(a.calib_ref)
        if meta["max_par"] != str(self.max_par):
            raise SystemExit("run_arms: the calibration reference ran at width %s, this run at %d"
                             % (meta["max_par"], self.max_par))
        if meta["rss_cap"] != self.rss_label:
            raise SystemExit("run_arms: the calibration reference ran with rss cap %s, this run "
                             "with %s" % (meta["rss_cap"], self.rss_label))
        cbin = os.path.abspath(os.path.expanduser(a.calib_bin))
        csha = sha256_file(cbin)
        if csha != meta["bin_sha"]:
            raise SystemExit("run_arms: --calib-bin sha256 %s differs from the reference's %s"
                             % (csha[:12], meta["bin_sha"][:12]))
        res, terminated = run_calibration_set(self, cbin, csha, meta["flags"], a.calib_root,
                                              ref_rows, CALIB_PASSES,
                                              os.path.join(self.out, "calibration.tsv"))
        if terminated:
            return "terminated"
        v = calibration_verdict(res)
        write_file(verdict_path, (
            "status=%s host=%s utc=%s ref_sha=%s rss=%s\n"
            "geomean_ratio=%.4f tolerance=%.2f n=%d unsolved=%d worst_single=%.3f (%s) "
            "single_tolerance=%.2f passes=%d\n"
            "lscpu=%s\n") % ("pass" if v["ok"] else "fail", self.host, utc_now(), ref_sha,
                            self.rss_label, v["geomean"], SPEED_TOLERANCE, v["n"],
                            v["unsolved"], v["worst"], v["worst_name"], SINGLE_TOLERANCE,
                            CALIB_PASSES, self.lscpu))
        self.log("calibration %s: geomean time ratio %.3f over %d problems (%d unsolved), worst "
                 "single %.3f off; tolerance +-%.0f%% (single +-%.0f%%)" % (
                     "PASS" if v["ok"] else "FAIL", v["geomean"], v["n"], v["unsolved"],
                     v["worst"], 100 * SPEED_TOLERANCE, 100 * SINGLE_TOLERANCE))
        return "pass" if v["ok"] else "fail"

    # -- the loop

    def acquire_lock(self):
        self.lock_fp = open(LOCK_PATH, "a+")
        try:
            fcntl.flock(self.lock_fp, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            self.lock_fp.seek(0)
            holder = self.lock_fp.read().strip()
            raise LockHeld(holder)
        self.lock_fp.seek(0)
        self.lock_fp.truncate()
        self.lock_fp.write(self.out + "\n")
        self.lock_fp.flush()
        self.locked = True

    def run(self):
        os.makedirs(self.out, exist_ok=True)
        self.acquire_lock()
        for f in ("run.done", "run.failed"):
            try:
                os.unlink(os.path.join(self.out, f))
            except OSError:
                pass
        if os.path.exists(os.path.join(self.out, "run.stop")):
            raise SystemExit("run_arms: %s/run.stop exists (a VerifiedBad stopped the line); "
                             "fix the cause, then remove it" % self.out)
        if self.sealed:
            self.unpack_sealed()
        os.makedirs(self.scratch, exist_ok=True)
        self.preflight()
        cfg_fp = self.check_config()
        ensure_header(self.rows_path, ROW_COLUMNS)
        ensure_header(self.dec_path, DECISION_COLUMNS)
        self.load_state()
        start = time.time()
        self.deadline = start + self.args.deadline_s
        if self.args.deadline_utc:
            self.deadline = min(self.deadline, parse_utc(self.args.deadline_utc))
        me = os.path.abspath(sys.argv[0])
        append_file(os.path.join(self.out, "run.started"), "\n".join([
            "--- start",
            "started=%s" % utc_now(),
            "host=%s cpus_affinity=%d cpu_quota=%s cpus_effective=%d" % (
                self.host, self.cpu["affinity"], self.cpu["quota"], self.cpu["eff"]),
            "mem %s caps_need_kb=%d rss_cap=%s" % (mem_text(self.mem),
                                                  caps_need_kb(self.max_par), self.rss_label),
            "lscpu=%s" % self.lscpu,
            "runner=%s sha256=%s" % (me, sha256_file(me)),
            "argv=%s" % " ".join(sys.argv),
            "config=%s (run.config)" % cfg_fp,
            "protocol=%s %s" % (self.protocol_sha, self.protocol_text),
            "problems=%d walls=%s passes=%d repeats=%d seed=%s sealed=%d deadline_utc=%s" % (
                len(self.problems), ",".join(fmt_num(w) for w in self.walls), self.passes,
                int(self.repeats), self.seed, int(self.sealed),
                time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(self.deadline))),
            "base_jobs=%d with_rows=%d" % (self.base_total, len(self.done_keys)),
            "checker=%s sha256=%s wall=%d" % (self.checker or "none",
                                              getattr(self, "checker_sha", "-"), self.check_wall),
            "cond=%s" % self.cond(),
        ] + ["arm=%s bin=%s sha256=%s flags=%s lanes=%s gated=%s" % (
            a.name, a.binary, a.sha, a.flags,
            ("%d sha256=%s" % (len(a.lanes), sha256_text("".join(
                "%s\t%s\n" % kv for kv in a.lanes.items()))[:12])) if a.lanes else
            "unavailable(%s)" % a.lanes_note,
            ",".join(k for k, v in a.lanes.items() if is_clock_gated(v)) if a.lanes else "-")
             for a in self.arms]
          + ["DEVIATION %s" % d for d in self.deviations]
          + ["example_argv=%s" % " ".join(self.argv_for(self.arms[0], self.problems[0],
                                                         self.walls[0],
                                                         "<scratch>/errdir" if self.errdir_on else None))])
            + "\n")
        if self.sealed:
            threading.Thread(target=self.heartbeat_loop, daemon=True).start()
            self.heartbeat()
        cal = self.calibrate()
        if cal != "pass":
            status = "terminated" if cal == "terminated" else "calibration-failed"
            self.finish(status)
            return 5 if cal == "terminated" else 3
        status = self.loop()
        self.finish(status)
        return {"complete": 0, "deadline": 7, "terminated": 5, "verifiedbad": 4}.get(status, 1)

    def pending_jobs(self, prob, passes, walls):
        jobs = []
        for p in passes:
            for w in walls:
                for arm in self.arms:
                    key = (prob.name, arm.name, fmt_num(w), str(p))
                    if key not in self.done_keys:
                        jobs.append((order_key(self.seed, prob.name, p, fmt_num(w), arm.name),
                                     arm, w, p))
        jobs.sort(key=lambda j: j[0])
        return jobs

    def run_jobs(self, prob, jobs):
        for _k, arm, w, p in jobs:
            if STOP["signal"]:
                return "terminated"
            if time.time() >= self.deadline:
                return "deadline"
            st = self.run_one(arm, prob, w, p)
            if st != "ok":
                return st
        return "ok"

    def decide(self, prob, wall):
        ws = fmt_num(wall)
        key = (prob.name, ws)
        if key in self.decisions:
            return self.decisions[key]
        pass1 = {}
        for arm in self.arms:
            r = self.rows_by_key.get((prob.name, arm.name, ws, "1"))
            if r is None:
                return None
            pass1[arm.name] = (int(r["solution"]), r["lane"])
        try:
            dec = repeat_decision(pass1, {a.name: a.lanes for a in self.arms})
        except UnknownLane as e:
            raise SystemExit("run_arms: %s; the repeat rule cannot decide (see lanes_<arm>.tsv)"
                             % e)
        append_line(self.dec_path, [prob.name, ws, dec[0], dec[1]])
        self.decisions[key] = dec
        return dec

    def loop(self):
        for prob in self.problems:
            st = self.run_jobs(prob, self.pending_jobs(prob, range(1, self.passes + 1), self.walls))
            if st != "ok":
                return st
            if not self.repeats:
                continue
            rep_walls = []
            for w in self.walls:
                dec = self.decide(prob, w)
                if dec is None:
                    raise SystemExit("run_arms: internal: pass-1 rows missing after a complete pass")
                if dec[0]:
                    rep_walls.append(w)
            if rep_walls:
                st = self.run_jobs(prob, self.pending_jobs(prob, REPEAT_PASSES, rep_walls))
                if st != "ok":
                    return st
        return "complete"

    def finish(self, status):
        self.hb_stop.set()
        with_rows = len(self.done_keys)
        if self.sealed:
            self.pack_sealed()
        else:
            shutil.rmtree(self.scratch, ignore_errors=True)
        write_file(os.path.join(self.out, "run.done"),
                   "status=%s\nfinished=%s\nrows=%d jobs_this_run=%d base_jobs=%d/%d\n" % (
                       status, utc_now(), with_rows, self.jobs_run, self.base_done,
                       self.base_total))
        self.log("runner: %s; %d rows; base jobs %d/%d" % (status, with_rows, self.base_done,
                                                           self.base_total))


class LockHeld(Exception):
    pass


def run_calibration_set(runner, cbin, csha, flags, croot, ref_rows, passes, out_tsv):
    """Run every calibration problem `passes` times with the calibration
    binary under the arms' environment rules.  Returns (one dict per run,
    terminated)."""
    croot = os.path.abspath(os.path.expanduser(croot))
    cwall = wall_of_flags(flags)
    if cwall is None:
        raise SystemExit("run_arms: calibration flags need --wall=N: %r" % flags)
    real = set(runner.real_bins) | {os.path.realpath(cbin)}
    ensure_header(out_tsv, CALIB_COLUMNS)
    os.makedirs(runner.scratch, exist_ok=True)
    results = []
    for p in range(1, passes + 1):
        for r in ref_rows:
            pabs = os.path.join(croot, r["path"])
            if not os.path.isfile(pabs):
                raise SystemExit("run_arms: calibration problem missing: %s" % pabs)
            argv = (["/usr/bin/env", "-i"] + runner.job_env(croot, None)
                    + [runner.timeout_bin, "-k", "5", fmt_num(cwall + 15), cbin]
                    + flags.split() + [pabs])
            out = os.path.join(runner.scratch, "calib.out")
            err = os.path.join(runner.scratch, "calib.err")
            rc, elapsed, _cpu, _rss, sid, _k = spawn(argv, runner.scratch, out, err, cwall + 50)
            kill_session(sid)
            kill_binaries(real)
            clean_tmp()
            if STOP["signal"]:
                return results, True
            status = parse_szs_status(read_text(out))
            ok = status in ("Unsatisfiable", "Theorem") and elapsed <= cwall
            ref = r.get("ref_s")
            ratio = (elapsed / ref) if (ok and ref) else float("nan")
            append_line(out_tsv, [r["name"], p, status, "%.2f" % elapsed,
                                  "%.2f" % ref if ref else "", "%.4f" % ratio if ok and ref else "",
                                  runner.host, runner.lscpu, csha, utc_now()])
            runner.log("calibration %s pass %d: %s %.2fs%s" % (
                r["name"], p, status, elapsed, (" ratio %.3f" % ratio) if ok and ref else ""))
            results.append({"name": r["name"], "path": r["path"], "pass": p, "ok": ok,
                            "wall_s": elapsed, "ref_s": ref})
    return results, False


def select_reference(results, keep, min_s, max_s, max_spread):
    """Pick the calibration problems for the reference table: solved in
    every pass, median time in [min_s, max_s], spread (max - min) / median
    at most max_spread; of those, the `keep` with the longest median time
    (fixed start-up costs weigh least there)."""
    by = {}
    for r in results:
        by.setdefault((r["name"], r["path"]), []).append(r)
    cands = []
    for (name, path), rs in by.items():
        if not all(x["ok"] for x in rs):
            continue
        ts = sorted(x["wall_s"] for x in rs)
        med = statistics.median(ts)
        spread = (ts[-1] - ts[0]) / med if med > 0 else float("inf")
        if min_s <= med <= max_s and spread <= max_spread:
            cands.append((spread, name, path, med, ts))
    cands.sort(key=lambda c: (-c[3], c[0], c[1]))
    return cands[:keep]


def cmd_calib_ref(args):
    r = Runner.__new__(Runner)
    r.args = args
    r.out = os.path.abspath(os.path.expanduser(args.out))
    r.scratch = os.path.join(r.out, "scratch")
    r.log_path = os.path.join(r.out, "calib_ref.log")
    r.host = socket.gethostname()
    r.max_par = args.max_par
    r.real_bins = set()
    r.deviations = []
    r.rss_label = args.rss_cap
    r.rss_cap = None if args.rss_cap == "default" else int(args.rss_cap)
    os.makedirs(r.out, exist_ok=True)
    r.timeout_bin = shutil.which("timeout", path="/usr/bin:/bin")
    if not r.timeout_bin or not os.path.isdir("/proc/self"):
        raise SystemExit("run_arms: calib-ref needs Linux with coreutils timeout")
    Runner.check_pod(r)
    try:
        Runner.acquire_lock(r)
    except LockHeld as e:
        raise SystemExit("run_arms: another run_arms (%s) holds %s on this pod" % (e, LOCK_PATH))
    cbin = os.path.abspath(os.path.expanduser(args.calib_bin))
    csha = sha256_file(cbin)
    r.real_bins = {os.path.realpath(cbin)}
    if running_binaries(r.real_bins):
        raise SystemExit("run_arms: the calibration binary is already running; need an idle pod")
    Runner.wait_idle(r)
    cands = []
    with open(args.calib) as fp:
        for line in fp:
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            f = line.split("\t")
            cands.append({"name": f[0], "path": f[1], "ref_s": None})
    res, terminated = run_calibration_set(r, cbin, csha, args.calib_flags, args.calib_root,
                                          cands, args.calib_passes,
                                          os.path.join(r.out, "calib_ref_runs.tsv"))
    if terminated:
        r.log("calib-ref: terminated")
        return 5
    chosen = select_reference(res, args.keep, args.min_s, args.max_s, args.max_spread)
    lines = ["#calib_ref v2",
             "#bin_sha=%s" % csha,
             "#flags=%s" % args.calib_flags,
             "#rss_cap=%s" % r.rss_label,
             "#max_par=%d" % args.max_par,
             "#lscpu=%s" % r.lscpu,
             "#mem=%s" % mem_text(r.mem),
             "#host=%s" % r.host,
             "#utc=%s" % utc_now(),
             "#passes=%d keep=%d min_s=%g max_s=%g max_spread=%g candidates=%d chosen=%d" % (
                 args.calib_passes, args.keep, args.min_s, args.max_s, args.max_spread,
                 len(cands), len(chosen)),
             "#name\tpath\tref_s\tspread\ttimes"]
    for spread, name, path, med, ts in sorted(chosen, key=lambda c: c[1]):
        lines.append("%s\t%s\t%.3f\t%.4f\t%s" % (name, path, med, spread,
                                                  ",".join("%.2f" % t for t in ts)))
    write_file(args.ref_out, "\n".join(lines) + "\n")
    r.log("calib-ref: %d of %d candidates chosen -> %s (sha256 %s); record this sha256 and the "
          "lscpu line in section 5.4 before any pod is judged against it" % (
              len(chosen), len(cands), args.ref_out, sha256_file(args.ref_out)))
    if len(chosen) < args.keep:
        r.log("WARNING: fewer than %d stable calibration problems; widen the candidates" % args.keep)
        return 6
    return 0


def cmd_hostfacts(args):
    mem = mem_facts()
    cpu = cpu_facts()
    need = caps_need_kb(args.max_par)
    print("memtotal_kb=%d" % mem["memtotal_kb"])
    print("cgroup_mem_kb=%s" % (mem["cgroup_kb"] if mem["cgroup_kb"] is not None else "none"))
    print("eff_mem_kb=%d" % mem["eff_kb"])
    print("caps_need_kb=%d" % need)
    print("default_caps_fit=%d" % int(mem["eff_kb"] >= need))
    print("affinity_cpus=%d" % cpu["affinity"])
    print("cpu_quota=%s" % ("%g" % cpu["quota"] if cpu["quota"] is not None else "none"))
    print("eff_cpus=%d" % cpu["eff"])
    oc = oom_count()
    print("oom_kill=%s" % (oc if oc is not None else "?"))
    return 0


def cmd_plan(args):
    r = Runner(args)
    r.timeout_bin = "timeout"
    print("problems=%d walls=%s arms=%s passes=%d repeats=%s sealed=%s rss_cap=%s base_jobs=%d" % (
        len(r.problems), ",".join(fmt_num(w) for w in r.walls),
        ",".join(a.name for a in r.arms), r.passes, r.repeats, r.sealed, r.rss_label,
        r.base_total))
    print("protocol=%s %s" % (r.protocol_sha, r.protocol_text))
    print("lane tables: read from each arm binary ('<flags> --list-lanes=flags') at preflight")
    cw = r.check_wall if r.checker else None
    per_problem = sum(job_worst_s(w, cw) for w in r.walls) * len(r.arms)
    base_h = per_problem * r.passes * len(r.problems) / 3600.0
    rep_h = (per_problem * len(REPEAT_PASSES) * len(r.problems) / 3600.0) if r.repeats else 0.0
    deadline_s = args.deadline_s
    if args.deadline_utc:
        deadline_s = min(deadline_s, parse_utc(args.deadline_utc) - time.time())
    deadline_h = deadline_s / 3600.0
    print("worst case on this shard (every job to its hard bound): base %.1f h; repeats on every "
          "problem add %.1f h; the runner deadline is %.1f h" % (base_h, rep_h, deadline_h))
    full_h = base_h * r.n_all_problems / float(len(r.problems))
    need = int(math.ceil(full_h / deadline_h)) if deadline_h > 0 else 0
    print("shards for the base worst case of all %d problems to fit the deadline: %d%s" % (
        r.n_all_problems, need, "" if base_h <= deadline_h else
        "  WARNING: this shard's base worst case exceeds the deadline"))
    first = r.problems[0]
    r.done_keys = set()
    jobs = r.pending_jobs(first, range(1, r.passes + 1), r.walls)
    print("first problem's pass-1 order: %s" % ", ".join(
        "%s@%s/p%d" % (a.name, fmt_num(w), p) for _k, a, w, p in jobs))
    a0, w0 = jobs[0][1], jobs[0][2]
    print("first argv: %s" % " ".join(r.argv_for(a0, first, w0,
                                                  "<scratch>/errdir" if r.errdir_on else None)))
    if r.sealed:
        print("(sealed: the problem name above is public; outcomes are never printed)")
    return 0


def build_parser():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd")

    def common_run(p):
        p.add_argument("--problems", required=True, help="TSV: name, path, split")
        p.add_argument("--arms", required=True, help="TSV: name, binary, flags")
        p.add_argument("--walls", required=True, help="comma list, e.g. 180,56")
        p.add_argument("--rss-cap", required=True, choices=RSS_CAPS,
                       help="the memory cap of the whole comparison: default (the schedule's "
                            "own; needs 16 x 4500 MiB of effective memory) or 3500")
        p.add_argument("--passes", type=int, default=1,
                       help="independent passes per arm (1 = protocol with repeats; 2 = L0 A/A)")
        p.add_argument("--no-repeats", action="store_true",
                       help="no extra passes for disagreeing or clock-gated problems")
        p.add_argument("--root", required=True, help="TPTP root the problem paths resolve against")
        p.add_argument("--split-root", action="append", metavar="SPLIT=DIR",
                       help="a different root for one split (e.g. j13=~/j13)")
        p.add_argument("--checker", help="strict CheckProof binary (tools/proover/Makefile)")
        p.add_argument("--no-check", action="store_true", help="no CheckProof (WDEP calibration)")
        p.add_argument("--check-wall", type=int, default=CHECK_WALL_S)
        p.add_argument("--same-lanes", action="append", metavar="A,B",
                       help="arms A and B may list the same lanes (e.g. PD and E)")
        p.add_argument("--allow-binaries", action="store_true",
                       help="arms on different binaries (a deviation from section 1.1, logged)")
        p.add_argument("--force-config", action="store_true",
                       help="resume under a different configuration (logged)")
        p.add_argument("--calib-ref", help="calibration reference table (calib-ref output)")
        p.add_argument("--calib-bin", help="the calibration binary the reference was measured with")
        p.add_argument("--calib-root", help="TPTP root of the calibration problems")
        p.add_argument("--no-calib", action="store_true", help="skip the pod calibration (logged)")
        p.add_argument("--seed", default=DEFAULT_SEED)
        p.add_argument("--deadline-s", type=int, default=43200,
                       help="no run starts after this many seconds from launch")
        p.add_argument("--deadline-utc", help="no run starts after this time (YYYY-MM-DDTHH:MM:SSZ)")
        p.add_argument("--shard", help="K/N: this pod's share of the problems")
        p.add_argument("--sealed", choices=("auto", "on", "off"), default="auto")
        p.add_argument("--no-errdir", action="store_true",
                       help="do not keep slice stderr (no lane end classes)")
        p.add_argument("--max-par", type=int, default=MAX_PAR)
        p.add_argument("--allow-nproc", action="store_true")
        p.add_argument("--out", required=True)

    run = sub.add_parser("run", help="run the comparison")
    common_run(run)
    plan = sub.add_parser("plan", help="print the plan and a time estimate; runs nothing")
    common_run(plan)
    cr = sub.add_parser("calib-ref", help="measure the calibration reference on this pod")
    cr.add_argument("--calib", required=True, help="candidates TSV: name, path")
    cr.add_argument("--calib-bin", required=True)
    cr.add_argument("--calib-root", required=True)
    cr.add_argument("--calib-flags", default="--szs --wall=120",
                    help="one deterministic configuration (the default preset)")
    cr.add_argument("--calib-passes", type=int, default=3)
    cr.add_argument("--keep", type=int, default=10)
    cr.add_argument("--min-s", type=float, default=2.5)
    cr.add_argument("--max-s", type=float, default=60.0)
    cr.add_argument("--max-spread", type=float, default=0.10)
    cr.add_argument("--rss-cap", required=True, choices=RSS_CAPS)
    cr.add_argument("--ref-out", required=True)
    cr.add_argument("--max-par", type=int, default=MAX_PAR)
    cr.add_argument("--allow-nproc", action="store_true")
    cr.add_argument("--out", required=True)
    hf = sub.add_parser("hostfacts", help="print this pod's effective memory and CPUs")
    hf.add_argument("--max-par", type=int, default=MAX_PAR)
    return ap


def on_signal(signum, _frame):
    STOP["signal"] = signum


def fail_sentinel(out, msg):
    try:
        os.makedirs(out, exist_ok=True)
        write_file(os.path.join(out, "run.failed"),
                   "failed=%s reason=%s\n" % (utc_now(), clean_field(msg)))
    except OSError:
        pass


def seal_after_failure(runner):
    """Pack the sealed outputs of a runner that stopped on an error."""
    if runner is None or not runner.locked or not runner.sealed:
        return ""
    runner.hb_stop.set()
    try:
        runner.pack_sealed()
    except (SystemExit, Exception) as e:  # noqa: BLE001
        return " (packing sealed.tgz failed: %s)" % clean_field(e)
    return ""


def main(argv=None):
    ap = build_parser()
    args = ap.parse_args(argv)
    if args.cmd is None:
        ap.print_help()
        return 2
    if args.cmd == "hostfacts":
        return cmd_hostfacts(args)
    if args.cmd == "plan":
        return cmd_plan(args)
    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    if args.cmd == "calib-ref":
        return cmd_calib_ref(args)
    out = os.path.abspath(os.path.expanduser(args.out))
    runner = None
    try:
        runner = Runner(args)
        return runner.run()
    except LockHeld as e:
        msg = "another run_arms (%s) holds %s on this pod" % (e, LOCK_PATH)
        # The live run owns its own out dir's sentinels.
        if str(e) != out:
            fail_sentinel(out, msg)
        print("run_arms: " + msg, file=sys.stderr)
        return 2
    except SystemExit as e:
        msg = str(e.code) if e.code is not None else "exit"
        msg += seal_after_failure(runner)
        fail_sentinel(out, msg)
        print(msg, file=sys.stderr)
        return 2
    except Exception as e:  # noqa: BLE001 - every crash must leave its sentinel
        fail_sentinel(out, "%s: %s%s" % (type(e).__name__, e, seal_after_failure(runner)))
        raise


if __name__ == "__main__":
    sys.exit(main())
