#!/usr/bin/env bash
# ProoVer checker container entrypoint.
#   glibc-probe    report the kernel binary's required GLIBC symbol version +
#                  ldd, then try to start it. A licensing error => glibc is fine;
#                  a GLIBC_x.y-not-found => the base is too old (CentOS 7).
#   corpus         run the bundled corpus through check.wls (needs a license)
#   <proof.p>      emit the SZS verdict for one proof (needs a license)
set -u
EXED=$(ls -d /usr/local/Wolfram/*/*/Executables 2>/dev/null | head -1)
export PATH="$PATH:$EXED"
# devtoolset libstdc++ (newer GLIBCXX/CXXABI), if present -- fixes the C++ ABI
# half of the CentOS-7 gap; the libm GLIBC_2.27 half is unfixable.
DT=/opt/rh/devtoolset-11/root/usr/lib64
[ -d "$DT" ] && export LD_LIBRARY_PATH="$DT:${LD_LIBRARY_PATH:-}"

case "${1:-glibc-probe}" in
  glibc-probe)
    echo "host glibc:   $(ldd --version | head -1)"
    K=$(find /usr/local/Wolfram -type f -name WolframKernel -path '*Linux-x86-64*' 2>/dev/null | head -1)
    echo "WolframKernel: ${K:-NOT FOUND}"
    if [ -n "$K" ]; then
      L=$(dirname "$K")/../../../Libraries/Linux-x86-64
      echo "GLIBC versions libWolframEngine.so requires:"
      grep -ao 'GLIBC_[0-9.]\+' "$L/libWolframEngine.so" 2>/dev/null | sort -V | uniq | tail -3 | sed 's/^/   /'
      echo "unresolved at load (ldd):"
      ldd "$L/libWolframEngine.so" 2>&1 | grep -i "not found" | sed 's/^/   /' | sort -u || echo "   (all resolved)"
    fi
    echo "-- start kernel (license error => glibc OK; GLIBC-not-found => too old) --"
    timeout 90 wolframscript -code 'Print["KERNEL OK ", 1+1]' 2>&1 | head -25
    ;;
  corpus)  wolframscript -f /opt/proover/check.wls /opt/proover/corpus ;;
  *)       wolframscript -f /opt/proover/check.wls "$@" ;;
esac
