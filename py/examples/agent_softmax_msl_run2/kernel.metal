// Final (iter 13): shape-specialized softmax with multi-row-per-TG large path.
// K_C is injected by score.py as a compile-time constant; all loop bounds
// are constexpr so the Metal compiler fully unrolls.
//
//   K_C <= 256: Mode A - 1 simdgroup per row, no TG memory, no barriers.
//               N_READS = ceildiv(K_C, 32). Strided per-lane loads.
//               Dispatch packs 4 simdgroups per TG (R=32 -> 8 TGs).
//
//   K_C >  256: Mode B - MLX softmax_single_row layout, but with multiple
//               rows per TG. N_READS=8 (2 float4 per lane).
//               LANES_PER_ROW = ceildiv(K_C, 8) rounded to 32.
//               ROWS_PER_TG  = 1024 / LANES_PER_ROW (=2 for K_C=4096).
//               Each row owns its own SIMD_SIZE slice of local_max[]/local_normalizer[];
//               the 4 barriers + 4 simd_*  reductions happen scoped per-row.
//
// ABI (unchanged):
//   buffer(0) = device const float *in    [R*C flat]
//   buffer(1) = device       float *out   [R*C flat]
//   buffer(2) = constant     int   &axis_size = C

#include <metal_stdlib>
using namespace metal;

#define SIMD_SIZE 32

constexpr constant float NEG_FLT_MAX = -FLT_MAX;

#if K_C <= 256
// ----------------------- Mode A -----------------------
constexpr constant int N_READS_A = (K_C + SIMD_SIZE - 1) / SIMD_SIZE;

[[kernel]] void k(
    device const float *in       [[buffer(0)]],
    device       float *out      [[buffer(1)]],
    constant     int   &axis_size [[buffer(2)]],
    uint tg_id        [[threadgroup_position_in_grid]],
    uint lid          [[thread_position_in_threadgroup]],
    uint sgs_per_tg   [[simdgroups_per_threadgroup]],
    uint simd_lane_id [[thread_index_in_simdgroup]],
    uint simd_group_id [[simdgroup_index_in_threadgroup]])
{
    int row = int(tg_id) * int(sgs_per_tg) + int(simd_group_id);
    float ld[N_READS_A];

    device const float *row_in_ptr = in + size_t(row) * size_t(K_C);
    #pragma unroll
    for (int k = 0; k < N_READS_A; k++) {
        int idx = simd_lane_id + k * SIMD_SIZE;
        ld[k] = (idx < K_C) ? row_in_ptr[idx] : NEG_FLT_MAX;
    }

    float maxval = NEG_FLT_MAX;
    #pragma unroll
    for (int k = 0; k < N_READS_A; k++) maxval = max(maxval, ld[k]);
    maxval = simd_max(maxval);

    float normalizer = 0.0f;
    #pragma unroll
    for (int k = 0; k < N_READS_A; k++) {
        float v = fast::exp(ld[k] - maxval);
        ld[k] = v;
        normalizer += v;
    }
    normalizer = simd_sum(normalizer);
    float inv = 1.0f / normalizer;

    device float *row_out_ptr = out + size_t(row) * size_t(K_C);
    #pragma unroll
    for (int k = 0; k < N_READS_A; k++) {
        int idx = simd_lane_id + k * SIMD_SIZE;
        if (idx < K_C) row_out_ptr[idx] = ld[k] * inv;
    }
}

#else
// ----------------------- Mode B (multi-row-per-TG) -----------------------
// Each TG processes ROWS_PER_TG rows in parallel. SGs split into row groups.
// For K_C=4096, N_READS=8 -> 512 lanes per row. With ROWS_PER_TG=2: 1024-thread TG.
constexpr constant int N_READS_B = 8;
constexpr constant int N_VEC_B = N_READS_B / 4;
constexpr constant int LANES_PER_ROW = (((K_C + N_READS_B - 1) / N_READS_B) + SIMD_SIZE - 1) / SIMD_SIZE * SIMD_SIZE;
constexpr constant int ROWS_PER_TG = (1024 / LANES_PER_ROW) > 0 ? (1024 / LANES_PER_ROW) : 1;
constexpr constant int LSIZE_B = LANES_PER_ROW * ROWS_PER_TG;
constexpr constant int SGS_PER_ROW = LANES_PER_ROW / SIMD_SIZE;

[[kernel]] void k(
    device const float *in       [[buffer(0)]],
    device       float *out      [[buffer(1)]],
    constant     int   &axis_size [[buffer(2)]],
    uint tg_id        [[threadgroup_position_in_grid]],
    uint lid          [[thread_position_in_threadgroup]],
    uint simd_lane_id [[thread_index_in_simdgroup]],
    uint simd_group_id [[simdgroup_index_in_threadgroup]])
{
    // Each TG handles ROWS_PER_TG rows. Lanes [0..LANES_PER_ROW) -> row 0; etc.
    // Each row gets its own SIMD_SIZE local_max + local_normalizer slots.
    threadgroup float local_max[ROWS_PER_TG * SIMD_SIZE];
    threadgroup float local_normalizer[ROWS_PER_TG * SIMD_SIZE];

    int row_in_tg = int(lid) / LANES_PER_ROW;
    int lid_in_row = int(lid) % LANES_PER_ROW;
    int sg_in_row = int(simd_group_id) % SGS_PER_ROW;  // 0..SGS_PER_ROW-1
    int row = int(tg_id) * ROWS_PER_TG + row_in_tg;

    int row_start = lid_in_row * N_READS_B;
    bool fully_in = (row_start + N_READS_B <= K_C);

    device const float *in_row = in + size_t(row) * size_t(K_C) + row_start;
    device       float *out_row = out + size_t(row) * size_t(K_C) + row_start;

    float4 ld[N_VEC_B];
    if (fully_in) {
        device const float4 *in4 = reinterpret_cast<device const float4*>(in_row);
        #pragma unroll
        for (int v = 0; v < N_VEC_B; v++) ld[v] = in4[v];
    } else {
        #pragma unroll
        for (int v = 0; v < N_VEC_B; v++) {
            int base = v * 4;
            float4 t;
            t.x = (row_start + base + 0 < K_C) ? in_row[base + 0] : NEG_FLT_MAX;
            t.y = (row_start + base + 1 < K_C) ? in_row[base + 1] : NEG_FLT_MAX;
            t.z = (row_start + base + 2 < K_C) ? in_row[base + 2] : NEG_FLT_MAX;
            t.w = (row_start + base + 3 < K_C) ? in_row[base + 3] : NEG_FLT_MAX;
            ld[v] = t;
        }
    }

    // Init partials. lid covers ROWS_PER_TG * LANES_PER_ROW lanes; only first
    // SIMD_SIZE lanes per row's region need to write.
    if (sg_in_row == 0) {
        local_max[row_in_tg * SIMD_SIZE + simd_lane_id] = NEG_FLT_MAX;
        local_normalizer[row_in_tg * SIMD_SIZE + simd_lane_id] = 0.0f;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);

    float maxval = NEG_FLT_MAX;
    #pragma unroll
    for (int v = 0; v < N_VEC_B; v++) {
        float4 a = ld[v];
        maxval = max(maxval, max(max(a.x, a.y), max(a.z, a.w)));
    }
    maxval = simd_max(maxval);
    if (simd_lane_id == 0) local_max[row_in_tg * SIMD_SIZE + sg_in_row] = maxval;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    // Stage 2: SG 0 of each row reduces its row's partials.
    if (sg_in_row == 0) {
        maxval = simd_max(local_max[row_in_tg * SIMD_SIZE + simd_lane_id]);
        if (simd_lane_id == 0) local_max[row_in_tg * SIMD_SIZE] = maxval;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    maxval = local_max[row_in_tg * SIMD_SIZE];

    float normalizer = 0.0f;
    #pragma unroll
    for (int v = 0; v < N_VEC_B; v++) {
        float4 a = ld[v];
        float4 e;
        e.x = fast::exp(a.x - maxval);
        e.y = fast::exp(a.y - maxval);
        e.z = fast::exp(a.z - maxval);
        e.w = fast::exp(a.w - maxval);
        ld[v] = e;
        normalizer += e.x + e.y + e.z + e.w;
    }
    normalizer = simd_sum(normalizer);
    if (simd_lane_id == 0) local_normalizer[row_in_tg * SIMD_SIZE + sg_in_row] = normalizer;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (sg_in_row == 0) {
        normalizer = simd_sum(local_normalizer[row_in_tg * SIMD_SIZE + simd_lane_id]);
        if (simd_lane_id == 0) local_normalizer[row_in_tg * SIMD_SIZE] = normalizer;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    normalizer = 1.0f / local_normalizer[row_in_tg * SIMD_SIZE];

    if (fully_in) {
        device float4 *out4 = reinterpret_cast<device float4*>(out_row);
        #pragma unroll
        for (int v = 0; v < N_VEC_B; v++) out4[v] = ld[v] * normalizer;
    } else {
        #pragma unroll
        for (int v = 0; v < N_VEC_B; v++) {
            int base = v * 4;
            float4 e = ld[v] * normalizer;
            if (row_start + base + 0 < K_C) out_row[base + 0] = e.x;
            if (row_start + base + 1 < K_C) out_row[base + 1] = e.y;
            if (row_start + base + 2 < K_C) out_row[base + 2] = e.z;
            if (row_start + base + 3 < K_C) out_row[base + 3] = e.w;
        }
    }
}

#endif
