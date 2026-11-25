// A disk tensor going to Metal is read from its file into the device buffer
// (thvm_metal_buf_wrap_external with the file known), not copied out of the
// mapping.  Checked on a tensor that starts off a page boundary and spans
// several read chunks, on a small one, on one whose file was unlinked before
// the upload, and with no descriptor, which copies from the mapping.  The
// tensor results are x + x computed on Metal against the bytes written.
// Built -DTHVM_HAS_METAL.
#include "../src/thvm.c"
#include "test.h"

static float pattern(u64 i) { return (float)((i * 2654435761ull) % 1000003ull) * 0.001f; }

// x + x on Metal for the f32 tensor at [byte_offset, +numel*4) of `path`;
// the number of wrong elements.
static u64 doubled_mismatches(const char *path, u64 byte_offset, u32 numel, u64 first_index) {
  u32 dims[1] = {numel};
  Term x = thvm_tensor_mmap(path, byte_offset, (u64)numel * 4, DT_FP32, 1, dims);
  if (x == 0) return numel;
  Term r = term_resolve(thvm_realize(
      uop_to_device_leaves(uop_binary(UOP_ADD, x, x), THVM_DEV_METAL)));
  if (term_tag(r) != TAG_TEN) return numel;
  TenDesc *td = &TENS[(u32)term_val(r)];
  float *got = (float *)malloc((size_t)numel * 4);
  td->backend->buf_read(td->buf_id, got, (u64)numel * 4);
  u64 wrong = 0;
  for (u32 i = 0; i < numel; i++) {
    if (got[i] != 2.0f * pattern(first_index + i)) wrong++;
  }
  free(got);
  return wrong;
}

int main(void) {
  setenv("DEV", "metal", 1);
  thvm_init();
  TEST_BEGIN("metal/disk-tensor-is-read-from-its-file");
  CHECK(CURRENT_BACKEND == &METAL_BACKEND);

  // 21 million floats (84 MB): past two 32 MB read chunks from any offset.
  const u64 total = 21u * 1000u * 1000u;
  char path[] = "/tmp/thvm_disk_read_XXXXXX";
  int fd = mkstemp(path);
  CHECK(fd >= 0);
  float *all = (float *)malloc((size_t)total * 4);
  for (u64 i = 0; i < total; i++) all[i] = pattern(i);
  CHECK(write(fd, all, (size_t)total * 4) == (ssize_t)(total * 4));
  close(fd);
  free(all);

  u64 before = thvm_metal_file_read_bytes();
  // Starts 4 * 1001 bytes in (off a page boundary), 20 million elements.
  CHECK_EQ(doubled_mismatches(path, 4u * 1001u, 20u * 1000u * 1000u, 1001u), 0);
  u64 after_big = thvm_metal_file_read_bytes();
  CHECK(after_big - before >= 80u * 1000u * 1000u);

  TEST_BEGIN("metal/small-disk-tensor-is-read-from-its-file");
  CHECK_EQ(doubled_mismatches(path, 4u * 20500000u, 37u, 20500000u), 0);
  u64 after_small = thvm_metal_file_read_bytes();
  CHECK(after_small > after_big);

  // The mapping and the tensor's descriptor outlive the file's name: a tensor
  // mapped before the unlink is still read from the file.
  TEST_BEGIN("metal/disk-tensor-outlives-its-file-name");
  u32 dims[1] = {4096};
  Term late = thvm_tensor_mmap(path, 4u * 300u, 4096u * 4, DT_FP32, 1, dims);
  Term raw  = thvm_tensor_mmap(path, 4u * 7000u, 4096u * 4, DT_FP32, 1, dims);
  CHECK(late != 0 && raw != 0);
  unlink(path);
  Term r = term_resolve(thvm_realize(
      uop_to_device_leaves(uop_binary(UOP_ADD, late, late), THVM_DEV_METAL)));
  CHECK(term_tag(r) == TAG_TEN);
  float got[4096];
  TenDesc *td = &TENS[(u32)term_val(r)];
  td->backend->buf_read(td->buf_id, got, sizeof(got));
  u64 wrong = 0;
  for (u32 i = 0; i < 4096; i++) {
    if (got[i] != 2.0f * pattern(300u + i)) wrong++;
  }
  CHECK_EQ(wrong, 0);
  u64 after_late = thvm_metal_file_read_bytes();
  CHECK(after_late > after_small);

  // Without a descriptor the bytes are copied out of the mapping.  A read of
  // the buffer starts at the tensor, `minor` bytes past the page-aligned base.
  TEST_BEGIN("metal/disk-tensor-without-a-descriptor-copies-from-the-mapping");
  void *base = NULL;
  u64 maplen = 0, minor = 0;
  CHECK(thvm_disk_buf_map_info(TENS[(u32)term_val(raw)].buf_id, &base, &maplen, &minor));
  CHECK(minor != 0);
  u32 bid = thvm_metal_buf_wrap_external(base, maplen, minor, -1, 0);
  CHECK(bid != 0);
  float copied[4096];
  METAL_BACKEND.buf_read(bid, copied, sizeof(copied));
  u64 off = 0;
  for (u32 i = 0; i < 4096; i++) {
    if (copied[i] != pattern(7000u + i)) off++;
  }
  CHECK_EQ(off, 0);
  CHECK_EQ(thvm_metal_file_read_bytes(), after_late);

  thvm_free();
  TEST_REPORT();
}
