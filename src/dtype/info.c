// dtype/info.c - dtype metadata table.
//
// The runtime carries a dtype tag on every numeric value (TAG_NUM ext,
// UOP_CONST ext, TAG_TEN ext, TenDesc.dtype, KProgOp.dtype).  The set
// mirrors tinygrad's full dtype enum (15 concrete dtypes plus packed
// int4/uint4 for modern quantization).  Phase A wires the table and
// the size primitives without enabling any new dtype: only DT_FP32
// (= DT_F32 alias) and DT_INT32 (= DT_I32 alias) are populated; every
// other slot is reserved with itemsize=0 so any accidental use trips
// the assertion in dtype_itemsize().  Subsequent phases populate the
// remaining rows and wire kernels.
//
// Reference: TinyHVM/tinygrad/tinygrad/dtype.py:130-146.
//
// Function prototypes (dtype_itemsize, dtype_storage_bytes, dtype_kind,
// ...) live in src/thvm.h so that translation units compiled
// separately from the single-TU thvm.c hub (currently just
// src/backend/metal/_.m, which only #includes thvm.h) can call them.

// Indexed by DT_* enum.  The 32-slot fixed size keeps the 6-bit ext
// field safe.  Phase A: only DT_FP32 (slot 13) and DT_INT32 (slot 5)
// are wired; reserved slots have itemsize=0.
static DTypeInfo const DTYPE_INFO[32] = {
    [DT_BOOL]      = { 1, DK_BOOL,      8, 0, "bool"     },
    [DT_INT8]      = { 1, DK_SINT,      8, 1, "i8"       },
    [DT_UINT8]     = { 1, DK_UINT,      8, 0, "u8"       },
    [DT_INT16]     = { 2, DK_SINT,     16, 1, "i16"      },
    [DT_UINT16]    = { 2, DK_UINT,     16, 0, "u16"      },
    [DT_INT32]     = { 4, DK_SINT,     32, 1, "i32"      },
    [DT_UINT32]    = { 4, DK_UINT,     32, 0, "u32"      },
    [DT_INT64]     = { 8, DK_SINT,     64, 1, "i64"      },
    [DT_UINT64]    = { 8, DK_UINT,     64, 0, "u64"      },
    [DT_FP8E4M3]   = { 1, DK_FP8,      8,  1, "fp8e4m3"  },
    [DT_FP8E5M2]   = { 1, DK_FP8,      8,  1, "fp8e5m2"  },
    [DT_FP16]      = { 2, DK_FP16,    16, 1, "f16"      },
    [DT_BF16]      = { 2, DK_BF16,    16, 1, "bf16"     },
    [DT_FP32]      = { 4, DK_FLOAT,    32, 1, "f32"      },
    [DT_FP64]      = { 8, DK_FLOAT,    64, 1, "f64"      },
    [DT_INT4]      = { 0, DK_INT4,    4,  1, "i4"       },
    [DT_UINT4]     = { 0, DK_UINT4,   4,  0, "u4"       },
};

_Static_assert(DT_COUNT <= 32, "dtype id must fit a 6-bit ext field with headroom");

DTypeInfo const *dtype_info(u32 dt) {
    if (dt >= 32) return NULL;
    return &DTYPE_INFO[dt];
}

u32 dtype_itemsize(u32 dt) {
    if (dt >= 32 || DTYPE_INFO[dt].itemsize == 0) {
        // Phase A: nothing past F32/I32 is wired yet.  Fail loudly so
        // call sites surface the missing rows during the per-phase
        // rollout instead of silently sizing buffers to 0.
        fprintf(stderr, "dtype_itemsize: dtype %u not yet enabled\n", dt);
        abort();
    }
    return DTYPE_INFO[dt].itemsize;
}

u64 dtype_storage_bytes(u32 dt, u64 numel) {
    if (dt >= 32) {
        fprintf(stderr, "dtype_storage_bytes: invalid dtype %u\n", dt);
        abort();
    }
    DTypeInfo const *di = &DTYPE_INFO[dt];
    if (di->bits == 4) return (numel + 1) >> 1;   // packed nibbles
    if (di->itemsize == 0) {
        fprintf(stderr, "dtype_storage_bytes: dtype %u not yet enabled\n", dt);
        abort();
    }
    return numel * (u64)di->itemsize;
}

char const *dtype_name(u32 dt) {
    if (dt >= 32 || DTYPE_INFO[dt].name == 0) return "?";
    return DTYPE_INFO[dt].name;
}

u8 dtype_kind(u32 dt) {
    if (dt >= 32) return DK_RESERVED;
    return DTYPE_INFO[dt].kind;
}

int dtype_is_float(u32 dt) {
    u8 k = dtype_kind(dt);
    return k == DK_FLOAT || k == DK_FP16 || k == DK_BF16 || k == DK_FP8;
}

int dtype_is_int(u32 dt) {
    u8 k = dtype_kind(dt);
    return k == DK_SINT || k == DK_UINT || k == DK_INT4 || k == DK_UINT4;
}

int dtype_is_signed(u32 dt) {
    if (dt >= 32) return 0;
    return DTYPE_INFO[dt].is_signed;
}

int dtype_is_bool(u32 dt) {
    return dtype_kind(dt) == DK_BOOL;
}

int dtype_is_packed(u32 dt) {
    u8 k = dtype_kind(dt);
    return k == DK_INT4 || k == DK_UINT4;
}
