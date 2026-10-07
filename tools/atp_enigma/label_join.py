#!/usr/bin/env python3
# The label join (docs/plans/enigma_portfolio.md, M2 deliverable 3 and
# section 3.2): listwise training groups for guide_net.py from a guided
# harvest (tools/atp_enigma/guide_harvest.sh).  Per (problem, lane) run it
# joins
#   - the run's THVM_ATP_GUIDE_DUMP (docs/atp/guide.md): every dumped pick's
#     candidates with their inputs x (layout v1) and seqs, with
#   - the same run's history-export labels (np0_extract.py, export v2) by
#     CP seq: a candidate is a proof positive when the run proved and the
#     CP is a trace-DAG ancestor of what the emitted proof cites (label),
#   - plus cross-lane positives: a candidate the run selected whose rule's
#     canonical equation key (rule_key) is in another lane's proof of the
#     same problem (proof_keys), only between lanes whose input signature
#     agrees (np0_extract's sig: the same input transform), from a keys
#     table (the keys subcommand over label-harvest and harvest npz files),
#   - plus hindsight positives for failed runs: the ancestors of the rules
#     born in the run's second half (np0_extract's hindsight mode).
# Export v2 holds no content for a CP the run never selected, so only
# selected candidates can carry a cross-lane or hindsight label.
#
# A group is one dumped pick (one scored call: an orphan re-pick at the
# same selection is its own group).  Its rows are its candidates in
# evaluation order; row 0 is the engine's next CP (top-K rank 0), and the
# chosen row is the layer's pick.  The labels are a bit mask per row:
#   1 OWN    a proof positive of this run
#   2 CROSS  a cross-lane positive
#   4 HIND   a hindsight positive (failed runs only)
# Each group also counts the proof positives pending in the whole queue
# at its selection t (pushed before t, selected at t or later), the
# readout's denominator, which a top-K candidate set alone cannot give.
#
# Usage:
#   label_join.py keys <out_keys.tsv> <index.tsv> <npz_dir> [<index.tsv> <npz_dir>]...
#   label_join.py join <out_dir> <keys.tsv> <harvest_dir>... [--jobs N]
#   label_join.py selftest
# keys reads every proof-mode row of a label-harvest or harvest index (19
# columns, tools/atp_enigma/README.md) and writes problem, lane, sig and
# the run's proof keys.  join reads every <harvest_dir>'s index.tsv
# (guide_harvest.sh: the 19 label-harvest columns, then the dump's sampling
# rate and bytes), npz/<problem>__<lane>.npz, dump/<problem>__<lane>.dump.gz
# and phi.tsv (FindProof --problem-features --header rows), and writes one
# groups_<lane>.npz per lane (arrays below), stats.tsv (per lane and label
# source) and summary.txt.  A run is joined only when its dump's last
# session, its npz and its phi(p) row agree: the chosen candidate of the
# last call at every dumped selection t is the CP the export records as
# selected at t, and every row's phi columns equal the phi(p) row.  A
# session without its footer was cut short, so its last selection's
# calls are dropped.  Only TRAIN and TRAIN-X rows are read (amendment
# A24): any other split stops the join.
#
# groups_<lane>.npz:
#   run-level (R): run_problem, run_split, run_stem, run_mode (proof /
#     hindsight), run_nsel (selections), run_rating (meta_rating, -1 when
#     the problem has no Rating header), run_every (the dump's sampling),
#     run_phi [R, n_phi] (float32), run_sig, run_complete (the footer was
#     there), lane (one string: the file's lane)
#   group-level (G): g_run, g_t, g_off [G + 1] (row offsets), g_chosen,
#     g_pend (proof positives pending at t)
#   row-level (N): x [N, 20] (float32, x[0..20) of layout v1), seq, lab
#     (the bit mask), sel (1 when the run selected the CP at some point)
import gzip
import multiprocessing
import os
import re
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import phi as phi_mod  # noqa: E402
from matrix_tables import stem, x_stem  # noqa: E402

N_DYN = 20          # x[0..20): the candidate and state columns of layout v1
OWN, CROSS, HIND = 1, 2, 4
SPLITS = ("train", "TRAIN-X")
NONE32 = 0xFFFFFFFF


def die(msg):
    sys.exit("label_join: " + msg)


def run_stem(name, split):
    """A24 item 8: stem() for pool names, x_stem() for corpus names."""
    return x_stem(name) if split == "TRAIN-X" else stem(name)


def read_index(path):
    """Rows of a label-harvest (19 columns) or harvest (21+) index."""
    rows = []
    with open(path) as f:
        for n, line in enumerate(f, 1):
            c = line.rstrip("\n").split("\t")
            if not c or c[0] in ("", "problem") or c[0].startswith("#"):
                continue
            if len(c) < 19:
                die(f"{path}:{n}: {len(c)} columns, want 19 or more")
            rows.append(c)
    return rows


# --- the keys table -------------------------------------------------------

def keys_one(args):
    problem, lane, path = args
    try:
        z = np.load(path)
        if not bool(z["proved"]):
            return None
        keys = np.unique(z["proof_keys"].astype(np.uint64))
        sig = int(z["sig"])
        zl = str(z["lane"])
    except (OSError, KeyError, ValueError) as e:
        return (problem, lane, None, f"{type(e).__name__}: {e}")
    if zl and zl != lane:
        return (problem, lane, None, f"npz lane {zl}")
    return (problem, lane, sig, ",".join("%016x" % int(k) for k in keys))


def cmd_keys(args):
    if len(args) < 3 or len(args) % 2 != 1:
        die("keys needs <out_keys.tsv> and pairs <index.tsv> <npz_dir>")
    out, pairs = args[0], args[1:]
    jobs = []
    for i in range(0, len(pairs), 2):
        for c in read_index(pairs[i]):
            if c[17] != "proof":
                continue
            path = os.path.join(pairs[i + 1], f"{c[0]}__{c[1]}.npz")
            if os.path.exists(path):
                jobs.append((c[0], c[1], path))
    bad = 0
    with multiprocessing.Pool(min(16, os.cpu_count() or 1)) as pool, open(out + ".tmp", "w") as f:
        f.write("problem\tlane\tsig\tn_keys\tkeys\n")
        for r in pool.imap(keys_one, jobs, chunksize=64):
            if r is None:
                continue
            problem, lane, sig, keys = r
            if sig is None:
                bad += 1
                print(f"label_join keys: {problem} {lane}: {keys}", file=sys.stderr)
                continue
            n = len(keys.split(",")) if keys else 0
            f.write(f"{problem}\t{lane}\t{sig:016x}\t{n}\t{keys or '-'}\n")
    os.replace(out + ".tmp", out)
    print(f"label_join keys: {len(jobs)} proof-mode npz files, {bad} unreadable -> {out}")
    return 1 if bad else 0


def load_keys(paths):
    """problem -> [(lane, sig, set of keys)]."""
    by = {}
    for path in paths:
        with open(path) as f:
            for n, line in enumerate(f, 1):
                c = line.rstrip("\n").split("\t")
                if c[0] == "problem":
                    continue
                if len(c) != 5:
                    die(f"{path}:{n}: {len(c)} columns, want 5")
                keys = set() if c[4] == "-" else {int(k, 16) for k in c[4].split(",")}
                by.setdefault(c[0], []).append((c[1], int(c[2], 16), keys))
    return by


def cross_keys(by, problem, lane, sig):
    """The proof keys of the OTHER lanes of problem whose sig is sig."""
    out = set()
    for ln, sg, keys in by.get(problem, ()):
        if ln != lane and sg == sig:
            out |= keys
    return out


# --- dumps ------------------------------------------------------------------

HDR_RE = re.compile(r"^# guide (?!end)(.*)$", re.M)


def read_dump(path):
    """The sessions of a dump file: [(header dict, float64 rows [n, 6 + n_x],
    complete)].  complete: the session ended with its '# guide end' footer;
    without it a partial last line is cut off."""
    opener = gzip.open if path.endswith(".gz") else open
    with opener(path, "rt") as f:
        text = f.read()
    heads = list(HDR_RE.finditer(text))
    out = []
    for i, m in enumerate(heads):
        hdr = dict(tok.split("=", 1) for tok in m.group(1).split() if "=" in tok)
        body = text[m.end() + 1:heads[i + 1].start() if i + 1 < len(heads) else len(text)]
        complete = False
        e = body.find("# guide end")
        if e >= 0:
            complete = True
            body = body[:e]
        elif body and not body.endswith("\n"):
            body = body[:body.rfind("\n") + 1]
        width = 6 + int(hdr.get("n_x", "0"))
        if body.strip():
            vals = np.fromstring(body.strip().replace("\n", "\t"), dtype=np.float64, sep="\t")
            if width <= 6 or len(vals) % width != 0:
                raise ValueError(f"{path}: session {i}: {len(vals)} values, rows of {width}")
            rows = vals.reshape(-1, width)
        else:
            rows = np.zeros((0, width))
        out.append((hdr, rows, complete))
    return out


# --- the join ----------------------------------------------------------------

def join_run(args):
    """One run's groups, or (key, None, why)."""
    key, problem, lane, split, n_sel_ix, dump_path, npz_path, phi_row, xkeys = args
    try:
        sess = read_dump(dump_path)
    except (OSError, ValueError, EOFError) as e:
        return key, None, f"dump unreadable: {e}"
    if not sess:
        return key, None, "dump without a session"
    hdr, a, complete = sess[-1]
    if hdr.get("layout") != "v1":
        return key, None, f"dump layout {hdr.get('layout')}"
    n_x = int(hdr["n_x"])
    n_phi = int(hdr.get("phi", "0"))
    if len(a) == 0:
        return key, None, "no dumped pick"
    t = a[:, 0].astype(np.int64)
    idx = a[:, 1].astype(np.int64)
    rseq = a[:, 2].astype(np.int64)
    chosen = a[:, 3].astype(np.int64)
    x = a[:, 4:4 + n_x]
    starts = np.nonzero(idx == 0)[0]
    if len(starts) == 0 or starts[0] != 0:
        return key, None, "dump rows do not start a pick"
    ends = np.append(starts[1:], len(a))
    if not complete:
        # The last selection's calls may be cut short.
        keep = t[starts] < t[starts[-1]]
        starts, ends = starts[keep], ends[keep]
        if len(starts) == 0:
            return key, None, "cut-short dump with one selection"
        a_end = ends[-1]
        t, idx, rseq, chosen, x = t[:a_end], idx[:a_end], rseq[:a_end], chosen[:a_end], x[:a_end]
    try:
        z = np.load(npz_path)
        zseq = z["seq"].astype(np.int64)
        zlab = z["label"].astype(bool)
        zhind = z["hind_label"].astype(bool)
        zsel = z["selected_at"].astype(np.int64)
        zpush = z["push_step"].astype(np.int64)
        zkey = z["rule_key"].astype(np.uint64)
        proved = bool(z["proved"])
        sig = int(z["sig"])
        zlane = str(z["lane"])
        n_sel = int(z["meta"][2])
    except (OSError, KeyError, ValueError) as e:
        return key, None, f"npz unreadable: {e}"
    if zlane and zlane != lane:
        return key, None, f"npz lane {zlane}"
    # The phi(p) columns: every row equals the problem's row.
    phi_bad = 0
    if n_phi:
        if phi_row is None or n_x < N_DYN + n_phi:
            return key, None, "no phi(p) row for a head that reads it"
        want = np.asarray(phi_row[:n_phi], np.float32).astype(np.float64)
        phi_bad = int(np.count_nonzero(np.any(x[:, N_DYN:N_DYN + n_phi] != want, axis=1)))
    if phi_bad:
        return key, None, f"{phi_bad} rows' phi(p) columns differ from the phi(p) row"
    # Labels by seq.
    j = np.searchsorted(zseq, rseq)
    found = (j < len(zseq)) & (zseq[np.minimum(j, max(len(zseq) - 1, 0))] == rseq) if len(zseq) \
        else np.zeros(len(rseq), bool)
    jj = np.minimum(j, max(len(zseq) - 1, 0))
    lab = np.zeros(len(rseq), np.uint8)
    sel = np.zeros(len(rseq), np.uint8)
    if len(zseq):
        own = found & zlab[jj] & proved
        hind = found & zhind[jj] & (not proved)
        rk = np.where(found, zkey[jj], np.uint64(0))
        if xkeys:
            kk = np.fromiter(xkeys, dtype=np.uint64, count=len(xkeys))
            cross = (rk != 0) & np.isin(rk, kk)
        else:
            cross = np.zeros(len(rseq), bool)
        lab = (own * OWN + cross * CROSS + hind * HIND).astype(np.uint8)
        sel = (found & (zsel[jj] >= 0)).astype(np.uint8)
    # The chosen CP of the last call at each t is the export's selection.
    by_t = {}
    for s0, s1 in zip(starts.tolist(), ends.tolist()):
        by_t[int(t[s0])] = (s0, s1)
    sel_seq = {}
    for q, st in zip(zseq[zsel >= 0].tolist(), zsel[zsel >= 0].tolist()):
        sel_seq[st] = q
    mism = 0
    for tt, (s0, s1) in by_t.items():
        c = np.nonzero(chosen[s0:s1])[0]
        if len(c) != 1 or sel_seq.get(tt) != int(rseq[s0 + c[0]]):
            mism += 1
    if mism:
        return key, None, f"{mism} of {len(by_t)} dumped selections disagree with the export"
    # Proof positives pending at each group's t.
    pos = zlab & (zsel >= 1) & proved
    pp = np.sort(zpush[pos])
    ps = np.sort(zsel[pos])
    gt = t[starts]
    pend = np.searchsorted(pp, gt - 1, side="right") - np.searchsorted(ps, gt - 1, side="right")
    g_chosen = np.array([int(np.nonzero(chosen[s0:s1])[0][0]) for s0, s1 in zip(starts, ends)],
                        np.int16)
    out = {
        "x": x[:, :N_DYN].astype(np.float32), "seq": rseq.astype(np.uint32), "lab": lab,
        "sel": sel, "g_t": gt.astype(np.int32), "g_n": (ends - starts).astype(np.int32),
        "g_chosen": g_chosen, "g_pend": pend.astype(np.int32),
        "phi": x[0, N_DYN:N_DYN + n_phi].astype(np.float32),
        "mode": "proof" if proved else "hindsight", "n_sel": n_sel if n_sel else n_sel_ix,
        "every": int(hdr.get("every", "1")), "sig": sig, "complete": complete,
    }
    return key, out, ""


def cmd_join(args):
    jobs_n = os.cpu_count() or 1
    if "--jobs" in args:
        i = args.index("--jobs")
        jobs_n = int(args[i + 1])
        args = args[:i] + args[i + 2:]
    if len(args) < 3:
        die("join needs <out_dir> <keys.tsv> <harvest_dir>...")
    out, key_path, hdirs = args[0], args[1], args[2:]
    os.makedirs(out, exist_ok=True)
    by = load_keys([key_path])
    phi_of, rows, seen = {}, [], set()
    for hdir in hdirs:
        names, phis, metas = phi_mod.load(os.path.join(hdir, "phi.tsv"))
        for nm, p, m in zip(names, phis, metas):
            if nm in phi_of and not np.array_equal(phi_of[nm][0], p):
                die(f"{nm}: the harvest directories' phi(p) rows differ")
            phi_of[nm] = (p, m)
        for c in read_index(os.path.join(hdir, "index.tsv")):
            if (c[0], c[1]) in seen:
                die(f"{c[0]} {c[1]}: two runs in the harvest directories")
            seen.add((c[0], c[1]))
            rows.append((hdir, c))
    lanes = sorted({c[1] for _h, c in rows})
    tasks, skipped = [], {}
    for hdir, c in rows:
        problem, lane, split = c[0], c[1], c[6]
        if split not in SPLITS:
            die(f"{problem} {lane}: split {split}: only TRAIN and TRAIN-X are training data (A24)")
        k = f"{problem}__{lane}"
        dump = os.path.join(hdir, "dump", k + ".dump.gz")
        npz = os.path.join(hdir, "npz", k + ".npz")
        why = None
        if c[17] not in ("proof", "hindsight"):
            why = f"mode {c[17]}"
        elif not os.path.exists(dump):
            why = "no dump"
        elif not os.path.exists(npz):
            why = "no npz"
        if why:
            skipped.setdefault(lane, []).append((problem, why))
            continue
        pr = phi_of.get(problem)
        try:
            sig = int(np.load(npz)["sig"])
        except (OSError, KeyError, ValueError):
            sig = None
        xk = cross_keys(by, problem, lane, sig) if sig is not None else set()
        n_sel = int(c[13]) if c[13].isdigit() else 0
        tasks.append((k, problem, lane, split, n_sel, dump, npz,
                      None if pr is None else pr[0], xk))
    res = {}
    with multiprocessing.Pool(max(1, jobs_n)) as pool:
        for k, r, why in pool.imap_unordered(join_run, tasks, chunksize=1):
            res[k] = (r, why)
    stat_lines = ["lane\truns\tjoined\tskipped\tproof_runs\thindsight_runs\tgroups\tgroups_own\t"
                  "groups_cross\tgroups_hind\tgroups_any\tgroups_cross_only\tgroups_hind_only\t"
                  "rows\tpos_own\tpos_cross\tpos_hind\truns_own\truns_cross\truns_hind\t"
                  "runs_cross_only\tpending_groups"]
    summary = []
    for lane in lanes:
        R = {nm: [] for nm in ("run_problem", "run_split", "run_stem", "run_mode", "run_nsel",
                               "run_rating", "run_every", "run_phi", "run_sig", "run_complete")}
        G = {nm: [] for nm in ("g_run", "g_t", "g_n", "g_chosen", "g_pend")}
        N = {nm: [] for nm in ("x", "seq", "lab", "sel")}
        bad = []
        st = dict.fromkeys(stat_lines[0].split("\t")[1:], 0)
        lane_tasks = sorted((t for t in tasks if t[2] == lane), key=lambda t: t[1])
        st["runs"] = len(lane_tasks) + len(skipped.get(lane, []))
        st["skipped"] = len(skipped.get(lane, []))
        for t_ in lane_tasks:
            r, why = res[t_[0]]
            if r is None:
                bad.append((t_[1], why))
                st["skipped"] += 1
                continue
            ri = len(R["run_problem"])
            pr = phi_of.get(t_[1])
            R["run_problem"].append(t_[1])
            R["run_split"].append(t_[3])
            R["run_stem"].append(run_stem(t_[1], t_[3]))
            R["run_mode"].append(r["mode"])
            R["run_nsel"].append(r["n_sel"])
            R["run_rating"].append(float(pr[1][1]) if pr is not None else -1.0)
            R["run_every"].append(r["every"])
            R["run_phi"].append(r["phi"])
            R["run_sig"].append(r["sig"])
            R["run_complete"].append(r["complete"])
            G["g_run"].append(np.full(len(r["g_t"]), ri, np.int32))
            for nm in ("g_t", "g_n", "g_chosen", "g_pend"):
                G[nm].append(r[nm])
            for nm in N:
                N[nm].append(r[nm])
            st["joined"] += 1
            st["proof_runs" if r["mode"] == "proof" else "hindsight_runs"] += 1
            off = np.concatenate([[0], np.cumsum(r["g_n"])])
            lab = r["lab"]
            g_or = np.bitwise_or.reduceat(lab, off[:-1]) if len(lab) else np.zeros(0, np.uint8)
            st["groups"] += len(g_or)
            st["groups_own"] += int(np.count_nonzero(g_or & OWN))
            st["groups_cross"] += int(np.count_nonzero(g_or & CROSS))
            st["groups_hind"] += int(np.count_nonzero(g_or & HIND))
            st["groups_any"] += int(np.count_nonzero(g_or))
            st["groups_cross_only"] += int(np.count_nonzero(g_or == CROSS))
            st["groups_hind_only"] += int(np.count_nonzero(g_or == HIND))
            st["rows"] += len(lab)
            st["pos_own"] += int(np.count_nonzero(lab & OWN))
            st["pos_cross"] += int(np.count_nonzero(lab & CROSS))
            st["pos_hind"] += int(np.count_nonzero(lab & HIND))
            st["runs_own"] += int(bool(np.any(g_or & OWN)))
            st["runs_cross"] += int(bool(np.any(g_or & CROSS)))
            st["runs_hind"] += int(bool(np.any(g_or & HIND)))
            st["runs_cross_only"] += int(bool(np.any(g_or & CROSS)) and not np.any(g_or & OWN))
            st["pending_groups"] += int(np.count_nonzero(r["g_pend"] > 0))
        stat_lines.append(lane + "\t" + "\t".join(str(st[k]) for k in stat_lines[0].split("\t")[1:]))
        n_phi = len(R["run_phi"][0]) if R["run_phi"] else 0
        g_n = np.concatenate(G["g_n"]) if G["g_n"] else np.zeros(0, np.int32)
        np.savez(os.path.join(out, f"groups_{lane}.npz"),
                 lane=np.array(lane),
                 run_problem=np.array(R["run_problem"], dtype=np.str_),
                 run_split=np.array(R["run_split"], dtype=np.str_),
                 run_stem=np.array(R["run_stem"], dtype=np.str_),
                 run_mode=np.array(R["run_mode"], dtype=np.str_),
                 run_nsel=np.array(R["run_nsel"], np.int64),
                 run_rating=np.array(R["run_rating"], np.float32),
                 run_every=np.array(R["run_every"], np.int32),
                 run_phi=(np.stack(R["run_phi"]) if R["run_phi"] else np.zeros((0, n_phi), np.float32)),
                 run_sig=np.array(R["run_sig"], np.uint64),
                 run_complete=np.array(R["run_complete"], bool),
                 g_run=np.concatenate(G["g_run"]) if G["g_run"] else np.zeros(0, np.int32),
                 g_t=np.concatenate(G["g_t"]) if G["g_t"] else np.zeros(0, np.int32),
                 g_off=np.concatenate([[0], np.cumsum(g_n)]).astype(np.int64),
                 g_chosen=np.concatenate(G["g_chosen"]) if G["g_chosen"] else np.zeros(0, np.int16),
                 g_pend=np.concatenate(G["g_pend"]) if G["g_pend"] else np.zeros(0, np.int32),
                 x=np.concatenate(N["x"]) if N["x"] else np.zeros((0, N_DYN), np.float32),
                 seq=np.concatenate(N["seq"]) if N["seq"] else np.zeros(0, np.uint32),
                 lab=np.concatenate(N["lab"]) if N["lab"] else np.zeros(0, np.uint8),
                 sel=np.concatenate(N["sel"]) if N["sel"] else np.zeros(0, np.uint8))
        summary.append(f"{lane}: {st['joined']} of {st['runs']} runs joined ({st['proof_runs']} proof, "
                       f"{st['hindsight_runs']} hindsight), {st['groups']} groups, {st['rows']} rows; "
                       f"groups with a positive {st['groups_any']} (own {st['groups_own']}, cross-lane "
                       f"{st['groups_cross']}, of them cross-lane only {st['groups_cross_only']}; "
                       f"hindsight {st['groups_hind']})")
        for problem, why in skipped.get(lane, []) + bad:
            summary.append(f"  {lane} {problem}: not joined: {why}")
    with open(os.path.join(out, "stats.tsv"), "w") as f:
        f.write("\n".join(stat_lines) + "\n")
    txt = "\n".join(summary)
    with open(os.path.join(out, "summary.txt"), "w") as f:
        f.write(txt + "\n")
    print(txt)
    return 0


# --- self-test: synthetic harvests ------------------------------------------

def st_npz(path, lane, proved, sig, n_sel, cps, proof_keys):
    """A synthetic np0_extract npz: cps is a list of (seq, push_step,
    selected_at, label, hind_label, rule_key)."""
    cps = sorted(cps)
    np.savez_compressed(
        path, seq=np.array([c[0] for c in cps], np.uint32),
        push_step=np.array([c[1] for c in cps], np.int64),
        selected_at=np.array([c[2] for c in cps], np.int64),
        label=np.array([c[3] for c in cps], bool), hind_label=np.array([c[4] for c in cps], bool),
        rule_key=np.array([c[5] for c in cps], np.uint64), proved=np.array(proved),
        sig=np.array(sig, np.uint64), lane=np.array(lane),
        proof_keys=np.array(sorted(proof_keys), np.uint64),
        meta=np.array([1.0, 1.0, float(n_sel)] + [0.0] * 9))


def st_dump(path, lane, phi, picks, footer=True, cut=False, n_phi=40, every=1, extra=None):
    """A synthetic dump: picks is a list of (t, [(seq, ...)], chosen)."""
    n_x = N_DYN + n_phi
    lines = [f"# guide model=harvest layout=v1 n_x={n_x} hidden=1 family={lane} mode=topk k=64 "
             f"tau=0 coop=0 zero=1 kappa=1 lane=0 lane_name={lane} phi={n_phi} every={every} "
             f"max=400 rows=0"]
    rows = 0
    for t, seqs, ch in picks:
        for i, q in enumerate(seqs):
            x = [float(np.log1p(i))] + [0.25 * ((q * 7 + i) % 5)] * (N_DYN - 1) + list(phi)
            lines.append("\t".join([str(t), str(i), str(q), str(int(i == ch))] +
                                   ["%.17g" % v for v in x] + ["0", "%.17g" % (-x[0])]))
            rows += 1
    text = "\n".join(lines) + "\n"
    if footer:
        text += f"# guide end picks={len(picks)} rows={rows}\n"
    if cut:
        text = text[:-7]
    if extra:
        text = extra + text
    with gzip.open(path, "wt") as f:
        f.write(text)


def selftest():
    fails = []

    def check(cond, what):
        if not cond:
            fails.append(what)

    phi_a = [float(i % 3) for i in range(40)]
    phi_b = [float((i + 1) % 4) for i in range(40)]
    with tempfile.TemporaryDirectory() as d:
        h = os.path.join(d, "harvest")
        for sub in ("npz", "dump"):
            os.makedirs(os.path.join(h, sub))
        hdr = "problem\t" + "\t".join(phi_mod.PHI_COLUMNS + phi_mod.META_COLUMNS)
        with open(os.path.join(h, "phi.tsv"), "w") as f:
            f.write(hdr + "\n")
            for nm, ph, rating in (("GRP001-1", phi_a, 0.25), ("GRP001-1-10", phi_b, -1.0),
                                   ("LAT002-1", phi_a, 0.5)):
                meta = [1.0, rating, 1, 1, 0, 0, 1]
                f.write(nm + "\t" + "\t".join("%.9g" % np.float32(v) for v in ph + meta) + "\n")
        idx = []

        def row(problem, lane, split, mode, steps, every=1):
            c = [problem, lane, "1" if mode == "proof" else "0", "0", "0", "0", split,
                 "Unsatisfiable" if mode == "proof" else "GaveUp", "0", "1", "1", "1", "instr",
                 str(steps), "0", "0", "0", mode, "ok", str(every), "1"]
            idx.append("\t".join(c))
        # GRP001-1 on tgt-sel: proved; seq 1 and 3 are proof positives, 3
        # also a cross-lane key; seq 5 (selected, rule key 0x55) matches
        # encompass's proof only, seq 6 is never selected.
        st_npz(os.path.join(h, "npz", "GRP001-1__tgt-sel.npz"), "tgt-sel", True, 0xA, 4,
               [(1, 0, 1, True, False, 0x11), (2, 0, 2, False, False, 0x22),
                (3, 0, 3, True, False, 0x33), (5, 1, 4, False, True, 0x55),
                (6, 2, -1, False, False, 0)], {0x11, 0x33})
        st_dump(os.path.join(h, "dump", "GRP001-1__tgt-sel.dump.gz"), "tgt-sel", phi_a,
                [(1, [1, 2, 6], 0), (2, [2, 3, 6], 0), (3, [3, 5], 0), (4, [5, 6], 0)])
        row("GRP001-1", "tgt-sel", "train", "proof", 4)
        # The same problem on encompass, failed: hindsight positive seq 2;
        # cross-lane keys from tgt-sel (same sig): 0x11 (seq 1), not 0x55.
        st_npz(os.path.join(h, "npz", "GRP001-1__encompass.npz"), "encompass", False, 0xA, 3,
               [(1, 0, 1, False, False, 0x11), (2, 0, 2, False, True, 0x99),
                (4, 1, 3, False, False, 0x55), (7, 2, -1, False, False, 0)], set())
        st_dump(os.path.join(h, "dump", "GRP001-1__encompass.dump.gz"), "encompass", phi_a,
                [(1, [1, 2, 7], 0), (2, [2, 4], 0), (3, [4, 7], 0)], footer=False)
        row("GRP001-1", "encompass", "train", "hindsight", 3)
        # The -10 corpus problem (TRAIN-X), another sig: no cross-lane label.
        st_npz(os.path.join(h, "npz", "GRP001-1-10__tgt-sel.npz"), "tgt-sel", True, 0xB, 2,
               [(1, 0, 1, True, False, 0x11), (2, 0, 2, False, False, 0x33)], {0x11})
        st_dump(os.path.join(h, "dump", "GRP001-1-10__tgt-sel.dump.gz"), "tgt-sel", phi_b,
                [(1, [1, 2], 0), (2, [2], 0)])
        row("GRP001-1-10", "tgt-sel", "TRAIN-X", "proof", 2)
        # A run whose dump disagrees with its export (chosen seq 2 at t 1).
        st_npz(os.path.join(h, "npz", "LAT002-1__tgt-sel.npz"), "tgt-sel", True, 0xC, 2,
               [(1, 0, 1, True, False, 0), (2, 0, 2, False, False, 0)], set())
        st_dump(os.path.join(h, "dump", "LAT002-1__tgt-sel.dump.gz"), "tgt-sel", phi_a,
                [(1, [1, 2], 1), (2, [2], 0)])
        row("LAT002-1", "tgt-sel", "train", "proof", 2)
        # A run whose phi(p) columns are not its problem's.
        st_npz(os.path.join(h, "npz", "LAT002-1__encompass.npz"), "encompass", True, 0xC, 1,
               [(1, 0, 1, True, False, 0)], set())
        st_dump(os.path.join(h, "dump", "LAT002-1__encompass.dump.gz"), "encompass", phi_b,
                [(1, [1], 0)])
        row("LAT002-1", "encompass", "train", "proof", 1)
        with open(os.path.join(h, "index.tsv"), "w") as f:
            f.write("\n".join(idx) + "\n")
        kp = os.path.join(d, "keys.tsv")
        check(cmd_keys([kp, os.path.join(h, "index.tsv"), os.path.join(h, "npz")]) == 0, "keys rc")
        kt = load_keys([kp])
        check(sorted(ln for ln, _, _ in kt.get("GRP001-1", [])) == ["tgt-sel"],
              "keys: proof-mode runs only")
        check(cross_keys(kt, "GRP001-1", "encompass", 0xA) == {0x11, 0x33}, "keys: other lanes")
        check(cross_keys(kt, "GRP001-1", "encompass", 0xB) == set(), "keys: sig must agree")
        check(cross_keys(kt, "GRP001-1", "tgt-sel", 0xA) == set(), "keys: not the lane itself")
        o = os.path.join(d, "out")
        check(cmd_join([o, kp, h, "--jobs", "2"]) == 0, "join rc")
        g = np.load(os.path.join(o, "groups_tgt-sel.npz"))
        check(g["run_problem"].tolist() == ["GRP001-1", "GRP001-1-10"], "tgt-sel runs joined")
        check(g["run_stem"].tolist() == ["GRP001", "GRP001"], "stems share across pool and -10")
        check(abs(float(g["run_rating"][0]) - 0.25) < 1e-6 and float(g["run_rating"][1]) == -1.0,
              "ratings")
        check(np.array_equal(g["run_phi"][1], np.array(phi_b, np.float32)), "run phi")
        off = g["g_off"]
        check(off.tolist() == [0, 3, 6, 8, 10, 12, 13], "group offsets")
        check(g["seq"].tolist() == [1, 2, 6, 2, 3, 6, 3, 5, 5, 6, 1, 2, 2], "row seqs")
        # own: seq 1, 3 of GRP001-1 (proved); seq 5 HIND bit is not set
        # (the run proved); seq 1 of the -10 run.
        check(g["lab"].tolist() == [OWN, 0, 0, 0, OWN, 0, OWN, 0, 0, 0, OWN, 0, 0], "tgt-sel labels")
        check(g["sel"].tolist() == [1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1], "selected flags")
        # pending proof positives at t: seq 1 (push 0, sel 1), seq 3 (push
        # 0, sel 3): t=1 -> 2, t=2 -> 1, t=3 -> 1, t=4 -> 0.
        check(g["g_pend"].tolist()[:4] == [2, 1, 1, 0], "pending positives")
        e = np.load(os.path.join(o, "groups_encompass.npz"))
        check(e["run_problem"].tolist() == ["GRP001-1"], "encompass runs joined")
        check(not bool(e["run_complete"][0]), "cut-short session")
        # t=3 (the last selection) is dropped; t=1: seq 1 CROSS (0x11),
        # seq 2 HIND; t=2: seq 2 HIND, seq 4 (0x55, tgt-sel's non-proof
        # key) nothing.
        check(e["g_t"].tolist() == [1, 2], "cut-short drops the last selection")
        check(e["lab"].tolist() == [CROSS, HIND, 0, HIND, 0], "encompass labels")
        stats = open(os.path.join(o, "stats.tsv")).read().splitlines()
        hd = stats[0].split("\t")
        st = {ln.split("\t")[0]: dict(zip(hd, ln.split("\t"))) for ln in stats[1:]}
        check(st["tgt-sel"]["joined"] == "2" and st["tgt-sel"]["skipped"] == "1",
              "tgt-sel: the mismatched run is not joined")
        check(st["encompass"]["skipped"] == "1", "encompass: the phi mismatch is not joined")
        check(st["encompass"]["groups_cross_only"] == "0" and st["encompass"]["groups_hind"] == "2",
              "encompass source counts")
        check(st["encompass"]["runs_cross_only"] == "1", "a failed run gains cross-lane positives")
        summ = open(os.path.join(o, "summary.txt")).read()
        check("disagree with the export" in summ, "summary names the mismatch")
        check("phi(p) columns differ" in summ, "summary names the phi mismatch")
        # The split rule: a DEV row stops the join.
        with open(os.path.join(h, "index.tsv"), "a") as f:
            c = idx[0].split("\t")
            c[0], c[6] = "REL001-1", "heldout"
            f.write("\t".join(c) + "\n")
        try:
            cmd_join([os.path.join(d, "out2"), kp, h, "--jobs", "1"])
            check(False, "a heldout row must stop the join")
        except SystemExit as ex:
            check("only TRAIN and TRAIN-X" in str(ex), "the split refusal names the rule")
        # The stems of matrix_tables.py (A24 item 8).
        check(run_stem("PUZ131+1-10", "TRAIN-X") == "PUZ131" and run_stem("GRP001-1", "train") == "GRP001",
              "stem rules")
    for f in fails:
        print(f"selftest FAILED: {f}")
    if not fails:
        print("label_join selftest ok")
    return 1 if fails else 0


def main():
    if len(sys.argv) < 2:
        die("usage: label_join.py keys|join|selftest ...")
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == "keys":
        return cmd_keys(args)
    if cmd == "join":
        return cmd_join(args)
    if cmd == "selftest":
        return selftest()
    die(f"unknown command {cmd}")


if __name__ == "__main__":
    sys.exit(main())
