#!/usr/bin/env bash
# flux_cuda_pod.sh -- provision a Prime Intellect GPU pod, build the CUDA-enabled
# THVMLink, and measure the FLUX.2-klein-4B transformer-forward warm step on CUDA.
#
# Self-contained: the bench runs with SYNTH=1 (weights synthesised at their known
# shapes), so there is NO 8 GB safetensors download -- the warm per-step time is
# identical to a real-weight run (same shapes, same kernels; validated against the
# Metal baseline, 625 ms/step synth vs 632-650 ms/step real).
#
# Usage:
#   WOLFRAM_ENTITLEMENT_ID=O-XXXX-... ./tools/flux_cuda_pod.sh [gpu-type] [pod-name]
#
# The entitlement is a Wolfram on-demand license; mint one from a cloud-connected
# kernel ($CloudConnected===True):
#   CreateLicenseEntitlement[<|"StandardKernelLimit" -> 2,
#     "LicenseExpiration" -> Quantity[12, "Hours"],
#     "EntitlementExpiration" -> Quantity[7, "Days"]|>]   (* first part is the O-... id *)
#
# Prereqs: prime CLI authenticated, ~/.prime/config.json with ssh_key_path, the
# custom wolfram-cuda template (WL 15.0 + WolframGPUSDK prebaked).  The CUDA
# backend itself is validated (81/81 C tests pass on H100).
set -euo pipefail

GPU_TYPE="${1:-H100 80GB}"
POD_NAME="${2:-flux-cuda}"
TEMPLATE_ID="${WOLFRAM_POD_TEMPLATE:-cmqct4j5z003fvxxj2e2gre02}"
ENT="${WOLFRAM_ENTITLEMENT_ID:?Set WOLFRAM_ENTITLEMENT_ID (see header for the mint one-liner)}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SSH_KEY="$(jq -r '.ssh_key_path' ~/.prime/config.json)"
SSH_OPTS=(-i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=10)

T0=$(date +%s); say() { echo "[$(( $(date +%s) - T0 ))s] $*"; }

# Cheapest 1x non-spot offer for GPU_TYPE from a custom-template-capable provider
# (lambdalabs/crusoecloud/nebius; lambdalabs is the one whose sshd honours the
# injected PUBLIC_KEY -- ssh lands on root@host at the entrypoint port, not 22).
OFFER_ID=$(prime availability list --output json 2>/dev/null | jq -r --arg t "$GPU_TYPE" '
  [.gpu_resources[] | select(.gpu_type == $t and .gpu_count == 1 and .is_spot != true
                             and .stock_status == "Available"
                             and (.provider == "lambdalabs" or .provider == "crusoecloud" or .provider == "nebius"))]
  | sort_by(.price_value) | .[0].id // empty')
[ -n "$OFFER_ID" ] || { echo "No custom-template-capable offer for '$GPU_TYPE'"; exit 1; }
say "offer: $OFFER_ID"

# --disk-size is REQUIRED non-interactively: without it `pods create` blocks on an
# interactive prompt even with -y (which only skips the final confirmation).
CREATE_OUT=$(prime --plain pods create --id "$OFFER_ID" --name "$POD_NAME" \
  --image custom_template --custom-template-id "$TEMPLATE_ID" \
  --disk-size "${DISK_GB:-128}" \
  --env WOLFRAMSCRIPT_ENTITLEMENTID="$ENT" -y < /dev/null)
POD_ID=$(echo "$CREATE_OUT" | grep -oE '[a-f0-9]{32}' | head -1)
[ -n "$POD_ID" ] || { echo "$CREATE_OUT"; echo "Could not parse pod ID"; exit 1; }
say "pod: $POD_ID  (terminate: prime pods terminate $POD_ID)"
trap 'echo; say "auto-terminating $POD_ID"; prime pods terminate "$POD_ID" -y >/dev/null 2>&1 || true' EXIT

for i in $(seq 1 180); do
  ST_OUT=$(prime --plain pods status "$POD_ID" 2>/dev/null || true)
  ST=$(echo "$ST_OUT" | awk '/^Status/{print $2; exit}')
  [ "$ST" = "ACTIVE" ] && break; sleep 5
done
[ "${ST:-}" = "ACTIVE" ] || { echo "Timed out (last: ${ST:-?})"; exit 1; }
# SSH target is "root@HOST -p PORT" (lambdalabs entrypoint port, not 22); split it.
SSH_LINE=$(echo "$ST_OUT" | awk '/^SSH/{$1=""; print substr($0,2); exit}')
HOST=$(echo "$SSH_LINE" | awk '{print $1}')
PORT=$(echo "$SSH_LINE" | grep -oE '\-p [0-9]+' | awk '{print $2}'); PORT="${PORT:-22}"
say "ACTIVE, ssh: $HOST -p $PORT"
SSH=(ssh "${SSH_OPTS[@]}" -p "$PORT" "$HOST")
# The custom-template sshd can take 10+ min to accept after ACTIVE (multi-GB WL image
# pull + entrypoint), refusing the port until then. Poll for a REAL success; fail loud
# if it never comes up (don't false-proceed into a refused connection).
ok=
for i in $(seq 1 150); do "${SSH[@]}" true 2>/dev/null && { ok=1; break; }; sleep 6; done
[ -n "$ok" ] || { echo "ssh never accepted on $HOST -p $PORT after ~15 min (sshd not up)"; exit 1; }
say "ssh reachable (attempt $i)"

# Ship the source (no .git/build/weights), build the CUDA .so, run the bench.
"${SSH[@]}" 'command -v rsync >/dev/null || (apt-get update -qq && apt-get install -y -qq rsync make) >/dev/null 2>&1 || true'
rsync -az --delete -e "ssh ${SSH_OPTS[*]} -p $PORT" \
  --exclude '.git' --exclude 'build' --exclude '*.dylib' --exclude '*.so' \
  --exclude 'tools/baselines' "$REPO"/ "$HOST":/root/thvm/
say "source synced"

ENT="$ENT" "${SSH[@]}" "ENT=$ENT bash -s" <<'REMOTE'
set -e
export WOLFRAMSCRIPT_ENTITLEMENTID="$ENT"
cd /root/thvm
GS=$(echo /root/.Wolfram/Paclets/Repository/WolframGPUSDK-LINUX-*/CUDAResources/Linux-x86-64/targets/x86_64-linux | tr ' ' '\n' | head -1)
[ -d "$GS" ] || { echo "WolframGPUSDK not found -- triggering paclet download"; \
  WOLFRAMSCRIPT_ENTITLEMENTID="$WOLFRAMSCRIPT_ENTITLEMENTID" wolframscript -code 'PacletInstall["WolframGPUSDK"]' >/dev/null 2>&1 || true; \
  GS=$(echo /root/.Wolfram/Paclets/Repository/WolframGPUSDK-LINUX-*/CUDAResources/Linux-x86-64/targets/x86_64-linux | tr ' ' '\n' | head -1); }
WL=$(echo /usr/local/Wolfram/*/*/*/SystemFiles/Components/StandaloneApplicationsSDK/Linux-x86-64 | tr ' ' '\n' | head -1)
echo "GPUSDK=$GS"; echo "WL=$WL"
ATP="-DATP_FV_INDEX -DATP_RULE_INDEX -DATP_VAR_NORM -DATP_ORPHAN_KILL -DATP_ORDERED_REWRITE -DATP_CP_GROUND_JOIN -DATP_MNF -DTHVM_ATP_AC -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM"
make -s build/thvm_runtime_blob_elf.c
mkdir -p wl/THVMLink/LibraryResources/Linux-x86-64
gcc -std=c11 -O2 -w -fPIC -shared -D_GNU_SOURCE -DTHVM_HAS_CUDA $ATP -I"$GS/include" -I"$WL" \
  -o wl/THVMLink/LibraryResources/Linux-x86-64/THVMLink-cuda.so \
  wl/THVMLink/CSource/thvmlink.c build/thvm_runtime_blob_elf.c \
  -L"$GS/lib" -L"$GS/lib/stubs" -Wl,-rpath,"$GS/lib" -lcuda -lnvrtc -lm -ldl
echo "built THVMLink-cuda.so"
# nvrtc dlopens its builtins from the system lib path, so LD_LIBRARY_PATH must include $GS/lib.
LD_LIBRARY_PATH="$GS/lib" SYNTH=1 DEV=cuda THVM_MAX_LIVE_BYTES=40000000000 \
  WOLFRAMSCRIPT_ENTITLEMENTID="$WOLFRAMSCRIPT_ENTITLEMENTID" \
  wolframscript -f wl/THVMLink/Examples/flux/flux_bench.wls
REMOTE
say "done"
