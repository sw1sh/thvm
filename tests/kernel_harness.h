// kernel_harness.h - run a hand-built kernel root through both CPU backends.
//
// A root (one STORE or an AFTER chain of STOREs) over UOP_BUFFER leaves is
// rendered through the production C entry (cg_render_uop_kernel_c_root),
// compiled with the JIT's C compiler and run, and walked by cpu_uop_walk.
// kh_check requires both results to equal a host reference byte for byte.
// The output is pre-filled with 0xA5 bytes, so an element a kernel fails
// to write shows up.  Include after test.h.

#ifndef THVM_KERNEL_HARNESS_H
#define THVM_KERNEL_HARNESS_H

#include <dlfcn.h>

#define KH_MAX_IN 4

typedef struct {
  u32         dtype;
  u32         numel;
  void const *data;
} KhIn;

typedef struct {
  Term buf[KH_MAX_IN];
  KhIn in[KH_MAX_IN];
  u32  n_in;
  Term out;
  u32  out_dtype;
  u32  out_numel;
} KhKernel;

static void kh_init(KhKernel *k, u32 out_dtype, u32 out_numel) {
  memset(k, 0, sizeof *k);
  u32 dims[1] = { out_numel };
  k->out       = uop_buffer_inst(UOP_SCOPE_GLOBAL, out_dtype, 1, dims, 0);
  k->out_dtype = out_dtype;
  k->out_numel = out_numel;
}

// Next input slot (instance slot + 1), a 1-D buffer of `numel` elements.
static Term kh_input(KhKernel *k, u32 dtype, u32 numel, void const *data) {
  u32 slot = k->n_in++;
  u32 dims[1] = { numel };
  k->buf[slot] = uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, 1, dims, slot + 1);
  k->in[slot].dtype = dtype;
  k->in[slot].numel = numel;
  k->in[slot].data  = data;
  return k->buf[slot];
}

static u32 kh_out_bytes(KhKernel const *k) {
  return k->out_numel * dtype_itemsize(k->out_dtype);
}

static Term kh_f32(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return uop_const(DT_FP32, b);
}

static Term kh_i32(i32 v) {
  return uop_const(DT_INT32, (u32)v);
}

// Render `root`, compile it with the JIT's C compiler and run it into `res`.
static int kh_run_rendered(Term root, KhKernel const *k, u8 *res) {
  char  *src = NULL;
  size_t src_n = 0;
  FILE  *fp = open_memstream(&src, &src_n);
  if (fp == NULL) {
    return 0;
  }
  cg_render_uop_kernel_c_root(root, "k_harness", fp);
  fclose(fp);
  char const *tmpdir = getenv("TMPDIR");
  char path[512];
  snprintf(path, sizeof path, "%s/thvm_kernel_harness_XXXXXX.c",
           (tmpdir != NULL && tmpdir[0] != '\0') ? tmpdir : ".");
  int fd = mkstemps(path, 2);
  if (fd < 0) {
    free(src);
    return 0;
  }
  FILE *cf = fdopen(fd, "w");
  fputs(src, cf);
  fclose(cf);
  char so[600];
  snprintf(so, sizeof so, "%s.so", path);
  char cmd[1400];
  snprintf(cmd, sizeof cmd, "%s -O1 -shared -fPIC -o '%s' '%s' 2>/dev/null",
           cpu_cc(), so, path);
  void *h = (system(cmd) == 0) ? dlopen(so, RTLD_NOW) : NULL;
  typedef void (*kfn)(void *, const void *const *, unsigned,
                      const unsigned *, const unsigned *);
  kfn kf = NULL;
  if (h != NULL) {
    void *sym = dlsym(h, "k_harness");
    memcpy(&kf, &sym, sizeof kf);
  }
  if (kf != NULL) {
    const void *ins[KH_MAX_IN] = {0};
    unsigned    numel[KH_MAX_IN] = {0};
    for (u32 i = 0; i < k->n_in; i++) {
      ins[i]   = k->in[i].data;
      numel[i] = k->in[i].numel;
    }
    memset(res, 0xA5, kh_out_bytes(k));
    kf(res, ins, k->out_numel, numel, NULL);
  } else {
    fprintf(stderr, "  render/compile failed:\n%s\n", src);
  }
  if (h != NULL) {
    dlclose(h);
  }
  unlink(path);
  unlink(so);
  free(src);
  return kf != NULL;
}

static u32 kh_tensor(u32 dtype, u32 numel, void const *data) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = numel;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, dtype);
  if (data != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data,
                          (u64)numel * dtype_itemsize(dtype));
  }
  return tid;
}

// Walk `root` with cpu_uop_walk into `res`.  Returns the walker's verdict.
static int kh_run_walker(Term root, KhKernel const *k, u8 *res) {
  KernelEntry ke;
  memset(&ke, 0, sizeof ke);
  u32  tids[KH_MAX_IN] = {0};
  u32  in_buf_ids[KH_MAX_IN] = {0};
  Term lift[KH_MAX_IN] = {0};
  for (u32 i = 0; i < k->n_in; i++) {
    in_buf_ids[i] = TENS[kh_tensor(k->in[i].dtype, k->in[i].numel,
                                   k->in[i].data)].buf_id;
    lift[i] = k->buf[i];
  }
  ke.cached_lift.in_bufs    = lift;
  ke.cached_lift.in_cap     = KH_MAX_IN;
  u32 nb = kh_out_bytes(k);
  u8 *fill = (u8 *)malloc(nb);
  memset(fill, 0xA5, nb);
  u32 tout = kh_tensor(k->out_dtype, k->out_numel, fill);
  free(fill);
  ke.n_inputs               = k->n_in;
  ke.input_tids             = tids;
  ke.output_dtype           = k->out_dtype;
  ke.cached_lift.store_root = root;
  ke.cached_lift.out_buf    = k->out;
  ke.cached_lift.n_inputs   = k->n_in;
  int ok = cpu_uop_walk(&ke, in_buf_ids, TENS[tout].buf_id);
  CPU_BACKEND.buf_read(TENS[tout].buf_id, res, nb);
  return ok;
}

static void kh_report(u8 const *got, void const *want, KhKernel const *k,
                      char const *what) {
  u32 esz = dtype_itemsize(k->out_dtype);
  u32 shown = 0;
  for (u32 i = 0; i < k->out_numel && shown < 16; i++) {
    u8 const *g = got + (size_t)i * esz;
    u8 const *w = (u8 const *)want + (size_t)i * esz;
    if (memcmp(g, w, esz) == 0) {
      continue;
    }
    shown++;
    if (k->out_dtype == DT_FP32) {
      f32 gv, wv;
      memcpy(&gv, g, 4);
      memcpy(&wv, w, 4);
      fprintf(stderr, "  %s: out[%u] got=%.9g want=%.9g\n", what, i, gv, wv);
    } else if (k->out_dtype == DT_FP64) {
      f64 gv, wv;
      memcpy(&gv, g, 8);
      memcpy(&wv, w, 8);
      fprintf(stderr, "  %s: out[%u] got=%.17g want=%.17g\n", what, i, gv, wv);
    } else {
      i32 gv, wv;
      memcpy(&gv, g, 4);
      memcpy(&wv, w, 4);
      fprintf(stderr, "  %s: out[%u] got=%d (0x%08x) want=%d (0x%08x)\n",
              what, i, gv, (u32)gv, wv, (u32)wv);
    }
  }
}

// The rendered C and the walker must both equal `want` byte for byte.
static void kh_check(Term root, KhKernel const *k, void const *want,
                     char const *what) {
  u32 nb  = kh_out_bytes(k);
  u8 *got = (u8 *)malloc(nb);
  char label[96];
  int rendered = kh_run_rendered(root, k, got);
  CHECK(rendered);
  snprintf(label, sizeof label, "%s rendered", what);
  if (rendered) {
    kh_report(got, want, k, label);
  }
  CHECK(rendered && memcmp(got, want, nb) == 0);
  int walked = kh_run_walker(root, k, got);
  CHECK(walked);
  snprintf(label, sizeof label, "%s walker", what);
  if (walked) {
    kh_report(got, want, k, label);
  }
  CHECK(walked && memcmp(got, want, nb) == 0);
  free(got);
}

#endif // THVM_KERNEL_HARNESS_H
