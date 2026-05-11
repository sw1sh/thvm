// instrument/multi.c -- multicomputation trace (M0 scope).  See
// docs/multicomputation.md for the conceptual reading and
// docs/plans/multicomputation_trace.md for the build trajectory.
//
// Everything in this file is gated by `#ifdef THVM_TRACE`.  Default
// thvm builds (no -DTHVM_TRACE on the command line) emit nothing
// from this translation unit: `multi_emit(...)` at every call site
// is `((void)0)`, the per-context fields (`multi_events`,
// `multi_events_len`, `multi_events_cap`, `trace`) don't exist on
// TContext, and the linker proves the whole module dead.
//
// In a THVM_TRACE build the runtime flag CURRENT_CTX->trace decides
// whether `multi_emit_body` is reached: the macro is the
// `if (__builtin_expect(trace, 0)) ...` and this file holds the
// out-of-line body so the call site stays tiny.
//
// WL-side surface is deferred to a follow-up commit; for the M0
// vertical slice the trace is consumed directly by
// `tests/test_multi_trace.c` via the multi_trace_* C-level API.

#ifdef THVM_TRACE

#include <stdlib.h>
#include <string.h>

#define MULTI_TRACE_INITIAL_CAP 256

// Append-one helper.  Doubles on overflow.  Returns a pointer to the
// just-pushed slot; never NULL (calls abort() on calloc failure --
// trace mode is a debugger / visualiser, not a production path).
fn MultiEvent *multi_events_push(void) {
    if (CURRENT_CTX->multi_events_len == CURRENT_CTX->multi_events_cap) {
        u64 new_cap = CURRENT_CTX->multi_events_cap
                      ? CURRENT_CTX->multi_events_cap * 2
                      : MULTI_TRACE_INITIAL_CAP;
        MultiEvent *grown = (MultiEvent *)realloc(
            CURRENT_CTX->multi_events, new_cap * sizeof(MultiEvent));
        if (!grown) abort();
        CURRENT_CTX->multi_events = grown;
        CURRENT_CTX->multi_events_cap = new_cap;
    }
    return &CURRENT_CTX->multi_events[CURRENT_CTX->multi_events_len++];
}

fn void multi_emit_body(u8 rule, u8 family,
                        u64 term_a, u64 term_b,
                        u32 delta_label) {
    MultiEvent *e   = multi_events_push();
    // `id` is the 0-indexed count of rule firings that preceded this
    // one in the current session.  The convention is `ITRS++` runs
    // first at the head of each interact_*; the emit hook then
    // captures `ITRS - 1` so id 0 is the very first rule that fired.
    // Globally unique within a session; survives multi_trace_reset()
    // (the buffer empties, but `id`s of the next batch start above
    // anything previously seen).
    e->id           = (u64)ITRS - 1;
    e->rule         = rule;
    e->family       = family;
    e->_pad[0] = e->_pad[1] = e->_pad[2]
              = e->_pad[3] = e->_pad[4] = e->_pad[5] = 0;
    e->term_a       = term_a;
    e->term_b       = term_b;
    e->delta_label  = delta_label;
    e->_pad2        = 0;
}

fn void multi_trace_init(u64 initial_cap) {
    if (CURRENT_CTX->multi_events) {
        free(CURRENT_CTX->multi_events);
    }
    if (!initial_cap) initial_cap = MULTI_TRACE_INITIAL_CAP;
    CURRENT_CTX->multi_events     = (MultiEvent *)calloc(initial_cap, sizeof(MultiEvent));
    CURRENT_CTX->multi_events_cap = CURRENT_CTX->multi_events ? initial_cap : 0;
    CURRENT_CTX->multi_events_len = 0;
    CURRENT_CTX->trace            = 0;
}

fn void multi_trace_reset(void) {
    CURRENT_CTX->multi_events_len = 0;
    CURRENT_CTX->trace            = 0;
}

fn void multi_trace_free(void) {
    if (CURRENT_CTX->multi_events) {
        free(CURRENT_CTX->multi_events);
        CURRENT_CTX->multi_events = NULL;
    }
    CURRENT_CTX->multi_events_len = 0;
    CURRENT_CTX->multi_events_cap = 0;
    CURRENT_CTX->trace            = 0;
}

fn u64 multi_trace_count(void) {
    return CURRENT_CTX->multi_events_len;
}

fn const MultiEvent *multi_trace_get(u64 i) {
    if (i >= CURRENT_CTX->multi_events_len) return NULL;
    return &CURRENT_CTX->multi_events[i];
}

#endif // THVM_TRACE
