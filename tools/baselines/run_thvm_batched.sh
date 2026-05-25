#!/usr/bin/env bash
# Batched single-kernel sweep: split AxiomaticTheory[] into chunks small
# enough to finish before the cumulative-state SIGTRAP crash (empirically
# fires after ~72 cases).  Each chunk runs in its OWN fresh wolframscript
# kernel -- so peak memory is one ~2GB spike at a time, sequential.  TSV
# is per-case flushed by the WL driver, so partial results survive any
# unforeseen mid-chunk crash.

set -u
BATCH_SIZE=${BATCH_SIZE:-8}    # theories per kernel.  Empirically 12 hits a license/state crash on the 3rd kernel of a sweep -- 8 keeps each batch small enough to dodge it.
TC=${TC:-30}
OUT=tools/baselines/thvm_notable_theorems.tsv

# Reset output once at the start
echo -e "theory\tthm\tstatus\tseconds\tproofLength\tverifies" > $OUT

# Enumerate theory names (one per line)
THEORY_FILE=$(mktemp)
/Applications/Wolfram.app/Contents/MacOS/wolframscript -code '
Scan[WriteString["stdout", ToString[#], "\n"] &, AxiomaticTheory[]]' \
    </dev/null > $THEORY_FILE 2>/dev/null

n_theories=$(wc -l < $THEORY_FILE | tr -d ' ')
echo "[batched] $n_theories theories, BATCH_SIZE=$BATCH_SIZE, TC=$TC"

# Process in chunks of BATCH_SIZE theories
batch=0
i=1
while [[ $i -le $n_theories ]]; do
    end=$((i + BATCH_SIZE - 1))
    [[ $end -gt $n_theories ]] && end=$n_theories
    batch=$((batch+1))
    chunk_theories=$(sed -n "${i},${end}p" $THEORY_FILE | tr '\n' ',' | sed 's/,$//')
    echo "[batched] === BATCH $batch: theories $i-$end ==="
    # Spawn a fresh kernel for this chunk -- $OUT is opened/appended
    # by the WL driver per case.
    THVM_CHUNK_THEORIES="$chunk_theories" THVM_CHUNK_TC=$TC \
    timeout --kill-after=10s 600 \
        /Applications/Wolfram.app/Contents/MacOS/wolframscript \
        -f tools/baselines/run_thvm_chunk.wls </dev/null 2>&1
    rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "[batched] batch $batch wolframscript exited rc=$rc (continuing)"
    fi
    i=$((end + 1))
    # Brief pause lets the OS clean up file handles / Wolfram release
    # any per-kernel license slot before the next kernel starts.
    sleep 3
done

rm -f $THEORY_FILE
echo "[batched] DONE"
echo -n "  Tally: "
awk -F'\t' 'NR>1 {c[$3]++} END {for (k in c) printf "%s=%d ", k, c[k]; print ""}' $OUT
