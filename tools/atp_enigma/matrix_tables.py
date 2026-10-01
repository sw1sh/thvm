#!/usr/bin/env python3
"""Lane matrix: the J13 seal, the G0.1 validation, the pool tables T1-T8
and the section 5.3 decisions of docs/plans/enigma_portfolio.md.

Text processing only: it runs no prover.  Inputs live in the matrix
directory (default ~/.cache/thvm/enigma/matrix; FORMAT.txt there describes
them): problems.tsv, lanes.txt, shards/shard_K.tsv (the job lists),
results/ (the harvested rows, sentinels and pod captures), tptp_pool.tgz.

Modes, in order:
  --seal       Streams the six raw shard files.  Every row with split j13
               goes into <sealed>/j13_matrix.tsv.gz (deterministic gzip,
               then read-only); the train and heldout rows go into
               matrix.tsv.  The raw shard files and every harvest file
               that holds j13 rows or names a j13 problem move, gzipped
               and read-only, under <sealed>/raw_shards and
               <sealed>/raw_aux, so the matrix directory holds no j13 row.
               It prints the archive's sha256 and row count and records
               them in seal.txt.  A re-run reads the sealed raw shards,
               rebuilds both files and checks that they are byte-identical.
  --validate   G0.1 on the train and heldout rows -> g01.txt, rerun.tsv.
  --tables     T1-T8 per split -> tables/.
  --decisions  The section 5.3 rules -> decisions_5_3.txt and job lists.
  --all        --validate, --tables, --decisions.

Every mode after --seal reads matrix.tsv only, asserts that no row has
split j13 and that every problem is a train or heldout problem of
problems.tsv, and never opens a path under <sealed>.  The only j13 facts it
reports are the sealed row count and the archive sha256 (from seal.txt).

matrix.tsv: the 14 raw columns of FORMAT.txt, plus 15 shard (1-6) and
16 par (the runner width of the pod that produced the row: 8 on the
8-vCPU pods of shards 1 and 3, 16 on the 16-vCPU pods of shards 2, 4, 5
and 6, and 4 for the 1184 rows shard 4 ran first on a 4-vCPU pod).
"""
import argparse
import collections
import datetime
import gzip
import hashlib
import io
import math
import os
import re
import statistics
import sys
import tarfile
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import static_gates  # noqa: E402

MATRIX_BIN_SHA = ("d3946ff1a965a9be4678f932a6abeec697768a690feb34b4f0e5d723"
                  "cd2af5c0")
N_SHARDS = 6
RAW_SHARDS = [f"shard_{k}.tsv" for k in range(1, N_SHARDS + 1)]
RAW_RUN1 = "shard_4.run1.tsv"
# Harvest files that hold j13 rows or name j13 problems: raw-row copies,
# runner logs (one line per job), smoke rows and notes.  Paths relative to
# the matrix directory.
AUX_J13 = [
    "results/shard_4.run1.partial.tgz",
    "results/shard_2_artifacts.tgz",
    "results/shard_6_extras.tgz",
    "results/shard_5.runner_log.txt",
    "results/shard_5.setup.txt",
    "results/shard_6_anomalies.txt",
    "prep_gate_logs.tgz",
]
# Archives whose non-j13 content stays readable as a redacted copy.
AUX_REDACTED_COPY = {"prep_gate_logs.tgz": "prep_gate_logs.redacted.tgz"}
# Files that name j13 problems by design and hold no outcome (the problem
# list, job lists, the problem files, FORMAT.txt's job-list description).
J13_NAME_HOLDERS = {"problems.tsv", "test20_shard.tsv", "stress24_shard.tsv",
                    "j13_ueq.tgz", "FORMAT.txt"}
CLASSES = ["proved", "gaveup-saturated", "wall", "step-cap", "memory-guard",
           "fast-exit-not-applicable", "crash", "killed-by-timeout"]
SPLITS = ["train", "heldout"]


def die(msg):
    sys.exit("matrix_tables: " + msg)


def sha256_bytes(b):
    return hashlib.sha256(b).hexdigest()


def gzip_bytes(data):
    """Deterministic gzip: no name, mtime 0, level 9."""
    buf = io.BytesIO()
    with gzip.GzipFile(filename="", mode="wb", compresslevel=9, fileobj=buf,
                       mtime=0) as gz:
        gz.write(data)
    return buf.getvalue()


def write_readonly(path, data):
    """Writes data to path read-only; an existing file must hold the same
    bytes (a seal is never overwritten)."""
    if os.path.exists(path):
        with open(path, "rb") as fp:
            old = fp.read()
        if old != data:
            die(f"{path} exists with different content; refusing to overwrite")
        return False
    tmp = path + ".tmp"
    with open(tmp, "wb") as fp:
        fp.write(data)
    os.chmod(tmp, 0o444)
    os.rename(tmp, path)
    return True


class Paths:
    def __init__(self, matrix_dir, sealed_dir):
        self.matrix = os.path.abspath(os.path.expanduser(matrix_dir))
        self.sealed = os.path.abspath(os.path.expanduser(sealed_dir))
        self.results = os.path.join(self.matrix, "results")
        self.raw = os.path.join(self.sealed, "raw_shards")
        self.aux = os.path.join(self.sealed, "raw_aux")
        self.archive = os.path.join(self.sealed, "j13_matrix.tsv.gz")
        self.matrix_tsv = os.path.join(self.matrix, "matrix.tsv")
        self.seal_txt = os.path.join(self.matrix, "seal.txt")
        self.tables = os.path.join(self.matrix, "tables")

    def m(self, *p):
        return os.path.join(self.matrix, *p)

    def guard(self, path):
        """Analysis modes call this on every path they open."""
        a = os.path.abspath(path)
        if a == self.sealed or a.startswith(self.sealed + os.sep):
            die(f"analysis mode tried to open a sealed path: {a}")
        if os.path.basename(a) in RAW_SHARDS + [RAW_RUN1] and \
                os.path.dirname(a) == self.results:
            die(f"analysis mode tried to open a raw shard: {a}")
        return a


def load_problems(paths):
    """name -> (path, split) for every problem of problems.tsv."""
    out = {}
    with open(paths.m("problems.tsv")) as fp:
        for line in fp:
            f = line.rstrip("\n").split("\t")
            if len(f) >= 3:
                out[f[0]] = (f[1], f[2])
    return out


def load_lanes(paths):
    with open(paths.m("lanes.txt")) as fp:
        return [l.strip() for l in fp if l.strip()]


def j13_name_regex(problems):
    names = sorted((n for n, (_p, s) in problems.items() if s == "j13"),
                   key=len, reverse=True)
    return re.compile(r"(?<![A-Za-z0-9])(" + "|".join(map(re.escape, names))
                      + r")(?![0-9])")


def read_started(path):
    out = {}
    with open(path) as fp:
        for line in fp:
            for m in re.finditer(r"(\w+)=(\S*)", line):
                out.setdefault(m.group(1), m.group(2))
    return out


def started_path(paths, k):
    for name in (f"shard_{k}.tsv.started", f"shard_{k}.started.txt"):
        p = os.path.join(paths.results, name)
        if os.path.exists(p):
            return p
    return None


def done_path(paths, k):
    for name in (f"shard_{k}.tsv.done", f"shard_{k}.done.txt"):
        p = os.path.join(paths.results, name)
        if os.path.exists(p):
            return p
    return None


# ---------------------------------------------------------------- seal


def raw_source(paths, name):
    """The bytes of a raw shard file: from results/ before the seal, from
    the sealed copy after it."""
    p = os.path.join(paths.results, name)
    if os.path.exists(p):
        with open(p, "rb") as fp:
            return fp.read(), "results"
    g = os.path.join(paths.raw, name + ".gz")
    if os.path.exists(g):
        with gzip.open(g, "rb") as fp:
            return fp.read(), "sealed"
    die(f"raw shard {name} is neither in results/ nor sealed")


def extract_shard6_extras(paths, problems):
    """shard 6's sentinels and its train / heldout crash logs live only in
    shard_6_extras.tgz; copy them out before the archive is sealed."""
    tgz = paths.m("results", "shard_6_extras.tgz")
    if not os.path.exists(tgz):
        return []
    got = []
    with tarfile.open(tgz) as tf:
        for mem in tf.getmembers():
            if not mem.isfile():
                continue
            base = os.path.basename(mem.name)
            dest = None
            if mem.name in ("out/shard_6.tsv.started", "out/shard_6.tsv.done"):
                dest = os.path.join(paths.results, base)
            elif mem.name.startswith("out/shard_6.tsv.logs/"):
                name = base.split("__")[0]
                if problems.get(name, ("", "j13"))[1] in SPLITS:
                    dest = os.path.join(paths.results, "shard_6.tsv.logs", base)
            if dest is None or os.path.exists(dest):
                continue
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with tf.extractfile(mem) as src, open(dest, "wb") as out:
                out.write(src.read())
            got.append(os.path.relpath(dest, paths.matrix))
    return got


def redacted_tgz(src, dest, j13re):
    """A copy of a .tgz whose text members lose every line naming a j13
    problem; members named after one are dropped."""
    if os.path.exists(dest):
        return False
    with tarfile.open(src) as tf, tarfile.open(dest + ".tmp", "w:gz") as out:
        for mem in tf.getmembers():
            if j13re.search(mem.name):
                continue
            if not mem.isfile():
                out.addfile(mem)
                continue
            data = tf.extractfile(mem).read()
            text = data.decode("utf-8", errors="replace")
            keep = "".join(l for l in text.splitlines(True)
                           if not j13re.search(l) and "\tj13\t" not in l)
            b = keep.encode()
            mem2 = tarfile.TarInfo(mem.name)
            mem2.size = len(b)
            mem2.mtime = mem.mtime
            mem2.mode = mem.mode
            out.addfile(mem2, io.BytesIO(b))
    os.rename(dest + ".tmp", dest)
    return True


def seal_file(src, dest_dir, gz):
    """Moves src into dest_dir read-only (gzipped when gz); an existing
    sealed copy must match.  Returns the sealed path."""
    os.makedirs(dest_dir, exist_ok=True)
    with open(src, "rb") as fp:
        data = fp.read()
    name = os.path.basename(src)
    if gz:
        dest = os.path.join(dest_dir, name + ".gz")
        blob = gzip_bytes(data)
        if gzip.decompress(blob) != data:
            die(f"gzip round trip failed for {src}")
    else:
        dest = os.path.join(dest_dir, name)
        blob = data
    write_readonly(dest, blob)
    os.unlink(src)
    return dest


def scan_for_j13(paths, j13re):
    """Every file under the matrix directory that names a j13 problem or
    holds a j13-split line, outside the files that name them by design."""
    hits = []
    for dirpath, _dirs, files in os.walk(paths.matrix):
        for fn in files:
            p = os.path.join(dirpath, fn)
            rel = os.path.relpath(p, paths.matrix)
            if rel in J13_NAME_HOLDERS or rel.startswith("shards" + os.sep):
                continue
            blobs = []
            if fn.endswith((".tgz", ".tar.gz")):
                try:
                    with tarfile.open(p) as tf:
                        for mem in tf.getmembers():
                            blobs.append(mem.name.encode())
                            if mem.isfile():
                                blobs.append(tf.extractfile(mem).read())
                except tarfile.TarError:
                    blobs.append(open(p, "rb").read())
            elif fn.endswith(".gz"):
                blobs.append(gzip.open(p).read())
            else:
                blobs.append(open(p, "rb").read())
            for b in blobs:
                t = b.decode("utf-8", errors="replace")
                if "\tj13\t" in t or j13re.search(t):
                    hits.append(rel)
                    break
    return sorted(hits)


def mode_seal(paths):
    problems = load_problems(paths)
    lanes = set(load_lanes(paths))
    j13re = j13_name_regex(problems)
    os.makedirs(paths.raw, exist_ok=True)
    os.makedirs(paths.aux, exist_ok=True)
    report = []
    extracted = extract_shard6_extras(paths, problems)

    par = {}
    for k in range(1, N_SHARDS + 1):
        sp = started_path(paths, k)
        if sp is None:
            die(f"no .started sentinel for shard {k}")
        par[k] = int(read_started(sp)["par"])
    run1 = paths.m("results", "shard_4.run1.started")
    par_run1 = int(read_started(run1)["par"]) if os.path.exists(run1) else None

    def rows_of(data, label):
        out = []
        for line in data.split(b"\n"):
            if not line:
                continue
            f = line.decode().split("\t")
            if len(f) != 14:
                die(f"{label}: a row without 14 columns")
            name, lane, split = f[0], f[1], f[2]
            if name not in problems or problems[name][1] != split:
                die(f"{label}: a row whose split disagrees with problems.tsv")
            if lane not in lanes:
                die(f"{label}: a row with an unknown lane")
            out.append((line, name, lane, split))
        return out

    run1_data, _src = raw_source(paths, RAW_RUN1)
    run1_rows = rows_of(run1_data, RAW_RUN1)
    run1_keys = {(n, l) for _line, n, l, s in run1_rows if s != "j13"}

    j13_lines = []
    matrix_lines = []
    raw_sha = {}
    srcs = set()
    for k in range(1, N_SHARDS + 1):
        name = RAW_SHARDS[k - 1]
        data, src = raw_source(paths, name)
        srcs.add(src)
        raw_sha[name] = sha256_bytes(data)
        if k == 4:
            prefix_ok = data.startswith(run1_data)
            report.append(f"shard_4.tsv begins with the {len(run1_rows)} rows "
                          f"of shard_4.run1.tsv byte for byte: {prefix_ok}")
            if not prefix_ok:
                die("shard_4.tsv does not begin with shard_4.run1.tsv")
        for line, n, l, s in rows_of(data, name):
            if s == "j13":
                j13_lines.append(line)
                continue
            p = par[k]
            if k == 4 and (n, l) in run1_keys:
                p = par_run1
            matrix_lines.append(line + f"\t{k}\t{p}".encode())
    raw_sha[RAW_RUN1] = sha256_bytes(run1_data)

    j13_blob = b"".join(l + b"\n" for l in j13_lines)
    archive = gzip_bytes(j13_blob)
    archive_sha = sha256_bytes(archive)
    content_sha = sha256_bytes(j13_blob)
    created = write_readonly(paths.archive, archive)

    matrix_blob = b"".join(l + b"\n" for l in matrix_lines)
    matrix_sha = sha256_bytes(matrix_blob)
    if os.path.exists(paths.matrix_tsv):
        with open(paths.matrix_tsv, "rb") as fp:
            same = fp.read() == matrix_blob
        if not same:
            die("matrix.tsv exists with different content")
        matrix_state = "unchanged (byte-identical)"
    else:
        with open(paths.matrix_tsv + ".tmp", "wb") as fp:
            fp.write(matrix_blob)
        os.rename(paths.matrix_tsv + ".tmp", paths.matrix_tsv)
        matrix_state = "written"

    moved = []
    for name in RAW_SHARDS + [RAW_RUN1]:
        src = os.path.join(paths.results, name)
        if os.path.exists(src):
            moved.append(os.path.relpath(seal_file(src, paths.raw, True),
                                         paths.sealed))
    for rel in AUX_J13:
        src = paths.m(rel)
        if not os.path.exists(src):
            continue
        base = os.path.basename(rel)
        if base in AUX_REDACTED_COPY:
            redacted_tgz(src, paths.m(os.path.dirname(rel),
                                      AUX_REDACTED_COPY[base]), j13re)
        gz = not base.endswith(".tgz")
        moved.append(os.path.relpath(seal_file(src, paths.aux, gz),
                                     paths.sealed))

    by_split = collections.Counter(
        l.split(b"\t")[2].decode() for l in matrix_lines)
    old = {}
    if os.path.exists(paths.seal_txt):
        with open(paths.seal_txt) as fp:
            for line in fp:
                if "=" in line:
                    k, v = line.rstrip("\n").split("=", 1)
                    old[k] = v
    sealed_on = old.get("sealed_on",
                        datetime.datetime.now(datetime.timezone.utc)
                        .date().isoformat())
    rec = [
        f"sealed_on={sealed_on}",
        f"archive={paths.archive}",
        f"archive_sha256={archive_sha}",
        f"archive_rows={len(j13_lines)}",
        f"archive_content_sha256={content_sha}",
        f"matrix_tsv_sha256={matrix_sha}",
        f"matrix_rows_train={by_split['train']}",
        f"matrix_rows_heldout={by_split['heldout']}",
    ]
    rec += [f"raw_sha256_{n}={raw_sha[n]}" for n in RAW_SHARDS + [RAW_RUN1]]
    if old and any(old.get(r.split("=", 1)[0]) != r.split("=", 1)[1]
                   for r in rec):
        die("seal.txt disagrees with this run")
    with open(paths.seal_txt, "w") as fp:
        fp.write("\n".join(rec) + "\n")

    hits = scan_for_j13(paths, j13re)
    print(f"sources: {', '.join(sorted(srcs))}")
    print(f"archive {paths.archive}: {'created' if created else 'exists, identical'}")
    print(f"  sha256 {archive_sha}")
    print(f"  rows   {len(j13_lines)}")
    print(f"  content sha256 (uncompressed) {content_sha}")
    print(f"matrix.tsv: {matrix_state}; sha256 {matrix_sha}; "
          f"train {by_split['train']} heldout {by_split['heldout']}")
    for line in report:
        print(line)
    for n in RAW_SHARDS + [RAW_RUN1]:
        print(f"raw {n} sha256 {raw_sha[n]}")
    for p in extracted:
        print(f"extracted before sealing: {p}")
    for p in moved:
        print(f"sealed: {p}")
    if hits:
        die("files in the matrix directory still name j13 problems: "
            + ", ".join(hits))
    print("matrix directory: no file names a j13 problem outside "
          + ", ".join(sorted(J13_NAME_HOLDERS)) + " and shards/")


# ---------------------------------------------------------------- rows


class Row:
    __slots__ = ("problem", "lane", "split", "exit", "szs", "wall", "cpu",
                 "rss", "cls", "steps", "rules", "cps", "slice_end",
                 "detail", "shard", "par")


def num(s, kind=float):
    return kind(s) if s != "" else None


def load_matrix(paths, problems, lanes):
    """The train and heldout rows; dies on any j13 row."""
    rows = []
    known = set(lanes)
    with open(paths.guard(paths.matrix_tsv)) as fp:
        for line in fp:
            f = line.rstrip("\n").split("\t")
            if len(f) != 16:
                die("matrix.tsv: a row without 16 columns")
            if f[2] == "j13":
                die("matrix.tsv holds a j13 row")
            if f[2] not in SPLITS or problems.get(f[0], ("", ""))[1] != f[2]:
                die("matrix.tsv: a row that is not a train or heldout problem")
            if f[1] not in known:
                die("matrix.tsv: a row with an unknown lane")
            r = Row()
            r.problem, r.lane, r.split = f[0], f[1], f[2]
            r.exit = int(f[3])
            r.szs = f[4]
            r.wall = float(f[5])
            r.cpu = num(f[6])
            r.rss = num(f[7], int)
            r.cls = f[8]
            r.steps = num(f[9], int)
            r.rules = num(f[10], int)
            r.cps = num(f[11], int)
            r.slice_end = f[12]
            r.detail = f[13]
            r.shard = int(f[14])
            r.par = int(f[15])
            rows.append(r)
    return rows


class Pool:
    """Per-problem views of the train and heldout rows."""

    def __init__(self, rows, problems, lanes):
        self.lanes = lanes
        self.idx = {l: i for i, l in enumerate(lanes)}
        self.rows = rows
        self.by = collections.defaultdict(dict)
        self.dups = []
        for r in rows:
            if r.lane in self.by[r.problem]:
                self.dups.append((r.problem, r.lane))
            self.by[r.problem][r.lane] = r
        self.split = {p: problems[p][1] for p in self.by}
        self.path = {p: problems[p][0] for p in self.by}
        self._solvers = {}
        for p, lr in self.by.items():
            self._solvers[p] = {l: r.wall for l, r in lr.items()
                                if r.cls == "proved"}

    def problems(self, splits):
        return sorted(p for p in self.by if self.split[p] in splits)

    def solvers(self, p):
        return self._solvers[p]

    def vbs(self, p):
        s = self._solvers[p]
        return min(s.values()) if s else None

    def fastest(self, p):
        s = self._solvers[p]
        if not s:
            return []
        v = min(s.values())
        return sorted((l for l, t in s.items() if t == v), key=self.idx.get)

    def searchers(self, p):
        return [r for r in self.by[p].values()
                if r.cls != "fast-exit-not-applicable"]


def stem(name):
    return name.split("-")[0]


def stem_hash(s):
    return int(hashlib.sha256(s.encode()).hexdigest()[:8], 16) % 100


def median(xs):
    return statistics.median(xs) if xs else None


def quantile(xs, q):
    if not xs:
        return None
    xs = sorted(xs)
    i = min(len(xs) - 1, max(0, int(math.ceil(q * len(xs))) - 1))
    return xs[i]


def fmt(v, nd=2):
    if v is None:
        return "-"
    if isinstance(v, float):
        return f"{v:.{nd}f}"
    return str(v)


def write_tsv(path, header, rows):
    with open(path, "w") as fp:
        fp.write("\t".join(header) + "\n")
        for r in rows:
            fp.write("\t".join(fmt(x) for x in r) + "\n")


def write_jobs(path, pool, pairs):
    """Runner job lines (name, path, split, lane), the shard file format."""
    with open(path, "w") as fp:
        for p, l in pairs:
            fp.write(f"{p}\t{pool.path[p]}\t{pool.split[p]}\t{l}\n")


def read_seal(paths):
    out = {}
    with open(paths.guard(paths.seal_txt)) as fp:
        for line in fp:
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                out[k] = v
    return out


class PoolFiles:
    """tptp_pool.tgz extracted to a temporary TPTP root."""

    def __init__(self, paths):
        self.tmp = tempfile.TemporaryDirectory(prefix="matrix_tables_")
        with tarfile.open(paths.guard(paths.m("tptp_pool.tgz"))) as tf:
            tf.extractall(self.tmp.name)
        self.root = self.tmp.name

    def header(self, relpath):
        """(status, rating) from a problem file's header."""
        status = None
        rating = None
        with open(os.path.join(self.root, relpath), errors="replace") as fp:
            for i, line in enumerate(fp):
                if i > 400 or line.startswith(("cnf(", "fof(", "include(",
                                               "tff(", "thf(")):
                    break
                m = re.match(r"%\s*Status\s*:\s*(\S+)", line)
                if m and status is None:
                    status = m.group(1)
                m = re.match(r"%\s*Rating\s*:\s*([0-9.]+)", line)
                if m and rating is None:
                    rating = float(m.group(1))
        return status, rating


def assert_no_j13(paths, problems, files):
    """The files an analysis mode wrote name no j13 problem."""
    j13re = j13_name_regex(problems)
    for f in files:
        with open(f, errors="replace") as fp:
            t = fp.read()
        if "\tj13\t" in t or j13re.search(t):
            die(f"{f} names a j13 problem")


def context(paths):
    problems = load_problems(paths)
    lanes = load_lanes(paths)
    rows = load_matrix(paths, problems, lanes)
    return problems, lanes, rows, Pool(rows, problems, lanes)


# ---------------------------------------------------------------- G0.1


def mode_validate(paths):
    problems, lanes, rows, pool = context(paths)
    seal = read_seal(paths)
    out = []
    verdicts = []

    def item(name, ok, lines):
        verdicts.append((name, ok))
        out.append(f"[{'PASS' if ok else 'FAIL'}] {name}")
        out.extend("    " + l for l in lines)
        out.append("")

    # Completeness against the job lists.
    lines = []
    ok = True
    n_jobs = 0
    n_j13_jobs = 0
    rows_by_shard = collections.defaultdict(list)
    for r in rows:
        rows_by_shard[r.shard].append((r.problem, r.lane))
    for k in range(1, N_SHARDS + 1):
        jobs = []
        with open(paths.guard(paths.m("shards", f"shard_{k}.tsv"))) as fp:
            for line in fp:
                f = line.rstrip("\n").split("\t")
                if len(f) == 4:
                    jobs.append(f)
        n_jobs += len(jobs)
        n_j13_jobs += sum(1 for j in jobs if j[2] == "j13")
        want = collections.Counter((j[0], j[3]) for j in jobs
                                   if j[2] in SPLITS)
        got = collections.Counter(rows_by_shard[k])
        missing = sum((want - got).values())
        extra = sum((got - want).values())
        dup = sum(c - 1 for c in got.values() if c > 1)
        lines.append(f"shard {k}: {len(jobs)} jobs, {sum(want.values())} "
                     f"train+heldout jobs, {len(rows_by_shard[k])} rows; "
                     f"missing {missing}, extra {extra}, duplicate {dup}")
        ok = ok and missing == 0 and extra == 0 and dup == 0
    j13_rows = int(seal["archive_rows"])
    j13_ok = j13_rows == 400 * 46 == n_j13_jobs
    ok = ok and j13_ok
    present = len(rows) + j13_rows
    lines.append(f"j13 (sealed): row count present {j13_rows} = 400 x 46: "
                 f"{j13_ok} (archive sha256 {seal['archive_sha256']})")
    lines.append(f"all shards: {present} of {n_jobs} jobs have rows "
                 f"({100.0 * present / n_jobs:.2f}%; G0.1 needs >= 98%)")
    ok = ok and present >= 0.98 * n_jobs and n_jobs == 56810
    item("completeness: rows for every job of the six job lists", ok, lines)

    # Crashes plus killed-by-timeout.
    bad = [r for r in rows if r.cls in ("crash", "killed-by-timeout")]
    frac = 100.0 * len(bad) / len(rows)
    lines = [f"{c}: {sum(1 for r in bad if r.cls == c)} "
             f"(train {sum(1 for r in bad if r.cls == c and r.split == 'train')}, "
             f"heldout {sum(1 for r in bad if r.cls == c and r.split == 'heldout')})"
             for c in ("crash", "killed-by-timeout")]
    lines.append(f"together {len(bad)} of {len(rows)} train+heldout rows = "
                 f"{frac:.3f}% (G0.1: at most 1%)")
    byp = collections.defaultdict(list)
    for r in bad:
        byp[(r.problem, r.split, r.detail)].append(r.lane)
    for (p, sp, d), ls in sorted(byp.items()):
        solved = "solved by another lane" if pool.vbs(p) is not None \
            else "unsolved by every lane"
        lines.append(f"  {p} ({sp}, {solved}): {d}: {len(ls)} lanes "
                     f"({', '.join(sorted(ls, key=pool.idx.get))})")
    lines.append("the j13 rows are sealed and not counted here")
    item("crashes plus killed-by-timeout at most 1% (train+heldout)",
         frac <= 1.0, lines)

    # Binary sha in every .started.
    lines = []
    ok = True
    sents = [(f"shard {k}", started_path(paths, k))
             for k in range(1, N_SHARDS + 1)]
    sents.append(("shard 4 run 1", paths.m("results", "shard_4.run1.started")))
    for label, sp in sents:
        if sp is None or not os.path.exists(sp):
            lines.append(f"{label}: no .started sentinel")
            ok = False
            continue
        st = read_started(paths.guard(sp))
        good = st.get("sha256") == MATRIX_BIN_SHA
        ok = ok and good
        lines.append(f"{label}: sha256 {st.get('sha256')} par={st.get('par')} "
                     f"nproc={st.get('nproc')} mem_gb={st.get('mem_gb')} "
                     f"{'=' if good else '!='} registry")
    for k in range(1, N_SHARDS + 1):
        dp = done_path(paths, k)
        st = read_started(paths.guard(dp)) if dp else {}
        good = st.get("status") == "complete" and st.get("pending_left") == "0"
        ok = ok and good
        lines.append(f"shard {k} .done: status={st.get('status')} "
                     f"pending_left={st.get('pending_left')}")
    item(f"binary sha256 {MATRIX_BIN_SHA[:8]} in every .started; every "
         ".done complete", ok, lines)

    # Kernel OOM evidence.
    lines = []
    kills = {}
    for k in range(1, N_SHARDS + 1):
        dm = paths.m("results", f"shard_{k}.dmesg_lscpu.txt")
        if not os.path.exists(dm):
            lines.append(f"shard {k}: no dmesg capture (its pod was "
                         "terminated at harvest without one)")
            continue
        with open(paths.guard(dm), errors="replace") as fp:
            hits = [l for l in fp if re.search(r"oom|killed process", l, re.I)
                    and not l.startswith(("Vulnerability", "Flags"))]
        kills[k] = len(hits)
        lines.append(f"shard {k}: dmesg capture holds {len(hits)} oom / "
                     "killed-process lines")
    lines.append("caveat: the capture ran `dmesg -T 2>/dev/null` as the "
                 "unprivileged pod user; Ubuntu 22.04 restricts dmesg to root "
                 "by default (kernel.dmesg_restrict=1), so an empty capture "
                 "does not by itself exclude an OOM kill")
    oom = [r for r in rows if r.cls == "crash" and (
        r.slice_end in ("signal 9", "exit 137") or r.exit == 137
        or "signal 9" in r.detail)]
    lines.append(f"rows with a SIGKILL end (class crash, slice signal 9 or "
                 f"exit 137, or the lane parent killed): {len(oom)}")
    for r in oom:
        lines.append(f"  {r.problem} {r.lane} maxrss {r.rss} kB shard "
                     f"{r.shard} par {r.par}")
    hi = [r for r in rows if r.rss is not None and r.rss > 4500 * 1024]
    lines.append(f"rows whose maxrss exceeds the 4.5 GB slice cap: {len(hi)} "
                 f"(max {max(r.rss for r in rows) // 1024} MB): the RSS guard "
                 "bounds growth over a baseline and is polled every 1024 "
                 "engine steps (src/atp/rerank.c), so it is not a hard cap")
    write_jobs(paths.m("rerun.tsv"), pool,
               [(r.problem, r.lane) for r in oom])
    lines.append(f"rerun.tsv: {len(oom)} jobs (runner job format)")
    if sum(kills.values()):
        lines.append("dmesg kill lines need matching to rows by time; the "
                     "sealed j13 rows cannot be matched here")
    item("kernel OOM victims found and listed for a rerun",
         sum(kills.values()) == 0, lines)

    # Sanity of the rows themselves.
    lines = []
    szs = collections.Counter(r.szs for r in rows if r.cls == "proved")
    exits = collections.Counter(r.exit for r in rows)
    lines.append(f"SZS of proved rows: {dict(szs)}")
    lines.append(f"exit codes: {dict(sorted(exits.items()))}")
    lines.append(f"rows without CPU or maxrss: "
                 f"{sum(1 for r in rows if r.cpu is None or r.rss is None)}")
    lines.append(f"duplicate (problem, lane) keys: {len(pool.dups)}")
    w = collections.Counter((r.shard, r.par) for r in rows)
    lines.append("rows per (shard, runner width): " + ", ".join(
        f"{k}/{p}: {n}" for (k, p), n in sorted(w.items())))
    ok = set(szs) <= {"Unsatisfiable"} and set(exits) <= {0, 1} \
        and not pool.dups
    item("row sanity (proved = Unsatisfiable, exits 0/1, no duplicates)",
         ok, lines)

    head = [
        "G0.1 (docs/plans/enigma_portfolio.md M0): lane-matrix validity",
        f"matrix.tsv sha256 {seal['matrix_tsv_sha256']}; train+heldout rows "
        f"only; the j13 rows are sealed (count {seal['archive_rows']}, "
        f"archive sha256 {seal['archive_sha256']})",
        f"overall: {'PASS' if all(ok for _n, ok in verdicts) else 'FAIL'}",
        "",
    ]
    with open(paths.m("g01.txt"), "w") as fp:
        fp.write("\n".join(head + out) + "\n")
    assert_no_j13(paths, problems, [paths.m("g01.txt"), paths.m("rerun.tsv")])
    for name, ok in verdicts:
        print(f"{'PASS' if ok else 'FAIL'}  {name}")


# ---------------------------------------------------------------- tables

VBS_GRID = [0.05, 0.1, 0.25, 0.5, 1, 2, 3, 5, 10, 20, 30, 40, 50, 57, 59.5]
WITHIN = [1, 3, 10, 30, 57]
TAIL_SEC = 10.0


def greedy_cover(pool, probs, lanes):
    sets = {l: {p for p in probs if l in pool.solvers(p)} for l in lanes}
    union = set().union(*sets.values())
    covered = set()
    picks = []
    while covered != union:
        def key(l):
            return (len(sets[l] - covered), len(sets[l]), -pool.idx[l])
        best = max(lanes, key=key)
        gain = len(sets[best] - covered)
        ties = [l for l in lanes if l != best
                and len(sets[l] - covered) == gain]
        covered |= sets[best]
        picks.append((best, gain, len(covered), ties))
    return picks, union


def hard_tail(pool, probs):
    out = []
    for p in probs:
        v = pool.vbs(p)
        if v is not None:
            if v > TAIL_SEC:
                out.append((p, "fastest>10s"))
            continue
        s = pool.searchers(p)
        if s and all(r.cls == "memory-guard" for r in s):
            out.append((p, "all-searchers-memory-guard"))
    return out


def t5_rates(pool, probs, lane):
    rs = [pool.by[p][lane] for p in probs]
    rates = [r.steps / r.cpu for r in rs
             if r.steps and r.cpu and r.cpu > 0]
    long_rates = [r.steps / r.cpu for r in rs
                  if r.steps and r.cpu and r.cpu >= 1.0]
    return rates, long_rates


def mode_tables(paths, pf):
    problems, lanes, rows, pool = context(paths)
    os.makedirs(paths.tables, exist_ok=True)
    gates = {p: static_gates.gates(os.path.join(pf.root, pool.path[p]), pf.root)
             for p in pool.by}
    summary = []
    for split in SPLITS:
        probs = pool.problems([split])

        def T(name):
            return os.path.join(paths.tables, f"{name}_{split}.tsv")

        n = len(probs)
        solved = [p for p in probs if pool.vbs(p) is not None]

        # T1
        rowsT1 = []
        for lane in lanes:
            rs = [pool.by[p][lane] for p in probs]
            pr = [r for r in rs if r.cls == "proved"]
            alone = sum(1 for r in pr if len(pool.solvers(r.problem)) == 1)
            cls = collections.Counter(r.cls for r in rs)
            rowsT1.append([lane, len(pr), alone]
                          + [sum(1 for r in pr if r.wall <= t) for t in WITHIN]
                          + [cls[c] for c in CLASSES])
        write_tsv(T("T1_lanes"), ["lane", "proved", "proved_alone"]
                  + [f"proved_le{t}s" for t in WITHIN] + CLASSES, rowsT1)

        # T2
        curve = [(t, sum(1 for p in solved if pool.vbs(p) <= t))
                 for t in VBS_GRID]
        write_tsv(T("T2_vbs_curve"), ["t_s", "vbs_solved", "of"],
                  [(t, c, n) for t, c in curve])
        totals = {r[0]: r[1] for r in rowsT1}
        best_n = max(totals.values())
        best = [l for l in lanes if totals[l] == best_n]
        picks, union = greedy_cover(pool, probs, lanes)
        write_tsv(T("T2_greedy_cover"),
                  ["k", "lane", "gain", "covered", "of_oracle", "tied_gain"],
                  [(k, l, g, c, len(union), ",".join(ti))
                   for k, (l, g, c, ti) in enumerate(picks, 1)])
        cov = {k: c for k, (_l, _g, c, _t) in enumerate(picks, 1)}
        full_k = len(picks)

        # T3
        unsolved = [p for p in probs if pool.vbs(p) is None]
        endc = [c for c in CLASSES if c not in ("proved",
                                                "fast-exit-not-applicable")]
        rowsT3 = []
        groups = collections.Counter()
        for p in unsolved:
            s = pool.searchers(p)
            c = collections.Counter(r.cls for r in s)
            pattern = "+".join(x for x in endc if c[x]) or "no-searcher"
            groups[pattern] += 1
            rowsT3.append([p, len(s)] + [c[x] for x in endc] + [pattern])
        write_tsv(T("T3_unsolved"), ["problem", "searching_lanes"] + endc
                  + ["end_pattern"], rowsT3)
        write_tsv(T("T3_groups"), ["end_pattern", "problems"],
                  sorted(groups.items(), key=lambda kv: (-kv[1], kv[0])))

        # T4
        tail = hard_tail(pool, probs)
        rowsT4 = []
        owners = collections.Counter()
        for p, cond in tail:
            v = pool.vbs(p)
            f = pool.fastest(p)
            for l in f:
                owners[l] += 1
            s = sorted(pool.solvers(p).items(), key=lambda kv: kv[1])
            runner = s[1] if len(s) > 1 else None
            rowsT4.append([p, cond, v, ",".join(f), len(s),
                           runner[0] if runner else "-",
                           runner[1] if runner else None, stem(p)])
        write_tsv(T("T4_tail"), ["problem", "condition", "fastest_s",
                                 "owner", "solving_lanes", "runner_up",
                                 "runner_up_s", "stem"], rowsT4)
        write_tsv(T("T4_owners"), ["lane", "tail_problems_owned"],
                  sorted(owners.items(), key=lambda kv: (-kv[1],
                                                         pool.idx[kv[0]])))

        # T5
        rowsT5 = []
        for lane in lanes:
            rates, long_rates = t5_rates(pool, probs, lane)
            m = median(rates)
            ml = median(long_rates)
            rowsT5.append([lane, len(rates), m,
                           1e6 / m if m else None,
                           0.05e6 / m if m else None,
                           len(long_rates), ml,
                           1e6 / ml if ml else None,
                           0.05e6 / ml if ml else None])
        write_tsv(T("T5_selection_rate"),
                  ["lane", "n_rows", "median_sel_per_cpu_s",
                   "us_per_sel", "budget_5pct_us", "n_rows_cpu_ge1s",
                   "median_sel_per_cpu_s_ge1s", "us_per_sel_ge1s",
                   "budget_5pct_us_ge1s"], rowsT5)

        # T6
        sets = {l: {p for p in probs if l in pool.solvers(p)} for l in lanes}
        live = [l for l in lanes if sets[l]]
        jac = {}
        for a in live:
            for b in live:
                u = len(sets[a] | sets[b])
                jac[(a, b)] = len(sets[a] & sets[b]) / u if u else None
        write_tsv(T("T6_jaccard"), ["lane"] + live,
                  [[a] + [jac[(a, b)] for b in live] for a in live])
        pairs = sorted(((jac[(a, b)], a, b) for i, a in enumerate(live)
                        for b in live[i + 1:]), reverse=True)
        write_tsv(T("T6_pairs"), ["jaccard", "lane_a", "lane_b", "solved_a",
                                  "solved_b", "both"],
                  [(j, a, b, len(sets[a]), len(sets[b]),
                    len(sets[a] & sets[b])) for j, a, b in pairs])
        tied = collections.Counter()
        strict = collections.Counter()
        for p in solved:
            f = pool.fastest(p)
            for l in f:
                tied[l] += 1
            if len(f) == 1:
                strict[f[0]] += 1
        write_tsv(T("T6_fastest"), ["lane", "solved", "fastest_incl_ties",
                                    "fastest_strict"],
                  [(l, len(sets[l]), tied[l], strict[l]) for l in lanes])
        never_tied = [l for l in lanes if tied[l] == 0]
        never_strict = [l for l in lanes if strict[l] == 0]

        # T7
        rowsT7 = []
        viol = []
        for lane in lanes:
            c = collections.Counter()
            for p in probs:
                r = pool.by[p][lane]
                v = static_gates.lane_live(lane, gates[p])
                rv = "unknown" if v is None else ("live" if v else "dead")
                rc = ("fast-exit" if r.cls == "fast-exit-not-applicable"
                      else "proved" if r.cls == "proved" else "searched")
                c[(rv, rc)] += 1
                if rv == "dead" and rc == "proved":
                    live_solvers = [
                        l for l in pool.solvers(p)
                        if static_gates.lane_live(l, gates[p]) is True]
                    viol.append((p, lane, r.wall, r.detail,
                                 len(live_solvers)))
                    if r.detail.startswith("front end"):
                        c["dead_front_end"] += 1
            rowsT7.append([lane] + [c[(a, b)] for a in ("dead", "live",
                                                        "unknown")
                                    for b in ("fast-exit", "proved",
                                              "searched")]
                          + [c["dead_front_end"]])
        write_tsv(T("T7_replica"), ["lane"] + [
            f"{a}_{b}" for a in ("dead", "live", "unknown")
            for b in ("fast-exit", "proved", "searched")]
            + ["dead_proved_by_front_end"], rowsT7)
        write_tsv(T("T7_violations"), ["problem", "lane", "wall_s", "detail",
                                       "replica_live_lanes_proving"], viol)
        dead_searched = sum(r[3] for r in rowsT7)
        live_fast = sum(r[4] for r in rowsT7)
        viol_fe = sum(1 for v in viol if v[3].startswith("front end"))
        viol_alone = sum(1 for v in viol if v[4] == 0)
        viol_lanes = collections.Counter(v[1] for v in viol)

        # T8
        rowsT8 = []
        unsolved_set = set(unsolved)
        for lane in lanes:
            rs = [pool.by[p][lane] for p in probs]
            mg = [r for r in rs if r.cls == "memory-guard"]
            mgu = [r for r in mg if r.problem in unsolved_set]
            heap = sum(1 for r in mg if "term heap" in r.detail)
            rss = [r.rss / 1024.0 for r in rs if r.rss is not None
                   and r.cls != "fast-exit-not-applicable"]
            rowsT8.append([lane, len(mg), len(mgu), heap, len(rss),
                           quantile(rss, 0.5), quantile(rss, 0.9),
                           quantile(rss, 0.99), max(rss) if rss else None,
                           sum(1 for x in rss if x > 4500),
                           sum(1 for x in rss if x > 6000)])
        write_tsv(T("T8_memory"), ["lane", "memory_guard", "memory_guard_on_"
                                   "unsolved", "term_heap_exhausted",
                                   "n_searched", "maxrss_p50_mb",
                                   "maxrss_p90_mb", "maxrss_p99_mb",
                                   "maxrss_max_mb", "n_over_4500mb",
                                   "n_over_6000mb"], rowsT8)

        top_owners = sorted(owners.items(), key=lambda kv: (-kv[1],
                                                             pool.idx[kv[0]]))
        summary += [
            f"== {split}: {n} problems, "
            f"{sum(1 for r in rows if r.split == split)} rows",
            f"T2 oracle (VBS) at 60 s: {len(solved)} of {n}; unsolved {len(unsolved)}",
            "T2 VBS curve: " + ", ".join(f"<={t}s {c}" for t, c in curve),
            f"T2 best single lane: {', '.join(best)} {best_n} "
            f"({best_n - len(solved)} vs oracle)",
            "T2 greedy cover: " + ", ".join(
                f"k={k} {cov[k]}" for k in range(1, 17) if k in cov)
            + f"; full union at k={full_k}",
            "T2 greedy picks: " + ", ".join(
                f"{l}(+{g})" for l, g, _c, _t in picks),
            f"T3 unsolved groups: " + ", ".join(
                f"{g} {c}" for g, c in sorted(groups.items(),
                                              key=lambda kv: -kv[1])),
            f"T4 hard tail: {len(tail)} ({sum(1 for _p, c in tail if c == 'fastest>10s')} "
            f"fastest>10s, {sum(1 for _p, c in tail if c != 'fastest>10s')} "
            f"all-searchers-memory-guard); owners: " + ", ".join(
                f"{l} {c}" for l, c in top_owners),
            f"T6 lanes fastest on no problem (ties count): {len(never_tied)}: "
            + ", ".join(never_tied),
            f"T6 lanes never strictly fastest: {len(never_strict)}: "
            + ", ".join(never_strict),
            f"T7 replica-dead lanes that proved: {len(viol)} (must be 0): "
            f"{viol_fe} front-end proofs (the Horn front end runs before the "
            f"lane's exit test), {len(viol) - viol_fe} engine proofs; "
            f"{viol_alone} with no replica-live lane also proving; by lane "
            + ", ".join(f"{l} {n}" for l, n in viol_lanes.most_common())
            + f"; replica-dead but searched {dead_searched}; replica-live "
            f"but fast-exit {live_fast} (capacity gates); otter-* not "
            "replicated",
            f"T8 memory-guard ends {sum(r[1] for r in rowsT8)} "
            f"({sum(r[2] for r in rowsT8)} on unsolved problems, "
            f"{sum(r[3] for r in rowsT8)} term heap); rows over 4.5 GB "
            f"{sum(r[9] for r in rowsT8)}, over 6 GB {sum(r[10] for r in rowsT8)}",
            "",
        ]
    with open(os.path.join(paths.tables, "summary.txt"), "w") as fp:
        fp.write("\n".join(TABLES_README + summary))
    assert_no_j13(paths, problems, [os.path.join(paths.tables, f)
                                    for f in os.listdir(paths.tables)])
    print("\n".join(summary))


# ---------------------------------------------------------------- 5.3

# Documented unique solves on a CASC problem, read from each lane's comment
# in SCHEDULE[] (tools/atp_cli/main.c at b8443afb): the P1 retirement rule's
# third condition.  None = the comment documents none.
DOC_UNIQUE = {
    "equals": "COL009-1 proves in 0.1s encoded (schedule-wide Timeout "
              "narrowing)",
    "narrowq": "COL011-1 0.54s where every completion-side treatment is "
               "structurally blind",
    "equals-twee": "LCL927-10 (CASC-J13's only Twee-solo UEQ solve) proves "
                   "in ~115s",
    "unfold": "COL002-5: 5 steps, 0.0s under any presentation; the "
              "canonical presentation loses in casc29/casc30/J12",
    "unfold-tgt": "KLE122-10: neither ingredient alone gets there (both "
                  "Time Out at 60s); composed 6.4s",
    "unfold-tgt-awr": "LCL306-10 from unreachable (TMO at every round-2 "
                      "config) to PROVED 46-49s",
    "flatten": "cracks deep-goal UEQ that completion stalls on (COL060-1, "
               "GRP167-4)",
    "flatten-bare": "ALG212-10 proves in 27.5s bare, TIMES OUT with any "
                    "extra (BOO017-10 too)",
    "twee": "LAT149-1, GRP177-1, GRP753-1, GRP766-1, each a prior "
            "schedule-wide Timeout; GRP706-1",
    "twee-deep": "LAT224-10 (schedule-wide GaveUp before) and LAT229-10",
    "twee-freq": "LAT138-1 0.5s (prior schedule-wide GaveUp at 240s)",
    "tgt-ax": "GRP689-1 1s (Timeout at every bound without)",
    "tgt-sel": "GRP661-1/662-1/663-1 (every FindProof slice TMO'd at "
               "CASC-J13); GRP664-10",
    "otter-lpo": "RNG027-5/7/10, 028-5/7, 029-5/6/7 of the J13 ring set "
                 "(Vampire + Prover9 only)",
    "otter-lpo-cmin": "of the otter lanes only this one proves RNG027-8",
    "encompass": "MVA003-1, MVA009-1, NUN134-1, LAT163-1, LAT174-1, "
                 "GRP721-1, each unreachable by every prior slice",
    "bare": "BOO014-10: the WM preset fails even at 150s, pure bare proves "
            "it in 6.0s",
    "twee-raw": "canonical-input-OFF-only solves LAT149-1, LAT159-1",
    "bare-raw": "canonical-input-OFF-only solve BOO015-10",
    "flatten-bare-raw": "canonical-input-OFF-only solve ALG212-10",
    "fof-avatar": "FOF: COM149+1, MGT061+1, SWV055+1 at 240s, lost by the "
                  "plain pre-pass",
    "fof-avatar2": "FOF: COM149+1, MGT061+1, SWV055+1 at 240s, lost by the "
                   "plain pre-pass",
}
DOC_NOTE = {
    "wm": "slower proofs named (LCL113-10, REL028-2); no uniqueness claimed",
    "horn-cd": "LCL195-10 is named as a Vampire timeout, not as a solve",
    "horn-back": "claims the deep LCL family as a class; no problem named",
    "otter-ladr": "10 of the J13 ring set and 21 of 31 held-out; no "
                  "uniqueness claimed",
    "ifeq": "does not yet prove any of the 7 gap targets",
    "tgt": "the SYN615-10 flip is superseded: the tgt-fold comment proves "
           "SYN615-10 in 0.1s",
    "tgt-fold": "SYN615-10 99x faster than tgt; not unique",
    "mnf-gj-amw": "no solve documented",
    "wm-gj": "backup lane; no solve documented",
    "lazy-gj": "backup lane; no solve documented",
    "bare-gj": "backup lane; no solve documented",
}
HEAD_US = (35.0, 113.0)
TWIN_FACTOR = 20.0
SIM_WALLS = (60.0, 53.0, 45.0)
SIM_WIDTH = 16
DEV_T_HASH_BELOW = 50


def schedule_time(pool, p, lanes, wall, width):
    """First-proof time of the schedule (SCHEDULE order, width slots, the
    run_schedule rules of tools/atp_cli/main.c: budget wall - 3 s, the
    remaining wall per slice, no launch with under 2 s left, the parent's
    stop at wall - 0.5 s) built from the single-lane rows, or None."""
    import heapq
    budget = wall - 3.0 if wall > 6.0 else wall
    deadline = wall - 0.5 if wall > 2.0 else wall
    lr = pool.by[p]
    t = 0.0
    nxt = 0
    run = []
    seq = 0
    while True:
        while len(run) < width and nxt < len(lanes):
            rem = budget - t
            if rem < 2.0:
                nxt = len(lanes)
                break
            r = lr[lanes[nxt]]
            nxt += 1
            if r.cls == "proved" and t + r.wall <= deadline:
                heapq.heappush(run, (t + r.wall, seq, True))
            else:
                heapq.heappush(run, (t + min(r.wall, rem), seq, False))
            seq += 1
        if not run:
            return None
        end, _s, ok = heapq.heappop(run)
        if end > deadline:
            return None
        if ok:
            return end
        t = end


def mode_decisions(paths, pf):
    problems, lanes, rows, pool = context(paths)
    seal = read_seal(paths)
    j13_stems = {stem(n) for n, (_p, s) in problems.items() if s == "j13"}
    train = pool.problems(["train"])
    held = pool.problems(["heldout"])
    allp = train + held
    mates = {p for p in allp if stem(p) in j13_stems}
    j13_with_mates = sum(1 for n, (_p, s) in problems.items()
                         if s == "j13" and stem(n) in {stem(p) for p in allp})

    def half(p):
        return "DEV-T" if stem_hash(stem(p)) < DEV_T_HASH_BELOW else "TRAIN-T"

    tail_train = hard_tail(pool, train)
    tail_held = hard_tail(pool, held)
    dev_t = [(p, c, "heldout") for p, c in tail_held] + \
        [(p, c, "train") for p, c in tail_train if half(p) == "DEV-T"]
    train_t = [(p, c) for p, c in tail_train
               if half(p) == "TRAIN-T" and p not in mates]
    dropped = [(p, c) for p, c in tail_train
               if half(p) == "TRAIN-T" and p in mates]
    dev_t_train = {p for p, _c, s in dev_t if s == "train"}
    TRAIN = [p for p in train if p not in mates and p not in dev_t_train]
    unsolved = [p for p in allp if pool.vbs(p) is None]
    out = []
    W = out.append

    W("Section 5.3 decisions (docs/plans/enigma_portfolio.md), applied to "
      "the lane matrix")
    W(f"Built by tools/atp_enigma/matrix_tables.py --decisions from "
      f"matrix.tsv (sha256 {seal['matrix_tsv_sha256']}): train and heldout "
      f"rows only.  The j13 rows are sealed (count {seal['archive_rows']}, "
      f"archive sha256 {seal['archive_sha256']}).")
    W("60 s wall, single lane, no --proof, CPU clock; shards 1 and 3 at "
      "width 8 on 8-vCPU pods, the others at width 16 (shard 4's first "
      "1184 jobs at width 4 on a 4-vCPU pod).  The 180 s supplement has "
      "not run, so every rule that reads it is provisional where noted.")
    W("")
    W("Sets (section 1.4)")
    W(f"  pool = train {len(train)} + heldout {len(held)} = {len(allp)} "
      f"problems; solved at 60 s by some lane: "
      f"{sum(1 for p in allp if pool.vbs(p) is not None)}; unsolved "
      f"{len(unsolved)}")
    W(f"  J13 stem-mates in the pool (stem = name up to '-'): {len(mates)} "
      f"(train {sum(1 for p in mates if pool.split[p] == 'train')}, heldout "
      f"{sum(1 for p in mates if pool.split[p] == 'heldout')}); J13 "
      f"problems with a pool stem-mate: {j13_with_mates} (names only)")
    W(f"  stem split: DEV-T half iff int(sha256(stem)[:8], 16) % 100 < "
      f"{DEV_T_HASH_BELOW}, else TRAIN-T half")
    W(f"  TRAIN = train - J13 stem-mates - the DEV-T half of the train "
      f"tail = {len(TRAIN)} problems")
    W("")

    # 1. P1 retirement candidates.
    uniq = collections.Counter()
    for p in allp:
        s = pool.solvers(p)
        if len(s) == 1:
            uniq[next(iter(s))] += 1
    picks, union = greedy_cover(pool, allp, lanes)
    picked = {l: (k, g) for k, (l, g, _c, _t) in enumerate(picks, 1)}
    tied = collections.defaultdict(list)
    for k, (l, _g, _c, ti) in enumerate(picks, 1):
        for x in ti:
            tied[x].append(k)
    W("1. P1 retirement candidates")
    W("   Rule: a lane qualifies only if all three hold: no unique pool "
      "solve at 60 s (and at 180 s, once the supplement is in); no marginal "
      "value in the pool cover; no documented unique solve on a CASC "
      "problem in the lane's main.c comment.")
    W(f"   Pool cover: greedy to the full union ({len(union)} problems) over "
      f"train + heldout, gain then total solves then SCHEDULE order: "
      + ", ".join(f"{l}(+{g})" for l, g, _c, _t in picks))
    W("   lane              unique60  cover            documented  qualifies")
    cands = []
    for l in lanes:
        doc = DOC_UNIQUE.get(l)
        cov = f"pick {picked[l][0]} +{picked[l][1]}" if l in picked else \
            ("tied at " + ",".join(map(str, tied[l])) if tied[l] else "none")
        q = uniq[l] == 0 and l not in picked and doc is None
        if q:
            cands.append(l)
        W(f"   {l:17s} {uniq[l]:8d}  {cov:16s} {'yes' if doc else 'no':10s}  "
          f"{'YES' if q else 'no'}")
    fof = [l for l in cands if l.startswith("fof-")]
    ueq = [l for l in cands if not l.startswith("fof-")]
    fastest_tied = collections.Counter()
    fastest_strict = collections.Counter()
    for p in allp:
        f = pool.fastest(p)
        for l in f:
            fastest_tied[l] += 1
        if len(f) == 1:
            fastest_strict[f[0]] += 1
    W(f"   Qualify (provisional until the 180 s supplement): {len(cands)}")
    W(f"     UEQ lanes: {', '.join(ueq) if ueq else 'none'}")
    for l in ueq:
        W(f"       {l}: {DOC_NOTE.get(l, '')}; tied at a cover step: "
          f"{'yes (step ' + ','.join(map(str, tied[l])) + ')' if tied[l] else 'no'}"
          f"; pool solves {sum(1 for p in allp if l in pool.solvers(p))}, "
          f"fastest on {fastest_tied[l]} (strictly {fastest_strict[l]})")
    keep = [l for l in lanes if l not in cands]
    kept = {p for p in allp if any(l in pool.solvers(p) for l in keep)}
    W(f"     joint check: without all {len(cands)} the pool union is "
      f"{len(kept)} of {len(union)} at 60 s")
    W("     The rule reads coverage only.  A qualifying lane can still be "
      "the fastest on many problems (wm is strictly fastest on "
      f"{fastest_strict['wm']} and owns the heldout tail problem"
      + "".join(f" {p}" for p, _c in tail_held if "wm" in pool.fastest(p))
      + "); speed is what WDEP and the J13 band read.  Each candidate "
      "still needs its own M1 paired test on DEV-H plus DEV-T at both "
      "walls.")
    W(f"     fof-* lanes ({len(fof)}): they exit at once on cnf input, so "
      "the UEQ pool carries no information about them; FindProof runs "
      "them on fof input, which the pool does not hold.  Their UEQ cost is "
      "the M1 liveness item (fof-* forks skipped on CNF input), not a "
      "retirement.")
    W("   Documented unique solves (condition 3): " + "; ".join(
        f"{l}: {DOC_UNIQUE[l]}" for l in lanes if l in DOC_UNIQUE))
    W("")

    # 2. Twin families.
    owners_t = collections.Counter()
    for p, c in train_t:
        for l in pool.fastest(p):
            owners_t[l] += 1
    owners_all = collections.Counter()
    for p, _c in tail_train:
        for l in pool.fastest(p):
            owners_all[l] += 1
    rate_all = {}
    rate_long = {}
    for l in lanes:
        r, rl = t5_rates(pool, TRAIN, l)
        rate_all[l] = median(r)
        rate_long[l] = median(rl)
    W("2. Twin families")
    W("   Rule: rank base lanes by how many TRAIN tail problems they solve "
      "fastest; take the top 4 whose median time per selection is at least "
      "20x the head's measured per-pick cost (35-113 us today).")
    W(f"   TRAIN tail = TRAIN-T: {len(train_t)} problems; owners: "
      + (", ".join(f"{l} {c}" for l, c in sorted(
          owners_t.items(), key=lambda kv: (-kv[1], pool.idx[kv[0]])))
         or "none"))
    W(f"   (whole train-split tail, both halves: {len(tail_train)} problems; "
      "owners: " + ", ".join(f"{l} {c}" for l, c in sorted(
          owners_all.items(), key=lambda kv: (-kv[1], pool.idx[kv[0]])))
      + ")")
    W("   median time per selection on TRAIN rows (us): all rows with steps "
      "and CPU > 0 (the T5 column) / rows with CPU >= 1 s")
    for l, _c in sorted(owners_all.items(),
                        key=lambda kv: (-owners_t[kv[0]], -kv[1],
                                        pool.idx[kv[0]])):
        a = 1e6 / rate_all[l] if rate_all[l] else None
        b = 1e6 / rate_long[l] if rate_long[l] else None
        W(f"     {l:17s} TRAIN-T owned {owners_t[l]}  {fmt(a, 0):>7s} / "
          f"{fmt(b, 0):>7s}")
    def twins(owners, rates, head):
        need = TWIN_FACTOR * head
        ranked = sorted((l for l in owners if owners[l] > 0),
                        key=lambda l: (-owners[l], pool.idx[l]))
        return [l for l in ranked if rates[l] and 1e6 / rates[l] >= need][:4]

    W("   Ownership ties rank in SCHEDULE order.  Primary reading: the T5 "
      "median (all rows), at both ends of the head's cost:")
    for label, rates in (("all rows (T5)", rate_all),
                         ("rows with CPU >= 1 s", rate_long)):
        for head in HEAD_US:
            ok = twins(owners_t, rates, head)
            W(f"   twins, median over {label}, head {head:.0f} us (needs >= "
              f"{TWIN_FACTOR * head:.0f} us per selection): "
              f"{', '.join(ok) or 'none'}")
    for head in HEAD_US:
        W(f"   (whole train-split tail, T5 median, head {head:.0f} us: "
          f"{', '.join(twins(owners_all, rate_all, head)) or 'none'})")
    W("   The tail is tiny (the pool has no band, section 2.3): every owner "
      "holds 1-2 problems, so this ranking is set by ties and the stem "
      "split.  It is provisional until the 180 s supplement adds the "
      "problems only it solves (condition 2 of the tail rule).")
    W("")

    # 3. DEV-T and TRAIN-T.
    W("3. DEV-T and TRAIN-T")
    W("   Rule: the hard-tail rule of section 1.4 (the fastest lane needs "
      "more than 10 s at 60 s; only the 180 s supplement solves it; every "
      "lane that searched it ended on the memory guard); the train tail "
      "problems split by sha256 of the stem.")
    W("   Condition 2 waits on the supplement; conditions 1 and 3 give:")
    W(f"   DEV-T ({len(dev_t)}): " + ", ".join(
        f"{p} ({s}, {c}, {fmt(pool.vbs(p))} s {','.join(pool.fastest(p))})"
        for p, c, s in dev_t))
    W(f"   TRAIN-T ({len(train_t)}): " + ", ".join(
        f"{p} ({c}, {fmt(pool.vbs(p))} s {','.join(pool.fastest(p))})"
        for p, c in train_t))
    if dropped:
        W(f"   dropped (J13 stem-mates in the TRAIN-T half): "
          + ", ".join(p for p, _c in dropped))
    W(f"   pending condition 2: the {len(unsolved)} pool problems no lane "
      "solves at 60 s join DEV-T (heldout, or train in the DEV-T half) or "
      "TRAIN-T (train in the TRAIN-T half, not a stem-mate) if only the "
      "180 s supplement solves them: train DEV-T half "
      f"{sum(1 for p in unsolved if pool.split[p] == 'train' and half(p) == 'DEV-T')}, "
      f"train TRAIN-T half {sum(1 for p in unsolved if pool.split[p] == 'train' and half(p) == 'TRAIN-T' and p not in mates)}, "
      f"heldout {sum(1 for p in unsolved if pool.split[p] == 'heldout')}")
    write_tsv(paths.m("dev_t.tsv"), ["problem", "split", "condition",
                                     "fastest_s", "owner", "stem",
                                     "stem_hash"],
              [(p, s, c, pool.vbs(p), ",".join(pool.fastest(p)), stem(p),
                stem_hash(stem(p))) for p, c, s in dev_t])
    write_tsv(paths.m("train_t.tsv"), ["problem", "condition", "fastest_s",
                                       "owner", "stem", "stem_hash"],
              [(p, c, pool.vbs(p), ",".join(pool.fastest(p)), stem(p),
                stem_hash(stem(p))) for p, c in train_t])
    W("   files: dev_t.tsv, train_t.tsv")
    W("")

    # 4. Supplement list.
    sup = [(p, l) for p in unsolved for l in lanes
           if pool.by[p][l].cls != "fast-exit-not-applicable"]
    write_jobs(paths.m("supplement_jobs.tsv"), pool, sup)
    W("4. Supplement list")
    W("   Rule: every pool problem no lane solves at 60 s, run on every lane "
      "whose class on it was not fast-exit.")
    W(f"   {len(unsolved)} problems (train "
      f"{sum(1 for p in unsolved if pool.split[p] == 'train')}, heldout "
      f"{sum(1 for p in unsolved if pool.split[p] == 'heldout')}), "
      f"{len(sup)} (problem, lane) jobs -> supplement_jobs.tsv (runner job "
      "format; run at --wall=180)")
    by_cls = collections.Counter(pool.by[p][l].cls for p, l in sup)
    W("   their 60 s end classes: " + ", ".join(
        f"{c} {n}" for c, n in by_cls.most_common()))
    if by_cls["crash"]:
        cp = sorted({p for p, l in sup if pool.by[p][l].cls == "crash"})
        W(f"   the {by_cls['crash']} crash jobs ({', '.join(cp)}) end in "
          "SIGSEGV; SYN986-10.006's is a stack overflow from unbounded "
          "recursion over a deep term, reproduced under ASan "
          "(results/shard_6_crash_SYN986-10.006.txt).  At 180 s they crash "
          "again unless the engine is fixed first.")
    W("")

    # 5. Label-harvest list.
    lab = [(p, l) for p in TRAIN for l in lanes
           if pool.by[p][l].cls == "proved"]
    write_jobs(paths.m("label_harvest_jobs.tsv"), pool, lab)
    W("5. Label-harvest list")
    W("   Rule: every TRAIN (problem, lane) pair proved at 60 s.")
    W(f"   {len(lab)} pairs on {len({p for p, _l in lab})} TRAIN problems "
      "-> label_harvest_jobs.tsv (runner job format; rerun with --proof and "
      "history export)")
    fe = sum(1 for p, l in lab if pool.by[p][l].detail.startswith("front end"))
    W(f"   of them front-end proofs (no engine selections to label): {fe}")
    W("")

    # 6. M4a inputs.
    mg = [(p, l) for p in unsolved for l in lanes
          if pool.by[p][l].cls == "memory-guard"]
    write_jobs(paths.m("oracle_rerun_jobs.tsv"), pool, mg)
    per = collections.Counter(l for _p, l in mg)
    per_all = collections.Counter(r.lane for r in rows
                                  if r.cls == "memory-guard")
    W("6. M4a inputs")
    W("   Rule: memory-guard misses per lane; the list of oracle reruns.")
    W("   memory-guard ends per lane, on pool problems no lane solves / all: "
      + ", ".join(f"{l} {per[l]}/{per_all[l]}" for l in lanes
                  if per_all[l]))
    W(f"   oracle reruns: {len(mg)} (problem, lane) pairs on "
      f"{len({p for p, _l in mg})} problems -> oracle_rerun_jobs.tsv (single "
      "lane at 4x the cap: THVM_ATP_RSS_ABORT_MB=18000, THVM_HEAP_CELLS "
      "raised)")
    heap = sum(1 for p, l in mg if "term heap" in pool.by[p][l].detail)
    W(f"   of them term-heap exhaustions (thvm fatal): {heap}")
    W("")

    # 7. M4b pre-gate.
    sim_set = TRAIN + [p for p, _c, _s in dev_t]
    single = sum(1 for p in allp if len(pool.solvers(p)) == 1)
    W("7. M4b pre-gate")
    W("   Rule: the simulated oracle plan against simulated P1 on TRAIN plus "
      "DEV-T; the count of problems solved by exactly one lane.")
    W(f"   P1 = P0 until an M1 change passes; simulated: SCHEDULE order at "
      f"width {SIM_WIDTH}, the run_schedule launch rules, single-lane 60 s "
      "times.  Oracle plan with P1's shape: the fastest solving lane in a "
      "t=0 slot.")
    for wall in SIM_WALLS:
        orc = [p for p in sim_set if pool.vbs(p) is not None
               and pool.vbs(p) <= wall - 0.5]
        sim = [p for p in sim_set
               if schedule_time(pool, p, lanes, wall, SIM_WIDTH) is not None]
        lost = sorted(set(orc) - set(sim))
        W(f"   wall {wall:.0f} s on TRAIN + DEV-T ({len(sim_set)} problems): "
          f"oracle {len(orc)}, simulated P1 {len(sim)}, oracle - P1 = "
          f"{len(orc) - len(sim)}" + (f" ({', '.join(lost)})" if lost else ""))
    W("   walls below 60 s reuse the 60 s single-lane times: a lane that "
      "reads the wall (LRS, the Horn release) is not re-run")
    W(f"   pool problems solved by exactly one lane at 60 s: {single} "
      f"(train {sum(1 for p in train if len(pool.solvers(p)) == 1)}, heldout "
      f"{sum(1 for p in held if len(pool.solvers(p)) == 1)}); the pre-gate "
      "needs at least 15")
    W("   at the shorter walls (a slower second solver drops out): " + ", ".join(
        f"{w:.0f} s {sum(1 for p in allp if sum(1 for t in pool.solvers(p).values() if t <= w - 0.5) == 1)}"
        for w in SIM_WALLS))
    W("   pre-gate verdict on the matrix: FAIL.  oracle - P1 is 0 at 45, 53 "
      "and 60 s (it needs +5 at WDEP), so the first condition fails at "
      "every wall the 60 s matrix can simulate, whatever the other two "
      "give.  Only a WDEP of 70 s (expected 53-60 s) lies outside it.  As "
      "section M4b expected: no learned scheduler.")
    W("")

    # 8. M4c pre-gate.
    m4c = []
    for p in TRAIN:
        st, rating = pf.header(pool.path[p])
        if st == "Unsatisfiable" and rating is not None and rating < 1.0 \
                and pool.vbs(p) is None:
            m4c.append((p, rating))
    W("8. M4c pre-gate")
    W("   Rule: TRAIN Unsatisfiable problems rated below 1.0 that no lane "
      "solves at 60 s or at 180 s.")
    W(f"   at 60 s: {len(m4c)} (an upper bound: the 180 s supplement can "
      "only remove problems); the pre-gate needs at least 25")
    W("   " + ", ".join(f"{p} {r:.2f}" for p, r in sorted(m4c)))
    W(f"   pre-gate verdict: {'FAIL' if len(m4c) < 25 else 'open'} "
      + ("(final: the supplement cannot raise the count).  There is no "
         "invention pilot." if len(m4c) < 25 else
         "(decided once the supplement is in)"))
    W("")

    # The main.c quotes name CASC problems, some of them J13 problems.
    text = j13_name_regex(problems).sub("[J13 problem]", "\n".join(out))
    with open(paths.m("decisions_5_3.txt"), "w") as fp:
        fp.write(text + "\n")
    assert_no_j13(paths, problems, [paths.m(f) for f in (
        "decisions_5_3.txt", "dev_t.tsv", "train_t.tsv",
        "supplement_jobs.tsv", "label_harvest_jobs.tsv",
        "oracle_rerun_jobs.tsv")])
    print(text)


TABLES_README = [
    "Lane-matrix pool tables (docs/plans/enigma_portfolio.md 5.2 step 4),",
    "built by tools/atp_enigma/matrix_tables.py --tables from matrix.tsv",
    "(train and heldout rows only; the j13 rows are sealed).  One file per",
    "table and split: <table>_<split>.tsv with a header row.  Times are the",
    "single-lane wall_s of a 60 s run (slice wall 57 s, stopped at 59.5 s).",
    "",
    "Pod caveat: shards 1 and 3 ran at width 8 on 8-vCPU / 32 GB pods; shards",
    "2, 5 and 6 at width 16 on 16-vCPU / 64 GB pods; shard 4 at width 16 on a",
    "16-vCPU pod except its first 1184 jobs (803 of them train or heldout),",
    "which ran at width 4 on a 4-vCPU / 16 GB pod.  All lanes of a problem",
    "ran in one shard, so per-problem lane comparisons share a pod; times",
    "across problems mix pod types and widths (matrix.tsv column 16).",
    "",
    "T1_lanes        per lane: proved, proved by that lane alone, proved",
    "                within 1/3/10/30/57 s, rows per end class",
    "T2_vbs_curve    virtual best solver: problems solved by time t",
    "T2_greedy_cover greedy lane cover to the full union (k = 1..16 and",
    "                beyond); ties = lanes with the same gain at that step",
    "T3_unsolved     problems no lane proves, the end classes of their",
    "                searching lanes; T3_groups counts the end patterns",
    "T4_tail         fastest lane > 10 s, or every searching lane ended on",
    "                the memory guard; owner = fastest lane; T4_owners",
    "T5_selection_rate  median engine steps (selections) per CPU second,",
    "                over rows with steps and CPU > 0, and over rows with",
    "                CPU >= 1 s; us per selection and the 5% per-pick budget",
    "T6_jaccard, T6_pairs  Jaccard similarity of solved sets",
    "T6_fastest      problems on which a lane is fastest (ties count) and",
    "                strictly fastest",
    "T7_replica      tools/atp_enigma/static_gates.py (the static gate",
    "                replica) against the end class: dead/live/unknown x",
    "                fast-exit/proved/searched; T7_violations = dead lanes",
    "                that proved (must be empty); otter-* are not static",
    "T8_memory       memory-guard ends (all, on unsolved problems, term heap",
    "                exhausted) and maxrss quantiles of searching rows (MB)",
    "",
]



def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--matrix-dir", default="~/.cache/thvm/enigma/matrix")
    ap.add_argument("--sealed-dir", default="~/.cache/thvm/enigma/sealed")
    g = ap.add_mutually_exclusive_group(required=True)
    for m in ("seal", "validate", "tables", "decisions", "all"):
        g.add_argument("--" + m, action="store_true")
    a = ap.parse_args()
    paths = Paths(a.matrix_dir, a.sealed_dir)
    if a.seal:
        mode_seal(paths)
        return
    if a.validate or a.all:
        mode_validate(paths)
    if a.tables or a.decisions or a.all:
        pf = PoolFiles(paths)
        try:
            if a.tables or a.all:
                mode_tables(paths, pf)
            if a.decisions or a.all:
                mode_decisions(paths, pf)
        finally:
            pf.tmp.cleanup()


if __name__ == "__main__":
    main()
