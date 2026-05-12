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

// wire_prov is keyed by heap loc.  Heap caps out at 1<<28 cells but
// most runs touch a tiny fraction; start small and grow doubling on
// out-of-range WIRE_PROV_BUMP.  4 bytes per cell at 1<<20 cells = 4
// MiB initial -- cheap enough to allocate alongside the event buffer
// in multi_trace_init.
#define MULTI_WIRE_PROV_INITIAL_CAP (1ULL << 20)

// Symbolic names for the RULE_* / MULTI_* enum codes (src/thvm.h),
// used by the WL surface to render a readable trace.  Designated
// initializers index by the enum constant, so adding a `#define
// RULE_FOO N` without a `[RULE_FOO] = "FOO"` entry leaves the slot
// NULL -- multi_rule_name then returns "RULE?" rather than silently
// returning a wrong name.  No order-coupling to maintain.
static const char *const MULTI_RULE_NAMES[] = {
    [RULE_APP_LAM]       = "APP_LAM",
    [RULE_APP_ERA]       = "APP_ERA",
    [RULE_APP_SUP]       = "APP_SUP",
    [RULE_APP_BRI]       = "APP_BRI",
    [RULE_APP_PRI]       = "APP_PRI",
    [RULE_APP_MAT_SUP]   = "APP_MAT_SUP",
    [RULE_ANN_LAM]       = "ANN_LAM",
    [RULE_ANN_BRI]       = "ANN_BRI",
    [RULE_DUP_LAM]       = "DUP_LAM",
    [RULE_DUP_BRI]       = "DUP_BRI",
    [RULE_DUP_CTR]       = "DUP_CTR",
    [RULE_DUP_NOD]       = "DUP_NOD",
    [RULE_DUP_ERA]       = "DUP_ERA",
    [RULE_DUP_NUM]       = "DUP_NUM",
    [RULE_DUP_TEN]       = "DUP_TEN",
    [RULE_DUP_UOP]       = "DUP_UOP",
    [RULE_DUP_ANY]       = "DUP_ANY",
    [RULE_DUP_SUP_ANN]   = "DUP_SUP_ANN",
    [RULE_DUP_SUP_COM]   = "DUP_SUP_COM",
    [RULE_OP2_SUP]       = "OP2_SUP",
    [RULE_OP2_NUM_SUP]   = "OP2_NUM_SUP",
    [RULE_DSU_NUM]       = "DSU_NUM",
    [RULE_DSU_SUP]       = "DSU_SUP",
    [RULE_DSU_ERA]       = "DSU_ERA",
    [RULE_DDU_NUM]       = "DDU_NUM",
    [RULE_DDU_SUP]       = "DDU_SUP",
    [RULE_DDU_ERA]       = "DDU_ERA",
    [RULE_UOP_ASSIGN]    = "UOP_ASSIGN",
    [RULE_UOP_KERNEL]    = "UOP_KERNEL",
    [RULE_GRAD_FWD]      = "GRAD_FWD",
    [RULE_GRAD_BWD]      = "GRAD_BWD",
    [RULE_MAT_DISPATCH]  = "MAT_DISPATCH",
    [RULE_OP2_NUM_NUM]   = "OP2_NUM_NUM",
    [RULE_EQL_NUM]       = "EQL_NUM",
    [RULE_EQL_ERA]       = "EQL_ERA",
    [RULE_EQL_ANY]       = "EQL_ANY",
    [RULE_EQL_SUP]       = "EQL_SUP",
    [RULE_AND_NUM]       = "AND_NUM",
    [RULE_AND_ERA]       = "AND_ERA",
    [RULE_AND_SUP]       = "AND_SUP",
    [RULE_OR_NUM]        = "OR_NUM",
    [RULE_OR_ERA]        = "OR_ERA",
    [RULE_OR_SUP]        = "OR_SUP",
    [RULE_WHEN_NUM]      = "WHEN_NUM",
    [RULE_WHEN_ERA]      = "WHEN_ERA",
    [RULE_WHEN_SUP]      = "WHEN_SUP",
};

static const char *const MULTI_FAMILY_NAMES[] = {
    [MULTI_TERM]  = "TERM",
    [MULTI_SLIDE] = "SLIDE",
    [MULTI_FORK]  = "FORK",
    [MULTI_SPLIT] = "SPLIT",
    [MULTI_MERGE] = "MERGE",
    [MULTI_PRUNE] = "PRUNE",
    [MULTI_PLUMB] = "PLUMB",
};

fn const char *multi_rule_name(u8 r) {
    if (r < (sizeof(MULTI_RULE_NAMES) / sizeof(MULTI_RULE_NAMES[0]))
        && MULTI_RULE_NAMES[r]) return MULTI_RULE_NAMES[r];
    return "RULE?";
}

fn const char *multi_family_name(u8 f) {
    if (f < (sizeof(MULTI_FAMILY_NAMES) / sizeof(MULTI_FAMILY_NAMES[0]))
        && MULTI_FAMILY_NAMES[f]) return MULTI_FAMILY_NAMES[f];
    return "FAMILY?";
}

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

// Grow wire_prov to cover [0, new_cap).  Doubling growth, sentinel-
// initialise the freshly-allocated tail so a read on a never-touched
// cell sees MULTI_WIRE_NONE.
static void multi_wire_prov_grow(u64 new_cap) {
    u64 cur = CURRENT_CTX->multi_wire_prov_cap;
    u64 grown = cur ? cur : MULTI_WIRE_PROV_INITIAL_CAP;
    while (grown < new_cap) grown *= 2;
    u32 *p = (u32 *)realloc(CURRENT_CTX->multi_wire_prov,
                            grown * sizeof(u32));
    if (!p) abort();
    // memset to 0xFF gives all-bits-set = (u32)-1 = MULTI_WIRE_NONE
    // for every freshly-grown slot.
    memset(p + cur, 0xFF, (grown - cur) * sizeof(u32));
    CURRENT_CTX->multi_wire_prov     = p;
    CURRENT_CTX->multi_wire_prov_cap = grown;
}

fn void multi_wire_prov_bump_body(u64 loc) {
    if (loc >= CURRENT_CTX->multi_wire_prov_cap) {
        multi_wire_prov_grow(loc + 1);
    }
    // ITRS has been bumped at the head of the in-flight interact_*
    // (or inline rule), so ITRS - 1 is this event's id.  Before any
    // event has fired, ITRS == 0 -> (u32)(u64)-1 == MULTI_WIRE_NONE,
    // which correctly says "produced outside the trace" even if
    // tracing happens to be on for some pre-event setup.
    CURRENT_CTX->multi_wire_prov[loc] = (u32)((u64)ITRS - 1);
}

// Sentinel-safe read: out-of-range locs (and locs never written
// while tracing) yield MULTI_WIRE_NONE without growing the table.
fn u32 multi_wire_prov_get(u64 loc) {
    if (loc >= CURRENT_CTX->multi_wire_prov_cap) return MULTI_WIRE_NONE;
    return CURRENT_CTX->multi_wire_prov[loc];
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
              = e->_pad[3] = e->_pad[4] = 0;
    e->term_a       = term_a;
    e->term_b       = term_b;
    e->delta_label  = delta_label;
    e->_pad2        = 0;
    // M1: wire provenance -- look up the producer event ids for the
    // two active-pair payload locs.  We use term_val of the packed
    // term word: for an APP/SUP/LAM/etc. Term, that's the loc of its
    // children block, which is the heap cell most recently written
    // by whoever constructed this term.  Zero `term_a` / `term_b`
    // (e.g. ERA-prune emits that pass 0,0,0) yield n_consumed slots
    // smaller than 2 and an unfilled consumed[i] = MULTI_WIRE_NONE.
    // Must run BEFORE any heap_set inside the current interact_*;
    // the standard pattern (ITRS++ -> multi_emit -> heap mutations)
    // guarantees this.
    e->consumed[0] = MULTI_WIRE_NONE;
    e->consumed[1] = MULTI_WIRE_NONE;
    u8 nc = 0;
    if (term_a) {
        e->consumed[nc++] = multi_wire_prov_get((u64)term_val((Term)term_a));
    }
    if (term_b) {
        e->consumed[nc++] = multi_wire_prov_get((u64)term_val((Term)term_b));
    }
    e->n_consumed = nc;
}

fn void multi_trace_init(u64 initial_cap) {
    if (CURRENT_CTX->multi_events) {
        free(CURRENT_CTX->multi_events);
    }
    if (CURRENT_CTX->multi_wire_prov) {
        free(CURRENT_CTX->multi_wire_prov);
    }
    if (!initial_cap) initial_cap = MULTI_TRACE_INITIAL_CAP;
    CURRENT_CTX->multi_events     = (MultiEvent *)calloc(initial_cap, sizeof(MultiEvent));
    CURRENT_CTX->multi_events_cap = CURRENT_CTX->multi_events ? initial_cap : 0;
    CURRENT_CTX->multi_events_len = 0;
    // M1: wire_prov starts sentinel-filled (MULTI_WIRE_NONE = all
    // bits set) so a read on any loc before its first WIRE_PROV_BUMP
    // says "no producer recorded".
    u32 *wp = (u32 *)malloc(MULTI_WIRE_PROV_INITIAL_CAP * sizeof(u32));
    if (wp) {
        memset(wp, 0xFF, MULTI_WIRE_PROV_INITIAL_CAP * sizeof(u32));
        CURRENT_CTX->multi_wire_prov     = wp;
        CURRENT_CTX->multi_wire_prov_cap = MULTI_WIRE_PROV_INITIAL_CAP;
    } else {
        CURRENT_CTX->multi_wire_prov     = NULL;
        CURRENT_CTX->multi_wire_prov_cap = 0;
    }
    CURRENT_CTX->trace            = 0;
}

fn void multi_trace_reset(void) {
    CURRENT_CTX->multi_events_len = 0;
    // Drop accumulated wire provenance too: a fresh trace shouldn't
    // see ids stamped by a prior reduction (those ids reference
    // events no longer in the buffer).  Cheap memset over the
    // currently allocated wire_prov slab; the slab itself is kept.
    if (CURRENT_CTX->multi_wire_prov) {
        memset(CURRENT_CTX->multi_wire_prov, 0xFF,
               CURRENT_CTX->multi_wire_prov_cap * sizeof(u32));
    }
    CURRENT_CTX->trace            = 0;
}

fn void multi_trace_free(void) {
    if (CURRENT_CTX->multi_events) {
        free(CURRENT_CTX->multi_events);
        CURRENT_CTX->multi_events = NULL;
    }
    if (CURRENT_CTX->multi_wire_prov) {
        free(CURRENT_CTX->multi_wire_prov);
        CURRENT_CTX->multi_wire_prov = NULL;
    }
    CURRENT_CTX->multi_events_len = 0;
    CURRENT_CTX->multi_events_cap = 0;
    CURRENT_CTX->multi_wire_prov_cap = 0;
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
