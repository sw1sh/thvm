// aot/programs/fib_nat.c -- hand-coded AOT for the bench/fib_nat triple.
//
// Three defs (@add, @fib, @u32) are compiled to specialised C
// functions that mirror what HVM4's AOT emitter produces for the
// same source.  Reduction proceeds via direct C control flow:
//
//   - aot_fn_add: tail loop on (a, b), peeling off SUC's from a.
//   - aot_fn_fib: case-tree on (n) with native recursion for the
//     two sub-fib calls; the "! P &L = p" dup is built inline via
//     aot_make_dup() so DP0/DP1 share a body cell (matching the
//     interpreter's dup_share semantics).
//   - aot_fn_u32: tail loop on (nat, acc), folding 1+ into acc.
//
// ITRS counts are bumped to match HVM4 / interpreted thvm exactly:
//   - add(ZER, b):    +2  (MAT-MAT for ZER + APP-LAM for the b lam)
//   - add(SUC{x}, b): +4  (MAT-MIS + MAT-MAT + APP-LAM a + APP-LAM b)
//   - fib(ZER):       +1  (outer MAT-MAT for ZER, arity-0 no APP)
//   - fib(SUC{ZER}):  +3  (MIS + MAT for outer SUC + MAT for inner ZER)
//   - fib(SUC{SUC{p}}): +5  (MIS + MAT outer + MIS + MAT inner + APP-LAM p)
//                            then add(fib(SUC{p0}), fib(p1)) recursively
//   - u32(ZER, b):    +2
//   - u32(SUC{p}, b): +4 + 1 (OP2 fold)
//
// Stack/AOT contract: the top-level _stack variants pop their args
// from the WNF stack via aot_pop_app_arg(); when an arg is missing
// (partial application) or reduces to an unexpected tag, they fall
// back to aot_fallback() which rebuilds the lazy ALO and lets the
// interpreter finish the work.

// Globals filled in by the registration call.  -1 sentinel = unbound.
static u32 G_DEF_ADD = (u32)-1;
static u32 G_DEF_FIB = (u32)-1;
static u32 G_DEF_U32 = (u32)-1;
#define LBL_ZER 0
#define LBL_SUC 1
#define DUP_LBL 7

// Forward decls: fib calls add, both call themselves recursively.
static Term aot_fn_add_direct(Term a, Term b);
static Term aot_fn_fib_direct(Term n);
static Term aot_fn_u32_direct(Term n, Term acc);

// ---------- @add direct entry point ----------

static Term aot_fn_add_direct(Term a, Term b) {
add_loop:
  a = aot_force(a);
  if (term_tag(a) != TAG_CTR) {
    // Stuck arg -- rebuild APP(APP(REF[add], a), b) for the
    // interpreter to continue.
    return aot_new_app(aot_new_app(term_new(0, TAG_REF, G_DEF_ADD, 0), a), b);
  }
  u32 ext = term_ext(a);
  if (ext == LBL_ZER) {
    // add(ZER, b) = b.  MAT-CTR-MAT (ZER, arity 0) + APP-LAM (TLam[b,b]).
    ITRS += 2;
    return b;
  }
  if (ext == LBL_SUC) {
    // add(SUC{x}, b) = add(x, SUC{b}).  Tail-recurse.
    // Cost: MAT-MIS (ZER arm) + MAT-MAT (SUC arm) + APP-LAM a + APP-LAM b.
    ITRS += 4;
    Term x = term_ctr_at(a, 0);
    a = x;
    b = aot_wrap_ctr1(LBL_SUC, b);
    goto add_loop;
  }
  // Unknown ctor -- fall back.
  return aot_new_app(aot_new_app(term_new(0, TAG_REF, G_DEF_ADD, 0), a), b);
}

// ---------- @fib direct entry point ----------

static Term aot_fn_fib_direct(Term n) {
  n = aot_force(n);
  if (term_tag(n) != TAG_CTR) {
    return aot_new_app(term_new(0, TAG_REF, G_DEF_FIB, 0), n);
  }
  u32 ext = term_ext(n);
  if (ext == LBL_ZER) {
    // fib(ZER) = ZER.  outer MAT-CTR-MAT (ZER, arity 0).
    ITRS += 1;
    return term_new_ctr(LBL_ZER, NULL, 0);
  }
  if (ext != LBL_SUC) {
    return aot_new_app(term_new(0, TAG_REF, G_DEF_FIB, 0), n);
  }
  // ext == SUC.  outer MAT-MIS + MAT-MAT.
  ITRS += 2;
  Term inner = term_ctr_at(n, 0);
  inner = aot_force(inner);
  if (term_tag(inner) != TAG_CTR) {
    Term suc_inner = aot_wrap_ctr1(LBL_SUC, inner);
    return aot_new_app(term_new(0, TAG_REF, G_DEF_FIB, 0), suc_inner);
  }
  if (term_ext(inner) == LBL_ZER) {
    // fib(SUC{ZER}) = SUC{ZER}.  inner MAT-CTR-MAT (ZER).
    ITRS += 1;
    return aot_wrap_ctr1(LBL_SUC, term_new_ctr(LBL_ZER, NULL, 0));
  }
  if (term_ext(inner) != LBL_SUC) {
    Term suc_inner = aot_wrap_ctr1(LBL_SUC, inner);
    return aot_new_app(term_new(0, TAG_REF, G_DEF_FIB, 0), suc_inner);
  }
  // inner == SUC{p}.  fib(SUC{SUC{p}}) = add(fib(SUC{p0}), fib(p1)).
  // Cost: inner MAT-MIS + MAT-MAT + APP-LAM (suc_handler).
  ITRS += 3;
  Term p = term_ctr_at(inner, 0);
  Term p0, p1;
  aot_make_dup(DUP_LBL, p, &p0, &p1);
  Term fib_suc_p0 = aot_fn_fib_direct(aot_wrap_ctr1(LBL_SUC, p0));
  Term fib_p1     = aot_fn_fib_direct(p1);
  return aot_fn_add_direct(fib_suc_p0, fib_p1);
}

// ---------- @u32 direct entry point ----------

static Term aot_fn_u32_direct(Term n, Term acc) {
u32_loop:
  n = aot_force(n);
  if (term_tag(n) != TAG_CTR) {
    return aot_new_app(aot_new_app(term_new(0, TAG_REF, G_DEF_U32, 0), n), acc);
  }
  u32 ext = term_ext(n);
  if (ext == LBL_ZER) {
    // u32(ZER, acc) = acc.  MAT-MAT + APP-LAM (TLam[n, n]).
    ITRS += 2;
    // Force acc to a NUM if it's a pending OP2.
    return aot_force(acc);
  }
  if (ext == LBL_SUC) {
    // u32(SUC{p}, acc) = u32(p, 1+acc).  Tail-recurse.
    // Cost: MAT-MIS + MAT-MAT + APP-LAM (suc_handler) + APP-LAM (TLam[n, ...]).
    ITRS += 4;
    Term p = term_ctr_at(n, 0);
    // 1+acc: if acc is already NUM, fold the OP2 immediately
    // (saves an OP2 cell + a re-entry through the wnf interpreter).
    Term acc_w = aot_force(acc);
    if (term_tag(acc_w) == TAG_NUM) {
      ITRS += 1;   // OP2-NUM-NUM
      acc = term_new(0, TAG_NUM, term_ext(acc_w), 1 + (u32)term_val(acc_w));
    } else {
      Term one = aot_num_i32(1);
      u64 op_loc = heap_alloc(2);
      heap_set(op_loc + 0, one);
      heap_set(op_loc + 1, acc_w);
      acc = term_new(0, TAG_OP2, OP_ADD, op_loc);
    }
    n = p;
    goto u32_loop;
  }
  return aot_new_app(aot_new_app(term_new(0, TAG_REF, G_DEF_U32, 0), n), acc);
}

// ---------- WNF stack entry points ----------
//
// These are what AOT_FNS[def_id] points at.  Each pops its expected
// args from the WNF spine, calls the _direct variant, and returns
// the result.  When an expected arg isn't an APP (partial application),
// fall back to the lazy ALO path.

static Term aot_fn_add(Term *stack, u32 *sp, u32 base) {
  Term a = aot_pop_app_arg(stack, sp, base);
  if (a == 0) return aot_fallback(G_DEF_ADD);
  Term b = aot_pop_app_arg(stack, sp, base);
  if (b == 0) {
    // Partial: only `a` available.  Push it back and fall back.
    aot_push_app_arg(stack, sp, base, a);
    return aot_fallback(G_DEF_ADD);
  }
  return aot_fn_add_direct(a, b);
}

static Term aot_fn_fib(Term *stack, u32 *sp, u32 base) {
  Term n = aot_pop_app_arg(stack, sp, base);
  if (n == 0) return aot_fallback(G_DEF_FIB);
  return aot_fn_fib_direct(n);
}

static Term aot_fn_u32(Term *stack, u32 *sp, u32 base) {
  Term n = aot_pop_app_arg(stack, sp, base);
  if (n == 0) return aot_fallback(G_DEF_U32);
  Term acc = aot_pop_app_arg(stack, sp, base);
  if (acc == 0) {
    aot_push_app_arg(stack, sp, base, n);
    return aot_fallback(G_DEF_U32);
  }
  return aot_fn_u32_direct(n, acc);
}

// ---------- Registration ----------

fn void aot_program_fib_nat_register(u32 def_add, u32 def_fib, u32 def_u32) {
  G_DEF_ADD = def_add;
  G_DEF_FIB = def_fib;
  G_DEF_U32 = def_u32;
  aot_register(def_add, aot_fn_add);
  aot_register(def_fib, aot_fn_fib);
  aot_register(def_u32, aot_fn_u32);
}
