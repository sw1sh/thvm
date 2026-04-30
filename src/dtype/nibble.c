// dtype/nibble.c -- packed int4 / uint4 nibble pack + unpack.
//
// Storage layout: 2 elements per byte, low nibble first (matches the
// ONNX / GGML int4 convention).  numel is the LOGICAL nibble count;
// dtype_storage_bytes(numel) = (numel + 1) / 2 bytes.
//
// int4: signed 4-bit two's complement, range [-8, 7]; sign-extended
//       on unpack.
// uint4: unsigned 4-bit, range [0, 15]; zero-extended on unpack.
//
// pack* writes (numel+1)/2 bytes; the trailing nibble (when numel is
// odd) lands in the low nibble of the last byte and the high nibble
// is zero.

void unpack_int4(i8 *dst, u8 const *src, u32 numel) {
  for (u32 i = 0; i < numel; i++) {
    u8 byte = src[i >> 1];
    u8 nib  = (i & 1) ? (byte >> 4) : (byte & 0x0Fu);
    // Sign-extend bit 3 into the upper nibble.
    if (nib & 0x08u) nib |= 0xF0u;
    dst[i] = (i8)nib;
  }
}

void unpack_uint4(u8 *dst, u8 const *src, u32 numel) {
  for (u32 i = 0; i < numel; i++) {
    u8 byte = src[i >> 1];
    dst[i]  = (i & 1) ? (byte >> 4) : (byte & 0x0Fu);
  }
}

// `src` must hold values in [-8, 7]; out-of-range inputs wrap modulo 16.
void pack_int4(u8 *dst, i8 const *src, u32 numel) {
  // Zero the destination so the trailing-odd-nibble byte's high
  // nibble stays clean.
  for (u32 b = 0; b < (numel + 1) >> 1; b++) dst[b] = 0;
  for (u32 i = 0; i < numel; i++) {
    u8 nib = (u8)(src[i] & 0x0Fu);
    if (i & 1) dst[i >> 1] |= (u8)(nib << 4);
    else       dst[i >> 1] |= nib;
  }
}

void pack_uint4(u8 *dst, u8 const *src, u32 numel) {
  for (u32 b = 0; b < (numel + 1) >> 1; b++) dst[b] = 0;
  for (u32 i = 0; i < numel; i++) {
    u8 nib = src[i] & 0x0Fu;
    if (i & 1) dst[i >> 1] |= (u8)(nib << 4);
    else       dst[i >> 1] |= nib;
  }
}
