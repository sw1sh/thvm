#!/bin/bash
# Run all ATP-track wlt files in SEPARATE wolframscript invocations
# and report aggregate pass/fail.  Used as a quick regression check
# after any change touching the ATP path.
#
# Each wlt gets its own kernel (per the memory rule about wlt
# pre-existing crashers; one bad file doesn't poison the rest).
#
# Usage:
#   tools/baselines/run_atp_tests.sh [WMCLI=/path] [PER_FILE_TIMEOUT=180]
set -uo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

WMCLI="${WMCLI:-$REPO_ROOT/../wolfram/waldmeister/wmcli}"
PER_FILE_TIMEOUT="${PER_FILE_TIMEOUT:-180}"

# Wrapper wlt-runner: takes a wlt path on argv, prints "OK/TOTAL"
# or "?/?" on crash.  Written to /tmp so the bash quoting of the
# wolframscript -code form doesn't have to escape backticks.
RUNNER=$(mktemp /tmp/run_atp_tests.XXXX.wls)
cat > "$RUNNER" <<'WLS'
#!/usr/bin/env wolframscript
PacletDirectoryLoad[Environment["THVM_LINK"]];
Get["THVMLink`ATP`"];
file = $ScriptCommandLine[[2]];
r = Quiet @ TestReport[file];
WriteString["stdout", "\n[[RESULT]] "];
If[ Head[r] === TestReportObject,
    WriteString["stdout",
        ToString[r["TestsSucceededCount"]], "/",
        ToString[r["TestsSucceededCount"] +
            r["TestsFailedCount"]]],
    WriteString["stdout", "?/?"]];
WriteString["stdout", "\n"];
WLS

trap 'rm -f "$RUNNER"' EXIT

TOTAL=0
FAILS=0
for f in wl/THVMLink/Tests/atp*.wlt; do
    base="$(basename "$f" .wlt)"
    raw=$(timeout "$PER_FILE_TIMEOUT" env \
            WMCLI="$WMCLI" \
            THVM_LINK="$REPO_ROOT/wl/THVMLink" \
        wolframscript -file "$RUNNER" "$REPO_ROOT/$f" 2>/dev/null)
    result=$(printf '%s\n' "$raw" | awk '/^\[\[RESULT\]\] / { print $2; exit }')
    if [ -z "$result" ] || [ "$result" = "?/?" ]; then
        result="?/? CRASH"
        FAILS=$((FAILS + 1))
    else
        passed="${result%/*}"
        total="${result#*/}"
        TOTAL=$((TOTAL + total))
        if [ "$passed" != "$total" ]; then
            failed=$((total - passed))
            FAILS=$((FAILS + failed))
            result="$result  FAIL"
        fi
    fi
    printf "%-18s %s\n" "$base" "$result"
done

echo "---"
if [ "$FAILS" -eq 0 ]; then
    echo "TOTAL: $((TOTAL - FAILS))/$TOTAL ALL GREEN"
else
    echo "TOTAL: $((TOTAL - FAILS))/$TOTAL (FAILS: $FAILS)"
fi
