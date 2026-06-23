// tensor/mmap.c - lazy, mmap-backed disk tensor (tinygrad's DISK device).
//
// Ported from tinygrad/runtime/ops_disk.py: a disk tensor is a view into
// an mmap of a file.  We map the file region [byte_offset, byte_offset +
// nbytes) read-only + copy-on-write (PROT_READ | MAP_PRIVATE), point a
// CPU TenDesc's buffer straight at the mapped bytes (zero copy; the OS
// pages them in lazily), and defer any device upload to the existing
// UOP_COPY path.  A CPU op over the tensor consumes the mapped bytes
// directly; a COPY to Metal/CUDA uploads.  So no new backend is needed --
// the mmap-backed CPU TenDesc + the existing lazy COPY *is* the disk
// tensor.
//
// mmap requires a page-aligned offset.  tinygrad does exactly this in
// DiskAllocator._copyout_sharded:
//   fd_offset = src.offset - (minor_offset := src.offset % mmap.PAGESIZE)
// so we map from the page-aligned base `byte_offset - minor_offset` for
// `nbytes + minor_offset` bytes and point the buffer at base + minor_offset.
//
// Ownership: the CpuBuf is external (owns_data == 0); its on_release
// munmaps the ORIGINAL mapping (base + maplen, NOT the user-visible
// buffer pointer) and frees the bookkeeping cell.  We never free() an
// mmap; a stray free() on mmap'd memory is undefined behaviour.

#include <pthread.h>   // thvm_file_prefetch_async: background page-cache warm

// Per-mapping bookkeeping carried as the CpuBuf `handle`, so the release
// callback can munmap the exact region it mapped (the page-aligned base
// and full length), independent of the buffer pointer handed to the
// TenDesc (which is offset into the mapping by minor_offset).
typedef struct {
  void *base;     // page-aligned mmap base (the munmap address)
  u64   maplen;   // mmap length in bytes (the munmap length)
} DiskMap;

static void disk_map_release(void *handle) {
  DiskMap *m = (DiskMap *)handle;
  if (m == NULL) return;
  if (m->base != NULL && m->base != MAP_FAILED && m->maplen != 0) {
    munmap(m->base, m->maplen);
  }
  free(m);
}

// After a disk-mapped weight has been uploaded to a device, its host
// pages are pure duplication of the device copy -- and they were paged
// fully resident by the MADV_WILLNEED prefetch at map time.  Drop them
// with MADV_DONTNEED so they no longer count against RSS; the mapping
// stays valid (file-backed, PROT_READ), so a later CPU read just
// re-faults the bytes from the SSD.  `cpu_buf_id` must be a CPU buf
// whose on_release is disk_map_release (a disk tensor); for any other
// buf this is a no-op.  Without this, a multi-GB safetensors weight set
// is held TWICE during a forward -- the host mmap (~8GB for Qwen3-4B)
// PLUS the device upload (~8GB) -- and the host half is the difference
// between a ~9GB and a ~14GB resident set.
fn void thvm_disk_buf_dontneed(u32 cpu_buf_id) {
  if (CPU_BUFS == NULL || cpu_buf_id == 0 || cpu_buf_id >= CPU_BUFS_NEXT)
    return;
  CpuBuf *b = &CPU_BUFS[cpu_buf_id];
  if (b->on_release != disk_map_release) return;   // not a disk mapping
  DiskMap *m = (DiskMap *)b->handle;
  if (m == NULL || m->base == NULL || m->base == MAP_FAILED || m->maplen == 0)
    return;
  madvise(m->base, (size_t)m->maplen, MADV_DONTNEED);
}

// Background sequential prefetch of a whole safetensors file into the OS
// page cache.  The FLUX cold start faults each weight resident at its first
// matmul (the zero-copy wrap's touch loop, src/backend/metal/_.m); even with
// per-region MADV_WILLNEED that fault-in is serial INSIDE the first forward,
// so the ~6 s of SSD read does not overlap the JIT capture.  Kicking off a
// detached pthread that read()s the file front-to-back at session-build start
// warms the SAME unified page cache the mmaps fault from, so by the time the
// capture's matmuls run the pages are already resident (warm fault = memory
// speed).  read() (not mmap) keeps this independent of the per-tensor maps and
// adds NO extra RSS beyond the page cache the faults would populate anyway.
// Best-effort: any failure (open/read) just leaves the cold fault-in path.
// One reader thread covers a [start, end) byte segment of a file: pread()s it
// front-to-back through a throwaway buffer so the pages land in the unified
// page cache.  Splitting a big file across several segment-readers raises the
// SSD queue depth -- a single sequential reader saturates one I/O stream
// (~1-2 GB/s on this NVMe), but N parallel pread streams hit ~the device's full
// bandwidth, so a 7.7 GB transformer warms in ~1.5 s instead of ~4 s.  This is
// what lets the prefetch fully LEAD the zero-copy fault-in (fault_ms -> ~0).
typedef struct { char path[4096]; u64 start, end; } SegArg;

static void *disk_prefetch_seg(void *vp) {
  SegArg *s = (SegArg *)vp;
  int fd = open(s->path, O_RDONLY);
  if (fd >= 0) {
#ifdef F_RDAHEAD
    fcntl(fd, F_RDAHEAD, 1);
#endif
    enum { CHUNK = 8u << 20 };           // 8 MB pread window
    void *buf = malloc(CHUNK);
    if (buf != NULL) {
      u64 off = s->start;
      while (off < s->end) {
        size_t want = (s->end - off < CHUNK) ? (size_t)(s->end - off) : CHUNK;
        ssize_t n = pread(fd, buf, want, (off_t)off);
        if (n <= 0) break;
        off += (u64)n;
      }
      free(buf);
    }
    close(fd);
  }
  free(s);
  return NULL;
}

// Background page-cache warm of a whole file, split across N parallel
// segment-readers (THVM_PREFETCH_THREADS, default 4).  Detached: the caller
// returns immediately; the readers race ahead of the loaders/fault-in.
typedef struct { char path[4096]; } DiskPrefetchArg;

static void *disk_prefetch_main(void *vp) {
  DiskPrefetchArg *a = (DiskPrefetchArg *)vp;
  static int nthreads = -1;
  if (nthreads < 0) {
    const char *e = getenv("THVM_PREFETCH_THREADS");
    int v = (e != NULL) ? atoi(e) : 4;
    nthreads = (v < 1) ? 1 : (v > 16 ? 16 : v);
  }
  struct stat st;
  u64 sz = 0;
  if (stat(a->path, &st) == 0 && st.st_size > 0) sz = (u64)st.st_size;
  if (sz == 0) { free(a); return NULL; }

  int nseg = nthreads;
  if ((u64)nseg > sz) nseg = 1;
  u64 t0 = cg_now_us();
  pthread_t th[16];
  int spawned = 0;
  u64 seg = (sz + (u64)nseg - 1) / (u64)nseg;
  for (int i = 0; i < nseg; i++) {
    SegArg *s = (SegArg *)malloc(sizeof(*s));
    if (s == NULL) break;
    memcpy(s->path, a->path, strlen(a->path) + 1);
    s->start = (u64)i * seg;
    s->end   = (s->start + seg < sz) ? s->start + seg : sz;
    if (s->start >= s->end) { free(s); continue; }
    if (pthread_create(&th[spawned], NULL, disk_prefetch_seg, s) == 0) spawned++;
    else { disk_prefetch_seg(s); }   // fall back to inline on spawn failure
  }
  for (int i = 0; i < spawned; i++) pthread_join(th[i], NULL);

  if (getenv("THVM_PREFETCH_STATS") != NULL) {
    const char *base = strrchr(a->path, '/');
    fprintf(stderr, "thvm: prefetch done %s -- %llu MB in %llu ms (%d threads)\n",
            base ? base + 1 : a->path, (unsigned long long)(sz >> 20),
            (unsigned long long)((cg_now_us() - t0) / 1000), spawned);
  }
  free(a);
  return NULL;
}

fn void thvm_file_prefetch_async(const char *path) {
  if (path == NULL || path[0] == '\0') return;
  DiskPrefetchArg *a = (DiskPrefetchArg *)malloc(sizeof(*a));
  if (a == NULL) return;
  size_t len = strlen(path);
  if (len >= sizeof(a->path)) { free(a); return; }   // path too long: skip
  memcpy(a->path, path, len + 1);
  pthread_t th;
  if (pthread_create(&th, NULL, disk_prefetch_main, a) != 0) { free(a); return; }
  pthread_detach(th);
}

// True iff `cpu_buf_id` is a disk-mmap CpuBuf (its on_release is
// disk_map_release).  Used by materialize_copy to decide whether a
// CPU->Metal COPY can be served by a zero-copy newBufferWithBytesNoCopy
// wrap of the underlying mmap instead of a staged upload.
fn int thvm_disk_buf_is_mapped(u32 cpu_buf_id) {
  if (CPU_BUFS == NULL || cpu_buf_id == 0 || cpu_buf_id >= CPU_BUFS_NEXT)
    return 0;
  return CPU_BUFS[cpu_buf_id].on_release == disk_map_release;
}

// For a disk-mmap CpuBuf, return its page-aligned mmap base + length and the
// within-map byte offset (minor) at which the buffer's bytes start (data -
// base).  Returns 0 (not a disk mapping / bad handle) without touching the
// out params, 1 on success.  materialize_copy wraps [base, base+maplen) as a
// borrowed MTLBuffer and threads `minor` into the dst view.offset so the GPU
// reads land on the weight's first byte.
fn int thvm_disk_buf_map_info(u32 cpu_buf_id, void **base_out, u64 *maplen_out,
                              u64 *minor_out) {
  if (!thvm_disk_buf_is_mapped(cpu_buf_id)) return 0;
  CpuBuf  *b = &CPU_BUFS[cpu_buf_id];
  DiskMap *m = (DiskMap *)b->handle;
  if (m == NULL || m->base == NULL || m->base == MAP_FAILED || m->maplen == 0)
    return 0;
  u64 minor = (u64)((char *)b->data - (char *)m->base);
  if (minor >= m->maplen) return 0;                // malformed: data outside map
  if (base_out)   *base_out   = m->base;
  if (maplen_out) *maplen_out = m->maplen;
  if (minor_out)  *minor_out  = minor;
  return 1;
}

// Map [byte_offset, byte_offset + nbytes) of `path` read-only and wrap it
// as a CPU TAG_TEN of the given dtype + shape.  Returns 0 (an invalid
// Term, all-zero) on any failure; callers must check.  The mapping is
// released (munmap) when the tensor's buffer refcount drops to zero, via
// the cpu_buf_free on_release hook.
fn Term thvm_tensor_mmap(const char *path, u64 byte_offset, u64 nbytes,
                         u32 dtype, u32 ndim, const u32 *dims) {
  if (path == NULL || nbytes == 0u || ndim > MAX_DIM) return 0;

  // Sanity: the logical element count must match the requested byte span
  // for the given dtype, so a malformed header can't hand us a view that
  // reads past the bytes it claims.
  u64 numel = 1u;
  for (u32 i = 0u; i < ndim; i++) numel *= (u64)dims[i];
  if (dtype_storage_bytes(dtype, numel) != nbytes) {
    fprintf(stderr,
      "tensor_mmap: byte span %llu != dtype %s * numel %llu = %llu\n",
      (unsigned long long)nbytes, dtype_name(dtype),
      (unsigned long long)numel,
      (unsigned long long)dtype_storage_bytes(dtype, numel));
    return 0;
  }

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "tensor_mmap: open(%s) failed\n", path);
    return 0;
  }

  // The file must actually contain [byte_offset, byte_offset + nbytes).
  struct stat st;
  if (fstat(fd, &st) != 0) {
    fprintf(stderr, "tensor_mmap: fstat(%s) failed\n", path);
    close(fd);
    return 0;
  }
  if ((u64)st.st_size < byte_offset + nbytes) {
    fprintf(stderr,
      "tensor_mmap: file %s is %llu bytes; need %llu (offset %llu + %llu)\n",
      path, (unsigned long long)st.st_size,
      (unsigned long long)(byte_offset + nbytes),
      (unsigned long long)byte_offset, (unsigned long long)nbytes);
    close(fd);
    return 0;
  }

  // Page-align the offset: mmap demands a page-multiple offset, so map
  // from the page-aligned base and point the buffer at base + minor.
  long pgl = sysconf(_SC_PAGESIZE);
  u64  page = (pgl > 0) ? (u64)pgl : 4096u;
  u64  minor = byte_offset % page;
  u64  map_off = byte_offset - minor;
  u64  map_len = nbytes + minor;

  // Read-only + private: PROT_READ keeps the file immutable through the
  // view; MAP_PRIVATE means any (disallowed) write would copy-on-write
  // rather than scribble the file.  Lazy paging is automatic.
  void *base = mmap(NULL, (size_t)map_len, PROT_READ, MAP_PRIVATE,
                    fd, (off_t)map_off);
  // The fd may be closed immediately: the mapping keeps its own reference
  // to the underlying file object (POSIX mmap semantics).
  close(fd);
  if (base == MAP_FAILED) {
    fprintf(stderr, "tensor_mmap: mmap(%s) failed\n", path);
    return 0;
  }
  // Prefetch: a disk tensor is mapped only to be read once (upload to a
  // device, or a CPU op).  Without a hint the first read faults pages in
  // one-by-one off the SSD in access order -- for a multi-GB weight file
  // mapped as ~hundreds of scattered tensors that is ~0.8 GB/s of random
  // faults.  MADV_WILLNEED kicks off async sequential readahead per region
  // so the bytes are resident (near SSD sequential bandwidth) by the time
  // the upload reads them.  Best-effort: ignore the return (an unsupported
  // hint just leaves the lazy-fault behaviour unchanged).
  //
  // THVM_MMAP_NO_WILLNEED disables it: TSafeTensorLoad maps EVERY tensor in
  // a file up front, so WILLNEED faults the WHOLE multi-GB file resident at
  // load time (the Qwen3-4B encoder pages ~8GB before a single weight is
  // uploaded).  Paired with the upload-time MADV_DONTNEED (materialize_copy),
  // leaving WILLNEED off keeps the host working set to roughly one weight at
  // a time -- each weight faults in only when the upload reads it, then its
  // pages are dropped -- trading a little cold-start readahead for a much
  // smaller peak RSS on a memory-bound, upload-everything-once forward.
  static int no_willneed = -1;
  if (no_willneed < 0) {
    const char *e = getenv("THVM_MMAP_NO_WILLNEED");
    no_willneed = (e != NULL && e[0] != '0') ? 1 : 0;
  }
  if (!no_willneed) madvise(base, (size_t)map_len, MADV_WILLNEED);

  void *buf = (void *)((char *)base + minor);

  DiskMap *m = (DiskMap *)malloc(sizeof(DiskMap));
  if (m == NULL) {
    munmap(base, (size_t)map_len);
    return 0;
  }
  m->base   = base;
  m->maplen = map_len;

  if (TENS_NEXT >= TENS_CAP) {
    fprintf(stderr, "tensor_mmap: out of descriptor slots\n");
    disk_map_release(m);
    return 0;
  }
  u32 id = TENS_NEXT++;
  TenDesc *d = &TENS[id];
  Shape shape;
  shape.ndim = ndim;
  for (u32 i = 0u; i < ndim && i < MAX_DIM; i++) shape.dims[i] = dims[i];
  for (u32 i = ndim; i < MAX_DIM; i++)           shape.dims[i] = 0u;
  d->dtype         = dtype;
  d->refcount      = 1;
  d->view          = view_create(shape);
  d->prior_views   = NULL;
  d->nviews        = 0;
  d->requires_grad = 0;
  d->grad          = 0;
  d->assign_kvar_id = 0;
  // The disk tensor is always CPU-resident: a CPU op reads the mapped
  // bytes directly; a non-CPU realize routes through UOP_COPY, which
  // uploads.  Mirror tensor_from_na_host's force-CPU residency.
  d->backend       = &CPU_BACKEND;
  d->producer_kid  = 0;
  // External buffer: munmap (not free) on release, via disk_map_release.
  d->buf_id        = cpu_buf_alloc_external(buf, nbytes, disk_map_release, m);

  return term_new(0, TAG_TEN, dtype, id);
}
