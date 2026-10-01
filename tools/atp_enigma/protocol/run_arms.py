#!/usr/bin/env python3
"""Pod-side runner for one ENIGMA arm comparison.

Implements the run protocol of docs/plans/enigma_portfolio.md section 1.3.
Python 3.8 or later, standard library only, Linux only (it reads /proc).

Every run of an arm, one problem at a time on the whole pod:

  env -i TPTP=<root> THVM_ATP_SCHED_MAX_PAR=16 [THVM_ATP_RSS_ABORT_MB=3500]
         [THVM_ATP_SCHED_ERRDIR=<scratch>/errdir]
    timeout -k 5 <W+15> <binary> <arm flags> --szs --proof --wall=<W> <problem>
    < /dev/null

THVM_ATP_RSS_ABORT_MB=3500 is set when MemTotal is below 72 GiB (16 slices
at the schedule's default 4500 MiB cap do not fit).  THVM_ATP_SCHED_ERRDIR
only redirects each slice's stderr from /dev/null to a file; the runner
reads it for the end class of every lane and deletes it (--no-errdir drops
it, and with it the end classes).

Solution (all three): SZS status Unsatisfiable; a complete CNFRefutation
block, with the run ended within W; a strict CheckProof verdict of
VerifiedGood (CheckProof --szs <stdout file>, THVM_PROOFCHECK_WALL=60,
under env -i TPTP=<root> and timeout -k 5 75).

Order and repeats:
- The pod first runs the calibration problems of --calib-ref with the
  calibration binary; a pod whose geometric-mean time ratio to the
  reference is outside [0.85, 1.15] is flagged (calibration.verdict,
  run.done status=calibration-failed) and the runner stops.
- Problems run in sha256(seed, name) order.  For each problem, its pass-1
  jobs (every wall x arm) run in sha256(seed, problem, pass, wall, arm)
  order, so the arm order is a fixed-seed shuffle per problem.
- With --passes 1 (the default) and repeats on: at each wall, when the
  arms' pass-1 Solution flags disagree or any arm's pass-1 winning lane is
  clock-gated (--lrs, --fol-lrs, --cp-set-ir-budget in its flags; a lane
  missing from the lane table counts as gated), every arm runs passes 2
  and 3 there.  The decision goes to decisions.tsv; the majority of three
  decides (stats.py).
- With --passes N > 1 (the L0 A/A design) every arm runs N independent
  passes and no repeats.

Outputs in --out (all of it per-problem data: sealed for J13):
  rows.tsv        one row per run, header line first (columns below)
  decisions.tsv   problem, wall, repeat (0/1), reason
  lanes.jsonl     per run: every launched lane's end class and detail
  runs/<problem>/w<W>_<arm>_p<pass>.{out,err,chk}.gz  stdout (the proof),
                  stderr, checker output
  calibration.tsv, calibration.verdict
  run.started (config), run.done (status=complete|deadline|terminated|
  verifiedbad|calibration-failed), run.failed (preflight or crash),
  run.stop (a VerifiedBad: the line stops until its cause is fixed),
  run.log
Resumable: a rerun with the same arguments skips every run that has a row.
With J13 problems (or --sealed on) the log and the sentinels carry only
job counts, never a per-problem outcome.

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
  12 lscpu     model name, threads per core, CPUs, cores per socket, sockets
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
  23 host   24 cond (max_par, rss cap, errdir, proof, check wall)   25 utc

Subcommands:
  run        the comparison
  calib-ref  measure the calibration reference table on this pod
  lane-flags print "lane TAB flags" from a FindProof source tools/atp_cli/main.c
  plan       print the job plan and the first command; runs nothing
Exit codes of run: 0 complete, 7 deadline, 5 terminated, 4 VerifiedBad,
3 calibration failed, 2 preflight error.

Typical pod use (from a snapshot copy, detached):
  cp run_arms.py ~/run_arms.snapshot.py
  nohup setsid python3 ~/run_arms.snapshot.py run --problems j13.tsv \\
    --arms arms.tsv --walls 180,56 --root ~/j13 --checker ~/thvm/bin/CheckProof \\
    --src-main ~/thvm/tools/atp_cli/main.c --calib-ref calib_ref.tsv \\
    --calib-bin ~/sub/FindProof/bin/FindProof --calib-root ~/tptp_pool \\
    --shard 1/4 --out ~/out_L0_1 </dev/null >~/run_arms_1.log 2>&1 &
"""
import argparse
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
import time

ROW_COLUMNS = [
    "problem", "split", "arm", "wall", "pass", "status", "wall_s", "lane",
    "proof", "verdict", "solution", "lscpu", "bin_sha",
    "in_wall", "rc", "cpu_s", "maxrss_kb", "end_class", "lane_classes",
    "proof_recs", "check_s", "strays", "host", "cond", "utc",
]
DECISION_COLUMNS = ["problem", "wall", "repeat", "reason"]
CALIB_COLUMNS = ["name", "pass", "status", "wall_s", "ref_s", "ratio",
                 "host", "lscpu", "bin_sha", "utc"]

MAX_PAR = 16
RAM_FOR_DEFAULT_CAPS_KB = 72 * 1024 * 1024
SMALL_POD_RSS_MB = 3500
SPEED_TOLERANCE = 0.15
CHECK_WALL_S = 60
REPEAT_PASSES = (2, 3)
GATED_FLAGS = ("--lrs", "--fol-lrs", "--cp-set-ir-budget")
SCHED_ENTRY = re.compile(r'^\s*\{"([^"]+)",\s*"((?:[^"\\]|\\.)*)",\s*[-+0-9.eE]+\s*\},')
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


# ----------------------------------------------------------------------------
# Output parsing (pure functions; unit-tested in test_run_arms.py)


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


def is_clock_gated(flags):
    for tok in flags.split():
        name, eq, val = tok.partition("=")
        if name in GATED_FLAGS:
            if eq and val in ("0", ""):
                continue
            return True
    return False


def parse_schedule_source(text):
    """lane -> flags for every {"name", "flags", frac}, entry in a source."""
    lanes = {}
    for line in text.splitlines():
        m = SCHED_ENTRY.match(line)
        if m:
            lanes[m.group(1)] = m.group(2)
    return lanes


def repeat_decision(pass1, gated_lanes, known_lanes):
    """pass1: arm -> (solution 0/1, winning lane).  Returns (repeat, reason)."""
    sols = {v[0] for v in pass1.values()}
    reasons = []
    if len(sols) > 1:
        reasons.append("disagree")
    gated = sorted({lane for _sol, lane in pass1.values()
                    if lane and (lane in gated_lanes or lane not in known_lanes)})
    if gated:
        reasons.append("clock-gated:" + "+".join(gated))
    return (1 if reasons else 0), (";".join(reasons) if reasons else "-")


def solution_flag(status, proof_present, form, in_wall, verdict):
    return int(status == "Unsatisfiable" and proof_present and form == "CNFRefutation"
               and in_wall and verdict == "VerifiedGood")


def shard_select(names, k, n):
    ordered = sorted(names, key=lambda x: (domain_of(x), x))
    return [x for i, x in enumerate(ordered) if i % n == k - 1]


# ----------------------------------------------------------------------------
# Host facts and process control


def lscpu_summary():
    info = {}
    try:
        out = subprocess.run(["lscpu"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             universal_newlines=True, timeout=30,
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
    return clean_field("%s|tpc=%s|cpus=%s|cps=%s|sockets=%s" % (
        model, info.get("Thread(s) per core", "?"), info.get("CPU(s)", "?"),
        info.get("Core(s) per socket", "?"), info.get("Socket(s)", "?")))


def mem_total_kb():
    txt = read_text("/proc/meminfo")
    m = re.search(r"^MemTotal:\s+(\d+)\s+kB", txt, re.M)
    return int(m.group(1)) if m else 0


def rss_cap_for(mem_kb):
    return SMALL_POD_RSS_MB if mem_kb < RAM_FOR_DEFAULT_CAPS_KB else None


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
            if not re.match(r"^[A-Za-z0-9_.+-]+$", name):
                raise SystemExit("run_arms: arm name %r: use letters, digits, _ . + -" % name)
            if name in seen:
                raise SystemExit("run_arms: duplicate arm %s" % name)
            if re.search(r"(?:^|\s)--(?:wall|szs|proof)(?:=|\s|$)", flags):
                raise SystemExit("run_arms: arm %s: --szs, --proof and --wall are added by the runner" % name)
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
    for k in ("bin_sha", "flags"):
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
        self.lane_flags = {}
        if args.lane_flags:
            with open(args.lane_flags) as fp:
                for line in fp:
                    line = line.rstrip("\n")
                    if not line.strip() or line.startswith("#"):
                        continue
                    name, _, flags = line.partition("\t")
                    self.lane_flags[name.strip()] = flags
        if args.src_main:
            self.lane_flags.update(parse_schedule_source(read_text(args.src_main)))
        if self.repeats and not self.lane_flags:
            raise SystemExit("run_arms: the repeat rule needs the lane flags: "
                             "--src-main <tools/atp_cli/main.c> or --lane-flags <tsv>")
        self.gated = {k for k, v in self.lane_flags.items() if is_clock_gated(v)}
        self.deadline = None
        self.max_par = args.max_par
        self.lscpu = ""
        self.host = socket.gethostname()
        self.rss_cap = None
        self.done_keys = set()
        self.decisions = {}
        self.jobs_run = 0
        self.real_bins = set()

    # -- logging

    def log(self, msg):
        line = "%s %s" % (utc_now(), msg)
        print(line, flush=True)
        try:
            with open(self.log_path, "a") as fp:
                fp.write(line + "\n")
        except OSError:
            pass

    def root_for(self, split):
        return self.split_roots.get(split, self.root)

    def problem_abs(self, prob):
        return os.path.join(self.root_for(prob.split), prob.path)

    # -- preflight

    def preflight(self):
        for tool in ("/usr/bin/env",):
            if not os.access(tool, os.X_OK):
                raise SystemExit("run_arms: missing %s" % tool)
        self.timeout_bin = shutil.which("timeout", path="/usr/bin:/bin")
        if not self.timeout_bin:
            raise SystemExit("run_arms: missing coreutils timeout")
        if not os.path.isdir("/proc/self"):
            raise SystemExit("run_arms: needs Linux /proc")
        nproc = os.cpu_count() or 0
        if nproc != self.max_par and not self.args.allow_nproc:
            raise SystemExit("run_arms: this pod has %d vCPUs; the protocol is %d-vCPU pods "
                             "(--allow-nproc to override, logged)" % (nproc, self.max_par))
        for arm in self.arms:
            if not os.access(arm.binary, os.X_OK):
                raise SystemExit("run_arms: arm %s: no executable %s" % (arm.name, arm.binary))
            arm.sha = sha256_file(arm.binary)
            self.real_bins.add(os.path.realpath(arm.binary))
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
        self.lscpu = lscpu_summary()
        mem = mem_total_kb()
        self.mem_kb = mem
        self.rss_cap = rss_cap_for(mem)
        if self.args.calib_bin:
            self.real_bins.add(os.path.realpath(os.path.abspath(os.path.expanduser(self.args.calib_bin))))
        busy = running_binaries(self.real_bins)
        if busy:
            raise SystemExit("run_arms: %d prover/checker processes already running; "
                             "the protocol needs an idle pod" % len(busy))
        self.wait_idle()

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
            time.sleep(15)

    def cond(self):
        return "maxpar=%d;rss=%s;errdir=%d;proof=1;check_wall=%s" % (
            self.max_par, self.rss_cap if self.rss_cap else "default",
            1 if self.errdir_on else 0, self.check_wall if self.checker else "none")

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

    # -- state

    def load_state(self):
        _h, rows = read_tsv(self.rows_path)
        for r in rows:
            self.done_keys.add((r["problem"], r["arm"], r["wall"], r["pass"]))
        self.rows_by_key = {(r["problem"], r["arm"], r["wall"], r["pass"]): r for r in rows}
        _h, decs = read_tsv(self.dec_path)
        for d in decs:
            self.decisions[(d["problem"], d["wall"])] = (int(d["repeat"]), d["reason"])

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
                                                     self.check_wall + 15 + 5 + 30)
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
        rc, elapsed, cpu, rss, sid, killed = spawn(argv, self.scratch, out, err, wall + 15 + 5 + 30)
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
               proof_records(stdout), check_s, strays, self.host, self.cond(), utc_now()]
        append_line(self.rows_path, row)
        key = (prob.name, arm.name, wall_s, str(pass_no))
        self.done_keys.add(key)
        self.rows_by_key[key] = dict(zip(ROW_COLUMNS, [str(x) for x in row]))
        self.jobs_run += 1
        if self.sealed:
            self.log("jobs done in this run: %d (outcomes sealed)" % self.jobs_run)
        else:
            self.log("%s %s w%s p%d %s %.1fs %s proof=%d %s sol=%d %s" % (
                prob.name, arm.name, wall_s, pass_no, status, elapsed, winner or "-",
                int(present), verdict, sol, end_class))
        if verdict == "VerifiedBad":
            # The cause must be found, so the stop file names the run even
            # under the seal; the log does not.
            write_file(os.path.join(self.out, "run.stop"),
                       "stopped=%s reason=VerifiedBad job=%d evidence=runs/%s/%s.*\n" % (
                           utc_now(), self.jobs_run, prob.name, tag))
            self.log("VerifiedBad: the line stops (see run.stop)")
            return "verifiedbad"
        return "ok"

    # -- calibration

    def calibrate(self):
        a = self.args
        verdict_path = os.path.join(self.out, "calibration.verdict")
        if a.no_calib:
            write_file(verdict_path, "status=skipped host=%s utc=%s\n" % (self.host, utc_now()))
            self.log("calibration skipped (--no-calib)")
            return True
        if not (a.calib_ref and a.calib_bin and a.calib_root):
            raise SystemExit("run_arms: the pod calibration needs --calib-ref, --calib-bin "
                             "and --calib-root (or --no-calib)")
        old = read_text(verdict_path)
        ref_sha = sha256_file(a.calib_ref)
        if ("status=pass" in old and ("host=%s " % self.host) in old
                and ("ref_sha=%s" % ref_sha) in old):
            self.log("calibration already passed on this host; skipping")
            return True
        meta, ref_rows = load_calib_ref(a.calib_ref)
        if meta.get("max_par", str(self.max_par)) != str(self.max_par):
            raise SystemExit("run_arms: the calibration reference ran at width %s, this run at %d"
                             % (meta.get("max_par"), self.max_par))
        cbin = os.path.abspath(os.path.expanduser(a.calib_bin))
        csha = sha256_file(cbin)
        if csha != meta["bin_sha"]:
            raise SystemExit("run_arms: --calib-bin sha256 %s differs from the reference's %s"
                             % (csha[:12], meta["bin_sha"][:12]))
        res = run_calibration_set(self, cbin, csha, meta["flags"], a.calib_root, ref_rows, 1,
                                  os.path.join(self.out, "calibration.tsv"))
        ratios = []
        unsolved = 0
        for r in res:
            if r["ok"]:
                ratios.append(r["wall_s"] / r["ref_s"])
            else:
                unsolved += 1
        gm = math.exp(sum(math.log(x) for x in ratios) / len(ratios)) if ratios else float("nan")
        ok = (unsolved == 0 and ratios and abs(gm - 1.0) <= SPEED_TOLERANCE)
        worst = max((abs(x - 1.0) for x in ratios), default=float("nan"))
        write_file(verdict_path, (
            "status=%s host=%s utc=%s ref_sha=%s\n"
            "geomean_ratio=%.4f tolerance=%.2f n=%d unsolved=%d worst_single=%.3f\n"
            "lscpu=%s\n") % ("pass" if ok else "fail", self.host, utc_now(), ref_sha,
                            gm, SPEED_TOLERANCE, len(ratios), unsolved, worst, self.lscpu))
        self.log("calibration %s: geomean time ratio %.3f over %d problems (%d unsolved); "
                 "tolerance +-%.0f%%" % ("PASS" if ok else "FAIL", gm, len(ratios), unsolved,
                                        100 * SPEED_TOLERANCE))
        return ok

    # -- the loop

    def run(self):
        os.makedirs(self.out, exist_ok=True)
        os.makedirs(self.scratch, exist_ok=True)
        for f in ("run.done", "run.failed"):
            try:
                os.unlink(os.path.join(self.out, f))
            except OSError:
                pass
        if os.path.exists(os.path.join(self.out, "run.stop")):
            raise SystemExit("run_arms: %s/run.stop exists (a VerifiedBad stopped the line); "
                             "fix the cause, then remove it" % self.out)
        lock = open(LOCK_PATH, "w")
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            raise SystemExit("run_arms: another run_arms holds %s on this pod" % LOCK_PATH)
        self.preflight()
        ensure_header(self.rows_path, ROW_COLUMNS)
        ensure_header(self.dec_path, DECISION_COLUMNS)
        self.load_state()
        start = time.time()
        self.deadline = start + self.args.deadline_s
        n_base = len(self.problems) * len(self.walls) * len(self.arms) * self.passes
        me = os.path.abspath(sys.argv[0])
        write_file(os.path.join(self.out, "run.started"), "\n".join([
            "started=%s" % utc_now(),
            "host=%s nproc=%d mem_kb=%d rss_cap=%s" % (self.host, os.cpu_count() or 0,
                                                      self.mem_kb, self.rss_cap or "default"),
            "lscpu=%s" % self.lscpu,
            "runner=%s sha256=%s" % (me, sha256_file(me)),
            "argv=%s" % " ".join(sys.argv),
            "problems=%d walls=%s passes=%d repeats=%d seed=%s sealed=%d deadline_s=%d" % (
                len(self.problems), ",".join(fmt_num(w) for w in self.walls), self.passes,
                int(self.repeats), self.seed, int(self.sealed), self.args.deadline_s),
            "base_jobs=%d with_rows=%d" % (n_base, len(self.done_keys)),
            "checker=%s sha256=%s wall=%d" % (self.checker or "none",
                                              getattr(self, "checker_sha", "-"), self.check_wall),
            "cond=%s" % self.cond(),
            "gated_lanes=%s" % ",".join(sorted(self.gated)),
        ] + ["arm=%s bin=%s sha256=%s flags=%s" % (a.name, a.binary, a.sha, a.flags)
             for a in self.arms]
          + ["example_argv=%s" % " ".join(self.argv_for(self.arms[0], self.problems[0],
                                                         self.walls[0],
                                                         "<scratch>/errdir" if self.errdir_on else None))])
            + "\n")
        if self.args.allow_nproc and (os.cpu_count() or 0) != self.max_par:
            self.log("DEVIATION: %d vCPUs (protocol: %d)" % (os.cpu_count() or 0, self.max_par))
        if not self.calibrate():
            self.finish("calibration-failed")
            return 3
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
        dec = repeat_decision(pass1, self.gated, set(self.lane_flags))
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
        with_rows = len(self.done_keys)
        write_file(os.path.join(self.out, "run.done"),
                   "status=%s\nfinished=%s\nrows=%d jobs_this_run=%d\n" % (
                       status, utc_now(), with_rows, self.jobs_run))
        self.log("runner: %s; %d rows in %s" % (status, with_rows, self.rows_path))


def run_calibration_set(runner, cbin, csha, flags, croot, ref_rows, passes, out_tsv):
    """Run every calibration problem `passes` times with the calibration
    binary under the arms' environment rules.  Returns one dict per run."""
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
            if STOP["signal"]:
                raise SystemExit("run_arms: terminated during calibration")
    return results


def select_reference(results, keep, min_s, max_s, max_spread):
    """Pick the calibration problems for the reference table: solved in
    every pass, median time in [min_s, max_s], spread (max - min) / median
    at most max_spread; the `keep` with the smallest spread."""
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
    cands.sort()
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
    os.makedirs(r.out, exist_ok=True)
    r.timeout_bin = shutil.which("timeout", path="/usr/bin:/bin")
    if not r.timeout_bin or not os.path.isdir("/proc/self"):
        raise SystemExit("run_arms: calib-ref needs Linux with coreutils timeout")
    nproc = os.cpu_count() or 0
    if nproc != args.max_par and not args.allow_nproc:
        raise SystemExit("run_arms: this pod has %d vCPUs; the protocol is %d" % (nproc, args.max_par))
    r.lscpu = lscpu_summary()
    r.mem_kb = mem_total_kb()
    r.rss_cap = rss_cap_for(r.mem_kb)
    lock = open(LOCK_PATH, "w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        raise SystemExit("run_arms: another run_arms holds %s on this pod" % LOCK_PATH)
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
    res = run_calibration_set(r, cbin, csha, args.calib_flags, args.calib_root, cands,
                              args.calib_passes, os.path.join(r.out, "calib_ref_runs.tsv"))
    chosen = select_reference(res, args.keep, args.min_s, args.max_s, args.max_spread)
    lines = ["#calib_ref v1",
             "#bin_sha=%s" % csha,
             "#flags=%s" % args.calib_flags,
             "#rss_cap=%s" % (r.rss_cap or "default"),
             "#max_par=%d" % args.max_par,
             "#lscpu=%s" % r.lscpu,
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
    r.log("calib-ref: %d of %d candidates chosen -> %s (sha256 %s)" % (
        len(chosen), len(cands), args.ref_out, sha256_file(args.ref_out)))
    if len(chosen) < args.keep:
        r.log("WARNING: fewer than %d stable calibration problems; widen the candidates" % args.keep)
        return 6
    return 0


def cmd_lane_flags(args):
    lanes = parse_schedule_source(read_text(args.src_main))
    for name in lanes:
        print("%s\t%s" % (name, lanes[name]))
    print("# %d lanes, %d clock-gated: %s" % (
        len(lanes), sum(1 for v in lanes.values() if is_clock_gated(v)),
        " ".join(k for k, v in lanes.items() if is_clock_gated(v))), file=sys.stderr)
    return 0


def cmd_plan(args):
    r = Runner(args)
    r.timeout_bin = "timeout"
    n_base = len(r.problems) * len(r.walls) * len(r.arms) * r.passes
    print("problems=%d walls=%s arms=%s passes=%d repeats=%s sealed=%s base_jobs=%d" % (
        len(r.problems), ",".join(fmt_num(w) for w in r.walls),
        ",".join(a.name for a in r.arms), r.passes, r.repeats, r.sealed, n_base))
    print("gated lanes: %s" % (",".join(sorted(r.gated)) or "-"))
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
        p.add_argument("--lane-flags", help="TSV: lane, flags (for the clock-gated test)")
        p.add_argument("--src-main", help="FindProof tools/atp_cli/main.c to read SCHEDULE[] from")
        p.add_argument("--calib-ref", help="calibration reference table (calib-ref output)")
        p.add_argument("--calib-bin", help="the calibration binary the reference was measured with")
        p.add_argument("--calib-root", help="TPTP root of the calibration problems")
        p.add_argument("--no-calib", action="store_true", help="skip the pod calibration (logged)")
        p.add_argument("--seed", default="20261001")
        p.add_argument("--deadline-s", type=int, default=43200,
                       help="no run starts after this many seconds")
        p.add_argument("--shard", help="K/N: this pod's share of the problems")
        p.add_argument("--sealed", choices=("auto", "on", "off"), default="auto")
        p.add_argument("--no-errdir", action="store_true",
                       help="do not keep slice stderr (no lane end classes)")
        p.add_argument("--max-par", type=int, default=MAX_PAR)
        p.add_argument("--allow-nproc", action="store_true")
        p.add_argument("--out", required=True)

    run = sub.add_parser("run", help="run the comparison")
    common_run(run)
    plan = sub.add_parser("plan", help="print the plan; runs nothing")
    common_run(plan)
    cr = sub.add_parser("calib-ref", help="measure the calibration reference on this pod")
    cr.add_argument("--calib", required=True, help="candidates TSV: name, path")
    cr.add_argument("--calib-bin", required=True)
    cr.add_argument("--calib-root", required=True)
    cr.add_argument("--calib-flags", default="--schedule --szs --wall=120")
    cr.add_argument("--calib-passes", type=int, default=3)
    cr.add_argument("--keep", type=int, default=10)
    cr.add_argument("--min-s", type=float, default=2.0)
    cr.add_argument("--max-s", type=float, default=60.0)
    cr.add_argument("--max-spread", type=float, default=0.10)
    cr.add_argument("--ref-out", required=True)
    cr.add_argument("--max-par", type=int, default=MAX_PAR)
    cr.add_argument("--allow-nproc", action="store_true")
    cr.add_argument("--out", required=True)
    lf = sub.add_parser("lane-flags", help="print lane TAB flags from a source main.c")
    lf.add_argument("--src-main", required=True)
    return ap


def on_signal(signum, _frame):
    STOP["signal"] = signum


def main(argv=None):
    ap = build_parser()
    args = ap.parse_args(argv)
    if args.cmd is None:
        ap.print_help()
        return 2
    if args.cmd == "lane-flags":
        return cmd_lane_flags(args)
    if args.cmd == "plan":
        return cmd_plan(args)
    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    if args.cmd == "calib-ref":
        return cmd_calib_ref(args)
    runner = Runner(args)
    os.makedirs(runner.out, exist_ok=True)
    try:
        return runner.run()
    except SystemExit as e:
        msg = str(e.code) if e.code is not None else "exit"
        write_file(os.path.join(runner.out, "run.failed"),
                   "failed=%s reason=%s\n" % (utc_now(), clean_field(msg)))
        print(msg, file=sys.stderr)
        return 2
    except Exception as e:  # noqa: BLE001 - every crash must leave its sentinel
        write_file(os.path.join(runner.out, "run.failed"),
                   "failed=%s reason=%s: %s\n" % (utc_now(), type(e).__name__, clean_field(e)))
        raise


if __name__ == "__main__":
    sys.exit(main())
