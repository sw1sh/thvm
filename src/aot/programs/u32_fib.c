// aot/programs/u32_fib.c -- hand-coded AOT for the bench/u32_fib
// case: naive Fibonacci on native U32.
//
// Source: HVM4
//   @u32_fib = lam {
//     0: 0;
//     1: 1;
//     lam &k. fib(k - 1) + fib(k - 2)
//   }
//   @main = u32_fib(38)
//
// One def, one entry point, single-arg dispatch on a NUM head
// (TMatNum, not CTR label).  Recursive calls operate on FRESH
// NUMs computed via OP2 SUB -- there's no passed-through arg
// across the recursion, so the AOT-friendly "case-tree to C"
// translation works cleanly here.  Compare with gab_tak.c, where
// the 3-way recursion shares lazy args across sub-calls and the
// interpreter's lazy ALO chain wins.
//
// Tree size: u32_fib(38) is a Fibonacci-shaped tree of ~63M
// recursive calls, so we want this fast.  The whole point of the
// AOT layer.
//
// Skip the auto-dup that HVM4's `lambda &k` would insert -- k is
// used twice (in `fib(k-1)` and `fib(k-2)`) but k is a NUM atom,
// and forcing a NUM is free; sharing the C-level Term value
// across both uses is correct + fast.

static u32 G_DEF_U32_FIB = (u32)-1;

// Forward decl.
static Term aot_fn_u32_fib_direct(Term n);

// ---------- @u32_fib direct entry point ----------

static Term aot_fn_u32_fib_direct(Term n) {
  n = aot_force(n);
  if (term_tag(n) != TAG_NUM) {
    return aot_new_app(term_new(0, TAG_REF, G_DEF_U32_FIB, 0), n);
  }
  u32 v = (u32)term_val(n);
  u32 dtype = term_ext(n);   // preserve dtype across the result
  if (v == 0) {
    // MAT-NUM-MAT (label 0).
    ITRS += 1;
    return term_new(0, TAG_NUM, dtype, 0);
  }
  if (v == 1) {
    // MAT-NUM-MIS (label 0 misses) + MAT-NUM-MAT (label 1).
    ITRS += 2;
    return term_new(0, TAG_NUM, dtype, 1);
  }
  // Default arm: lam &k. fib(k - 1) + fib(k - 2)
  // Cost: 2 MAT-MIS (labels 0, 1) + APP-LAM (k binder).
  ITRS += 3;
  // Compute k - 1 and k - 2 directly -- both are NUM-NUM ops, so
  // the interpreter would fire two OP2-NUM-NUM steps; bump ITRS
  // accordingly.  Using v - 2 for the deeper recursion (avoiding
  // unsigned underflow because we already filtered v < 2 above).
  ITRS += 1;
  Term arg1 = term_new(0, TAG_NUM, dtype, v - 1);
  ITRS += 1;
  Term arg2 = term_new(0, TAG_NUM, dtype, v - 2);

  Term sub1 = aot_fn_u32_fib_direct(arg1);
  Term sub2 = aot_fn_u32_fib_direct(arg2);
  // Both sub-results should be NUMs by construction; force just
  // to be safe (a no-op when they already are).
  sub1 = aot_force(sub1);
  sub2 = aot_force(sub2);
  if (term_tag(sub1) == TAG_NUM && term_tag(sub2) == TAG_NUM) {
    // OP2-NUM-NUM (the outer ADD).
    ITRS += 1;
    return term_new(0, TAG_NUM, term_ext(sub1),
                    (u32)term_val(sub1) + (u32)term_val(sub2));
  }
  // Lazy fallback if either side didn't reduce -- build the OP2
  // and let the interpreter handle it.
  u64 op_loc = heap_alloc(2);
  heap_set(op_loc + 0, sub1);
  heap_set(op_loc + 1, sub2);
  return term_new(0, TAG_OP2, OP_ADD, op_loc);
}

// ---------- WNF stack entry point ----------

static Term aot_fn_u32_fib(Term *stack, u32 *sp, u32 base) {
  Term n = aot_pop_app_arg(stack, sp, base);
  if (n == 0) return aot_fallback(G_DEF_U32_FIB);
  return aot_fn_u32_fib_direct(n);
}

// ---------- Registration ----------

fn void aot_program_u32_fib_register(u32 def_u32_fib) {
  G_DEF_U32_FIB = def_u32_fib;
  aot_register(def_u32_fib, aot_fn_u32_fib);
}
