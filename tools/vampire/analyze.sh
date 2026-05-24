#!/usr/bin/env bash
# analyze.sh - summarize /tmp/vampire/results.tsv: counts, technique
# patterns by theorem class, top winning strategies.

set -u
TSV="/tmp/vampire/results.tsv"
if [ ! -s "$TSV" ]; then
    echo "no results: $TSV missing or empty" >&2
    exit 1
fi

echo "=== Vampire batch results ==="
echo
echo "Total rows: $(tail -n +2 "$TSV" | wc -l | tr -d ' ')"
echo "PROVED:   $(awk -F'\t' 'NR>1 && $4=="PROVED"' "$TSV" | wc -l | tr -d ' ')"
echo "TIMEOUT:  $(awk -F'\t' 'NR>1 && $4=="TIMEOUT"' "$TSV" | wc -l | tr -d ' ')"
echo "SATURATED:$(awk -F'\t' 'NR>1 && $4=="SATURATED"' "$TSV" | wc -l | tr -d ' ')"
echo "OTHER:    $(awk -F'\t' 'NR>1 && $4=="OTHER"' "$TSV" | wc -l | tr -d ' ')"

echo
echo "=== Per-theory class breakdown ==="
awk -F'\t' 'NR>1 {tot[$1]++; if ($4=="PROVED") prv[$1]++} END {
  for (t in tot) printf "  %-32s  %d/%d proved\n", t, prv[t]+0, tot[t]
}' "$TSV" | sort

echo
echo "=== Winning strategy frequency (top 12) ==="
awk -F'\t' 'NR>1 && $4=="PROVED" {
  # strategy looks like "lrs+10_4:7_drc=off:..." -- key on the FIRST token (algo + opts before first colon-arg)
  split($6, parts, ":");
  algo = parts[1];
  count[algo]++
} END {
  for (a in count) printf "%6d  %s\n", count[a], a
}' "$TSV" | sort -rn | head -12

echo
echo "=== Saturation algorithm (lrs/dis/ott/etc) frequency ==="
awk -F'\t' 'NR>1 && $4=="PROVED" {
  # First two chars of strategy = saturation alg prefix
  m = $6
  sub(/[+0-9_].*$/, "", m)
  count[m]++
} END {
  for (a in count) printf "%6d  %s\n", count[a], a
}' "$TSV" | sort -rn | head -10

echo
echo "=== Time distribution ==="
awk -F'\t' 'NR>1 && $4=="PROVED" {
  t = $5+0
  if (t < 0.1) b="<0.1s"
  else if (t < 1) b="0.1-1s"
  else if (t < 5) b="1-5s"
  else if (t < 15) b="5-15s"
  else b="15-30s"
  count[b]++
} END {
  for (b in count) printf "%6d  %s\n", count[b], b
}' "$TSV" | sort

echo
echo "=== Hardest 10 proven (longest time) ==="
awk -F'\t' 'NR>1 && $4=="PROVED" {printf "%6.2fs  %s::%s.c%s  %s\n", $5, $1, $2, $3, $6}' "$TSV" | sort -rn | head -10

echo
echo "=== Unproven ==="
awk -F'\t' 'NR>1 && $4!="PROVED" {printf "  [%s] %s::%s.c%s\n", $4, $1, $2, $3}' "$TSV"
