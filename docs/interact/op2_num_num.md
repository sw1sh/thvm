# OP2-NUM-NUM

Source: [src/interact/op2_num_num.c](../../src/interact/op2_num_num.c).
Tested by [tests/test_mat_op2.c](../../tests/test_mat_op2.c).

## Rule

```
(#a op #b)
---------- OP2-NUM-NUM
#(a op b)
```

## Implementation

`interact_op2_num_num(op, dtype, a, b, op2_loc)` folds two u32 operands
and returns a NUM in `dtype` (the left operand's). Division and modulus
by zero give 0, and shift counts wrap at 32. wnf fires it from both of
its strict OP2 frames (left operand reduced first, then the right), and
`redex_fire` fires it when both operand cells already hold NUMs.

## Cost

- One `ITRS++`; no heap access, no allocation
