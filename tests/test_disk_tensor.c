// test_disk_tensor.c - the mmap-backed disk tensor (tinygrad's DISK device).
//
// Write known bytes to a temp file, mmap-load a region as a CPU tensor
// (thvm_tensor_mmap), and check: (a) the mapped bytes read back exactly,
// (b) a CPU op over it (+1) is correct -- proving it is a usable lazy
// tensor whose bytes the OS pages in on demand, and (c) the page-aligned
// offset path works (a non-zero, non-page-aligned byte_offset).
#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // Build a temp file: a small header of junk bytes, then 6 f32 values.
  // The data starts at a NON-page-aligned offset so the page-alignment
  // arithmetic (map from byte_offset - minor_offset) is exercised.
  char path[] = "/tmp/thvm_disk_tensor_XXXXXX";
  int fd = mkstemp(path);
  CHECK(fd >= 0);

  const u32   N = 6u;
  const u64   header = 13u;            // non-page-aligned, non-multiple-of-4
  float       vals[6] = {1.5f, -2.0f, 3.25f, 0.0f, 100.0f, -0.5f};
  char        junk[13];
  for (u32 i = 0u; i < header; i++) junk[i] = (char)(0xA0 + i);
  CHECK(write(fd, junk, header) == (ssize_t)header);
  CHECK(write(fd, vals, N * sizeof(float)) == (ssize_t)(N * sizeof(float)));
  close(fd);

  // --- (a) lazy mmap-load + exact read-back ---
  TEST_BEGIN("disk/mmap-load-readback");
  u32  dims[1] = {N};
  Term t = thvm_tensor_mmap(path, header, N * sizeof(float),
                            DT_FP32, 1u, dims);
  CHECK(t != 0);
  CHECK_EQ(term_tag(t), TAG_TEN);
  CHECK_EQ(TENS[(u32)term_val(t)].dtype, DT_FP32);
  CHECK_EQ(TENS[(u32)term_val(t)].view.numel, N);
  // The CpuBuf must be external (munmap on release, not free).
  CHECK_EQ(CPU_BUFS[TENS[(u32)term_val(t)].buf_id].owns_data, 0u);
  CHECK(CPU_BUFS[TENS[(u32)term_val(t)].buf_id].on_release != NULL);
  {
    float got[6];
    TENS[(u32)term_val(t)].backend->buf_read(
        TENS[(u32)term_val(t)].buf_id, got, N * sizeof(float));
    for (u32 i = 0u; i < N; i++) CHECK(fabsf(got[i] - vals[i]) < 1e-6f);
  }

  // --- (b) a CPU op over the disk tensor: +1 ---
  TEST_BEGIN("disk/cpu-op-plus-one");
  {
    Term one = uop_const(DT_FP32, 0x3f800000u);   // 1.0f bit pattern
    // broadcast the scalar const to {N} so the binary op shapes line up.
    u32  ed[1] = {N};
    Term one_b = uop_expand(uop_reshape(one, 1u, ed), 1u, ed);
    Term sum   = uop_binary(UOP_ADD, t, one_b);
    Term r     = term_resolve(thvm_realize(sum));
    float out[6];
    TENS[(u32)term_val(r)].backend->buf_read(
        TENS[(u32)term_val(r)].buf_id, out, N * sizeof(float));
    for (u32 i = 0u; i < N; i++) CHECK(fabsf(out[i] - (vals[i] + 1.0f)) < 1e-6f);
  }

  // --- (c) release path: dropping the tensor munmaps cleanly (no crash,
  // no double free).  Decref the buffer to zero and confirm the slot is
  // cleared. ---
  TEST_BEGIN("disk/release-munmaps");
  {
    u32 bid = TENS[(u32)term_val(t)].buf_id;
    cpu_buf_decref(bid);
    CHECK_EQ(CPU_BUFS[bid].refcount, 0u);
    CHECK(CPU_BUFS[bid].on_release == NULL);   // cleared by cpu_buf_free
    CHECK(CPU_BUFS[bid].data == NULL);
  }

  unlink(path);
  TEST_REPORT();
}
