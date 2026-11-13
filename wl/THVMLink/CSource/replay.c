// replay.c - replay one captured thvm_wl_atp_run_proof call through THVMLink's own entry,
// without a Wolfram kernel, and time it: exactly the engine work TFindProof does.
//
//   CAP_OUT=case.txt CAP_THM=OrAssociativity wolframscript -f wl/scripts/capture_run_proof.wls
//   make bin/atp_replay
//   THVM_ATP_FLATTERM=1 THVM_ATP_KBO_FLAT=1 THVM_ATP_CP_INDEX=1 THVM_ATP_TRACE_MAX=4194304 \
//     bin/atp_replay case.txt
//
// The environment is the one ATP.wl sets at load.  The output line gives the status, the rule
// count, the wire length, an FNV-1a hash of the whole wire (equal hashes mean byte-identical
// proof output) and the time.  The LibraryLink table serves only what run_proof uses, as
// wl/ATP/CSource/determinism.c does.

#include "wl/THVMLink/CSource/thvmlink.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct st_MNumericArray {
  numericarray_data_t type;
  mint                n;
  void               *data;
};

static struct st_MNumericArray *fake_array(numericarray_data_t type, mint n) {
  struct st_MNumericArray *a = (struct st_MNumericArray *)calloc(1, sizeof *a);
  a->type = type;
  a->n    = n;
  a->data = calloc((size_t)(n > 0 ? n : 1), 8);
  return a;
}
static errcode_t fake_na_new(const numericarray_data_t type, const mint rank, const mint *dims, MNumericArray *out) {
  mint n = 1;
  for (mint i = 0; i < rank; i++) n *= dims[i];
  *out = fake_array(type, n);
  return 0;
}
static void fake_na_free(MNumericArray a) {
  if (a == NULL) return;
  free(a->data);
  free(a);
}
static numericarray_data_t fake_na_type(const MNumericArray a) { return a->type; }
static mint fake_na_len(const MNumericArray a) { return a->n; }
static void *fake_na_data(const MNumericArray a) { return a->data; }
static mint fake_t_len(const MTensor t) { return t->n; }
static mint *fake_t_int(const MTensor t) { return (mint *)t->data; }
static mint fake_abort(void) { return 0; }

static struct st_WolframNumericArrayLibrary_Functions g_naf;
static struct st_WolframLibraryData g_lib;

static FILE *g_in;
static int tok(char *buf) { return fscanf(g_in, "%255s", buf) == 1; }
static int64_t next_int(void) {
  char buf[256];
  if (!tok(buf)) { fprintf(stderr, "replay: unexpected end of input\n"); exit(2); }
  return strtoll(buf, NULL, 10);
}

static int build(const int64_t *d, mint n, mint *pos, Term *out, u32 depth) {
  if (*pos >= n || depth > 100000u) return 0;
  int64_t v = d[(*pos)++];
  if (v < 0) { *out = term_new_fvr((u32)(-v - 1)); return 1; }
  if (v == 0 || *pos >= n) return 0;
  int64_t ar = d[(*pos)++];
  Term cs[64];
  if (ar < 0 || ar > 64) return 0;
  for (int64_t k = 0; k < ar; k++) {
    if (!build(d, n, pos, &cs[k], depth + 1u)) return 0;
  }
  *out = term_new_ctr((u32)v, cs, (u32)ar);
  return 1;
}

static double now_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

int main(int argc, char **argv) {
  if (argc != 2 || (g_in = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "usage: replay case.txt\n");
    return 2;
  }
  setenv("THVM_HEAP_CELLS", "536870912", 0);
  g_naf.MNumericArray_new                = fake_na_new;
  g_naf.MNumericArray_free               = fake_na_free;
  g_naf.MNumericArray_getType            = fake_na_type;
  g_naf.MNumericArray_getFlattenedLength = fake_na_len;
  g_naf.MNumericArray_getData            = fake_na_data;
  g_lib.numericarrayLibraryFunctions     = &g_naf;
  g_lib.MTensor_getFlattenedLength       = fake_t_len;
  g_lib.MTensor_getIntegerData           = fake_t_int;
  g_lib.AbortQ                           = fake_abort;
  thvm_init();

  char word[256], name[256];
  while (tok(word) && strcmp(word, "case") != 0) {}
  tok(name);
  tok(word);  // "args"
  mint    ints[66];
  mreal   real = 0.0;
  MTensor tensors[66];
  int     kind[66];
  memset(tensors, 0, sizeof tensors);
  for (int k = 0; k < 66; k++) {
    tok(word);
    if (k == 0) {
      kind[k] = 0;
    } else if (strcmp(word, "A") == 0) {
      mint n = (mint)next_int();
      tensors[k] = fake_array(MNumericArray_Type_Bit64, n);
      for (mint i = 0; i < n; i++) ((int64_t *)tensors[k]->data)[i] = next_int();
      kind[k] = 2;
    } else if (strchr(word, '.') != NULL) {
      real = strtod(word, NULL);
      kind[k] = 3;
    } else {
      ints[k] = (mint)strtoll(word, NULL, 10);
      kind[k] = 1;
    }
  }
  tok(word);  // "wire"
  int64_t n_g = next_int(), n_ax = next_int(), n_fl = next_int();
  int64_t *flags = (int64_t *)calloc((size_t)(n_fl + 1), sizeof(int64_t));
  for (int64_t i = 0; i < n_fl; i++) flags[i] = next_int();
  tok(word);  // "terms"
  mint len = (mint)next_int();
  int64_t *pre = (int64_t *)calloc((size_t)len, sizeof(int64_t));
  for (mint i = 0; i < len; i++) pre[i] = next_int();

  mint n_terms = (mint)pre[0], pos = 1;
  struct st_MNumericArray *wire = fake_array(MNumericArray_Type_Bit64, 2 + n_terms + n_fl);
  int64_t *w = (int64_t *)wire->data;
  w[0] = n_g;
  w[1] = n_ax;
  for (mint i = 0; i < n_terms; i++) {
    Term t = 0;
    if (!build(pre, len, &pos, &t, 0u)) { fprintf(stderr, "replay: bad prefix\n"); return 2; }
    w[2 + i] = (int64_t)t;
  }
  for (int64_t i = 0; i < n_fl; i++) w[2 + n_terms + i] = flags[i];

  MArgument     args[66];
  MNumericArray wire_ref = wire;
  for (int k = 0; k < 66; k++) {
    switch (kind[k]) {
      case 0: args[k].numeric = &wire_ref; break;
      case 1: args[k].integer = &ints[k]; break;
      case 2: args[k].tensor  = &tensors[k]; break;
      default: args[k].real   = &real; break;
    }
  }
  MNumericArray out = NULL;
  MArgument     res;
  res.numeric = &out;
  double t0 = now_s();
  int rc = thvm_wl_atp_run_proof(&g_lib, 66, args, res);
  double t1 = now_s();
  if (rc != LIBRARY_NO_ERROR || out == NULL) {
    printf("REPLAY %s failed rc=%d time=%.3f\n", name, rc, t1 - t0);
    return 1;
  }
  const int64_t *raw = (const int64_t *)out->data;
  // FNV-1a over the whole wire: equal hashes mean byte-identical proof output.
  uint64_t h = 1469598103934665603ull;
  for (mint i = 0; i < out->n; i++) {
    h ^= (uint64_t)raw[i];
    h *= 1099511628211ull;
  }
  printf("REPLAY %s status=%lld rules=%lld wire=%lld hash=%016llx time=%.3f\n", name,
         (long long)raw[0], (long long)raw[1], (long long)out->n, (unsigned long long)h, t1 - t0);
  return 0;
}
