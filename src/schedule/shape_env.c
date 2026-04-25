// schedule/shape_env.c - shape environment threaded through the
// heap-walk materialize pass.
//
// As the walker descends through APP[LAM, arg] pairs, it
// statically propagates `arg`'s shape onto the LAM's binder loc
// so a deeper VAR(binder_loc) reference inside a UOP graph can be
// kernelised with a known output shape -- even before wnf has
// performed the actual beta substitution.  Mirrors TinyHVM's
// shape inference; the env stays small (one binding per active
// LAM scope).
//
// Linked-list discipline: a child node points at its parent so
// pushing a new binding doesn't disturb existing ones.  ID 0
// represents the empty environment.

typedef struct {
    u32   parent;
    u64   var_loc;     // LAM binder heap loc (= where APP-LAM substitutes)
    Shape shape;
} ShapeBinding;

#define SHAPE_ENV_CAP (1ULL << 14)

static ShapeBinding SHAPE_ENV[SHAPE_ENV_CAP];
static u32          SHAPE_ENV_NEXT = 1;

fn u32 shape_env_push(u32 parent, u64 var_loc, Shape shape) {
    if (SHAPE_ENV_NEXT >= SHAPE_ENV_CAP) {
        fprintf(stderr, "shape_env_push: overflow\n");
        exit(1);
    }
    u32 id = SHAPE_ENV_NEXT++;
    SHAPE_ENV[id].parent  = parent;
    SHAPE_ENV[id].var_loc = var_loc;
    SHAPE_ENV[id].shape   = shape;
    return id;
}

fn int shape_env_lookup(u32 env_id, u64 var_loc, Shape *out) {
    for (u32 cur = env_id; cur != 0; cur = SHAPE_ENV[cur].parent) {
        if (SHAPE_ENV[cur].var_loc == var_loc) {
            *out = SHAPE_ENV[cur].shape;
            return 1;
        }
    }
    return 0;
}

fn void shape_env_reset(void) {
    SHAPE_ENV_NEXT = 1;
}

// Best-effort shape lookup for a Term given env.  Handles TEN
// (lookup TENS), VAR (lookup env), and UOP_KERNEL (read its
// output_tid).  Returns 1 on success.
fn int term_shape_in(Term t, u32 env_id, Shape *out) {
    u8 tag = term_tag(t);
    if (tag == TAG_TEN) {
        u32 tid = (u32)term_val(t);
        if (tid != 0 && tid < TENS_NEXT) {
            *out = TENS[tid].view.shape;
            return 1;
        }
        return 0;
    }
    if (tag == TAG_VAR) {
        return shape_env_lookup(env_id, term_val(t), out);
    }
    if (tag == TAG_UOP && term_ext(t) == UOP_KERNEL) {
        Term outbuf = heap_read(term_val(t));
        if (term_tag(outbuf) == TAG_TEN) {
            u32 tid = (u32)term_val(outbuf);
            if (tid != 0 && tid < TENS_NEXT) {
                *out = TENS[tid].view.shape;
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

// Best-effort dtype lookup similarly.
fn int term_dtype_in(Term t, u32 env_id, u32 *out) {
    (void)env_id;
    u8 tag = term_tag(t);
    if (tag == TAG_TEN) {
        u32 tid = (u32)term_val(t);
        if (tid != 0 && tid < TENS_NEXT) {
            *out = TENS[tid].dtype;
            return 1;
        }
    }
    if (tag == TAG_UOP && term_ext(t) == UOP_KERNEL) {
        Term outbuf = heap_read(term_val(t));
        if (term_tag(outbuf) == TAG_TEN) {
            u32 tid = (u32)term_val(outbuf);
            if (tid != 0 && tid < TENS_NEXT) {
                *out = TENS[tid].dtype;
                return 1;
            }
        }
    }
    // VAR / NUM / etc -- assume f32 until fire time tells us otherwise.
    *out = DT_F32;
    return 1;
}
