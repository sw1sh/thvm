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
  // The disk tensor is always CPU-resident: a CPU op reads the mapped
  // bytes directly; a non-CPU realize routes through UOP_COPY, which
  // uploads.  Mirror tensor_from_na_host's force-CPU residency.
  d->backend       = &CPU_BACKEND;
  d->producer_kid  = 0;
  // External buffer: munmap (not free) on release, via disk_map_release.
  d->buf_id        = cpu_buf_alloc_external(buf, nbytes, disk_map_release, m);

  return term_new(0, TAG_TEN, dtype, id);
}
