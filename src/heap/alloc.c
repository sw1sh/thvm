// Bump allocator over CURRENT_CTX->heap_next; atomic so the parallel WNF pool
// (wnf/pool.c) allocates without losing cells.

// A LibraryLink entry sets thvm_heap_exhaust_jmp (THVM_WL_RECOVER) so that
// thvm_fatal returns an error to WL instead of exiting, which would orphan
// WolframKernel; NULL exits, as stand-alone binaries want.
#include <setjmp.h>
// execinfo (backtrace) is glibc/Darwin only, so THVM_FATAL_BACKTRACE is
// compiled out on musl and other minimal libcs.
#if defined(__GLIBC__) || defined(__APPLE__)
#define THVM_HAVE_EXECINFO 1
#include <execinfo.h>
#endif
jmp_buf  *thvm_heap_exhaust_jmp = NULL;
volatile int thvm_heap_exhausted = 0;

const char *g_thvm_phase = "init";   // DIAG: last ATP phase entered

__attribute__((noreturn))
fn void thvm_fatal(const char *msg) {
  fprintf(stderr, "thvm fatal: %s [phase=%s]\n", msg, g_thvm_phase);
  // THVM_FATAL_BACKTRACE=1: the C call stack, since the phase tag names only
  // the enclosing step.
#ifdef THVM_HAVE_EXECINFO
  if (getenv("THVM_FATAL_BACKTRACE") != NULL) {
    void *bt[48];
    int n = backtrace(bt, 48);
    backtrace_symbols_fd(bt, n, 2);
  }
#endif
  if (thvm_heap_exhaust_jmp != NULL) {
    thvm_heap_exhausted = 1;
    longjmp(*thvm_heap_exhaust_jmp, 1);
  }
  exit(1);
}

fn u64 heap_alloc(u64 size) {
  u64 cap = gc_enabled() ? gc_from_end() : thvm_heap_cells();
  u64 at  = (u64)__atomic_fetch_add(&CURRENT_CTX->heap_next, size,
                                    __ATOMIC_RELAXED);
  if (at + size > cap) {
    char buf[256];
    snprintf(buf, sizeof buf,
             "heap_alloc: from-space exhausted (need %llu cells, "
             "HEAP_NEXT=%llu, cap=%llu); call gc_collect or grow GC_SPACE_SZ",
             (unsigned long long)size,
             (unsigned long long)at,
             (unsigned long long)cap);
    thvm_fatal(buf);
  }
  return at;
}
