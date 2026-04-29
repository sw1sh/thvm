// codegen/cg.c - backend-independent kernel-program emitter.
//
// Walks a KernelEntry's program[] and dispatches each KProgOp to a
// Renderer, which knows how to emit code for one target language
// (C, MSL, ...).  The emitter owns the high-level structure (function
// preamble, loop opening, per-op temporaries, loop closing, function
// postamble); the Renderer fills in the target-specific syntax
// (signature, intrinsic names, broadcast access, loop headers).
//
// Two emission modes:
//
//   1. "elementwise": program contains only CONST + ALU ops.  Single
//      per-output loop binds `i = 0..numel-1`; each op writes a
//      temporary; the last temporary is stored to `out[i]`.
//
//   2. "reduce-tail": last op is a REDUCE; everything before it is
//      elementwise.  Outer loop iterates over output elements (`oi`);
//      an inner k-loop accumulates over the reduction axis;
//      inside the inner loop `i` shadows oi to point at the source
//      index, so existing per-op emitters work unchanged.
//
// Anything outside these two shapes (movement ops, multiple REDUCEs,
// non-tail REDUCE) bails at cg_supports and the caller falls back to
// the interpreter / per-op shaders.

#include <stdarg.h>

// === growable text buffer ==============================================

typedef struct { char *buf; u32 cap; u32 len; } CgBuf;

static int cg_append(CgBuf *b, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  u32 remain = b->cap - b->len;
  int n = vsnprintf(b->buf + b->len, remain, fmt, ap);
  va_end(ap);
  if (n < 0) return 0;
  if ((u32)n >= remain) {
    u32 want = b->len + (u32)n + 1;
    u32 cap  = b->cap;
    while (cap < want) cap *= 2;
    char *nb = (char *)realloc(b->buf, cap);
    if (!nb) return 0;
    b->buf = nb;
    b->cap = cap;
    va_start(ap, fmt);
    vsnprintf(b->buf + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
  }
  b->len += (u32)n;
  return 1;
}

// === Renderer interface ================================================
//
// Each method emits target-language tokens into the buffer.  Methods
// are stateless w.r.t. each other; their only "context" is the
// arguments the emitter passes (input numel for broadcast, src/dst
// indices, etc.).
//
// Naming convention in the generated source:
//   - Inputs named in0, in1, ... (one pointer per ke->n_inputs).
//   - Output named `out` (single pointer).
//   - Per-op temporaries named r0, r1, ..., r{n_ops-1}.
//   - Iteration variable named `i` -- in elementwise mode this is the
//     thread/loop index 0..n-1; in reduce mode it's shadowed inside
//     the inner k-loop to point at the source index, so per-op input
//     refs (`in0[i]`) keep working unchanged.

typedef struct Renderer {
  // Function preamble: includes / using-declarations + signature +
  // input pointer unpack (no loop yet).
  void (*prologue)(CgBuf *b, u32 n_inputs);

  // Function postamble: closing brace, etc.
  void (*epilogue)(CgBuf *b, u32 n_inputs);

  // Open/close the per-output loop in the elementwise case.  open
  // binds `i` as 0..n-1.  close emits `out[i] = r{step};` and closes
  // the loop.
  void (*loop_open_elementwise) (CgBuf *b);
  void (*loop_close_elementwise)(CgBuf *b, u32 last_step);

  // Open the reduce-tail loop nest: outer over `oi`, inner over `_k`.
  // Declares `acc` (zero for SUM, -INFINITY for MAX) and binds `i`
  // inside the inner loop to the source index.  Caller passes the
  // REDUCE op's kind / inner / axis_size pre-decoded.
  void (*loop_open_reduce)(CgBuf *b, u8 kind, u32 inner, u32 axis_size);

  // Close the reduce-tail loop: emit the accumulator update reading
  // `reduce_src_raw` (input slot or program-step result), close the
  // inner k-loop, store `out[oi] = acc;`, close the outer loop.
  void (*loop_close_reduce)(CgBuf *b, u32 reduce_src_raw, u8 kind,
                            u32 const *in_numels);

  // CONST temporary: `<dtype> r{step} = <const>;`
  void (*emit_const)(CgBuf *b, u32 step, u32 dtype, u32 bits);

  // Binary ALU: `<dtype> r{step} = <lhs> <op> <rhs>;`.  Compares
  // become `(... < ...) ? 1.0f : 0.0f`.
  void (*emit_binary)(CgBuf *b, u32 step, u8 opcode,
                      u32 src_a, u32 src_b, u32 const *in_numels);

  // Unary ALU: `<dtype> r{step} = op(<src>);`
  void (*emit_unary)(CgBuf *b, u32 step, u8 opcode,
                     u32 src, u32 const *in_numels);
} Renderer;

// === supported-op predicate ============================================
//
// Accepts CONST + the elementwise ALU set anywhere; accepts REDUCE
// only as the last op (and only SUM / MAX kinds).  Anything else --
// movement, multi-REDUCE, mid-program REDUCE -- bails and the caller
// falls back to the interpreter.

int cg_supports(KernelEntry const *ke) {
  for (u32 i = 0; i < ke->n_ops; i++) {
    u8 op = ke->program[i].opcode;
    if (op == UOP_REDUCE) {
      // REDUCE has to be the last op (the "reduce-tail" pattern --
      // outer per-output loop + inner accumulator).  A REDUCE
      // followed by more ops would need a two-pass shader.
      if (i + 1 != ke->n_ops) return 0;
      u8 kind = (u8)((ke->program[i].arg >> 24) & 0xFFu);
      if (kind != REDUCE_SUM && kind != REDUCE_MAX) return 0;
    } else {
      switch (op) {
        case UOP_CONST:
        case UOP_ADD: case UOP_MUL:
        case UOP_NEG: case UOP_RECIP: case UOP_SQRT:
        case UOP_EXP2: case UOP_LOG2:
        case UOP_CMPLT: case UOP_CMPEQ:
          break;
        default:
          return 0;
      }
    }
    if (ke->program[i].dtype != DT_F32) return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_F32) return 0;
  }
  return 1;
}

// === driver: walk ke->program[] using a Renderer =======================
//
// Returned char* is owned by the caller (free).  Returns NULL on
// codegen failure (unsupported op or buffer alloc OOM).

fn char *cg_emit(KernelEntry const *ke, Renderer const *r) {
  if (!cg_supports(ke)) return NULL;
  CgBuf b = { .buf = (char *)malloc(4096), .cap = 4096, .len = 0 };
  if (!b.buf) return NULL;

  int has_reduce_tail = ke->n_ops > 0
                     && ke->program[ke->n_ops - 1].opcode == UOP_REDUCE;
  u32 chain_end = has_reduce_tail ? ke->n_ops - 1 : ke->n_ops;

  r->prologue(&b, ke->n_inputs);

  if (has_reduce_tail) {
    KProgOp const *rd = &ke->program[ke->n_ops - 1];
    u8  kind  = (u8)((rd->arg >> 24) & 0xFFu);
    u32 inner = rd->arg & 0xFFFFFFu;
    if (inner == 0) inner = 1;
    // Recover axis_size = src_numel / out_numel.  src is either an
    // input or an earlier program slot; both numel sources sit in ke.
    u32 src_numel;
    {
      u32 raw = rd->src[0];
      if (KSRC_IS_INPUT(raw)) src_numel = ke->input_numels[KSRC_INDEX(raw)];
      else                    src_numel = ke->program[KSRC_INDEX(raw)].numel;
    }
    u32 out_numel = rd->numel ? rd->numel : 1;
    u32 axis_size = src_numel / out_numel;
    r->loop_open_reduce(&b, kind, inner, axis_size);
  } else {
    r->loop_open_elementwise(&b);
  }

  for (u32 step = 0; step < chain_end; step++) {
    KProgOp const *p = &ke->program[step];
    switch (p->opcode) {
      case UOP_CONST:
        r->emit_const(&b, step, p->dtype, p->arg);
        break;
      case UOP_ADD: case UOP_MUL:
      case UOP_CMPLT: case UOP_CMPEQ:
        r->emit_binary(&b, step, p->opcode, p->src[0], p->src[1],
                       ke->input_numels);
        break;
      case UOP_NEG: case UOP_RECIP: case UOP_SQRT:
      case UOP_EXP2: case UOP_LOG2:
        r->emit_unary(&b, step, p->opcode, p->src[0],
                      ke->input_numels);
        break;
      default:
        free(b.buf);
        return NULL;
    }
  }

  if (has_reduce_tail) {
    KProgOp const *rd = &ke->program[ke->n_ops - 1];
    u8 kind = (u8)((rd->arg >> 24) & 0xFFu);
    r->loop_close_reduce(&b, rd->src[0], kind, ke->input_numels);
  } else {
    r->loop_close_elementwise(&b, ke->n_ops - 1);
  }

  r->epilogue(&b, ke->n_inputs);
  return b.buf;
}
