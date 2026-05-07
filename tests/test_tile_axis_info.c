// test_tile_axis_info.c - Phase D1: TileAxisInfo pack/unpack contract.
//
// Verifies the bit-layout helpers in thvm.h round-trip every field
// independently and that the legacy `(extra >> 32)` / `(extra &
// 0xFFFFFFFF)` reads still observe kax_type / extent correctly when
// memory_scope and vector_width are 0 (the default; D2/D3 set them
// later).

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("tile-axis-info/round-trip-default");
  TileAxisInfo info = { KAX_LOOP, 1024, 0, 0 };
  u64 packed = tile_axis_pack(info);
  TileAxisInfo got = tile_axis_unpack(packed);
  CHECK_EQ(got.kax_type,     KAX_LOOP);
  CHECK_EQ(got.extent,       1024);
  CHECK_EQ(got.memory_scope, 0);
  CHECK_EQ(got.vector_width, 0);

  TEST_BEGIN("tile-axis-info/legacy-extent-low-32-bits");
  // Existing readers compute extent as `extra & 0xFFFFFFFF`; verify
  // the bit layout still satisfies that for default-scope axes.
  CHECK_EQ((u32)(packed & 0xFFFFFFFFu), 1024);

  TEST_BEGIN("tile-axis-info/legacy-kax-type-shift-32");
  // Existing readers compute kax_type as `(extra >> 32)`; with
  // memory_scope/vector_width=0 they read the kax_type unchanged.
  CHECK_EQ((u32)(packed >> 32), KAX_LOOP);

  TEST_BEGIN("tile-axis-info/all-fields-distinct");
  TileAxisInfo full = { KAX_GROUP_REDUCE, 65536, 1, 4 };
  u64 p = tile_axis_pack(full);
  TileAxisInfo r = tile_axis_unpack(p);
  CHECK_EQ(r.kax_type,     KAX_GROUP_REDUCE);
  CHECK_EQ(r.extent,       65536);
  CHECK_EQ(r.memory_scope, 1);
  CHECK_EQ(r.vector_width, 4);

  TEST_BEGIN("tile-axis-info/large-extent-fits-32-bits");
  // Largest extent we expect to handle: BN-grad reduce_size 204800
  // for BS=512, ample headroom in u32.
  TileAxisInfo big = { KAX_REDUCE, 204800, 0, 0 };
  TileAxisInfo b2 = tile_axis_unpack(tile_axis_pack(big));
  CHECK_EQ(b2.extent, 204800);

  TEST_BEGIN("tile-axis-info/memory-scope-doesnt-leak-to-extent");
  // With memory_scope set, legacy `(extra & 0xFFFFFFFF)` for extent
  // must still be intact (memory_scope is in bits 48..55).
  TileAxisInfo with_scope = { KAX_LOOP, 1024, 1, 0 };
  u64 ps = tile_axis_pack(with_scope);
  CHECK_EQ((u32)(ps & 0xFFFFFFFFu), 1024);

  TEST_BEGIN("tile-axis-info/kax-type-fits-16-bits");
  // The unpack masks kax_type to low 16 bits; if a future KAX_*
  // value exceeded 0xFFFF we'd silently truncate.  Sanity-check the
  // current ceiling has headroom.
  CHECK(KAX_GROUP_REDUCE < 0xFFFF);

  thvm_free();
  TEST_REPORT();
}
