// MLX-inspired layer_norm_single_row port + float4 vectorized loads/stores.
// One threadgroup per row. Each thread reads N_READS floats (= N_READS/4 float4s),
// then does two-pass MLX-style reduce:
//   pass 1: mean via simd_sum + TG reduce
//   pass 2: variance via centered sum-of-squares + simd_sum + TG reduce
// Then write w*x_hat + b vectorized.
// ABI:
//   buffer(0) = device const float *x      [R*C flat]
//   buffer(1) = device const float *gamma  [C]
//   buffer(2) = device const float *beta   [C]
//   buffer(3) = device       float *out    [R*C flat]
//   buffer(4) = constant     int   &axis_size = C
//   function name = "k"

#include <metal_stdlib>
#include <metal_simdgroup>
using namespace metal;

#ifndef N_READS
#define N_READS 8
#endif
#define N_VEC   (N_READS / 4)
#define SIMD_SIZE 32

constant float EPS = 1e-5f;

inline void threadgroup_sum(
    thread float &x,
    threadgroup float *xs,
    uint simd_lane_id,
    uint simd_group_id)
{
    x = simd_sum(x);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (simd_lane_id == 0) {
        xs[simd_group_id] = x;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    x = xs[simd_lane_id];
    x = simd_sum(x);
}

[[kernel]] void k(
    device const float *x        [[buffer(0)]],
    device const float *gamma    [[buffer(1)]],
    device const float *beta     [[buffer(2)]],
    device       float *out      [[buffer(3)]],
    constant     int   &axis_size [[buffer(4)]],
    uint gid [[threadgroup_position_in_grid]],
    uint _lid [[thread_position_in_threadgroup]],
    uint simd_lane_id [[thread_index_in_simdgroup]],
    uint simd_group_id [[simdgroup_index_in_threadgroup]])
{
    int lid = int(_lid);

    threadgroup float local_buffer[SIMD_SIZE];
    if (simd_group_id == 0) {
        local_buffer[simd_lane_id] = 0.0f;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);

    int row_start = lid * N_READS;
    bool safe = (row_start + N_READS <= axis_size);
    int n = axis_size - row_start;

    x   += gid * size_t(axis_size) + row_start;
    out += gid * size_t(axis_size) + row_start;

    // Vectorized read of N_READS floats per thread (= N_VEC float4s).
    float4 v[N_VEC];
    if (safe) {
        device const float4 *x4 = reinterpret_cast<device const float4*>(x);
        for (int j = 0; j < N_VEC; j++) v[j] = x4[j];
    } else {
        for (int j = 0; j < N_VEC; j++) {
            int b = j * 4;
            float4 t;
            t.x = (b + 0 < n) ? x[b + 0] : 0.0f;
            t.y = (b + 1 < n) ? x[b + 1] : 0.0f;
            t.z = (b + 2 < n) ? x[b + 2] : 0.0f;
            t.w = (b + 3 < n) ? x[b + 3] : 0.0f;
            v[j] = t;
        }
    }

    // Pass 1: mean
    float inv_axis = 1.0f / float(axis_size);
    float mean = 0.0f;
    for (int j = 0; j < N_VEC; j++) {
        float4 a = v[j];
        mean += a.x + a.y + a.z + a.w;
    }
    threadgroup_sum(mean, local_buffer, simd_lane_id, simd_group_id);
    mean *= inv_axis;

    // Pass 2: normalizer (variance + eps then rsqrt).
    if (!safe) {
        for (int j = 0; j < N_VEC; j++) {
            int b = j * 4;
            float4 a = v[j];
            if (b + 0 >= n) a.x = mean;
            if (b + 1 >= n) a.y = mean;
            if (b + 2 >= n) a.z = mean;
            if (b + 3 >= n) a.w = mean;
            v[j] = a;
        }
    }
    float normalizer = 0.0f;
    for (int j = 0; j < N_VEC; j++) {
        float4 a = v[j] - mean;
        v[j] = a;
        normalizer += a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w;
    }
    threadgroup_sum(normalizer, local_buffer, simd_lane_id, simd_group_id);
    normalizer = precise::rsqrt(normalizer * inv_axis + EPS);

    // Write outputs: gamma * (x - mean)/sqrt(var+eps) + beta, vectorized.
    if (safe) {
        device const float4 *gamma4 = reinterpret_cast<device const float4*>(gamma + row_start);
        device const float4 *beta4  = reinterpret_cast<device const float4*>(beta  + row_start);
        device       float4 *out4   = reinterpret_cast<device       float4*>(out);
        for (int j = 0; j < N_VEC; j++) {
            float4 g = gamma4[j];
            float4 b = beta4[j];
            float4 xh = v[j] * normalizer;
            out4[j] = g * xh + b;
        }
    } else {
        for (int j = 0; j < N_VEC; j++) {
            int b = j * 4;
            float4 a = v[j] * normalizer;
            for (int k = 0; k < 4; k++) {
                int idx = b + k;
                if (idx < n) {
                    out[idx] = gamma[row_start + idx] * a[k] + beta[row_start + idx];
                }
            }
        }
    }
}
