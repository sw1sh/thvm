// tinygrad's least_upper_dtype (dtype.py:180-194): the lowest dtype both a and
// b promote to in its lattice, so f32 with f64 computes in f64 and an int with
// f16 in f16.  Nodes 0 .. DT_FP64 are the DT_* ids, in tinygrad's priority
// order; the lattice's weak int and weak float nodes resolve to i32 and f32,
// and the packed nibble ints sit with their byte ints.
#define NN_WEAK_INT   15
#define NN_WEAK_FLOAT 16

fn u32 nn_promote(u32 a, u32 b) {
  static u32 const up[17] = {
    [DT_BOOL] = 1u << NN_WEAK_INT,
    [DT_INT8] = 1u << DT_INT16,
    [DT_UINT8] = 1u << DT_INT16 | 1u << DT_UINT16,
    [DT_INT16] = 1u << DT_INT32,
    [DT_UINT16] = 1u << DT_INT32 | 1u << DT_UINT32,
    [DT_INT32] = 1u << DT_INT64,
    [DT_UINT32] = 1u << DT_INT64 | 1u << DT_UINT64,
    [DT_INT64] = 1u << NN_WEAK_FLOAT,
    [DT_UINT64] = 1u << NN_WEAK_FLOAT,
    [DT_FP8E4M3] = 1u << DT_FP16 | 1u << DT_BF16,
    [DT_FP8E5M2] = 1u << DT_FP16 | 1u << DT_BF16,
    [DT_FP16] = 1u << DT_FP32,
    [DT_BF16] = 1u << DT_FP32,
    [DT_FP32] = 1u << DT_FP64,
    [NN_WEAK_INT] = 1u << DT_INT8 | 1u << DT_UINT8,
    [NN_WEAK_FLOAT] = 1u << DT_FP8E4M3 | 1u << DT_FP8E5M2,
  };
  // The nodes in tinygrad's (priority, bitsize) order.
  static u32 const order[17] = {
    DT_BOOL,  NN_WEAK_INT, DT_INT8,   DT_UINT8,      DT_INT16,   DT_UINT16,  DT_INT32,
    DT_UINT32, DT_INT64,   DT_UINT64, NN_WEAK_FLOAT, DT_FP8E4M3, DT_FP8E5M2, DT_FP16,
    DT_BF16,  DT_FP32,     DT_FP64,
  };
  u32 node[2] = {a == DT_INT4 ? DT_INT8 : a == DT_UINT4 ? DT_UINT8 : a,
                 b == DT_INT4 ? DT_INT8 : b == DT_UINT4 ? DT_UINT8 : b};
  u32 parents[2];
  for (u32 k = 0; k < 2; k++) {
    if (node[k] > DT_FP64) {
      return DT_FP32;
    }
    // Closed under up[]: grow until no new node appears.
    u32 set = 1u << node[k], prev = 0;
    while (set != prev) {
      prev = set;
      for (u32 n = 0; n < 17; n++) {
        if (set >> n & 1) {
          set |= up[n];
        }
      }
    }
    parents[k] = set;
  }
  u32 common = parents[0] & parents[1];
  for (u32 i = 0; i < 17; i++) {
    if (common >> order[i] & 1) {
      return order[i] == NN_WEAK_INT ? DT_INT32 : order[i] == NN_WEAK_FLOAT ? DT_FP32 : order[i];
    }
  }
  return DT_FP32;
}
