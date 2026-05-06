#!/usr/bin/env python3
# tools/inline_includes.py
#
# Recursively expand `#include "..."` (relative-path) directives
# starting from a root .c file, leaving `#include <...>` (system
# headers) intact.  Each file is included AT MOST ONCE -- header
# guards already prevent re-inclusion at the C level, but we
# enforce it at flatten time too to keep output linear.
#
# Used to bake src/thvm.c (and all of its OUR-side transitively-
# included files) into THVMLink.dylib at paclet build time so the
# AOT compile pipeline doesn't depend on the source tree being
# present at runtime.
#
# Usage:
#   python3 tools/inline_includes.py src/thvm.c > build/thvm_inline.c

import sys
import os
import re

INCLUDE_RE = re.compile(r'^\s*#include\s+"([^"]+)"')


def expand(path, seen, out):
    canon = os.path.realpath(path)
    if canon in seen:
        out.append('// (already inlined: {})\n'.format(path))
        return
    seen.add(canon)
    here = os.path.dirname(path)
    out.append('// === inlined: {} ===\n'.format(path))
    with open(path, 'r') as f:
        for line in f:
            m = INCLUDE_RE.match(line)
            if m:
                inc = m.group(1)
                resolved = os.path.normpath(os.path.join(here, inc))
                if os.path.exists(resolved):
                    expand(resolved, seen, out)
                    continue
                # Fall through (let clang error if the include is broken).
            out.append(line)
    out.append('// === end: {} ===\n'.format(path))


def main():
    if len(sys.argv) != 2:
        sys.stderr.write('usage: inline_includes.py <root.c>\n')
        sys.exit(1)
    out = []
    expand(sys.argv[1], set(), out)
    sys.stdout.write(''.join(out))


if __name__ == '__main__':
    main()
