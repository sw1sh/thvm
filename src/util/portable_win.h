// util/portable_win.h - Win32 shims for the CPU-only Windows paclet
// cross-build (zig cc -target x86_64-windows-gnu).
//
// mingw's CRT lacks the POSIX/glibc functions the runtime calls.  This
// header maps them to their Win32 equivalents.  It is included only
// under _WIN32 (from thvm.h, after the system headers); the macOS and
// Linux builds never see it.
//
// GPU backends never compile on Windows (Metal is Apple-only Objective-C;
// CUDA is Linux-gated), and the CPU JIT's compile+dlopen step degrades
// gracefully to the interpreter when no compiler is on the box -- so the
// dlopen/popen shims exist to satisfy the link, and run only if a user
// has clang on PATH.
#ifndef THVM_PORTABLE_WIN_H
#define THVM_PORTABLE_WIN_H
#ifdef _WIN32

// Trim windows.h: WIN32_LEAN_AND_MEAN drops winsock (whose
// `#define s_addr ...` would clobber the runtime's `Term s_addr`
// locals); NOMINMAX/NOGDI drop the min/max macros and GDI symbols.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <malloc.h>
#include <time.h>

// --- dlopen family -> LoadLibrary / GetProcAddress ---
#define RTLD_NOW   0
#define RTLD_LOCAL 0
static inline void *dlopen(const char *path, int flags) {
  (void)flags;
  return (void *)LoadLibraryA(path);
}
static inline void *dlsym(void *handle, const char *symbol) {
  return (void *)(uintptr_t)GetProcAddress((HMODULE)handle, symbol);
}
static inline int dlclose(void *handle) {
  return FreeLibrary((HMODULE)handle) ? 0 : -1;
}
static inline char *dlerror(void) { return NULL; }

// --- process + string helpers ---
#define popen  _popen
#define pclose _pclose
#define strdup _strdup

// --- mkdir(path, mode): Win32 _mkdir takes no mode ---
#define mkdir(path, mode) _mkdir(path)

// --- posix_memalign: the only caller (util/wsq.c) frees with plain
//     free(), so we must NOT use _aligned_malloc (which would require
//     _aligned_free).  Use malloc and ignore the alignment request --
//     it is a cache-line false-sharing optimisation for the parallel
//     work-stealing deque, not a correctness requirement, so a CPU-only
//     Windows build trades a little false-sharing for a portable free. ---
static inline int posix_memalign(void **memptr, size_t alignment, size_t size) {
  (void)alignment;
  void *p = malloc(size);
  if (!p) return 12;  // ENOMEM
  *memptr = p;
  return 0;
}

// --- aligned_alloc (C11): not in mingw's CRT.  The runtime frees the
//     result with plain free(), so map to malloc and drop the alignment
//     request (a huge-page hint on Apple Silicon, not a Windows
//     correctness requirement). ---
static inline void *aligned_alloc(size_t alignment, size_t size) {
  (void)alignment;
  return malloc(size);
}

// --- sysconf(_SC_NPROCESSORS_ONLN) -> GetSystemInfo ---
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 1
#endif
static inline long sysconf(int name) {
  (void)name;
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return (long)si.dwNumberOfProcessors;
}

// clock_gettime + CLOCK_MONOTONIC and <sys/time.h>/gettimeofday come
// from zig's bundled mingw (winpthreads), so they are not shimmed here.

#endif  // _WIN32
#endif  // THVM_PORTABLE_WIN_H
