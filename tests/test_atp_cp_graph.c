// test_atp_cp_graph.c - anonymised CP hypergraph extractor (ENIGMA Tier 2).
//
// Exercises thvm_atp_cp_graph, which turns a critical pair (lhs, rhs)
// into a typed hypergraph whose node features are PURELY STRUCTURAL --
// node kind / arity / occurrence count, NEVER the concrete symbol label,
// variable id, or numeric value.  The headline test is renaming
// invariance: two CPs equal up to a consistent bijection of symbols +
// variables produce bit-identical graphs.  A non-isomorphic pair is
// checked to differ so the invariance is not vacuous.

#include "../src/thvm.c"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// First CP's labels.
#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define VAR_x 0u

// A bijection of the first CP's labels + variable for the invariance
// test: f->g, i->j, e->d, x:0 -> x:5.  Chosen to NOT overlap the
// originals, so a leak of any concrete id into node_feat would show up.
#define LAB_d 11u   // image of e
#define LAB_j 12u   // image of i
#define LAB_g 13u   // image of f
#define VAR_y 5u    // image of x

static Term ctr0(u32 l)                 { return term_new_ctr(l, NULL, 0); }
static Term ctr1(u32 l, Term a)         { Term cs[1] = {a};    return term_new_ctr(l, cs, 1); }
static Term ctr2(u32 l, Term a, Term b) { Term cs[2] = {a, b}; return term_new_ctr(l, cs, 2); }

// Count nodes / edges of a given type.
static u32 count_nodes(const AtpCpGraph *g, u8 type) {
  u32 c = 0u;
  for (u32 i = 0; i < g->n_nodes; i++) if (g->node_type[i] == type) c++;
  return c;
}
static u32 count_edges(const AtpCpGraph *g, u8 type) {
  u32 c = 0u;
  for (u32 i = 0; i < g->n_edges; i++) if (g->edge_type[i] == type) c++;
  return c;
}

// Feature column `col` of node `node`.
static float feat_at(const AtpCpGraph *g, u32 node, u32 col) {
  return g->node_feat[(size_t)node * ATP_CPG_FEAT_DIM + col];
}

int main(void) {
  thvm_init();

  // === Schema on a hand-built CP: f(x, i(x)) = e =====================
  TEST_BEGIN("atp/cp_graph/schema-on-group-cp");
  {
    Term lhs = ctr2(LAB_f, mk_v(VAR_x), ctr1(LAB_i, mk_v(VAR_x)));
    Term rhs = ctr0(LAB_e);
    AtpCpGraph g;
    CHECK_EQ(thvm_atp_cp_graph(lhs, rhs, &g), 1);
    CHECK_EQ(g.overflow, 0u);

    // Node 0 is the CP super-node.
    CHECK_EQ(g.node_type[0], (u8)ATP_CPG_CPSUPER);
    CHECK_EQ((u32)feat_at(&g, 0, 5), 1u);   // is_cpsuper

    // Counts by type:
    //   CPSUPER 1; TERM = 4 (lhs: f,x,i,x) + 1 (rhs: e) = 5;
    //   SYMBOL = {f,i,e} = 3; VAR = {x} = 1.
    CHECK_EQ(count_nodes(&g, ATP_CPG_CPSUPER), 1u);
    CHECK_EQ(count_nodes(&g, ATP_CPG_TERM),    5u);
    CHECK_EQ(count_nodes(&g, ATP_CPG_SYMBOL),  3u);
    CHECK_EQ(count_nodes(&g, ATP_CPG_VAR),     1u);
    CHECK_EQ(g.n_nodes, 10u);

    // Edges:
    //   E_TERM_SYM = one per TERM = 5;
    //   E_TERM_CHILD = f(2) + i(1) = 3;
    //   E_CP_LHS = 1; E_CP_RHS = 1.
    CHECK_EQ(count_edges(&g, ATP_CPG_E_TERM_SYM),   5u);
    CHECK_EQ(count_edges(&g, ATP_CPG_E_TERM_CHILD), 3u);
    CHECK_EQ(count_edges(&g, ATP_CPG_E_CP_LHS),     1u);
    CHECK_EQ(count_edges(&g, ATP_CPG_E_CP_RHS),     1u);
    CHECK_EQ(g.n_edges, 10u);

    // Every node feature is finite and the indicator columns are 0/1.
    for (u32 i = 0; i < g.n_nodes; i++) {
      for (u32 j = 0; j < ATP_CPG_FEAT_DIM; j++) CHECK(isfinite(feat_at(&g, i, j)));
      // is_term / is_symbol / is_var / is_cpsuper are mutually exclusive
      // and sum to exactly 1.
      u32 ind = (u32)feat_at(&g, i, 0) + (u32)feat_at(&g, i, 1)
              + (u32)feat_at(&g, i, 2) + (u32)feat_at(&g, i, 5);
      CHECK_EQ(ind, 1u);
    }

    // The CP super-node has exactly two outgoing edges, one LHS one RHS,
    // and they target TERM nodes.
    u32 super_out = 0u, lhs_edges = 0u, rhs_edges = 0u;
    for (u32 e = 0; e < g.n_edges; e++) {
      if (g.edge_src[e] == 0u) {
        super_out++;
        if (g.edge_type[e] == ATP_CPG_E_CP_LHS) {
          lhs_edges++;
          CHECK_EQ(g.node_type[g.edge_dst[e]], (u8)ATP_CPG_TERM);
        }
        if (g.edge_type[e] == ATP_CPG_E_CP_RHS) {
          rhs_edges++;
          CHECK_EQ(g.node_type[g.edge_dst[e]], (u8)ATP_CPG_TERM);
        }
      }
    }
    CHECK_EQ(super_out, 2u);
    CHECK_EQ(lhs_edges, 1u);
    CHECK_EQ(rhs_edges, 1u);

    // Each TERM node has exactly one outgoing E_TERM_SYM edge, to a
    // SYMBOL or VAR node.
    for (u32 i = 0; i < g.n_nodes; i++) {
      if (g.node_type[i] != ATP_CPG_TERM) continue;
      u32 sym_out = 0u;
      for (u32 e = 0; e < g.n_edges; e++) {
        if (g.edge_src[e] == i && g.edge_type[e] == ATP_CPG_E_TERM_SYM) {
          sym_out++;
          u8 dt = g.node_type[g.edge_dst[e]];
          CHECK(dt == ATP_CPG_SYMBOL || dt == ATP_CPG_VAR);
        }
      }
      CHECK_EQ(sym_out, 1u);
    }

    // Structural occurrence counts: the single VAR (x) occurs twice; each
    // of the three symbols (f, i, e) occurs once.  arity: f=2, i=1, e=0.
    for (u32 i = 0; i < g.n_nodes; i++) {
      if (g.node_type[i] == ATP_CPG_VAR) CHECK_EQ((u32)feat_at(&g, i, 4), 2u);
      if (g.node_type[i] == ATP_CPG_SYMBOL) CHECK_EQ((u32)feat_at(&g, i, 4), 1u);
    }
  }

  // === RENAMING INVARIANCE (headline) ================================
  // Same structure, every symbol + variable remapped by a bijection.
  // The two graphs must be bit-for-bit identical: same n_nodes, same
  // node_type[], same node_feat[] matrix, and the same edge arrays
  // element-for-element (first-appearance order is preserved under a
  // consistent renaming).
  TEST_BEGIN("atp/cp_graph/renaming-invariance");
  {
    Term l1 = ctr2(LAB_f, mk_v(VAR_x), ctr1(LAB_i, mk_v(VAR_x)));
    Term r1 = ctr0(LAB_e);
    Term l2 = ctr2(LAB_g, mk_v(VAR_y), ctr1(LAB_j, mk_v(VAR_y)));
    Term r2 = ctr0(LAB_d);

    AtpCpGraph a, b;
    CHECK_EQ(thvm_atp_cp_graph(l1, r1, &a), 1);
    CHECK_EQ(thvm_atp_cp_graph(l2, r2, &b), 1);

    CHECK_EQ(a.n_nodes, b.n_nodes);
    CHECK_EQ(a.n_edges, b.n_edges);
    CHECK_EQ(a.overflow, b.overflow);

    // node_type[] identical.
    for (u32 i = 0; i < a.n_nodes; i++) CHECK_EQ(a.node_type[i], b.node_type[i]);

    // node_feat[] identical (all integer-valued; exact compare).
    int feat_identical = 1;
    for (u32 i = 0; i < a.n_nodes * ATP_CPG_FEAT_DIM; i++) {
      if (a.node_feat[i] != b.node_feat[i]) feat_identical = 0;
    }
    CHECK_EQ(feat_identical, 1);

    // Edge arrays identical element-for-element.
    int edges_identical = 1;
    for (u32 e = 0; e < a.n_edges; e++) {
      if (a.edge_src[e]  != b.edge_src[e])  edges_identical = 0;
      if (a.edge_dst[e]  != b.edge_dst[e])  edges_identical = 0;
      if (a.edge_type[e] != b.edge_type[e]) edges_identical = 0;
    }
    CHECK_EQ(edges_identical, 1);

    // Sanity: the two CPs really are built from DIFFERENT concrete ids
    // (so the test is not comparing a graph to itself).
    CHECK(LAB_f != LAB_g);
    CHECK(VAR_x != VAR_y);
  }

  // === Invariance also covers NUM atoms (deduped by value) ===========
  // f(7, 7) = 7 and f(9, 9) = 9 share structure under 7 -> 9: one NUM
  // SYMBOL node with occurrence_count 3 in each, identical graphs.
  TEST_BEGIN("atp/cp_graph/num-atom-invariance");
  {
    Term l1 = ctr2(LAB_f, build_num(7u), build_num(7u));
    Term r1 = build_num(7u);
    Term l2 = ctr2(LAB_f, build_num(9u), build_num(9u));
    Term r2 = build_num(9u);

    AtpCpGraph a, b;
    CHECK_EQ(thvm_atp_cp_graph(l1, r1, &a), 1);
    CHECK_EQ(thvm_atp_cp_graph(l2, r2, &b), 1);

    // 1 NUM atom (deduped) -> 1 SYMBOL node for the constant + 1 SYMBOL
    // for f; occurrence_count of the NUM symbol is 3 (two in lhs, one in
    // rhs).  Graphs identical despite different raw values.
    CHECK_EQ(count_nodes(&a, ATP_CPG_SYMBOL), 2u);
    CHECK_EQ(a.n_nodes, b.n_nodes);
    for (u32 i = 0; i < a.n_nodes * ATP_CPG_FEAT_DIM; i++) {
      CHECK(a.node_feat[i] == b.node_feat[i]);
    }
    // The NUM symbol node (arity 0, occ 3) is present.
    int found_num = 0;
    for (u32 i = 0; i < a.n_nodes; i++) {
      if (a.node_type[i] == ATP_CPG_SYMBOL
          && (u32)feat_at(&a, i, 3) == 0u && (u32)feat_at(&a, i, 4) == 3u) found_num = 1;
    }
    CHECK_EQ(found_num, 1);
  }

  // === NON-isomorphic CPs differ (invariance is not vacuous) =========
  // f(x, x) = e  vs  f(x, y) = e: the first has ONE var (occ 2), the
  // second has TWO vars (occ 1 each) -> different node count + different
  // VAR occurrence structure.
  TEST_BEGIN("atp/cp_graph/non-iso-differs");
  {
    Term l_same = ctr2(LAB_f, mk_v(VAR_x), mk_v(VAR_x));
    Term l_diff = ctr2(LAB_f, mk_v(VAR_x), mk_v(1u));
    Term r      = ctr0(LAB_e);

    AtpCpGraph a, b;
    CHECK_EQ(thvm_atp_cp_graph(l_same, r, &a), 1);
    CHECK_EQ(thvm_atp_cp_graph(l_diff, r, &b), 1);

    // One VAR node vs two -> different node count.
    CHECK_EQ(count_nodes(&a, ATP_CPG_VAR), 1u);
    CHECK_EQ(count_nodes(&b, ATP_CPG_VAR), 2u);
    CHECK(a.n_nodes != b.n_nodes);

    // The single var in `a` occurs twice; both vars in `b` occur once.
    for (u32 i = 0; i < a.n_nodes; i++) {
      if (a.node_type[i] == ATP_CPG_VAR) CHECK_EQ((u32)feat_at(&a, i, 4), 2u);
    }
    for (u32 i = 0; i < b.n_nodes; i++) {
      if (b.node_type[i] == ATP_CPG_VAR) CHECK_EQ((u32)feat_at(&b, i, 4), 1u);
    }
  }

  // === Overflow path: a tiny graph never overflows; the cap is huge ==
  // We can't cheaply build a >1024-node term here, but assert the guard
  // contract holds on a NULL out (returns 0, no crash).
  TEST_BEGIN("atp/cp_graph/null-out-rejected");
  {
    CHECK_EQ(thvm_atp_cp_graph(ctr0(LAB_e), ctr0(LAB_e), NULL), 0);
  }

  thvm_free();
  TEST_REPORT();
}
