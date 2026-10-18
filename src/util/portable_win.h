// Win32 shims for the CPU-only Windows paclet cross-build (zig cc -target
// x86_64-windows-gnu), whose mingw CRT lacks these POSIX calls. thvm.h includes
// this after the system headers; off _WIN32 it is empty.
//
// No GPU backend builds here, and the CPU JIT falls back to the interpreter
// without a compiler, so the dlopen/popen shims run only when clang is on PATH.
#ifndef THVM_PORTABLE_WIN_H
#define THVM_PORTABLE_WIN_H
#ifdef _WIN32

// Trim windows.h so its macros (winsock's s_addr, min, max) and GDI names stay
// out of the runtime's identifiers.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <malloc.h>
#include <time.h>

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

#define popen  _popen
#define pclose _pclose
#define strdup _strdup

#define mkdir(path, mode) _mkdir(path)

// malloc, not _aligned_malloc: the one caller (util/wsq.c) frees with free(),
// and the alignment only spares false sharing.
static inline int posix_memalign(void **memptr, size_t alignment, size_t size) {
  (void)alignment;
  void *p = malloc(size);
  if (!p) return 12;  // ENOMEM
  *memptr = p;
  return 0;
}

#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 1
#endif
static inline long sysconf(int name) {
  (void)name;
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return (long)si.dwNumberOfProcessors;
}

// zig's bundled mingw (winpthreads) supplies clock_gettime, CLOCK_MONOTONIC and
// gettimeofday, so they need no shim.

#endif  // _WIN32
#endif  // THVM_PORTABLE_WIN_H
