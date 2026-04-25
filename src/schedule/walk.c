// schedule/walk.c - heap-walk materialize.
//
// Drives materialize_uop_in_env over every reachable cell from a
// root, in-place rewriting UOPs into UOP_KERNELs.  Bottom-up:
// children get walked first so the parent can read their (now
// possibly UOP_KERNEL) cell content when it goes to kernelise
// itself.
//
// Walks through:
//   - LAM       : descend into body cell (single-cell layout).
//   - APP       : peek arg's shape, push (LAM_binder_loc -> shape)
//                 onto the env, recurse into body with the
//                 extended env.  Recurse into f / arg cells
//                 normally so other UOPs in them get processed.
//   - REF       : walk the def's book template cells once per
//                 name; mutates BOOK_HEAP via book_set.
//   - ALO       : force one layer (memoised), recurse on result.
//   - UOP       : recurse into compute slots, then attempt to
//                 materialize_uop_in_env -- on success the UOP's
//                 cell at the parent gets rewritten to UOP_KERNEL.
//   - others    : leaves; return unchanged.

static u8 BOOK_REF_VISITED[DEFS_CAP];
static u8 WALK_IN_BOOK = 0;          // toggled when we descend into a book template

static Term materialize_walk_rec(Term t, u32 env_id);

static Term walk_dyn_cell(u64 loc, u32 env_id) {
    Term old = heap_read(loc);
    Term nw  = materialize_walk_rec(old, env_id);
    if (nw != old) heap_set(loc, nw);
    return nw;
}

static Term walk_book_cell(u64 loc, u32 env_id) {
    Term old = book_read(loc);
    u8 prev = WALK_IN_BOOK;
    WALK_IN_BOOK = 1;
    Term nw = materialize_walk_rec(old, env_id);
    WALK_IN_BOOK = prev;
    if (nw != old) book_set(loc, nw);
    return nw;
}

static Term walk_cell(u64 loc, u32 env_id) {
    return WALK_IN_BOOK ? walk_book_cell(loc, env_id) : walk_dyn_cell(loc, env_id);
}

static Term materialize_walk_rec(Term t, u32 env_id) {
    u8 tag = term_tag(t);
    switch (tag) {
        case TAG_LAM: {
            walk_cell(term_val(t), env_id);
            return t;
        }

        case TAG_APP: {
            u64  loc = term_val(t);
            Term f   = heap_read(loc);
            Term arg = heap_read(loc + 1);
            // Resolve f one layer (VAR/ALO) so we can peek a LAM head.
            Term fr  = term_resolve(f);
            u32  body_env = env_id;
            if (term_tag(fr) == TAG_LAM) {
                Shape arg_shape;
                if (term_shape_in(arg, env_id, &arg_shape)) {
                    body_env = shape_env_push(env_id, term_val(fr), arg_shape);
                }
                // Recurse into LAM body with extended env.
                walk_cell(term_val(fr), body_env);
            }
            // Recurse into f/arg cells under the *outer* env so we
            // don't double-bind.  walk_cell on the LAM cell again
            // would re-enter the body without env help, so guard
            // that case (the body cell was already walked above).
            if (term_tag(fr) != TAG_LAM) walk_cell(loc, env_id);
            walk_cell(loc + 1, env_id);
            return t;
        }

        case TAG_REF: {
            u32 name = term_ext(t);
            if (name >= DEFS_CAP || DEFS[name] == 0) return t;
            if (BOOK_REF_VISITED[name])              return t;
            BOOK_REF_VISITED[name] = 1;
            // Switch into book context and walk the def's root.
            // DEFS[name] is the root book term; if it's a compound
            // we walk its children via walk_book_cell.
            u8 prev = WALK_IN_BOOK;
            WALK_IN_BOOK = 1;
            (void)materialize_walk_rec(DEFS[name], env_id);
            WALK_IN_BOOK = prev;
            return t;
        }

        case TAG_ALO: {
            Term real = alo_force(t);
            return materialize_walk_rec(real, env_id);
        }

        case TAG_UOP: {
            u32 op = term_ext(t);
            if (op == UOP_KERNEL) return t;
            // GRAD is a pure rewrite that interact_grad fires
            // lazily on the original UOP structure of `y` and
            // `target`.  If we kernelize the children first,
            // grad_rec sees TAG_UOP_KERNEL and falls through to
            // the "unhandled" warning.  Leave GRAD's subtree
            // alone -- when wnf later fires this GRAD, the chain
            // rule's emitted graph (post-rewrite) gets walked on
            // its own pass via thvm_materialize.
            if (op == UOP_GRAD) return t;
            u8  ar  = uop_arity(op);
            u64 loc = term_val(t);
            for (u8 i = 0; i < ar; i++) walk_cell(loc + i, env_id);
            // Inside a book template we MUST NOT kernelize: the
            // emitted UOP_KERNEL cell is heap_alloc'd in the dyn
            // heap, and storing it back into a BOOK_HEAP cell would
            // leave a book term whose val points into dyn space --
            // alo_realize then book_read()s an out-of-bounds address
            // when it later instantiates the def.  Each TRef call
            // re-walks the post-instantiated body in dyn space, so
            // kernelisation happens there instead.
            if (WALK_IN_BOOK) return t;
            // Try to kernelise; if not, return unchanged for a
            // later pass (or for runtime materialize) to retry.
            return materialize_uop_in_env(t, env_id);
        }

        case TAG_OP2:
        case TAG_MAT:
        case TAG_SUP: {
            u64 loc = term_val(t);
            walk_cell(loc + 0, env_id);
            walk_cell(loc + 1, env_id);
            return t;
        }
        case TAG_DUP: {
            walk_cell(term_val(t), env_id);
            return t;
        }
        default:
            return t;
    }
}

// Public entry point.  Runs the heap walker from `root` and
// returns the (possibly rewritten) root term.
Term materialize_walk(Term root) {
    shape_env_reset();
    for (u32 i = 0; i < DEFS_CAP; i++) BOOK_REF_VISITED[i] = 0;
    WALK_IN_BOOK = 0;
    return materialize_walk_rec(root, 0);
}
