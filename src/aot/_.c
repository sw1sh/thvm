// src/aot/_.c -- the Bend2-style AOT (ahead-of-time) compiler, included once
// from src/thvm.c after src/heap/.  Each file follows the ones it calls: the
// task runtime first, then the C and Metal emitters and the dylib build.

#include "task.h"
#include "halloc.h"
#include "halloc.c"
#include "cont.c"
#include "resolve.c"
#include "worker.c"
#include "emit.c"
#include "metal_emit.c"
#include "build.c"
