#!/bin/bash
# The synthetic -10 corpus (docs/ATP/enigma_portfolio.md, M2 deliverable
# 5, decision D1): every CNF and FOF problem of a TPTP tree whose status
# is Unsatisfiable or Theorem, encoded by FindProof --emit-ifeq10 into
# TPTP's -10 unit-equality scheme, split 80/20 into TRAIN-X and DEV-X by
# sha256 of the name stem.  Pods only.
#
# Usage: TRAIN_NAMES=FILE make_ifeq10_corpus.sh FINDPROOF TPTP_ROOT OUT_DIR J13_NAMES EXCLUDE_NAMES ...
#
# At least one EXCLUDE_NAMES file and TRAIN_NAMES are required, so that
# no DEV-H or DEV-T stem lands in either split and no pool train stem in
# DEV-X; ALLOW_LEAKY=1 runs without them, records allow_leaky=1 in the
# inputs and ends status=complete-unguarded, a corpus M3 must not train
# on.
#
# The stem of a name is its DOMNNN prefix (^[A-Z]{3}[0-9]{3}), so a FOF
# source shares the stem of its CNF siblings; a stem goes to TRAIN-X when
# int(sha256(stem)[:8], 16) % 100 < 80, matrix_tables.py's stem_hash.  A
# tool that needs the stem or the split of an emitted problem reads them
# from manifest.tsv or pool.tsv; name.split("-")[0] of PUZ131+1-10 is not
# its stem.  A candidate is skipped, with the reason in manifest.tsv, when
#   j13        its stem is the stem of a name in J13_NAMES (the CASC-J13 UEQ
#              list): this drops the -1/-2/-3 sources of J13's -10 theorems
#              with every other variant;
#   excluded   its stem is the stem of a name in an EXCLUDE_NAMES file (the
#              pool's held-out names and DEV-T, so that no DEV-H or DEV-T
#              problem has an encoded copy in TRAIN-X or DEV-X);
#   ueq        it is unit equality already (SPC ends in UEQ);
#   tptp10     TPTP ships a -10 version of its stem, which the pool holds;
#   pooltrain  it would go to DEV-X and its stem is the stem of a name in
#              the file TRAIN_NAMES (the pool's train names), so that DEV-X
#              is held out from everything a model trains on;
#   noheader   no formula line starts in column 0, so the header was not
#              read (the file is accounted for, not encoded);
# and it is not emitted when the encoding adds nothing, duplicates J13 or
# fails:
#   unit       the output's Encoding line counts no dispatcher, tuple or
#              predicate: unit equality after clausification (the FOF
#              version of a UEQ problem), the ueq rule applied late;
#   j13dup     the output equals a J13 problem file modulo variable and
#              symbol renaming, clause order and names
#              (ifeq10_keys.py; the J13 files are keyed once per launch,
#              the statistics plan section 1.4 allows on them), whatever
#              its stem;
#   keyerror   ifeq10_keys.py could not key the output;
#   nonhorn (exit 3), sat1 (exit 4, a one-element model), memory, write or
#   input (exit 2), timeout (exit 124, or 137 at the time limit), killed
#   (137 before it: the OOM killer), crash, truncated (exit 0 without the
#   Encoding line or the closing line).
#
# Output in OUT_DIR:
#   TRAIN-X/Problems/DOM/NAME.p and DEV-X/Problems/DOM/NAME.p, where NAME
#     is the source name plus "-10" (LCL096-1 -> LCL096-1-10), so that no
#     name collides with a TPTP problem;
#   TRAIN-X/pool.tsv, DEV-X/pool.tsv: name, relpath, status, spc, split
#     (np0_pool.py's columns, split train or heldout), relpath relative to
#     that split's root, so the np0 tools take TPTP=OUT_DIR/TRAIN-X.  The
#     DEV-X label is heldout, as np0_harvest.sh expects of an evaluation
#     set, so DEV-X/pool.tsv must never be concatenated with the pool's
#     pool.tsv, whose heldout rows are DEV-H;
#   manifest.tsv: one row per candidate (header line first): source, name,
#     stem, status, spc, outcome, split, clauses, dispatchers, definitions
#     (sP<n> predicates the clausifier introduced; no TPTP -10 file has
#     one, so these encodings have no reference), seconds, source_rating
#     (the first number of the source's Rating line), source_includes;
#   summary.txt: the inputs, counts per outcome and split, ALERT lines;
#   .started (this launch) and .done: status=complete; complete-alerts
#     (sat1 on any candidate, nonhorn on an _HRN source, or a truncated
#     output: encoder bugs by construction); complete-unguarded (ALLOW_LEAKY
#     =1, alerts=N appended); incomplete pending=N (the deadline, or write
#     errors: rerun to resume); error (the corpus check failed: a J13 or
#     excluded stem emitted, an emitted problem without a key or with a
#     J13 key, a stem in both splits, a split that is not the stem's hash,
#     pool.tsv out of step with the files; exit 1).
#
# OUT_DIR/.work/inputs fingerprints the run: the sha256 of FINDPROOF, of
# this script, of ifeq10_keys.py and ifeq10_diff.py and of every name
# list, the TPTP release and problem list, TIMEOUT_S, MEM_MB and
# ALLOW_LEAKY.  A rerun resumes only when they match and exits 2
# otherwise, so one corpus never mixes two encoder builds or two sets of
# exclusions.  FINDPROOF must have passed ALL=1 tests/atp_ifeq10_tptp.sh,
# whose pass file FINDPROOF.ifeq10-pass names its sha256 (IFEQ10_PASS
# names another file; IFEQ10_UNCHECKED=1 runs without one and says so in
# summary.txt).  The TPTP tree must be v9.3.1, the release after which
# T-NEW (plan section 1.4) counts; ALLOW_TPTP=1 accepts another.
#
# Env: JOBS (parallel encoders, default the core count), TIMEOUT_S (per
# problem, 120), MEM_MB (virtual memory per encoder, 3072), DEADLINE_S
# (no launch after this many seconds, 14400), TRAIN_NAMES, ALLOW_LEAKY.  The run
# executes a private copy of this script, ifeq10_keys.py and
# ifeq10_diff.py under OUT_DIR, so editing them mid-run cannot break it,
# and holds OUT_DIR/.work/.lock (flock), so a second launch on the same
# OUT_DIR exits 2.
set -u

usage() {
  awk 'NR == 1 { next } /^#/ { sub(/^# ?/, ""); print; next } { exit }' "$0"
  exit 2
}

die() {
  echo "make_ifeq10_corpus: $*" >&2
  exit 2
}

if [ "${IFEQ10_SNAPSHOT:-}" != 1 ]; then
  [ $# -ge 4 ] || usage
  mkdir -p "$3" || exit 2
  snap=$(mktemp -d "$3/.make_ifeq10_corpus.XXXXXX") || exit 2
  here=$(dirname "$0")
  if ! cp "$0" "$snap/make_ifeq10_corpus.sh" || ! cp "$here/ifeq10_keys.py" "$here/ifeq10_diff.py" "$snap/"; then
    rm -rf "$snap"
    exit 2
  fi
  exec env IFEQ10_SNAPSHOT=1 IFEQ10_SELF="$snap" bash "$snap/make_ifeq10_corpus.sh" "$@"
fi
case ${IFEQ10_SELF:-} in
  */.make_ifeq10_corpus.*) trap 'rm -rf "$IFEQ10_SELF"' EXIT ;;
  *) die "run make_ifeq10_corpus.sh itself, not its snapshot" ;;
esac

FP=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
TPTP_ROOT=$(cd "$2" && pwd)
OUT=$(cd "$3" && pwd)
J13=$4
shift 4
EXTRA=("$@")

JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2> /dev/null || echo 8)}
TIMEOUT_S=${TIMEOUT_S:-120}
MEM_MB=${MEM_MB:-3072}
DEADLINE_S=${DEADLINE_S:-14400}
TRAIN_NAMES=${TRAIN_NAMES:-}
ALLOW_LEAKY=${ALLOW_LEAKY:-0}
IFEQ10_PASS=${IFEQ10_PASS:-$FP.ifeq10-pass}
WORK=$OUT/.work

[ -x "$FP" ] || die "$FP is not an executable"
"$FP" --help 2> /dev/null | grep -q -- '--emit-ifeq10' || die "$FP has no --emit-ifeq10"
[ -d "$TPTP_ROOT/Problems" ] || die "$2 has no Problems/"
[ -s "$J13" ] || die "the J13 name list $J13 is missing or empty"
if [ "$ALLOW_LEAKY" != 1 ]; then
  [ ${#EXTRA[@]} -gt 0 ] || die "no EXCLUDE_NAMES (DEV-H and DEV-T names): ALLOW_LEAKY=1 runs without and marks the corpus unguarded"
  [ -n "$TRAIN_NAMES" ] || die "no TRAIN_NAMES (the pool's train names): ALLOW_LEAKY=1 runs without and marks the corpus unguarded"
fi
for f in "${EXTRA[@]+"${EXTRA[@]}"}"; do
  [ -s "$f" ] || die "the exclusion list $f is missing or empty"
done
if [ -n "$TRAIN_NAMES" ]; then
  [ -s "$TRAIN_NAMES" ] || die "TRAIN_NAMES=$TRAIN_NAMES is missing or empty"
fi
for c in timeout flock python3; do
  command -v "$c" > /dev/null 2>&1 || die "no $c(1)"
done
if command -v sha256sum > /dev/null 2>&1; then
  SHA=sha256sum
else
  SHA="shasum -a 256"
fi

sha_of() {
  $SHA "$1" | cut -d' ' -f1
}

KEYS=$IFEQ10_SELF/ifeq10_keys.py

mkdir -p "$WORK/res" "$WORK/err" "$WORK/tmp" "$OUT/TRAIN-X/Problems" "$OUT/DEV-X/Problems" || exit 2
exec 9> "$WORK/.lock" || exit 2
flock -n 9 || die "another run holds $WORK/.lock"

TPTP_RELEASE=$(basename "$TPTP_ROOT" | grep -oE 'v[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)
if [ -z "$TPTP_RELEASE" ] && [ -f "$TPTP_ROOT/ReadMe" ]; then
  TPTP_RELEASE=$(grep -oE 'v[0-9]+\.[0-9]+\.[0-9]+' "$TPTP_ROOT/ReadMe" | head -n 1)
fi
if [ "$TPTP_RELEASE" != v9.3.1 ] && [ "${ALLOW_TPTP:-0}" != 1 ]; then
  die "$TPTP_ROOT is TPTP ${TPTP_RELEASE:-of an unknown release}, not v9.3.1 (ALLOW_TPTP=1 accepts it)"
fi
(cd "$TPTP_ROOT" && find Problems -type f -name '*.p') | LC_ALL=C sort > "$WORK/all_problems"

FP_SHA=$(sha_of "$FP")
if [ "${IFEQ10_UNCHECKED:-0}" = 1 ]; then
  PASS="unchecked (IFEQ10_UNCHECKED=1): no ALL=1 tests/atp_ifeq10_tptp.sh pass on record"
elif [ ! -s "$IFEQ10_PASS" ]; then
  die "no pass file $IFEQ10_PASS: run ALL=1 tests/atp_ifeq10_tptp.sh on this binary first"
elif ! grep -qx "findproof_sha256=$FP_SHA" "$IFEQ10_PASS"; then
  die "$IFEQ10_PASS is the pass file of another binary (this one is $FP_SHA)"
else
  PASS="$IFEQ10_PASS: $(grep -E '^(cases|survey|verdict)=' "$IFEQ10_PASS" | tr '\n' ' ')"
fi

{
  echo "findproof_sha256=$FP_SHA"
  echo "driver_sha256=$(sha_of "$0")"
  echo "keys_sha256=$(sha_of "$KEYS") diff_sha256=$(sha_of "$(dirname "$KEYS")/ifeq10_diff.py")"
  echo "tptp_release=${TPTP_RELEASE:-unknown}"
  echo "tptp_problems_sha256=$(sha_of "$WORK/all_problems") ($(wc -l < "$WORK/all_problems" | tr -d ' ') files)"
  echo "j13_names_sha256=$(sha_of "$J13")"
  for f in "${EXTRA[@]+"${EXTRA[@]}"}"; do
    echo "exclude_names_sha256=$(sha_of "$f")"
  done
  if [ -n "$TRAIN_NAMES" ]; then
    echo "train_names_sha256=$(sha_of "$TRAIN_NAMES")"
  fi
  echo "timeout_s=$TIMEOUT_S mem_mb=$MEM_MB"
  echo "allow_leaky=$ALLOW_LEAKY"
} > "$WORK/inputs.now"
if [ -f "$WORK/inputs" ]; then
  if ! cmp -s "$WORK/inputs" "$WORK/inputs.now"; then
    diff "$WORK/inputs" "$WORK/inputs.now" >&2
    die "inputs changed since the first run in $OUT: use a fresh OUT_DIR"
  fi
  rm -f "$WORK/inputs.now"
elif [ -n "$(find "$WORK/res" "$OUT/TRAIN-X/Problems" "$OUT/DEV-X/Problems" -type f -print 2> /dev/null | head -n 1)" ]; then
  die "$OUT holds results without an inputs record: use a fresh OUT_DIR"
else
  mv "$WORK/inputs.now" "$WORK/inputs"
fi

T0=$(date +%s)
DEADLINE_AT=$((T0 + DEADLINE_S))
rm -f "$OUT/.done"
{
  echo "started=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "host=$(hostname)"
  echo "findproof=$FP"
  echo "pass=$PASS"
  echo "tptp_root=$TPTP_ROOT"
  echo "j13_names=$J13 ($(wc -l < "$J13" | tr -d ' ') lines)"
  for f in "${EXTRA[@]+"${EXTRA[@]}"}"; do
    echo "exclude_names=$f ($(wc -l < "$f" | tr -d ' ') lines)"
  done
  if [ -n "$TRAIN_NAMES" ]; then
    echo "train_names=$TRAIN_NAMES ($(wc -l < "$TRAIN_NAMES" | tr -d ' ') lines)"
  fi
  echo "jobs=$JOBS deadline_s=$DEADLINE_S"
} > "$OUT/.started"
cat "$OUT/.started" >> "$WORK/launches"

stems_of() {
  sed -E 's/^[[:space:]]*([A-Z]{3}[0-9]{3}).*/\1/' "$@" | grep -E '^[A-Z]{3}[0-9]{3}$' | LC_ALL=C sort -u
}
stems_of "$J13" > "$WORK/j13_stems"
grep -E -- '-10(\.[0-9]+)?[[:space:]]*$' "$J13" | stems_of > "$WORK/j13_10_stems"
if [ ${#EXTRA[@]} -gt 0 ]; then
  stems_of "${EXTRA[@]}" > "$WORK/excl_stems"
else
  : > "$WORK/excl_stems"
fi
if [ -n "$TRAIN_NAMES" ]; then
  stems_of "$TRAIN_NAMES" > "$WORK/train_stems"
else
  : > "$WORK/train_stems"
fi

# The keys of the J13 problem files, against which every emitted problem
# is checked; a J13 name without a file, or a file without a key, stops
# the run, since its duplicates could not be found.
mkdir -p "$WORK/keys"
sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//; s/\.p$//' "$J13" | grep -v '^$' | LC_ALL=C sort -u > "$WORK/j13_list"
awk -v r="$TPTP_ROOT/" 'FILENAME == ARGV[1] { want[$0] = 1; next }
  { n = $0; sub(/.*\//, "", n); sub(/\.p$/, "", n); if (n in want) { print r $0; seen[n] = 1 } }
  END { for (n in want) if (!(n in seen)) print "MISSING " n > "/dev/stderr" }' \
  "$WORK/j13_list" "$WORK/all_problems" > "$WORK/j13_files" 2> "$WORK/j13_missing"
[ -s "$WORK/j13_missing" ] && die "J13 names without a problem file in $TPTP_ROOT: $(head -n 5 "$WORK/j13_missing" | tr '\n' ' ')"
(
  cd "$WORK/tmp" || exit 2
  ulimit -v $((MEM_MB * 1024))
  exec timeout -k 5 $((TIMEOUT_S * 10)) python3 "$KEYS" "$TPTP_ROOT" "$WORK/j13_files"
) < /dev/null > "$WORK/j13_keys.tsv" 2> "$WORK/j13_keys.err" || die "ifeq10_keys.py failed on the J13 files: $(head -n 1 "$WORK/j13_keys.err")"
[ "$(wc -l < "$WORK/j13_keys.tsv" | tr -d ' ')" = "$(wc -l < "$WORK/j13_files" | tr -d ' ')" ] \
  || die "ifeq10_keys.py keyed $(wc -l < "$WORK/j13_keys.tsv" | tr -d ' ') of $(wc -l < "$WORK/j13_files" | tr -d ' ') J13 files"
if awk -F'\t' '$2 == "ERROR"' "$WORK/j13_keys.tsv" | grep -q .; then
  die "J13 files without a key: $(awk -F'\t' '$2 == "ERROR" { print $1 ": " $3 }' "$WORK/j13_keys.tsv" | head -n 3 | tr '\n' ' ')"
fi
cut -f2 "$WORK/j13_keys.tsv" | LC_ALL=C sort -u > "$WORK/j13_keyset"

# The candidates: CNF (-) and FOF (+) problem files, with their header's
# Status, SPC, first Rating number and the include()s before the first
# formula.
grep -E '^Problems/[A-Z]{3}/[A-Z]{3}[0-9]{3}[-+][0-9][^/]*\.p$' "$WORK/all_problems" > "$WORK/files"
grep -E '^Problems/[A-Z]{3}/[A-Z]{3}[0-9]{3}-10(\.[0-9]+)?\.p$' "$WORK/all_problems" \
  | sed -E 's|.*/([A-Z]{3}[0-9]{3})-10[^/]*$|\1|' | LC_ALL=C sort -u > "$WORK/tptp10_stems"
awk -v r="$TPTP_ROOT/" '{ print r $0 }' "$WORK/files" | tr '\n' '\0' | xargs -0 -r awk -v q="'" '
  FNR == 1 { st = ""; spc = ""; rt = ""; inc = "" }
  /^%[ \t]*Status[ \t]*:/ && st == "" { s = $0; sub(/^%[ \t]*Status[ \t]*:[ \t]*/, "", s); split(s, a, /[ \t]/); st = a[1] }
  /^%[ \t]*SPC[ \t]*:/ && spc == "" { s = $0; sub(/^%[ \t]*SPC[ \t]*:[ \t]*/, "", s); split(s, a, /[ \t]/); spc = a[1] }
  /^%[ \t]*Rating[ \t]*:/ && rt == "" { s = $0; sub(/^%[ \t]*Rating[ \t]*:[ \t]*/, "", s); split(s, a, /[ \t,]/); rt = a[1] }
  /^include\(/ { s = $0; sub("^include\\([ \t]*" q, "", s); sub(q ".*", "", s); inc = (inc == "") ? s : inc "," s; next }
  /^[a-z]/ {
    print FILENAME "\t" st "\t" (spc == "" ? "-" : spc) "\t" (rt == "" ? "-" : rt) "\t" (inc == "" ? "-" : inc)
    nextfile
  }
' > "$WORK/headers.tsv"

# A file whose header awk printed no row is accounted for as noheader.
awk -F'\t' -v root="$TPTP_ROOT/" 'FILENAME == ARGV[1] { seen[substr($1, length(root) + 1)] = 1; next }
  !($0 in seen) {
    n = $0; sub(/.*\//, "", n); sub(/\.p$/, "", n)
    print $0 "\t-\t" substr(n, 1, 6) "\t-\t-\tnoheader\t-\t-\t-\t-\t-\t-\t-"
  }' "$WORK/headers.tsv" "$WORK/files" > "$WORK/noheader.tsv"

awk -F'\t' -v root="$TPTP_ROOT/" '
  $2 == "Unsatisfiable" || $2 == "Theorem" {
    n = $1; sub(/.*\//, "", n); sub(/\.p$/, "", n)
    print substr($1, length(root) + 1) "\t" n "\t" substr(n, 1, 6) "\t" substr(n, 1, 3) "\t" $2 "\t" $3 "\t" $4 "\t" $5
  }' "$WORK/headers.tsv" > "$WORK/cands0.tsv"

cut -f3 "$WORK/cands0.tsv" | LC_ALL=C sort -u | while IFS= read -r st; do
  h=$(printf '%s' "$st" | $SHA | cut -c1-8)
  if [ $((16#$h % 100)) -lt 80 ]; then
    printf '%s\tTRAIN-X\n' "$st"
  else
    printf '%s\tDEV-X\n' "$st"
  fi
done > "$WORK/stem_split"

# candidates.tsv: source, name, stem, dom, status, spc, split, rating,
# includes, outcome ("encode" for a job).
awk -F'\t' -v jf="$WORK/j13_stems" -v ef="$WORK/excl_stems" -v tf="$WORK/tptp10_stems" \
    -v pf="$WORK/train_stems" -v sf="$WORK/stem_split" '
  BEGIN {
    while ((getline s < jf) > 0) J[s] = 1
    while ((getline s < ef) > 0) E[s] = 1
    while ((getline s < tf) > 0) T[s] = 1
    while ((getline s < pf) > 0) P[s] = 1
    while ((getline s < sf) > 0) { split(s, a, "\t"); S[a[1]] = a[2] }
  }
  {
    stem = $3; sp = S[stem]
    out = "encode"
    if (stem in J) out = "j13"
    else if (stem in E) out = "excluded"
    else if ($6 ~ /UEQ$/) out = "ueq"
    else if (stem in T) out = "tptp10"
    else if (sp == "DEV-X" && (stem in P)) out = "pooltrain"
    print $1 "\t" $2 "\t" stem "\t" $4 "\t" $5 "\t" $6 "\t" sp "\t" $7 "\t" $8 "\t" out
  }' "$WORK/cands0.tsv" > "$WORK/candidates.tsv"

{
  awk -F'\t' '$10 != "encode" { print $1 "\t-\t" $3 "\t" $5 "\t" $6 "\t" $10 "\t-\t-\t-\t-\t-\t" $8 "\t" $9 }' \
    "$WORK/candidates.tsv"
  cat "$WORK/noheader.tsv"
} > "$WORK/skipped.tsv"
awk -F'\t' '$10 == "encode" { print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5 "\t" $6 "\t" $7 "\t" $8 "\t" $9 }' \
  "$WORK/candidates.tsv" > "$WORK/jobs.tsv"

# One encoding: OUT/<split>/Problems/DOM/NAME-10.p and a manifest row in
# .work/res/NAME.tsv.  A row is final (a rerun skips the job) unless its
# outcome is write, a failure of the pod's disk rather than of the
# encoder.
encode_one() {
  local rel name stem dom st spc split rating incs
  IFS=$'\t' read -r rel name stem dom st spc split rating incs <<< "$1"
  [ -n "$rel" ] || return 0
  local res=$WORK/res/$name.tsv
  if [ -s "$res" ] && [ "$(cut -f6 "$res")" != write ]; then
    return 0
  fi
  [ "$(date +%s)" -lt "$DEADLINE_AT" ] || return 0
  local tmp=$WORK/tmp/$name.p err=$WORK/err/$name.err
  local t0 t1 rc outcome
  t0=$(date +%s.%N)
  (
    cd "$WORK/tmp" || exit 2
    ulimit -v $((MEM_MB * 1024))
    exec env TPTP="$TPTP_ROOT" timeout -k 5 "$TIMEOUT_S" "$FP" --emit-ifeq10 "$TPTP_ROOT/$rel"
  ) < /dev/null > "$tmp" 2> "$err"
  rc=$?
  t1=$(date +%s.%N)
  case $rc in
    0) outcome=emitted ;;
    2) outcome=input ;;
    3) outcome=nonhorn ;;
    4) outcome=sat1 ;;
    124) outcome=timeout ;;
    137) outcome=killed ;;
    *) outcome=crash ;;
  esac
  if [ "$outcome" = input ]; then
    if grep -q 'out of memory' "$err"; then
      outcome=memory
    elif grep -q 'write error' "$err"; then
      outcome=write
    fi
  fi
  local out_name=- nc=- nd=- ndef=- ntup npred enc secs
  secs=$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f", b - a }')
  # 137 is any SIGKILL: timeout's -k after the limit, or the OOM killer.
  if [ "$outcome" = killed ] && awk -v s="$secs" -v t="$TIMEOUT_S" 'BEGIN { exit !(s >= t) }'; then
    outcome=timeout
  fi
  if [ "$outcome" = emitted ]; then
    enc=$(sed -n 's/^% Encoding : dispatchers \([0-9]*\), tuples \([0-9]*\), predicates \([0-9]*\), definitions \([0-9]*\)$/\1 \2 \3 \4/p' "$tmp")
    if [ -z "$enc" ] || [ "$(tail -n 1 "$tmp" | cut -c1-10)" != '%---------' ]; then
      outcome=truncated
    else
      read -r nd ntup npred ndef <<< "$enc"
      if [ "$nd" = 0 ] && [ "$ntup" = 0 ] && [ "$npred" = 0 ]; then
        outcome=unit
      fi
    fi
  fi
  local key=
  if [ "$outcome" = emitted ]; then
    printf '%s\n' "$tmp" > "$WORK/tmp/$name.list"
    key=$(
      cd "$WORK/tmp" || exit 2
      ulimit -v $((MEM_MB * 1024))
      timeout -k 5 "$TIMEOUT_S" python3 "$KEYS" "$TPTP_ROOT" "$WORK/tmp/$name.list" < /dev/null 2>> "$err" | cut -f2
    )
    rm -f "$WORK/tmp/$name.list"
    if [ -z "$key" ] || [ "$key" = ERROR ]; then
      outcome=keyerror
    elif grep -qxF "$key" "$WORK/j13_keyset"; then
      outcome=j13dup
    fi
  fi
  if [ "$outcome" = emitted ]; then
    out_name=$name-10
    printf '%s\n' "$key" > "$WORK/keys/$out_name.key"
    nc=$(grep -c '^cnf(' "$tmp")
    mkdir -p "$OUT/$split/Problems/$dom"
    mv "$tmp" "$OUT/$split/Problems/$dom/$out_name.p"
  else
    rm -f "$tmp"
    split=-
    nd=-
    ndef=-
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$rel" "$out_name" "$stem" "$st" \
    "$spc" "$outcome" "$split" "$nc" "$nd" "$ndef" "$secs" "$rating" "$incs" > "$res.part"
  mv "$res.part" "$res"
}
export -f encode_one
export WORK OUT FP TPTP_ROOT TIMEOUT_S MEM_MB DEADLINE_AT KEYS

if [ -s "$WORK/jobs.tsv" ]; then
  tr '\n' '\0' < "$WORK/jobs.tsv" | xargs -0 -r -P "$JOBS" -n 1 bash -c 'encode_one "$1"' _
fi

# The manifest and the per-split pool tables, from the skipped candidates
# and the result rows of this run's jobs only.
{
  printf 'source\tname\tstem\tstatus\tspc\toutcome\tsplit\tclauses\tdispatchers\tdefinitions\tseconds\tsource_rating\tsource_includes\n'
  {
    cat "$WORK/skipped.tsv"
    cut -f2 "$WORK/jobs.tsv" | while IFS= read -r name; do
      if [ -s "$WORK/res/$name.tsv" ]; then
        cat "$WORK/res/$name.tsv"
      fi
    done
  } | LC_ALL=C sort
} > "$OUT/manifest.tsv"
for split in TRAIN-X DEV-X; do
  label=train
  [ "$split" = DEV-X ] && label=heldout
  awk -F'\t' -v s="$split" -v l="$label" 'NR > 1 && $6 == "emitted" && $7 == s {
      print $2 "\tProblems/" substr($2, 1, 3) "/" $2 ".p\tUnsatisfiable\tCNF_UNS_RFO_PEQ_UEQ\t" l
    }' "$OUT/manifest.tsv" > "$OUT/$split/pool.tsv"
done

# The data rule, checked on what was written.
check_corpus() {
  awk -F'\t' -v jf="$WORK/j13_stems" -v ef="$WORK/excl_stems" -v pf="$WORK/train_stems" '
    BEGIN {
      while ((getline s < jf) > 0) J[s] = 1
      while ((getline s < ef) > 0) E[s] = 1
      while ((getline s < pf) > 0) P[s] = 1
    }
    NR == 1 { next }
    $6 != "emitted" {
      if ($7 != "-" || $2 != "-") print "the " $6 " row of " $1 " names an output"
      next
    }
    {
      st = $3
      if (st != substr($2, 1, 6)) print "the stem " st " is not the prefix of " $2
      if (st in J) print $2 " has the J13 stem " st
      if (st in E) print $2 " has the excluded stem " st
      if ($7 == "DEV-X" && (st in P)) print "the DEV-X problem " $2 " has the pool train stem " st
      if ($7 != "TRAIN-X" && $7 != "DEV-X") print $2 " has the split " $7
      if ((st in seen) && seen[st] != $7) print "the stem " st " is in both splits"
      seen[st] = $7
    }' "$OUT/manifest.tsv"
  awk -F'\t' 'NR > 1 && $6 == "emitted" { print $2 }' "$OUT/manifest.tsv" | while IFS= read -r n; do
    if [ ! -s "$WORK/keys/$n.key" ]; then
      echo "the emitted $n has no duplicate key"
    elif grep -qxF "$(cat "$WORK/keys/$n.key")" "$WORK/j13_keyset"; then
      echo "the emitted $n has the key of a J13 problem"
    fi
  done
  awk -F'\t' 'NR > 1 && $6 == "emitted" { print $3 "\t" $7 }' "$OUT/manifest.tsv" | LC_ALL=C sort -u \
    | while IFS=$'\t' read -r st sp; do
        h=$(printf '%s' "$st" | $SHA | cut -c1-8)
        want=DEV-X
        if [ $((16#$h % 100)) -lt 80 ]; then
          want=TRAIN-X
        fi
        [ "$sp" = "$want" ] || echo "the stem $st is in $sp, its hash puts it in $want"
      done
  for split in TRAIN-X DEV-X; do
    awk -F'\t' -v s="$split" 'NR > 1 && $6 == "emitted" && $7 == s { print $2 }' "$OUT/manifest.tsv" \
      | LC_ALL=C sort > "$WORK/check_emitted"
    cut -f1 "$OUT/$split/pool.tsv" | LC_ALL=C sort > "$WORK/check_pool"
    (cd "$OUT/$split" && find Problems -type f -name '*.p') | sed -E 's|.*/||; s|\.p$||' \
      | LC_ALL=C sort > "$WORK/check_files"
    cmp -s "$WORK/check_emitted" "$WORK/check_pool" \
      || echo "$split/pool.tsv does not list exactly the emitted $split problems"
    cmp -s "$WORK/check_emitted" "$WORK/check_files" \
      || echo "$split/Problems does not hold exactly the emitted $split problems"
    while IFS=$'\t' read -r _ p _; do
      [ -s "$OUT/$split/$p" ] || echo "$split/pool.tsv names the missing file $p"
    done < "$OUT/$split/pool.tsv"
  done
}
check_corpus > "$WORK/check_errors"

left=$(cut -f2 "$WORK/jobs.tsv" | while IFS= read -r name; do
  if [ ! -s "$WORK/res/$name.tsv" ] || [ "$(cut -f6 "$WORK/res/$name.tsv")" = write ]; then
    echo "$name"
  fi
done | wc -l | tr -d ' ')
awk -F'\t' 'NR > 1 && ($6 == "sat1" || $6 == "truncated" || ($5 ~ /_HRN$/ && $6 == "nonhorn")) {
    print "ALERT " $6 ": " $1 " (" $5 ")"
  }' "$OUT/manifest.tsv" > "$WORK/alerts"
{
  echo "inputs:"
  sed 's/^/  /' "$WORK/inputs"
  cat "$OUT/.started"
  if [ ${#EXTRA[@]} -eq 0 ]; then
    echo "UNGUARDED: no EXCLUDE_NAMES: DEV-H and DEV-T stems are not excluded"
  fi
  if [ -z "$TRAIN_NAMES" ]; then
    echo "UNGUARDED: no TRAIN_NAMES: DEV-X may share stems with the pool's train split"
  fi
  echo "j13_files=$(wc -l < "$WORK/j13_files" | tr -d ' ') keyed ($(wc -l < "$WORK/j13_keyset" | tr -d ' ') distinct keys)"
  echo "candidates=$(wc -l < "$WORK/candidates.tsv" | tr -d ' ') (CNF and FOF, Unsatisfiable or Theorem)"
  echo "j13_stems=$(wc -l < "$WORK/j13_stems" | tr -d ' ')"
  echo "j13_10_variants_skipped=$(awk -F'\t' 'FILENAME == ARGV[1] { s[$1] = 1; next }
      $10 == "j13" && ($3 in s) && $2 !~ /-10(\.[0-9]+)?$/' "$WORK/j13_10_stems" "$WORK/candidates.tsv" \
      | wc -l | tr -d ' ')" \
    "(sources sharing a stem with a J13 -10 problem, the -10 files not counted)"
  echo "outcomes:"
  awk -F'\t' 'NR > 1 { n[$6]++ } END { for (k in n) printf "  %-9s %d\n", k, n[k] }' "$OUT/manifest.tsv" \
    | LC_ALL=C sort
  echo "splits:"
  awk -F'\t' 'NR > 1 && $6 == "emitted" { n[$7]++; d[$7] += $9; s[$7] += ($10 > 0) } END {
      for (k in n) printf "  %-8s %d problems, %d dispatcher axioms, %d with sP definitions\n", k, n[k], d[k], s[k]
    }' "$OUT/manifest.tsv" | LC_ALL=C sort
  echo "with_definitions=$(awk -F'\t' 'NR > 1 && $6 == "emitted" && $10 > 0' "$OUT/manifest.tsv" | wc -l | tr -d ' ')" \
    "(emitted problems whose clausification introduced sP<n> definitions; no TPTP -10 file is a reference for them)"
  echo "pending=$left"
  head -n 100 "$WORK/alerts"
  if [ -s "$WORK/check_errors" ]; then
    echo "corpus check: FAILED"
    sed 's/^/  ERROR /' "$WORK/check_errors" | head -n 100
  else
    echo "corpus check: ok"
  fi
  echo "finished=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$OUT/summary.txt"
cat "$OUT/summary.txt"
if [ -s "$WORK/check_errors" ]; then
  echo "status=error" > "$OUT/.done"
  exit 1
fi
if [ "$left" -gt 0 ]; then
  echo "status=incomplete pending=$left" > "$OUT/.done"
elif [ "$ALLOW_LEAKY" = 1 ]; then
  echo "status=complete-unguarded alerts=$(wc -l < "$WORK/alerts" | tr -d ' ')" > "$OUT/.done"
elif [ -s "$WORK/alerts" ]; then
  echo "status=complete-alerts alerts=$(wc -l < "$WORK/alerts" | tr -d ' ')" > "$OUT/.done"
else
  echo "status=complete" > "$OUT/.done"
fi
