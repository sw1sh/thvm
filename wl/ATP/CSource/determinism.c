// determinism.c - run the WolframInstitute`ATP` package's completions and
// normal-form queries through libthvm_atp's own entries, without a Wolfram
// kernel, and print each completed system and the normal forms in prefix
// form.  The same case file must print the same output on every platform.
//
//   bin/atp_lib_determinism cases.txt > out.txt
//
// The case file comes from the package (WolframInstitute`ATP`Private`
// determinismCase), so the run arguments are exactly the ones it sends.  The
// LibraryLink table below serves only what the bridge's entries use.

#include "libthvm_atp.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void fake_init(void) {
  g_naf.MNumericArray_new                = fake_na_new;
  g_naf.MNumericArray_free               = fake_na_free;
  g_naf.MNumericArray_getType            = fake_na_type;
  g_naf.MNumericArray_getFlattenedLength = fake_na_len;
  g_naf.MNumericArray_getData            = fake_na_data;
  g_lib.numericarrayLibraryFunctions     = &g_naf;
  g_lib.MTensor_getFlattenedLength       = fake_t_len;
  g_lib.MTensor_getIntegerData           = fake_t_int;
  g_lib.AbortQ                           = fake_abort;
}

// --- reading the case file ---------------------------------------------

static FILE *g_in;

static int next_token(char *buf, size_t cap) {
  return fscanf(g_in, "%255s", buf) == 1 && cap > 0;
}

static int64_t next_int(void) {
  char buf[256];
  if (!next_token(buf, sizeof buf)) {
    fprintf(stderr, "determinism: unexpected end of input\n");
    exit(2);
  }
  return strtoll(buf, NULL, 10);
}

static int64_t *read_ints(mint n) {
  int64_t *v = (int64_t *)calloc((size_t)(n > 0 ? n : 1), sizeof(int64_t));
  for (mint i = 0; i < n; i++) v[i] = next_int();
  return v;
}

// --- output ------------------------------------------------------------

static void print_prefix(const int64_t *d, mint n) {
  for (mint i = 0; i < n; i++) printf(" %lld", (long long)d[i]);
}

// The heap Terms of `terms` in prefix form, one line per term after `tag`.
static void print_terms(const char *tag, const int64_t *terms, mint n) {
  AtpPrefixBuf b = {NULL, 0, 0};
  for (mint i = 0; i < n; i++) {
    b.n = 0;
    atp_prefix_write(&b, (Term)terms[i], 0u);
    printf("%s", tag);
    print_prefix(b.d, b.n);
    printf("\n");
  }
  free(b.d);
}

static int64_t *build_terms(const int64_t *pre, mint len, mint count) {
  int64_t *out = (int64_t *)calloc((size_t)(count > 0 ? count : 1), sizeof(int64_t));
  mint pos = 0;
  for (mint i = 0; i < count; i++) {
    Term t = 0;
    if (!atp_prefix_build(pre, len, &pos, &t, 0u)) {
      fprintf(stderr, "determinism: bad prefix\n");
      exit(2);
    }
    out[i] = (int64_t)t;
  }
  return out;
}

int main(int argc, char **argv) {
  if (argc != 2 || (g_in = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "usage: atp_lib_determinism cases.txt\n");
    return 2;
  }
  setenv("THVM_HEAP_CELLS", "536870912", 0);
  fake_init();
  thvm_init();
  char word[256], name[256];
  mint id = 0;
  while (next_token(word, sizeof word)) {
    if (strcmp(word, "case") != 0) continue;
    next_token(name, sizeof name);
    id++;
    mint alloc = 0;
    // args: 66 values, "W" the wire, "A n v..." an Integer array, a value
    // with a '.' the Real.
    mint     ints[66];
    mreal    real = 0.0;
    MTensor  tensors[66];
    int      kind[66];
    memset(tensors, 0, sizeof tensors);
    while (next_token(word, sizeof word) && strcmp(word, "args") != 0) {
      if (strcmp(word, "alloc") == 0) alloc = (mint)next_int();
    }
    for (int k = 0; k < 66; k++) {
      next_token(word, sizeof word);
      if (strcmp(word, "W") == 0) {
        kind[k] = 0;
      } else if (strcmp(word, "A") == 0) {
        mint n = (mint)next_int();
        tensors[k] = fake_array(MNumericArray_Type_Bit64, n);
        int64_t *v = read_ints(n);
        memcpy(tensors[k]->data, v, (size_t)n * sizeof(int64_t));
        free(v);
        kind[k] = 2;
      } else if (strchr(word, '.') != NULL) {
        real = strtod(word, NULL);
        kind[k] = 3;
      } else {
        ints[k] = (mint)strtoll(word, NULL, 10);
        kind[k] = 1;
      }
    }
    next_token(word, sizeof word);                 // "axioms"
    mint n_ax = (mint)next_int(), ax_len = (mint)next_int();
    int64_t *ax_pre = read_ints(ax_len);
    next_token(word, sizeof word);                 // "queries"
    mint n_q = (mint)next_int(), q_len = (mint)next_int();
    int64_t *q_pre = read_ints(q_len);

    // The wire: [n_goals = 0, n_axioms, lhs_0, rhs_0, ..., flags].
    int64_t *ax_terms = build_terms(ax_pre, ax_len, 2 * n_ax);
    struct st_MNumericArray *wire = fake_array(MNumericArray_Type_Bit64, 2 + 3 * n_ax);
    int64_t *w = (int64_t *)wire->data;
    w[0] = 0;
    w[1] = n_ax;
    for (mint i = 0; i < 2 * n_ax; i++) w[2 + i] = ax_terms[i];

    MArgument args[66];
    MNumericArray wire_ref = wire;
    for (int k = 0; k < 66; k++) {
      switch (kind[k]) {
        case 0: args[k].numeric = &wire_ref; break;
        case 1: args[k].integer = &ints[k]; break;
        case 2: args[k].tensor  = &tensors[k]; break;
        default: args[k].real   = &real; break;
      }
    }
    mint keep_args[2] = {id, alloc};
    MArgument nr_args[2], nr_res;
    mint nr_out = 0;
    nr_args[0].integer = &keep_args[0];
    nr_args[1].integer = &keep_args[1];
    nr_res.integer = &nr_out;
    thvm_wl_atp_next_run(&g_lib, 2, nr_args, nr_res);

    MNumericArray out = NULL;
    MArgument res;
    res.numeric = &out;
    int rc = thvm_wl_atp_run_proof(&g_lib, 66, args, res);
    printf("case %s\n", name);
    if (rc != LIBRARY_NO_ERROR || out == NULL) {
      printf("failed %d\n", rc);
      continue;
    }
    const int64_t *raw = (const int64_t *)out->data;
    mint n_rules = (mint)raw[1];
    printf("status %lld rules %lld\n", (long long)raw[0], (long long)n_rules);
    // Rule kinds end the wire.
    for (mint r = 0; r < n_rules; r++) {
      printf("kind %lld\n", (long long)raw[out->n - n_rules + r]);
      print_terms(" side", &raw[8 + 2 * r], 2);
    }

    // Queries through the kept system.
    int64_t *q_terms = build_terms(q_pre, q_len, n_q);
    struct st_MNumericArray *qa = fake_array(MNumericArray_Type_Bit64, n_q);
    memcpy(qa->data, q_terms, (size_t)n_q * sizeof(int64_t));
    MNumericArray qa_ref = qa, nf = NULL;
    mint sys_id = id;
    MArgument q_args[2], q_res;
    q_args[0].integer = &sys_id;
    q_args[1].numeric = &qa_ref;
    q_res.numeric = &nf;
    if (thvm_wl_atp_system_normalize(&g_lib, 2, q_args, q_res) == LIBRARY_NO_ERROR && nf != NULL) {
      print_terms("nf", (const int64_t *)nf->data, nf->n);
    } else {
      printf("nf failed\n");
    }
    atp_sys_manager(&g_lib, 1, id);
    free(ax_pre); free(q_pre); free(ax_terms); free(q_terms);
  }
  return 0;
}
