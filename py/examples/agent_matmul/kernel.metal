// Stage-4 tiled fp32 matmul: simdgroup_matrix MMA + register accumulator.
//
// out = a @ b   with a:(M,K), b:(K,N), out:(M,N), all row-major fp32.
// Shape arrives as compile-time #define K_M / K_N / K_K (score.py prepends).
//
// Tile layout (best config found over a 12-iteration sweep):
//   Each threadgroup computes a BM x BN = 64 x 64 output tile.
//   8 simdgroups (256 threads), SG grid 4 rows x 2 cols.
//   Each SG owns a 16 x 32 sub-tile = 2 x 4 = 8 simdgroup_matrix<float,8,8>
//     register accumulators (8 frags/SG sits just below the spill cliff;
//     16 frags -- 128x128 tile or an 8x1 SG grid -- measured to spill).
//   K loop steps BK = 32; cooperative float4 loads of a 64x32 A tile and
//     a 32x64 B tile into 16 KB of threadgroup memory, one barrier, then
//     4 MMA k-substeps of width 8.
//
// Not used (measured regressions on Apple M3 Max):
//   - bank-conflict padding of the threadgroup arrays slowed simdgroup_load.
//   - double-buffering gave no measurable gain (the compiler already
//     overlaps load and MMA) and doubled the TG-memory footprint.
#include <metal_stdlib>
#include <metal_simdgroup_matrix>
using namespace metal;

constant constexpr int BM = 64;
constant constexpr int BN = 64;
constant constexpr int BK = 32;
constant constexpr int NSG = 8;            // simdgroups per threadgroup
constant constexpr int SG_ROWS = 4;        // SG grid: 4 x 2
constant constexpr int SG_COLS = 2;
constant constexpr int TM = BM / SG_ROWS;  // 16
constant constexpr int TN = BN / SG_COLS;  // 32
constant constexpr int MFR = TM / 8;       // 2 frag rows per SG
constant constexpr int NFR = TN / 8;       // 4 frag cols per SG
constant constexpr int NTHREAD = NSG * 32; // 256

[[kernel]] void k(
    device       float *out [[buffer(0)]],   // [M*N] row-major
    device const float *a   [[buffer(1)]],   // [M*K] row-major
    device const float *b   [[buffer(2)]],   // [K*N] row-major
    uint  tgx   [[threadgroup_position_in_grid]],
    uint  lid   [[thread_position_in_threadgroup]],
    uint  sgid  [[simdgroup_index_in_threadgroup]])
{
    threadgroup float As[BM * BK];
    threadgroup float Bs[BK * BN];

    constexpr uint M = (uint)K_M;
    constexpr uint N = (uint)K_N;
    constexpr uint Kdim = (uint)K_K;
    constexpr uint tg_cols = N / BN;

    const uint m_base = (tgx / tg_cols) * BM;   // this TG's tile origin
    const uint n_base = (tgx % tg_cols) * BN;

    const uint sg_r = sgid / SG_COLS;
    const uint sg_c = sgid % SG_COLS;
    const uint sm = sg_r * TM;                  // SG sub-tile origin
    const uint sn = sg_c * TN;

    simdgroup_matrix<float, 8, 8> Cf[MFR * NFR];
    for (int i = 0; i < MFR * NFR; ++i)
        Cf[i] = simdgroup_matrix<float, 8, 8>(0.0f);

    const uint t = lid;

    constexpr int A_VEC = (BM * BK) / 4;        // float4 elements in As
    constexpr int B_VEC = (BK * BN) / 4;
    constexpr int A_PER = A_VEC / NTHREAD;      // float4 per thread for A
    constexpr int B_PER = B_VEC / NTHREAD;

    for (uint k0 = 0; k0 < Kdim; k0 += BK) {
        // ---- cooperative float4 load of A tile: As[r][c] = a[(m_base+r)K + k0 + c]
        for (int v = 0; v < A_PER; ++v) {
            uint idx = (t + (uint)v * NTHREAD) * 4;
            uint r = idx / BK, c = idx % BK;
            device const float4 *Ap =
                (device const float4 *)(a + (m_base + r) * Kdim + k0 + c);
            *((threadgroup float4 *)(As + r * BK + c)) = *Ap;
        }
        // ---- cooperative float4 load of B tile: Bs[r][c] = b[(k0+r)N + n_base + c]
        for (int v = 0; v < B_PER; ++v) {
            uint idx = (t + (uint)v * NTHREAD) * 4;
            uint r = idx / BN, c = idx % BN;
            device const float4 *Bp =
                (device const float4 *)(b + (k0 + r) * N + n_base + c);
            *((threadgroup float4 *)(Bs + r * BN + c)) = *Bp;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);

        // ---- MMA over BK in 8-wide substeps; the kk loop is unrolled
        //      so the compiler sees all fragment loads as independent and
        //      can interleave them with MMAs to hide threadgroup-load latency.
        #pragma unroll
        for (uint kk = 0; kk < BK; kk += 8) {
            simdgroup_matrix<float, 8, 8> Af[MFR];
            simdgroup_matrix<float, 8, 8> Bf[NFR];
            for (int i = 0; i < MFR; ++i)
                simdgroup_load(Af[i], As + (sm + i * 8) * BK + kk, BK);
            for (int j = 0; j < NFR; ++j)
                simdgroup_load(Bf[j], Bs + kk * BN + sn + j * 8, BN);
            for (int i = 0; i < MFR; ++i)
                for (int j = 0; j < NFR; ++j)
                    simdgroup_multiply_accumulate(
                        Cf[i * NFR + j], Af[i], Bf[j], Cf[i * NFR + j]);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }

    // ---- store register accumulator to global out
    for (int i = 0; i < MFR; ++i)
        for (int j = 0; j < NFR; ++j) {
            uint gr = m_base + sm + i * 8;
            uint gc = n_base + sn + j * 8;
            simdgroup_store(Cf[i * NFR + j], out + gr * N + gc, N);
        }
}
