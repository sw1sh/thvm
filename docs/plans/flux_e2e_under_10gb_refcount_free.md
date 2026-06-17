# FLUX e2e at <3s AND <10GB — SOLVED (zero-copy wrap, not refcount-free)

Status: **DONE (2026-06-15).** `flux_generate.wls` (FLUX.2-klein-4B, 256x256) runs end
to end with **warm 4-step sampling ~2.6s, peak RSS ~6.9GB**, producing valid distinct
images. The original premise of this doc — that a refcount-driven buffer free was the
required lever — was WRONG. This file records what actually solved it.

## The key realization: the "15.5GB peak" was a macOS RSS artifact

A mmap'd safetensors weight contributes ~7.4GB to **RSS** but ~0 to **`phys_footprint`**
on Darwin (clean file-backed pages, instantly reclaimable, NOT swap). A standalone probe
confirmed NO `madvise`/`MADV_FREE`/`msync(MS_INVALIDATE)` variant drops them from RSS
while RAM is free. So the real memory cost of the staged-upload path was always ~7.8GB
footprint; the 15.5GB RSS was the 7.75GB device copy PLUS the 7.75GB mmap file cache
double-counted.

## The fix: zero-copy wrap (the mmap region IS the Metal buffer)

`THVM_ZEROCOPY=1` (materialize.c) wraps each disk-mmap weight's page-aligned region as a
borrowed `MTLBuffer` via `newBufferWithBytesNoCopy` — no separate device copy, instant
cold weight load, RSS == footprint. On Apple unified memory the GPU reads the file-backed
pages in place. Result: `flux_bench` 7.75GB / 595ms/step, byte-identical to staged
(maxAbsDiff=0); `flux_generate` e2e 6.9GB peak.

Made safe (the prior 100GB blowup was unbounded RE-wrapping):
- **Dedup cache** (`thvm_metal_buf_wrap_external`, `_.m`): a `host_base` scan wraps each
  mmap region EXACTLY once for the context lifetime; a repeat returns the slot, refcount++.
- **Refcount-aware** `thvm_metal_buf_free_borrowed` (decref; free only at last ref).
- **`metal_buf_read`/`_write` apply `byte_offset`** so a host readback of a wrapped weight
  reads at base+minor, not the page-alignment padding.

## The five C bugs found + fixed along the way (all with green nn/grad/flux_jit_replay)

1. **setBarrier ICB** (`capture.c`): confirmed `[cmd setBarrier]` DOES order double-writes
   on M3 Max (`flux_jit_replay` rename-off+force-icb green) — the old "no barrier recovers
   it" comment was a misdiagnosis. Split `metal_graph_recycle` from `metal_graph_unsafe`;
   `THVM_JIT_GRAPH_FORCE_ICB` default-on keeps the batched ICB for buffer-recycling
   streams; the SSA-rename is now obsolete (default off); the packer
   (`jit_capture_pack_replay_temporaries`) is the memory planner (default on).
2. **Capture buffer-lifetime** (`capture.c` `jit_capture_release_retained_except`): the
   recording-time retain set was released unconditionally then re-retained, dropping a
   capture-referenced weight to refcount 0 in the gap → hard-freed under freelist pressure
   → replay bound a NIL buffer (Metal validation: "missing Buffer binding at index 2") →
   silent stale read. Fixed with a diff-based release that keeps live-op buffers.
3. **Realize fwd-reclaim over-free** (`realize.c`): `thvm_realize_fwd_reclaim` freed a
   Metal buffer still live through a persistent hash-consed `TAG_TEN` leaf — because
   `mark_heap_rooted_preserve()` was defined and documented as overlaid but NEVER CALLED.
   This was the cross-stage VAE corruption (valid latent → flat image); the "bf16
   content-dependence" was a red-herring allocator slot-numbering artifact. Fixed: call it
   + `thvm_metal_buf_clear_preserved(1)` after.
4. **Kernel over-binding** (`render_uop.c` `cg_render_input_inst_count` + `materialize.c`):
   truncate `ke->n_inputs` to the rendered input count so dispatch binds exactly the
   buffers the function reads (a rewrite collapsing view-clones left dead inputs bound).
5. **`kvar_reset` on `TReset`** (`thvm.c`): a latent cross-frame dangling symbolic-axis
   table.

## The WL restructure (FluxForward.wl / FluxGenerate.wl)

- **bf16 temb** (`fxTembFn` casts the {1,3072} output): makes the block modulation matmuls
  bf16xbf16 on the tensor cores / batched ICB (~0.6 vs ~4 s/step; an f32 temb makes them
  mixed f32xbf16 → CPU scalar expand). The {1,256} sinusoid stays f32 (its codegen bug).
- **Shared capture** (`fxVelocityJit` + `fxSampleJit[velJit,...]`): capture the velocity
  net ONCE with {z, enc, temb} ALL rebound (3-input rebind verified correct vs eager), so a
  multi-image batch pays ONE cold capture and every later image is a warm replay — NOT a
  per-image re-capture (which was the 5s→2.6s warm lever).

## Validation

`flux_generate.wls` BATCH=2: cold 19s, warm 2.6s, peak 6.9GB, img1 (apple) mean 0.377 +
img2 (bird) mean 0.817 (full-range, finite, distinct). Regressions: nn 72/0, grad 62/0,
flux_jit_replay 2/0, metal_transposed_matmul 3/0, jit_persistent_weight_lifetime 1/0,
jit_dtypes 13/0, metal_dtypes 19/0, symbolic 20/0. Keep `THVM_ZEROCOPY=1` for FLUX
(flux_generate.wls auto-enables THVM_FWD_RECLAIM + THVM_MMAP_NO_WILLNEED).
