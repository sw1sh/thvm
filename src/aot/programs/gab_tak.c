// aot/programs/gab_tak.c -- hand-coded AOT for the bench/gab_tak
// (Gabriel TAK / Takeuchi function on Peano nats).
//
// Five defs (@pred, @lte, @tak_go, @tak, @u32_to) compiled to
// specialised C functions.  Conventions:
//   ZER = label 0   SUC = label 1   T = label 2   F = label 3
//
// PERFORMANCE CAVEAT.  Unlike fib_nat (where AOT delivers ~14x
// speedup), this AOT is FASTER than the interpreter only for small
// inputs (tak(5,4,3): 1.34x) and SLOWER for large ones (tak(12,8,4):
// 0.37x; tak(20,12,6): heap exhaustion).  Reason: the interpreter's
// lazy ALO chain memoises shared sub-computations across the 3-way
// recursion in tak's F case, but my AOT bypasses ALO entirely and
// recurses directly in C without that memoisation.  Adding eager
// auto-dups (which is what HVM4's `lambda &x` would do) makes it
// worse, not better -- DUP-CTR fires through the recursion explode.
//
// What this case-tree style AOT is good for: programs whose
// recursive sub-calls work on FRESH terms (built from constants,
// not passed-through args).  fib_nat is one; gab_tak is not.  The
// "right" AOT for tak would either hoist common sub-expressions
// or fall back to the interpreter's lazy machinery for the deep
// recursive case.  Both are out of scope for this hand-coded
// proof-of-concept; the auto-emitter that lands later will need to
// recognise this pattern and either skip AOT or use a different
// codegen strategy for it.
//
// Despite the perf caveat, this file demonstrates that the AOT
// mechanism handles 5 inter-call defs, multiple CTR labels (T/F
// in addition to ZER/SUC), 4-arg dispatch (tak_go), and a tail
// loop (lte) -- all the mechanical bits the auto-emitter will
// need to support.
//
// Differences from HVM4's source:
//   - HVM4's `lambda &x` auto-inserts DUPs.  We share the C-level
//     Term value across uses instead.  Functionally correct
//     because pred / lte / tak only inspect their args via
//     aot_force (read-only on CTRs / NUMs).  ITRS counts diverge
//     from HVM4 by a constant factor per call.
//
// Tail-recursion: @lte loops on (a, b), @u32_to is genuinely
// recursive (peels SUC, recurses, then ADDs 1).  @tak's F-case
// recursion is depth-3 (three inner taks + one outer); the C
// stack grows linearly with the call depth, which the Takeuchi
// recursion bounds by ~max(args).

// Constructor labels (matched against the WL setup in the bench file).
// We DON'T share LBL_ZER / LBL_SUC with fib_nat.c because ABI: each
// program is independently compiled and may pick its own labels via
// the bench setup.  Constants here happen to match fib_nat's choices.
#ifndef GAB_TAK_LBL_ZER
#define GAB_TAK_LBL_ZER 0
#define GAB_TAK_LBL_SUC 1
#define GAB_TAK_LBL_T   2
#define GAB_TAK_LBL_F   3
#endif

// Globals filled in by the registration call.
static u32 G_DEF_PRED   = (u32)-1;
static u32 G_DEF_LTE    = (u32)-1;
static u32 G_DEF_TAK_GO = (u32)-1;
static u32 G_DEF_TAK    = (u32)-1;
static u32 G_DEF_U32_TO = (u32)-1;

// Forward decls -- all five functions can call any of the others.
static Term aot_fn_pred_direct(Term n);
static Term aot_fn_lte_direct(Term a, Term b);
static Term aot_fn_tak_go_direct(Term cmp, Term x, Term y, Term z);
static Term aot_fn_tak_direct(Term x, Term y, Term z);
static Term aot_fn_u32_to_direct(Term n);

// ---------- @pred direct entry point ----------
//
// pred(ZER) = ZER
// pred(SUC{p}) = p

static Term aot_fn_pred_direct(Term n) {
  n = aot_force(n);
  if (term_tag(n) != TAG_CTR) {
    return aot_new_app(term_new(0, TAG_REF, G_DEF_PRED, 0), n);
  }
  u32 ext = term_ext(n);
  if (ext == GAB_TAK_LBL_ZER) {
    ITRS += 1;
    return term_new_ctr(GAB_TAK_LBL_ZER, NULL, 0);
  }
  if (ext == GAB_TAK_LBL_SUC) {
    // MAT-MIS + MAT-MAT + APP-LAM (the unused-binder lambda).
    ITRS += 3;
    return term_ctr_at(n, 0);
  }
  return aot_new_app(term_new(0, TAG_REF, G_DEF_PRED, 0), n);
}

// ---------- @lte direct entry point ----------
//
// lte(ZER, b)         = T
// lte(SUC{p}, ZER)    = F
// lte(SUC{p}, SUC{q}) = lte(p, q)   -- tail-recurse

static Term aot_fn_lte_direct(Term a, Term b) {
lte_loop:
  a = aot_force(a);
  if (term_tag(a) != TAG_CTR) {
    return aot_new_app(aot_new_app(term_new(0, TAG_REF, G_DEF_LTE, 0), a), b);
  }
  u32 ext_a = term_ext(a);
  if (ext_a == GAB_TAK_LBL_ZER) {
    // lte(ZER, b) = T.  MAT-MAT + APP-LAM (b unused).
    ITRS += 2;
    return term_new_ctr(GAB_TAK_LBL_T, NULL, 0);
  }
  if (ext_a != GAB_TAK_LBL_SUC) {
    return aot_new_app(aot_new_app(term_new(0, TAG_REF, G_DEF_LTE, 0), a), b);
  }
  // a == SUC{p}.  Cost: outer MIS + MAT-MAT + APP-LAM (a's binder).
  ITRS += 3;
  Term p = term_ctr_at(a, 0);
  Term b_w = aot_force(b);
  if (term_tag(b_w) != TAG_CTR) {
    return aot_new_app(aot_new_app(term_new(0, TAG_REF, G_DEF_LTE, 0), a), b);
  }
  u32 ext_b = term_ext(b_w);
  if (ext_b == GAB_TAK_LBL_ZER) {
    // inner MAT-CTR-MAT (ZER) + APP-LAM (b binder, unused).
    ITRS += 2;
    return term_new_ctr(GAB_TAK_LBL_F, NULL, 0);
  }
  if (ext_b == GAB_TAK_LBL_SUC) {
    // inner MIS + MAT-MAT + APP-LAM (q binder).
    ITRS += 3;
    Term q = term_ctr_at(b_w, 0);
    a = p;
    b = q;
    goto lte_loop;
  }
  return aot_new_app(aot_new_app(term_new(0, TAG_REF, G_DEF_LTE, 0), a), b);
}

// ---------- @tak_go direct entry point ----------
//
// tak_go(T, x, y, z) = y
// tak_go(F, x, y, z) =
//     let a = tak(pred(x), y, z)
//     let b = tak(pred(y), z, x)
//     let c = tak(pred(z), x, y)
//     in  tak(a, b, c)
//
// AOT skips the auto-dups HVM4 inserts for `lambda &x`.  Adding dups
// here turned out to be a pessimization (eager DUP-CTR fires through
// the 3-way recursion explode the work) -- the interpreter's lazy
// ALO chain is actually optimal for this access pattern.  See the
// note at the top of the file.  Sharing C-level Term values across
// the three sub-calls is correct (SUB-resolution makes repeated
// forces of the same heap loc free) but ITRS counts diverge from
// HVM4 because we don't fire the auto-dup-induced DUP-NODs.

static Term aot_fn_tak_go_direct(Term cmp, Term x, Term y, Term z) {
  cmp = aot_force(cmp);
  if (term_tag(cmp) != TAG_CTR) {
    Term ref = term_new(0, TAG_REF, G_DEF_TAK_GO, 0);
    return aot_new_app(aot_new_app(aot_new_app(aot_new_app(ref, cmp), x), y), z);
  }
  u32 ext = term_ext(cmp);
  if (ext == GAB_TAK_LBL_T) {
    // MAT-CTR-MAT (T) + 3 * APP-LAM (x/y/z binders; x and z unused).
    ITRS += 4;
    return y;
  }
  if (ext == GAB_TAK_LBL_F) {
    // MAT-MIS + MAT-MAT + 3 * APP-LAM.
    ITRS += 5;
    Term a = aot_fn_tak_direct(aot_fn_pred_direct(x), y, z);
    Term b = aot_fn_tak_direct(aot_fn_pred_direct(y), z, x);
    Term c = aot_fn_tak_direct(aot_fn_pred_direct(z), x, y);
    return aot_fn_tak_direct(a, b, c);
  }
  Term ref = term_new(0, TAG_REF, G_DEF_TAK_GO, 0);
  return aot_new_app(aot_new_app(aot_new_app(aot_new_app(ref, cmp), x), y), z);
}

// ---------- @tak direct entry point ----------
//
// tak(x, y, z) = tak_go(lte(x, y), x, y, z)
//
// Shares x / y across lte and tak_go without dups -- see the note
// at tak_go above.

static Term aot_fn_tak_direct(Term x, Term y, Term z) {
  // 3 * APP-LAM (x, y, z binders).
  ITRS += 3;
  Term cmp = aot_fn_lte_direct(x, y);
  return aot_fn_tak_go_direct(cmp, x, y, z);
}

// ---------- @u32_to direct entry point ----------
//
// u32_to(ZER)     = NUM(0)
// u32_to(SUC{p})  = u32_to(p) + 1

static Term aot_fn_u32_to_direct(Term n) {
  n = aot_force(n);
  if (term_tag(n) != TAG_CTR) {
    return aot_new_app(term_new(0, TAG_REF, G_DEF_U32_TO, 0), n);
  }
  u32 ext = term_ext(n);
  if (ext == GAB_TAK_LBL_ZER) {
    ITRS += 1;
    return aot_num_i32(0);
  }
  if (ext == GAB_TAK_LBL_SUC) {
    // MAT-MIS + MAT-MAT + APP-LAM.
    ITRS += 3;
    Term p = term_ctr_at(n, 0);
    Term sub = aot_fn_u32_to_direct(p);
    sub = aot_force(sub);
    if (term_tag(sub) == TAG_NUM) {
      ITRS += 1;
      return term_new(0, TAG_NUM, term_ext(sub), (u32)term_val(sub) + 1);
    }
    // Lazy OP2 if sub didn't reduce.
    Term one = aot_num_i32(1);
    u64 op_loc = heap_alloc(2);
    heap_set(op_loc + 0, sub);
    heap_set(op_loc + 1, one);
    return term_new(0, TAG_OP2, OP_ADD, op_loc);
  }
  return aot_new_app(term_new(0, TAG_REF, G_DEF_U32_TO, 0), n);
}

// ---------- WNF stack entry points ----------

static Term aot_fn_pred(Term *stack, u32 *sp, u32 base) {
  Term n = aot_pop_app_arg(stack, sp, base);
  if (n == 0) return aot_fallback(G_DEF_PRED);
  return aot_fn_pred_direct(n);
}

static Term aot_fn_lte(Term *stack, u32 *sp, u32 base) {
  Term a = aot_pop_app_arg(stack, sp, base);
  if (a == 0) return aot_fallback(G_DEF_LTE);
  Term b = aot_pop_app_arg(stack, sp, base);
  if (b == 0) {
    aot_push_app_arg(stack, sp, base, a);
    return aot_fallback(G_DEF_LTE);
  }
  return aot_fn_lte_direct(a, b);
}

static Term aot_fn_tak_go(Term *stack, u32 *sp, u32 base) {
  Term cmp = aot_pop_app_arg(stack, sp, base);
  if (cmp == 0) return aot_fallback(G_DEF_TAK_GO);
  Term x = aot_pop_app_arg(stack, sp, base);
  if (x == 0) {
    aot_push_app_arg(stack, sp, base, cmp);
    return aot_fallback(G_DEF_TAK_GO);
  }
  Term y = aot_pop_app_arg(stack, sp, base);
  if (y == 0) {
    aot_push_app_arg(stack, sp, base, x);
    aot_push_app_arg(stack, sp, base, cmp);
    return aot_fallback(G_DEF_TAK_GO);
  }
  Term z = aot_pop_app_arg(stack, sp, base);
  if (z == 0) {
    aot_push_app_arg(stack, sp, base, y);
    aot_push_app_arg(stack, sp, base, x);
    aot_push_app_arg(stack, sp, base, cmp);
    return aot_fallback(G_DEF_TAK_GO);
  }
  return aot_fn_tak_go_direct(cmp, x, y, z);
}

static Term aot_fn_tak(Term *stack, u32 *sp, u32 base) {
  Term x = aot_pop_app_arg(stack, sp, base);
  if (x == 0) return aot_fallback(G_DEF_TAK);
  Term y = aot_pop_app_arg(stack, sp, base);
  if (y == 0) {
    aot_push_app_arg(stack, sp, base, x);
    return aot_fallback(G_DEF_TAK);
  }
  Term z = aot_pop_app_arg(stack, sp, base);
  if (z == 0) {
    aot_push_app_arg(stack, sp, base, y);
    aot_push_app_arg(stack, sp, base, x);
    return aot_fallback(G_DEF_TAK);
  }
  return aot_fn_tak_direct(x, y, z);
}

static Term aot_fn_u32_to(Term *stack, u32 *sp, u32 base) {
  Term n = aot_pop_app_arg(stack, sp, base);
  if (n == 0) return aot_fallback(G_DEF_U32_TO);
  return aot_fn_u32_to_direct(n);
}

// ---------- Registration ----------

fn void aot_program_gab_tak_register(u32 def_pred, u32 def_lte, u32 def_tak_go,
                                     u32 def_tak, u32 def_u32_to) {
  G_DEF_PRED   = def_pred;
  G_DEF_LTE    = def_lte;
  G_DEF_TAK_GO = def_tak_go;
  G_DEF_TAK    = def_tak;
  G_DEF_U32_TO = def_u32_to;
  aot_register(def_pred,   aot_fn_pred);
  aot_register(def_lte,    aot_fn_lte);
  aot_register(def_tak_go, aot_fn_tak_go);
  aot_register(def_tak,    aot_fn_tak);
  aot_register(def_u32_to, aot_fn_u32_to);
}
