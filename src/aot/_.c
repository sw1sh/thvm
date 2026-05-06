// src/aot/_.c
//
// Bundle for the Bend2-style fork-emitting AOT runtime.  thvm.c
// includes this single file; everything else lives in subordinate
// .c/.h files included from here so the include order is
// self-documenting:
//
//   task.h     -- AotTask / AotResult / aot_enc_ret + the make_*
//                 constructors.  Pure types, no dependencies.
//   cont.c     -- continuation cell layout + alloc/write_slot/
//                 fire_cont.  Uses HEAP[]/heap_alloc/heap_set, so
//                 must come after src/heap/*.
//   resolve.c  -- aot_resolve loop + AotProgram/AotDispatchFn types.
//                 Calls cont.c helpers.
//   worker.c   -- AotBarrier + AotRun + serial/parallel runners.
//                 Calls aot_resolve + program->dispatch.
//
// Phase 2 will add emit.c here for the CPS-transforming auto-emitter.
// Phase 4 will add a build.c for whole-program clang invocations
// driven from the WL bridge.

#include "task.h"
#include "halloc.h"
#include "halloc.c"
#include "cont.c"
#include "resolve.c"
#include "worker.c"
#include "emit.c"
#include "metal_emit.c"
#include "build.c"
