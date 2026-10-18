// Multicomputation trace (docs/multicomputation.md). Without -DTHVM_TRACE this
// file is empty and multi_emit / WIRE_PROV_BUMP compile to nothing.
//
// With it, the macros (thvm.h) test CURRENT_CTX->trace and call the out-of-line
// bodies here.

#ifdef THVM_TRACE

#include <stdlib.h>
#include <string.h>

#define MULTI_TRACE_INITIAL_CAP 256

// wire_prov holds one u32 per heap loc; it starts at 2^20 locs (4 MiB) and
// doubles on demand, since a run touches a small part of the heap.
#define MULTI_WIRE_PROV_INITIAL_CAP (1ULL << 20)

// Indexed by RULE_* code: a rule missing here reads "RULE?", not a wrong name.
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
    [MULTI_DIST]  = "DIST",
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

// Never NULL: out of memory aborts, tracing being a debugging aid.
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

static void multi_wire_prov_grow(u64 new_cap) {
    u64 cur = CURRENT_CTX->multi_wire_prov_cap;
    u64 grown = cur ? cur : MULTI_WIRE_PROV_INITIAL_CAP;
    while (grown < new_cap) grown *= 2;
    u32 *p = (u32 *)realloc(CURRENT_CTX->multi_wire_prov,
                            grown * sizeof(u32));
    if (!p) abort();
    // 0xFF bytes make every new slot MULTI_WIRE_NONE ((u32)-1).
    memset(p + cur, 0xFF, (grown - cur) * sizeof(u32));
    CURRENT_CTX->multi_wire_prov     = p;
    CURRENT_CTX->multi_wire_prov_cap = grown;
}

fn void multi_wire_prov_bump_body(u64 loc) {
    if (loc >= CURRENT_CTX->multi_wire_prov_cap) {
        multi_wire_prov_grow(loc + 1);
    }
    // The in-flight rule bumped ITRS first, so ITRS - 1 is its event id; before
    // any event it wraps to MULTI_WIRE_NONE, "made outside the trace".
    CURRENT_CTX->multi_wire_prov[loc] = (u32)((u64)ITRS - 1);
}

fn u32 multi_wire_prov_get(u64 loc) {
    if (loc >= CURRENT_CTX->multi_wire_prov_cap) return MULTI_WIRE_NONE;
    return CURRENT_CTX->multi_wire_prov[loc];
}

// A VAR/DP cell over a SUB binder was produced by whoever wrote the
// substitution (a DUP_NUM, say), not by the node that allocated the cell.
fn u32 multi_resolve_producer(u64 loc) {
    if (loc >= CURRENT_CTX->multi_wire_prov_cap) return MULTI_WIRE_NONE;
    Term cell = HEAP[loc];
    u8   tag  = term_tag(cell);
    if (tag == TAG_DP0 || tag == TAG_DP1 || tag == TAG_VAR) {
        u64 target = term_val(cell);
        if (target < CURRENT_CTX->multi_wire_prov_cap) {
            Term tcell = HEAP[target];
            if (term_sub_get(tcell)) {
                return CURRENT_CTX->multi_wire_prov[target];
            }
        }
    }
    return CURRENT_CTX->multi_wire_prov[loc];
}

fn void multi_emit_body(u8 rule, u8 family,
                        u64 term_a, u64 term_b,
                        u32 delta_label) {
    MultiEvent *e   = multi_events_push();
    // ITRS - 1, as for wire_prov; multi_trace_reset leaves ITRS alone, so ids
    // keep rising across resets.
    e->id           = (u64)ITRS - 1;
    e->rule         = rule;
    e->family       = family;
    e->_pad[0] = e->_pad[1] = e->_pad[2]
              = e->_pad[3] = e->_pad[4] = 0;
    e->term_a       = term_a;
    e->term_b       = term_b;
    e->delta_label  = delta_label;
    e->_pad2        = 0;
    // Producers of the two payloads' child blocks (term_val of each), zero
    // payloads skipped; must run before the rule's heap writes (ITRS++, emit,
    // then mutate).
    e->consumed[0] = MULTI_WIRE_NONE;
    e->consumed[1] = MULTI_WIRE_NONE;
    u8 nc = 0;
    if (term_a) {
        e->consumed[nc++] = multi_resolve_producer(
            (u64)term_val((Term)term_a));
    }
    if (term_b) {
        e->consumed[nc++] = multi_resolve_producer(
            (u64)term_val((Term)term_b));
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
    // MULTI_WIRE_NONE everywhere until a WIRE_PROV_BUMP.
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
    // Old provenance ids would name events no longer in the buffer.
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
