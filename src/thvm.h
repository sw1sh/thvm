// thvm.h - public surface of the thvm runtime
//
// Single-TU build: every .c file under src/ is #included by src/thvm.c
// in dependency order. Tests #include "../src/thvm.c" directly.
//
// Conventions:
//   - File path = function name. `wnf/_.c` defines wnf().
//                                `interact/app_lam.c` defines
//                                interact_app_lam().
//   - `fn` macro = static inline. Used on every internal function.

#ifndef THVM_H
#define THVM_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// === Types ===
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef float    f32;

typedef u64 Term;

#define fn static inline

// === Term bit layout ===
//
//   bit  63        62..56     55..38         37..0
//        [SUB:1]   [TAG:7]    [EXT:18]       [VAL:38]
//
//   SUB = substitution flag - when 1, the heap cell is the *value* a
//         variable was substituted to (read by VAR/DP0/DP1 enter rules).
//   TAG = term type (128 max).
//   EXT = label / opcode / arity. Per-tag meaning.
//   VAL = heap location (38 bits ≈ 256 GiB worth of u64 cells).

#define SUB_BITS  1
#define TAG_BITS  7
#define EXT_BITS  18
#define VAL_BITS  38

#define SUB_SHIFT 63
#define TAG_SHIFT 56
#define EXT_SHIFT 38
#define VAL_SHIFT 0

#define SUB_MASK  0x1ULL
#define TAG_MASK  0x7FULL
#define EXT_MASK  0x3FFFFULL
#define VAL_MASK  0x3FFFFFFFFFULL

// === Tags (minimal initial set) ===
// Order chosen so the hot interaction tags sit in the low byte.

#define TAG_APP  0   // (f x).             Heap[loc..loc+1] = [f, x]
#define TAG_LAM  1   // λx.body.           Heap[loc]        = body; var subst at loc
#define TAG_VAR  2   // bound variable.    val = binder loc
#define TAG_ERA  3   // erased.            no heap
#define TAG_DP0  4   // dup projection 0.  val = dup loc, ext = label
#define TAG_DP1  5   // dup projection 1.  val = dup loc, ext = label
#define TAG_SUP  6   // {a, b}.            Heap[loc..loc+1] = [a, b]; ext = label
#define TAG_DUP  7   // ! &L{x0,x1} = e; … val = dup loc; Heap[loc] = e

#define TAG_COUNT 8

// === Capacities ===
#define HEAP_CAP (1ULL << 24)   // 16M cells * 8B = 128 MiB. Plenty for tests.
#define WNF_CAP  (1ULL << 16)   // 64K stack slots.

// === Globals ===
// Single-threaded for now. Extern in the header, defined in src/thvm.c.

extern Term *HEAP;
extern u64   HEAP_NEXT;

extern Term *WNF_STACK;
extern u32   WNF_S_POS;

extern u64   ITRS;   // interaction counter (for tests + tracing)

// === term/ ===
fn Term term_new(u8 sub, u8 tag, u32 ext, u64 val);
fn u8   term_tag(Term t);
fn u32  term_ext(Term t);
fn u64  term_val(Term t);
fn u8   term_sub_get(Term t);
fn Term term_sub_set(Term t, u8 sub);

// === heap/ ===
fn u64  heap_alloc(u64 size);
fn Term heap_read(u64 loc);
fn void heap_set(u64 loc, Term t);
fn Term heap_take(u64 loc);                                 // read + zero
fn void heap_subst_var(u64 loc, Term value);
fn Term heap_subst_cop(u8 side, u64 loc, Term r0, Term r1); // pair subst

// === interact/ ===
// One file per active pair.  Each rule increments ITRS when it fires.
fn Term interact_app_lam(Term lam, Term arg);
fn Term interact_app_era(void);
fn Term interact_dup_sup(u32 lab, u64 loc, u8 side, Term sup);
fn Term interact_dup_era(u8 side, u64 loc, Term era);

// === wnf/ ===
// Stack-machine reducer to weak normal form.  See src/wnf/_.c for the
// enter/apply protocol.
fn Term wnf(Term t);

// === runtime lifecycle ===
void thvm_init(void);
void thvm_free(void);

#endif // THVM_H
