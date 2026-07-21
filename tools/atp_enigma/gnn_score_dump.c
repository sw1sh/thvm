// ENIGMA Tier-2 offline parity harness for the in-engine GCN scorer.
//
// Reads a JSONL graph file (the THVM_ATP_ENIGMA_GRAPH_EXPORT format) and a
// .safetensors GCN model, scores each graph with the EXACT in-engine
// forward (thvm_atp_gnn_score_graphs, which runs the same UOP graph the
// live re-rank hook uses), and prints one score per line:
//
//   bin/gnn_score_dump <graphs.jsonl> <model.safetensors>
//     -> "<i>\t<score>" per graph on stdout, "OK <n>" on stderr
//
// The Python trainer (train_tier2_gnn.py parity) runs the SAME math on the
// SAME graphs + weights and asserts the scores agree to float tolerance,
// pinning the trainer's forward to the engine's -- so a model trained in
// Python and dropped in via THVM_ATP_GNN_ASSET scores identically online.
//
// CPU-only: the GCN forward runs in thvm's dedicated "cpu" sandbox context.

#include "../../src/thvm.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Parse one JSONL line of the rigid export format into an AtpCpGraph:
//   {"y":<int>,"x":[[f,f,f,f,f,f],...],"e":[[i,i],...]}
// Only n_nodes / node_feat / n_edges / edge_src / edge_dst are filled --
// the forward reads nothing else (node_type / node_label are unused).
// Returns 1 on success, 0 on a malformed line or a cap overflow.
static int parse_graph_line(const char *line, AtpCpGraph *g) {
  memset(g, 0, sizeof *g);
  const char *p = strstr(line, "\"x\":[");
  if (p == NULL) return 0;
  p += 5;                                   // at first row '[' (or ']' if empty)
  u32 nn = 0u;
  while (*p == '[') {
    if (nn >= ATP_CPG_MAX_NODES) return 0;
    p++;                                    // skip '['
    for (u32 fj = 0u; fj < ATP_CPG_FEAT_DIM; fj++) {
      char *end;
      double v = strtod(p, &end);
      if (end == p) return 0;
      g->node_feat[(size_t)nn * ATP_CPG_FEAT_DIM + fj] = (float)v;
      p = end;
      if (*p == ',') p++;
    }
    if (*p != ']') return 0;
    p++;                                    // skip ']'
    nn++;
    if (*p == ',') p++;                     // separator before the next row
  }
  g->n_nodes = nn;

  p = strstr(p, "\"e\":[");
  if (p == NULL) return 0;
  p += 5;
  u32 ne = 0u;
  while (*p == '[') {
    if (ne >= ATP_CPG_MAX_EDGES) return 0;
    p++;
    char *end;
    long s = strtol(p, &end, 10); if (end == p) return 0; p = end;
    if (*p == ',') p++;
    long d = strtol(p, &end, 10); if (end == p) return 0; p = end;
    if (*p != ']') return 0;
    p++;
    g->edge_src[ne] = (u32)s;
    g->edge_dst[ne] = (u32)d;
    ne++;
    if (*p == ',') p++;
  }
  g->n_edges = ne;
  return 1;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s <graphs.jsonl> <model.safetensors>\n", argv[0]);
    return 2;
  }
  thvm_init();
  if (!thvm_atp_load_gnn_safetensors(argv[2])) {
    fprintf(stderr, "failed to load model %s\n", argv[2]);
    return 1;
  }
  FILE *f = fopen(argv[1], "r");
  if (f == NULL) { fprintf(stderr, "failed to open %s\n", argv[1]); return 1; }

  size_t cap = 64u, n = 0u;
  AtpCpGraph *gs = (AtpCpGraph *)malloc(cap * sizeof *gs);
  char *line = NULL; size_t lcap = 0u; ssize_t len;
  while ((len = getline(&line, &lcap, f)) > 0) {
    if (len < 2) continue;                          // blank line
    if (n >= cap) { cap *= 2u; gs = (AtpCpGraph *)realloc(gs, cap * sizeof *gs); }
    if (!parse_graph_line(line, &gs[n])) {
      fprintf(stderr, "parse error at line %zu\n", n + 1u);
      return 1;
    }
    n++;
  }
  fclose(f);
  free(line);
  if (n == 0u) { fprintf(stderr, "no graphs in %s\n", argv[1]); return 1; }

  float *sc = (float *)malloc(n * sizeof(float));
  if (!thvm_atp_gnn_score_graphs(gs, (u32)n, sc)) {
    fprintf(stderr, "scoring failed\n");
    return 1;
  }
  for (size_t i = 0u; i < n; i++) printf("%zu\t%.9g\n", i, (double)sc[i]);
  fprintf(stderr, "OK %zu\n", n);

  free(sc);
  free(gs);
  thvm_free();
  return 0;
}
