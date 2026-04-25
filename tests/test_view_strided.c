// test_view_strided.c - exercise view_strided_index (sub-item f3a
// of the kernel-fusion / ShapeTracker arc).  Verifies the helper
// short-circuits for contiguous Views and walks per-axis strides
// + offset correctly for strided ones.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("view_strided/contiguous-shortcircuit");
  // Contiguous {2, 3} -> view_strided_index should return flat_idx
  // + offset (offset = 0 here).
  Shape s2x3 = {0};
  s2x3.ndim = 2;
  s2x3.dims[0] = 2;
  s2x3.dims[1] = 3;
  View v = view_create(s2x3);
  CHECK_EQ(v.contiguous, 1);
  CHECK_EQ(view_strided_index(&v, 0), 0);
  CHECK_EQ(view_strided_index(&v, 5), 5);

  TEST_BEGIN("view_strided/contiguous-with-offset");
  // Same {2, 3} but with offset = 7 -- still contiguous since
  // strides are row-major from offset; flat_idx + offset.
  v.offset = 7;
  CHECK_EQ(view_strided_index(&v, 0), 7);
  CHECK_EQ(view_strided_index(&v, 4), 11);

  TEST_BEGIN("view_strided/broadcast-stride-zero");
  // EXPAND-style: source shape {1, 3} expanded to {4, 3}.  Stride
  // [0] = 0 (broadcast), stride [1] = 1.  Output flat indices 0..11
  // map to underlying buffer indices 0, 1, 2, 0, 1, 2, ... (the
  // single source row repeated 4 times).
  View vb = {0};
  vb.shape.ndim    = 2;
  vb.shape.dims[0] = 4;
  vb.shape.dims[1] = 3;
  vb.strides[0]    = 0;       // broadcast
  vb.strides[1]    = 1;
  vb.numel         = 12;
  vb.contiguous    = 0;
  vb.offset        = 0;
  CHECK_EQ(view_strided_index(&vb, 0),  0);
  CHECK_EQ(view_strided_index(&vb, 1),  1);
  CHECK_EQ(view_strided_index(&vb, 2),  2);
  CHECK_EQ(view_strided_index(&vb, 3),  0);  // wraparound on axis 0
  CHECK_EQ(view_strided_index(&vb, 4),  1);
  CHECK_EQ(view_strided_index(&vb, 5),  2);
  CHECK_EQ(view_strided_index(&vb, 11), 2);

  TEST_BEGIN("view_strided/permute-2d-transpose");
  // PERMUTE-style: source {2, 3} viewed as {3, 2} via transpose.
  // Underlying row-major buffer for {2, 3}:
  //     [a, b, c, d, e, f]   shape {2, 3}, strides {3, 1}
  // After transpose to {3, 2}: strides {1, 3}.
  // Output[0,0]=a, [0,1]=d, [1,0]=b, [1,1]=e, [2,0]=c, [2,1]=f.
  // Flat output indices 0..5 -> buffer indices 0, 3, 1, 4, 2, 5.
  View vt = {0};
  vt.shape.ndim    = 2;
  vt.shape.dims[0] = 3;
  vt.shape.dims[1] = 2;
  vt.strides[0]    = 1;
  vt.strides[1]    = 3;
  vt.numel         = 6;
  vt.contiguous    = 0;
  vt.offset        = 0;
  CHECK_EQ(view_strided_index(&vt, 0), 0);
  CHECK_EQ(view_strided_index(&vt, 1), 3);
  CHECK_EQ(view_strided_index(&vt, 2), 1);
  CHECK_EQ(view_strided_index(&vt, 3), 4);
  CHECK_EQ(view_strided_index(&vt, 4), 2);
  CHECK_EQ(view_strided_index(&vt, 5), 5);

  TEST_BEGIN("view_strided/flip-negative-stride");
  // FLIP-style: source {3} viewed as {3} with stride[0] = -1 +
  // offset = 2.  Flat indices 0, 1, 2 -> buffer 2, 1, 0.
  View vf = {0};
  vf.shape.ndim    = 1;
  vf.shape.dims[0] = 3;
  vf.strides[0]    = -1;
  vf.numel         = 3;
  vf.contiguous    = 0;
  vf.offset        = 2;
  CHECK_EQ(view_strided_index(&vf, 0), 2);
  CHECK_EQ(view_strided_index(&vf, 1), 1);
  CHECK_EQ(view_strided_index(&vf, 2), 0);

  thvm_free();
  TEST_REPORT();
}
