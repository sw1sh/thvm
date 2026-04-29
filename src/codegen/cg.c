// codegen/cg.c - backend-independent kernel-program emitter.
//
// Walks a KernelEntry's program[] and dispatches each KProgOp to a
// Renderer, which knows how to emit code for one target language
// (C, MSL, ...).  The emitter is responsible for the iteration
// order and the high-level structure (prologue, per-op body,
// epilogue, store).  The Renderer fills in the target-specific
// syntax (function signature, intrinsic names, broadcast access,
// ...).
//
// Scope today: pure elementwise + CONST chains.  REDUCE / movement
// ops bail at the predicate (cg_supports) so the caller falls back
// to the interpreter.  Adding REDUCE later means extending the IR
// (or rather, the per-op dispatch in cg_emit) with explicit
// accumulator slots; the Renderer interface stays the same.

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
//   - Loop induction var named `i`.

typedef struct Renderer {
  // Emit prologue: includes / using-declarations + function signature
  // + any "unpack inputs" boilerplate + the per-element loop opener.
  // After this, the emitter starts emitting per-op temporaries.
  void (*prologue)(CgBuf *b, u32 n_inputs);

  // Emit per-element loop closer + function close.
  void (*epilogue)(CgBuf *b, u32 n_inputs);

  // Emit a CONST temporary: `<dtype> r{step} = <const>;`
  void (*emit_const)(CgBuf *b, u32 step, u32 dtype, u32 bits);

  // Emit a binary ALU temporary: `<dtype> r{step} = <lhs> <op> <rhs>;`
  // or for compares, `<dtype> r{step} = (<lhs> <op> <rhs>) ? 1 : 0;`
  // src refs: KSRC_AS_INPUT(slot) for input-i references (use bcast
  // info from in_numels) or program slot index for r{j}.
  void (*emit_binary)(CgBuf *b, u32 step, u8 opcode,
                      u32 src_a, u32 src_b, u32 const *in_numels);

  // Emit a unary ALU temporary: `<dtype> r{step} = op(<src>);`.
  void (*emit_unary)(CgBuf *b, u32 step, u8 opcode,
                     u32 src, u32 const *in_numels);

  // Emit the final store: `out[i] = r{step};`
  void (*emit_store)(CgBuf *b, u32 step);
} Renderer;

// === supported-op predicate ============================================
//
// The current cg_emit / Renderer interface only handles CONST + the
// elementwise ALU set.  REDUCE and movement ops bail; the caller
// (cpu_jit_dispatch) falls back to the interpreter.

int cg_supports(KernelEntry const *ke) {
  for (u32 i = 0; i < ke->n_ops; i++) {
    u8 op = ke->program[i].opcode;
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

  r->prologue(&b, ke->n_inputs);
  for (u32 step = 0; step < ke->n_ops; step++) {
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
  r->emit_store(&b, ke->n_ops - 1);
  r->epilogue(&b, ke->n_inputs);
  return b.buf;
}
