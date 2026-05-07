// backend/metal/shaders/aot_sp_surveys.metal -- Path B step 2.
//
// Per-variable survey computation from converged SP eta messages.
// One thread per variable.  Reads all eta_b->i for clauses b
// containing variable i, partitions by polarity (V^+ where i is
// positive literal, V^- where negative), and computes:
//
//   p_pos = prod over V^+ of (1 - eta)    // "no V^+ clause needs i=T"
//   p_neg = prod over V^- of (1 - eta)    // "no V^- clause needs i=F"
//
//   pi_pos = (1 - p_pos) * p_neg          // i fixed to TRUE
//   pi_neg = (1 - p_neg) * p_pos          // i fixed to FALSE
//   pi_star = p_pos * p_neg                // i unconstrained
//
//   W+ = pi_pos / (pi_pos + pi_neg + pi_star)
//   W- = pi_neg / sum
//   W* = pi_star / sum
//
//   bias = |W+ - W-|
//
// Buffer convention:
//   buffer(0) : device const uint  *var_edges_off   [n_vars+1]
//   buffer(1) : device const uint  *var_edges_flat  [n_edges]
//   buffer(2) : device const uchar *edges_sign      [n_edges]
//   buffer(3) : device const float *eta             [n_edges]
//   buffer(4) : device       float *w_pos           [n_vars]
//   buffer(5) : device       float *w_neg           [n_vars]
//   buffer(6) : device       float *bias            [n_vars]
//   buffer(7) : constant     uint  *n_vars_buf      [1]

#include <metal_stdlib>
using namespace metal;

kernel void aot_sp_surveys(
    device const uint  *var_edges_off   [[buffer(0)]],
    device const uint  *var_edges_flat  [[buffer(1)]],
    device const uchar *edges_sign      [[buffer(2)]],
    device const float *eta             [[buffer(3)]],
    device       float *w_pos           [[buffer(4)]],
    device       float *w_neg           [[buffer(5)]],
    device       float *bias            [[buffer(6)]],
    constant     uint  *n_vars_buf      [[buffer(7)]],
    uint                i               [[thread_position_in_grid]])
{
  if (i >= *n_vars_buf) return;
  uint v_start = var_edges_off[i];
  uint v_end   = var_edges_off[i + 1];

  float p_pos = 1.0f;   // prod over V^+_i (sign=0, +literal) of (1 - eta)
  float p_neg = 1.0f;   // prod over V^-_i (sign=1, -literal) of (1 - eta)
  for (uint k = v_start; k < v_end; k++) {
    uint e = var_edges_flat[k];
    float one_minus = 1.0f - eta[e];
    if (edges_sign[e] == 0u) p_pos *= one_minus;
    else                     p_neg *= one_minus;
  }

  float pi_pos  = (1.0f - p_pos) * p_neg;
  float pi_neg  = (1.0f - p_neg) * p_pos;
  float pi_star = p_pos * p_neg;
  float sum     = pi_pos + pi_neg + pi_star;
  float Wp      = (sum > 0.0f) ? pi_pos / sum : 0.0f;
  float Wn      = (sum > 0.0f) ? pi_neg / sum : 0.0f;

  w_pos[i] = Wp;
  w_neg[i] = Wn;
  bias[i]  = (Wp > Wn) ? (Wp - Wn) : (Wn - Wp);
}
