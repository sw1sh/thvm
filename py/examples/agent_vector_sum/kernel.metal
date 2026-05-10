// vector_sum: full-array reduction `s = sum(a)` over fp32.
//
// Strategy (iteration 7, locked): cooperative two-stage in one kernel.
//   Pass 1 (in-flight per TG): each TG computes a partial sum, writes it
//     to part[tg_id], then atomically increments a counter. The TG that
//     gets the last increment (counter == num_tgs - 1) loads all partials
//     and reduces them with simd_sum, writing to out[0].
//   Avoids atomic_compare_exchange contention on `out` and avoids needing
//     a second dispatch (pure GPU sync, no CPU round trip).
//
// ABI:
//   buffer(0) = device const float *in       [N flat]
//   buffer(1) = device       float *out      [1] final result
//   buffer(2) = constant     int   &n        input length
//   buffer(3) = device       float *part     [num_tgs] scratch
//   buffer(4) = device atomic_uint *counter  [1] init 0; kernel resets

#include <metal_stdlib>
using namespace metal;

#define N_READS 16
#define SIMD_SIZE 32

[[kernel]] void k(
    device const float *in       [[buffer(0)]],
    device       float *out      [[buffer(1)]],
    constant     int   &n        [[buffer(2)]],
    device       float *part     [[buffer(3)]],
    device atomic_uint *counter  [[buffer(4)]],
    uint  tg_id      [[threadgroup_position_in_grid]],
    uint  num_tgs    [[threadgroups_per_grid]],
    uint  lid        [[thread_position_in_threadgroup]],
    uint  lsize      [[threads_per_threadgroup]],
    uint  simd_lane_id [[thread_index_in_simdgroup]],
    uint  simd_group_id [[simdgroup_index_in_threadgroup]],
    uint  simds_per_tg [[simdgroups_per_threadgroup]])
{
    threadgroup float sg_partials[SIMD_SIZE];
    threadgroup uint  is_last;

    int row_size = (n + int(num_tgs) - 1) / int(num_tgs);
    int slab_start = int(tg_id) * row_size;
    int slab_end = min(slab_start + row_size, n);

    int actual = slab_end - slab_start;
    int per_iter = int(lsize) * N_READS;
    int blocks = actual / per_iter;
    int extra = actual - blocks * per_iter;

    int lid_off = int(lid) * N_READS;
    extra -= lid_off;
    int idx = slab_start + lid_off;

    if (extra >= N_READS) { blocks++; extra = 0; }

    float thread_sum = 0.0f;
    for (int b = 0; b < blocks; b++) {
        device const float4 *p = reinterpret_cast<device const float4 *>(in + idx);
        float4 v0 = p[0];
        float4 v1 = p[1];
        float4 v2 = p[2];
        float4 v3 = p[3];
        thread_sum += (v0.x + v0.y + v0.z + v0.w) + (v1.x + v1.y + v1.z + v1.w)
                    + (v2.x + v2.y + v2.z + v2.w) + (v3.x + v3.y + v3.z + v3.w);
        idx += per_iter;
    }
    if (extra > 0) {
        for (int i = 0; i < extra; i++) thread_sum += in[idx + i];
    }

    float sg_sum = simd_sum(thread_sum);
    if (simds_per_tg > 1) {
        if (simd_lane_id == 0) sg_partials[simd_group_id] = sg_sum;
        threadgroup_barrier(mem_flags::mem_threadgroup);
        float v = (lid < simds_per_tg) ? sg_partials[lid] : 0.0f;
        sg_sum = simd_sum(v);
    }

    if (lid == 0) {
        part[tg_id] = sg_sum;
    }
    threadgroup_barrier(mem_flags::mem_device);

    if (lid == 0) {
        uint prev = atomic_fetch_add_explicit(counter, 1u, memory_order_relaxed);
        is_last = (prev + 1u == num_tgs) ? 1u : 0u;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);

    if (is_last == 1u) {
        // The last TG performs the final reduce.
        float t = 0.0f;
        for (uint i = lid; i < num_tgs; i += lsize) {
            t += part[i];
        }
        float total = simd_sum(t);
        if (simds_per_tg > 1) {
            if (simd_lane_id == 0) sg_partials[simd_group_id] = total;
            threadgroup_barrier(mem_flags::mem_threadgroup);
            float v = (lid < simds_per_tg) ? sg_partials[lid] : 0.0f;
            total = simd_sum(v);
        }
        if (lid == 0) {
            out[0] = total;
            atomic_store_explicit(counter, 0u, memory_order_relaxed);
        }
    }
}
