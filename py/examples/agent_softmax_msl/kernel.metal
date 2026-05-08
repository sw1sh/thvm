// MLX-inspired softmax_single_row port with vectorized float4 loads.
// One threadgroup per row. Each thread holds N_READS floats in registers,
// loaded as N_READS/4 float4s, reduces via simd_max/simd_sum, then writes
// from registers (no second global read like softmax_looped).
// ABI:
//   buffer(0) = device const float *in    [R*C flat]
//   buffer(1) = device       float *out   [R*C flat]
//   buffer(2) = constant     int   &axis_size = C
//   function name = "k"

#include <metal_stdlib>
using namespace metal;

#define N_READS 8
#define N_VEC   (N_READS / 4)
#define SIMD_SIZE 32

[[kernel]] void k(
    device const float *in       [[buffer(0)]],
    device       float *out      [[buffer(1)]],
    constant     int   &axis_size [[buffer(2)]],
    uint gid [[threadgroup_position_in_grid]],
    uint _lid [[thread_position_in_threadgroup]],
    uint simd_lane_id [[thread_index_in_simdgroup]],
    uint simd_group_id [[simdgroup_index_in_threadgroup]])
{
    int lid = int(_lid);

    threadgroup float local_max[SIMD_SIZE];
    threadgroup float local_normalizer[SIMD_SIZE];

    // -FLT_MAX (finite) so OOB padding never produces NaN via inf-inf in simd reductions.
    constexpr float NEG_FLT_MAX = -FLT_MAX;

    float4 ld[N_VEC];

    int row_start = lid * N_READS;
    bool fully_in = (row_start + N_READS <= axis_size);

    in  += gid * size_t(axis_size) + row_start;
    out += gid * size_t(axis_size) + row_start;

    // Load N_READS values per thread as float4 vectors.
    if (fully_in) {
        device const float4 *in4 = reinterpret_cast<device const float4*>(in);
        for (int v = 0; v < N_VEC; v++) ld[v] = in4[v];
    } else {
        for (int v = 0; v < N_VEC; v++) {
            int base = v * 4;
            float4 t;
            t.x = (row_start + base + 0 < axis_size) ? in[base + 0] : NEG_FLT_MAX;
            t.y = (row_start + base + 1 < axis_size) ? in[base + 1] : NEG_FLT_MAX;
            t.z = (row_start + base + 2 < axis_size) ? in[base + 2] : NEG_FLT_MAX;
            t.w = (row_start + base + 3 < axis_size) ? in[base + 3] : NEG_FLT_MAX;
            ld[v] = t;
        }
    }

    // Initialize TG-shared partials (only first simdgroup writes).
    if (simd_group_id == 0) {
        local_max[simd_lane_id] = NEG_FLT_MAX;
        local_normalizer[simd_lane_id] = 0.0f;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);

    // Per-thread max -> simd_max -> per-simdgroup partial.
    float maxval = NEG_FLT_MAX;
    for (int v = 0; v < N_VEC; v++) {
        float4 a = ld[v];
        maxval = max(maxval, max(max(a.x, a.y), max(a.z, a.w)));
    }
    maxval = simd_max(maxval);
    if (simd_lane_id == 0) local_max[simd_group_id] = maxval;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    // First simdgroup reduces partials.
    if (simd_group_id == 0) {
        maxval = simd_max(local_max[simd_lane_id]);
        if (simd_lane_id == 0) local_max[0] = maxval;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    maxval = local_max[0];

    // Compute exp(x - maxval), accumulate per-thread sum, store back to ld[].
    float normalizer = 0.0f;
    for (int v = 0; v < N_VEC; v++) {
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
    if (simd_lane_id == 0) local_normalizer[simd_group_id] = normalizer;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (simd_group_id == 0) {
        normalizer = simd_sum(local_normalizer[simd_lane_id]);
        if (simd_lane_id == 0) local_normalizer[0] = normalizer;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    normalizer = 1.0f / local_normalizer[0];

    // Write output (multiply ld[] by 1/normalizer, vectorized store).
    if (fully_in) {
        device float4 *out4 = reinterpret_cast<device float4*>(out);
        for (int v = 0; v < N_VEC; v++) out4[v] = ld[v] * normalizer;
    } else {
        for (int v = 0; v < N_VEC; v++) {
            int base = v * 4;
            float4 e = ld[v] * normalizer;
            if (row_start + base + 0 < axis_size) out[base + 0] = e.x;
            if (row_start + base + 1 < axis_size) out[base + 1] = e.y;
            if (row_start + base + 2 < axis_size) out[base + 2] = e.z;
            if (row_start + base + 3 < axis_size) out[base + 3] = e.w;
        }
    }
}
