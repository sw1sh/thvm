#!/bin/bash
# The synthetic -10 corpus (docs/plans/enigma_portfolio.md, M2 deliverable
# 5, decision D1): every CNF and FOF problem of a TPTP tree whose status
# is Unsatisfiable or Theorem, encoded by FindProof --emit-ifeq10 into
# TPTP's -10 unit-equality scheme, split 80/20 into TRAIN-X and DEV-X by
# sha256 of the name stem.  Pods only.
#
# Usage: make_ifeq10_corpus.sh FINDPROOF TPTP_ROOT OUT_DIR J13_NAMES [EXCLUDE_NAMES ...]
#
# The stem of a name is its DOMNNN prefix (the name before the -N or +N
# suffix), so a FOF source shares the stem of its CNF siblings.  A problem
# is skipped, with the reason in OUT_DIR/manifest.tsv, when
#   j13       its stem is the stem of a name in J13_NAMES (the CASC-J13 UEQ
#             list): this drops the -1/-2/-3 sources of J13's -10 theorems
#             with every other variant;
#   excluded  its stem is the stem of a name in an EXCLUDE_NAMES file (for
#             example the pool's held-out names);
#   ueq       it is unit equality already (SPC ends in UEQ): the pool's
#             domain, which the encoding would only copy;
#   tptp10    TPTP ships a -10 version of its stem, which the pool holds;
# and it is not emitted when the encoder fails: nonhorn (exit 3), sat1
# (exit 4, a one-element model), memory or input (exit 2), timeout, crash.
#
# Output in OUT_DIR:
#   TRAIN-X/Problems/DOM/NAME.p and DEV-X/Problems/DOM/NAME.p, where NAME
#     is the source name plus "-10" (LCL096-1 -> LCL096-1-10), so that no
#     name collides with a TPTP problem;
#   TRAIN-X/pool.tsv, DEV-X/pool.tsv: name, relpath, status, spc, split
#     (np0_pool.py's columns, split train or heldout), relpath relative to
#     that split's root, so the np0 tools take TPTP=OUT_DIR/TRAIN-X;
#   manifest.tsv: one row per candidate (header line first);
#   summary.txt: counts per outcome and split, the inputs' identities;
#   .started, .done (status=complete, or deadline with jobs left; a rerun
#     with the same arguments resumes).
#
# Env: JOBS (parallel encoders, default the core count), TIMEOUT_S (per
# problem, 120), MEM_MB (virtual memory per encoder, 3072), DEADLINE_S
# (no launch after this many seconds, 14400).  The run executes a copy of
# this script, so editing the file mid-run cannot break it.
set -u

usage() {
  sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
}

if [ "${IFEQ10_SNAPSHOT:-}" != 1 ]; then
  [ $# -ge 4 ] || usage
  mkdir -p "$3" || exit 2
  cp "$0" "$3/.make_ifeq10_corpus.sh" || exit 2
  IFEQ10_SNAPSHOT=1 exec bash "$3/.make_ifeq10_corpus.sh" "$@"
fi

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
WORK=$OUT/.work

die() {
  echo "make_ifeq10_corpus: $*" >&2
  exit 2
}

[ -x "$FP" ] || die "$FP is not an executable"
"$FP" --help 2> /dev/null | grep -q -- '--emit-ifeq10' || die "$FP has no --emit-ifeq10"
[ -d "$TPTP_ROOT/Problems" ] || die "$TPTP_ROOT has no Problems/"
[ -s "$J13" ] || die "the J13 name list $J13 is missing or empty"
for f in "${EXTRA[@]+"${EXTRA[@]}"}"; do
  [ -s "$f" ] || die "the exclusion list $f is missing or empty"
done
command -v timeout > /dev/null 2>&1 || die "no timeout(1)"
if command -v sha256sum > /dev/null 2>&1; then
  SHA=sha256sum
else
  SHA="shasum -a 256"
fi

mkdir -p "$WORK/res" "$WORK/err" "$WORK/tmp" "$OUT/TRAIN-X/Problems" "$OUT/DEV-X/Problems" || exit 2
T0=$(date +%s)
DEADLINE_AT=$((T0 + DEADLINE_S))
rm -f "$OUT/.done"
{
  echo "started=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "host=$(hostname)"
  echo "findproof=$FP"
  echo "findproof_sha256=$($SHA "$FP" | cut -d' ' -f1)"
  echo "tptp_root=$TPTP_ROOT"
  echo "j13_names=$J13 ($(wc -l < "$J13" | tr -d ' ') names, sha256 $($SHA "$J13" | cut -d' ' -f1))"
  for f in "${EXTRA[@]+"${EXTRA[@]}"}"; do
    echo "exclude_names=$f ($(wc -l < "$f" | tr -d ' ') names, sha256 $($SHA "$f" | cut -d' ' -f1))"
  done
  echo "jobs=$JOBS timeout_s=$TIMEOUT_S mem_mb=$MEM_MB deadline_s=$DEADLINE_S"
} > "$OUT/.started"

stems_of() {
  sed -E 's/^[[:space:]]*([A-Z]{3}[0-9]{3}).*/\1/' "$@" | grep -E '^[A-Z]{3}[0-9]{3}$' | sort -u
}
stems_of "$J13" > "$WORK/j13_stems"
grep -E -- '-10(\.[0-9]+)?[[:space:]]*$' "$J13" | stems_of > "$WORK/j13_10_stems"
if [ ${#EXTRA[@]} -gt 0 ]; then
  stems_of "${EXTRA[@]}" > "$WORK/excl_stems"
else
  : > "$WORK/excl_stems"
fi

# The candidates: CNF (-) and FOF (+) problem files, with their header's
# Status and SPC (read up to the first record).
find "$TPTP_ROOT/Problems" -type f -name '*.p' | grep -E '/[A-Z]{3}[0-9]{3}[-+][0-9][^/]*\.p$' \
  | sort > "$WORK/files"
find "$TPTP_ROOT/Problems" -type f -name '*-10*.p' | grep -E '/[A-Z]{3}[0-9]{3}-10(\.[0-9]+)?\.p$' \
  | sed -E 's|.*/([A-Z]{3}[0-9]{3})-10[^/]*$|\1|' | sort -u > "$WORK/tptp10_stems"
tr '\n' '\0' < "$WORK/files" | xargs -0 awk '
  FNR == 1 { st = ""; spc = "" }
  /^%[ \t]*Status[ \t]*:/ && st == "" { s = $0; sub(/^%[ \t]*Status[ \t]*:[ \t]*/, "", s); split(s, a, /[ \t]/); st = a[1] }
  /^%[ \t]*SPC[ \t]*:/ { s = $0; sub(/^%[ \t]*SPC[ \t]*:[ \t]*/, "", s); split(s, a, /[ \t]/); spc = a[1] }
  /^[a-z]/ { print FILENAME "\t" st "\t" spc; nextfile }
' > "$WORK/headers.tsv"

awk -F'\t' -v root="$TPTP_ROOT/" \
    -v jf="$WORK/j13_stems" -v ef="$WORK/excl_stems" -v tf="$WORK/tptp10_stems" '
  BEGIN {
    while ((getline s < jf) > 0) J[s] = 1
    while ((getline s < ef) > 0) E[s] = 1
    while ((getline s < tf) > 0) T[s] = 1
  }
  {
    path = $1; st = $2; spc = $3
    n = path; sub(/.*\//, "", n); sub(/\.p$/, "", n)
    stem = substr(n, 1, 6); dom = substr(n, 1, 3)
    rel = substr(path, length(root) + 1)
    if (st != "Unsatisfiable" && st != "Theorem") { next }
    out = "encode"
    if (stem in J) out = "j13"
    else if (stem in E) out = "excluded"
    else if (spc ~ /UEQ$/) out = "ueq"
    else if (stem in T) out = "tptp10"
    print rel "\t" n "\t" stem "\t" dom "\t" st "\t" out
  }' "$WORK/headers.tsv" > "$WORK/candidates.tsv"

awk -F'\t' '$6 != "encode" {print $1 "\t-\t" $3 "\t" $5 "\t" $6 "\t-\t-\t-\t-"}' \
  "$WORK/candidates.tsv" > "$WORK/skipped.tsv"
awk -F'\t' '$6 == "encode" {print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5}' \
  "$WORK/candidates.tsv" > "$WORK/jobs.tsv"

# One encoding: OUT/<split>/Problems/DOM/NAME-10.p and a manifest row in
# .work/res/NAME.tsv (its presence makes a rerun skip the job).
encode_one() {
  local rel name stem dom st
  IFS=$'\t' read -r rel name stem dom st <<< "$1"
  local res=$WORK/res/$name.tsv
  [ -s "$res" ] && return 0
  [ "$(date +%s)" -lt "$DEADLINE_AT" ] || return 0
  local h split
  h=$(printf '%s' "$stem" | $SHA | cut -c1-8)
  if [ $((16#$h % 100)) -lt 80 ]; then
    split=TRAIN-X
  else
    split=DEV-X
  fi
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
    124 | 137) outcome=timeout ;;
    *) outcome=crash ;;
  esac
  if [ "$outcome" = input ] && grep -q 'out of memory' "$err"; then
    outcome=memory
  fi
  local out_name=- nc=- nd=- secs
  secs=$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f", b - a }')
  if [ "$outcome" = emitted ]; then
    out_name=$name-10
    mkdir -p "$OUT/$split/Problems/$dom"
    nc=$(grep -c '^cnf(' "$tmp")
    nd=$(grep -c '^cnf(ifeq_axiom' "$tmp")
    mv "$tmp" "$OUT/$split/Problems/$dom/$out_name.p"
  else
    rm -f "$tmp"
    split=-
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$rel" "$out_name" "$stem" "$st" "$outcome" \
    "$split" "$nc" "$nd" "$secs" > "$res.part"
  mv "$res.part" "$res"
}
export -f encode_one
export WORK OUT FP TPTP_ROOT SHA TIMEOUT_S MEM_MB DEADLINE_AT

tr '\n' '\0' < "$WORK/jobs.tsv" | xargs -0 -P "$JOBS" -n 1 bash -c 'encode_one "$1"' _

# Assemble the manifest and the per-split pool tables from every result,
# this run's and a previous run's.
{
  printf 'source\tname\tstem\tstatus\toutcome\tsplit\tclauses\tdispatchers\tseconds\n'
  {
    cat "$WORK/skipped.tsv"
    find "$WORK/res" -type f -name '*.tsv' -exec cat {} +
  } | sort
} > "$OUT/manifest.tsv"
for split in TRAIN-X DEV-X; do
  label=train
  [ "$split" = DEV-X ] && label=heldout
  awk -F'\t' -v s="$split" -v l="$label" 'NR > 1 && $6 == s {
      dom = substr($2, 1, 3)
      print $2 "\tProblems/" dom "/" $2 ".p\tUnsatisfiable\tCNF_UNS_RFO_PEQ_UEQ\t" l
    }' "$OUT/manifest.tsv" > "$OUT/$split/pool.tsv"
done

n_jobs=$(wc -l < "$WORK/jobs.tsv" | tr -d ' ')
n_done=$(find "$WORK/res" -type f -name '*.tsv' | wc -l | tr -d ' ')
left=$((n_jobs - n_done))
{
  cat "$OUT/.started"
  echo "candidates=$(wc -l < "$WORK/candidates.tsv" | tr -d ' ') (CNF and FOF, Unsatisfiable or Theorem)"
  echo "j13_stems=$(wc -l < "$WORK/j13_stems" | tr -d ' ')"
  echo "j13_10_variants_skipped=$(awk -F'\t' 'FILENAME == ARGV[1] { s[$1] = 1; next }
      $6 == "j13" && ($3 in s)' "$WORK/j13_10_stems" "$WORK/candidates.tsv" | wc -l | tr -d ' ')" \
    "(sources sharing a stem with a J13 -10 problem)"
  echo "outcomes:"
  awk -F'\t' 'NR > 1 { n[$5]++ } END { for (k in n) printf "  %-9s %d\n", k, n[k] }' "$OUT/manifest.tsv" | sort
  echo "splits:"
  awk -F'\t' 'NR > 1 && $6 != "-" { n[$6]++; d[$6] += $8 } END {
      for (k in n) printf "  %-8s %d problems, %d dispatcher axioms\n", k, n[k], d[k] }' "$OUT/manifest.tsv" | sort
  echo "pending=$left"
  echo "finished=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$OUT/summary.txt"
cat "$OUT/summary.txt"
if [ "$left" -gt 0 ]; then
  echo "status=deadline pending=$left" > "$OUT/.done"
else
  echo "status=complete" > "$OUT/.done"
fi
