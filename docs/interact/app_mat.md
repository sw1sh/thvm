# APP-MAT

Source: [src/interact/app_mat.c](../../src/interact/app_mat.c), with the
SUP case in [src/interact/app_mat_sup.c](../../src/interact/app_mat_sup.c).
Tested by [tests/test_mat_op2.c](../../tests/test_mat_op2.c).

## Rule

```
(λ{#K:h; m} #K)
--------------- APP-MAT-NUM
h

(λ{#K:h; m} #K{a, b, ...})
-------------------------- APP-MAT-CTR
(h a b ...)

(λ{#K:h; m} &L{a, b})
--------------------- APP-MAT-SUP
! H &L = h
! M &L = m
&L{(λ{#K:H0; M0} a), (λ{#K:H1; M1} b)}

(λ{#K:h; m} x)
-------------- APP-MAT-MIS, for any other x
(m x)
```

A MAT is a one-case switch: handler `h` for the number or constructor
`#K`, fallback `m` for anything else.

## Implementation

`interact_app_mat(mat, arg)` takes the scrutinee already forced. Both
reducers force it the same way before calling: wnf (`src/wnf/_.c`) and
the single-redex step `redex_fire` (`src/wnf/redex.c`, behind nf and
TInteract) run `wnf` on the argument and drive a DP0/DP1 head through
`cnf`. Plain DP projections are Levy-opaque under wnf, so without the
cnf drive a duplicated constructor would read as a miss. One function
for both reducers keeps their normal forms equal.

Every APP the rule builds goes in fresh cells, as in thvm's other
interactions. HVM4's `wnf_app_mat_ctr` reuses the consumed MAT and CTR
cells instead, but nf's step session (`src/wnf/redex.c`) indexes only the
cells a fire allocates: an APP written over a consumed cell is never
pushed, and a later replacement of the term it holds misses that cell.

## Cost

- One `ITRS++` per dispatch (APP-MAT-SUP adds its own)
- APP-MAT-CTR on arity n allocates `2 * n` cells
- APP-MAT-MIS allocates 2 cells
