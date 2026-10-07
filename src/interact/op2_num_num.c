// (#a op #b)
// ---------- OP2-NUM-NUM
// #(a op b)
//
// u32 arithmetic in the left operand's dtype; division and modulus by
// zero give 0 and shift counts wrap at 32.  `op2_loc` is the OP2 cell,
// the event's wire carrier.
fn Term interact_op2_num_num(u32 op, u32 dtype, u32 x, u32 y, u64 op2_loc) {
  ITRS++;
  multi_emit(RULE_OP2_NUM_NUM, MULTI_TERM, op2_loc, op2_loc + 1, op);
  u32 r;
  switch (op) {
    case OP_ADD: r = x + y; break;
    case OP_SUB: r = x - y; break;
    case OP_MUL: r = x * y; break;
    case OP_EQ:  r = (x == y) ? 1 : 0; break;
    case OP_LT:  r = (x <  y) ? 1 : 0; break;
    case OP_DIV: r = (y == 0) ? 0 : x / y; break;
    case OP_MOD: r = (y == 0) ? 0 : x % y; break;
    case OP_XOR: r = x ^ y; break;
    case OP_AND: r = x & y; break;
    case OP_OR:  r = x | y; break;
    case OP_SHL: r = x << (y & 31); break;
    case OP_SHR: r = x >> (y & 31); break;
    case OP_GT:  r = (x >  y) ? 1 : 0; break;
    case OP_LE:  r = (x <= y) ? 1 : 0; break;
    case OP_GE:  r = (x >= y) ? 1 : 0; break;
    case OP_NE:  r = (x != y) ? 1 : 0; break;
    default:     r = 0; break;
  }
  return term_new(0, TAG_NUM, dtype, r);
}
