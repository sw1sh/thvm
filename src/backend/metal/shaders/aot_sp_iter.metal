// backend/metal/shaders/aot_sp_iter.metal -- Path B step 1.
//
// Survey Propagation iteration kernel.  One thread per edge.  Each
// edge represents a (clause, variable, sign) incidence in the
// factor graph of a CNF formula.  The kernel computes one
// synchronous SP update over all edges in parallel.
//
// Algorithm (Mezard-Parisi-Zecchina, "Survey propagation: an algorithm
// for satisfiability", arXiv cs/0212002):
//
// For each edge e = (clause a, variable i, sign s_a_i):
//   For each variable j in clause a, j != i:
//     Let s_a_j = j's sign in clause a
//     V^s_a(j) = clauses b != a where j has sign s_a_j (agreement w/ a)
//     V^u_a(j) = clauses b != a where j has sign -s_a_j (disagreement w/ a)
//     p_s = prod_{b in V^s_a(j)} (1 - eta_b->j)
//     p_u = prod_{b in V^u_a(j)} (1 - eta_b->j)
//     Pi^u = (1 - p_u) * p_s   // some V^u-clause active, no V^s blocks it
//     Pi^s = p_u * (1 - p_s)   // some V^s-clause active, no V^u blocks
//     Pi^* = p_u * p_s         // no clause active
//     f_j  = Pi^u / (Pi^u + Pi^s + Pi^*)
//   eta_new[e] = product of f_j over all j != i in clause a
//
// Damping (host applies optional alpha in [0,1]):
//   eta_actual[e] = (1 - alpha) * eta_old[e] + alpha * eta_new[e]
//
// Buffer-binding convention (set by thvm_aot_metal_sp_iter in _.m):
//   buffer(0) : device const uint  *edges_clause       [n_edges]
//   buffer(1) : device const uint  *edges_var          [n_edges]
//   buffer(2) : device const uchar *edges_sign         [n_edges]   0=positive, 1=negative
//   buffer(3) : device const uint  *clause_edges_off   [n_clauses+1]  CSR row ptrs
//   buffer(4) : device const uint  *clause_edges_flat  [n_edges]      CSR col idx
//   buffer(5) : device const uint  *var_edges_off      [n_vars+1]
//   buffer(6) : device const uint  *var_edges_flat     [n_edges]
//   buffer(7) : device const float *eta_in             [n_edges]
//   buffer(8) : device       float *eta_out            [n_edges]
//   buffer(9) : constant     uint  *n_edges_buf        [1]

#include <metal_stdlib>
using namespace metal;

kernel void aot_sp_iter(
    device const uint  *edges_clause       [[buffer(0)]],
    device const uint  *edges_var          [[buffer(1)]],
    device const uchar *edges_sign         [[buffer(2)]],
    device const uint  *clause_edges_off   [[buffer(3)]],
    device const uint  *clause_edges_flat  [[buffer(4)]],
    device const uint  *var_edges_off      [[buffer(5)]],
    device const uint  *var_edges_flat     [[buffer(6)]],
    device const float *eta_in             [[buffer(7)]],
    device       float *eta_out            [[buffer(8)]],
    constant     uint  *n_edges_buf        [[buffer(9)]],
    uint                e                  [[thread_position_in_grid]])
{
  if (e >= *n_edges_buf) return;
  uint a = edges_clause[e];
  uint i = edges_var[e];

  float eta_new = 1.0f;

  // For each j in clause a, j != i.
  uint c_start = clause_edges_off[a];
  uint c_end   = clause_edges_off[a + 1];
  for (uint k = c_start; k < c_end; k++) {
    uint other_e = clause_edges_flat[k];
    if (other_e == e) continue;
    uint  j        = edges_var[other_e];
    uchar s_a_j    = edges_sign[other_e];

    // Walk j's clauses; partition into V^s_a(j) and V^u_a(j) by
    // sign agreement with the literal in clause a; skip clause a.
    uint v_start = var_edges_off[j];
    uint v_end   = var_edges_off[j + 1];
    float p_s = 1.0f;   // prod_{V^s_a(j)} (1 - eta_b->j)
    float p_u = 1.0f;   // prod_{V^u_a(j)} (1 - eta_b->j)
    for (uint m = v_start; m < v_end; m++) {
      uint  b_edge = var_edges_flat[m];
      if (edges_clause[b_edge] == a) continue;
      uchar s_b_j  = edges_sign[b_edge];
      float eta_b  = eta_in[b_edge];
      float one_minus = 1.0f - eta_b;
      if (s_b_j == s_a_j) {
        p_s *= one_minus;   // V^s: same sign as in clause a
      } else {
        p_u *= one_minus;   // V^u: opposite sign
      }
    }

    float Pi_u    = (1.0f - p_u) * p_s;
    float Pi_s    = p_u * (1.0f - p_s);
    float Pi_star = p_u * p_s;
    float denom   = Pi_u + Pi_s + Pi_star;
    float f_j     = (denom > 0.0f) ? (Pi_u / denom) : 0.0f;

    eta_new *= f_j;
  }

  eta_out[e] = eta_new;
}
