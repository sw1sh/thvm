# Term layout

Every value in the runtime is a single 64-bit word called a **term**.
Defined in [src/thvm.h](../src/thvm.h), packed by
[src/term/new.c](../src/term/new.c), unpacked by the four accessors
under [src/term/](../src/term/).

For the visual / IC interpretation of these terms (agents, ports,
wires, eraser dots, substitution holders), see
[heap_graph.md](heap_graph.md).

## Terminology

These words show up in the rest of the docs. They are easy to
conflate; the rest of this section pins each one down.

| Word         | Meaning                                                                              |
| ------------ | ------------------------------------------------------------------------------------ |
| **term**     | A 64-bit value of type `Term`. The atom of the runtime; everything is one of these.  |
| **tag**      | The 7-bit `TAG` field of a term, e.g. `TAG_APP`, `TAG_LAM`. Picks the term's type.   |
| **cell**     | One element of the heap array `HEAP[loc]`. A `u64` slot that holds exactly one term. |
| **loc**      | A heap location. An unsigned integer in `[0, HEAP_NEXT)`. Indexes into `HEAP`.       |
| **slot**     | Synonym for `cell` when emphasising that it belongs to some compound term's payload. |
| **agent**    | A compound term (LAM, APP, SUP, DUP) viewed as an IC interaction-net node. An agent  |
|              | is identified by its **args base** (the `val` field of the term value).              |
| **args base**| The first `loc` of an agent's payload. `LAM(val=B)` means body at `HEAP[B]`,         |
|              | `APP(val=B)` means fun at `HEAP[B]`, arg at `HEAP[B+1]`, etc.                        |
| **port**     | One of an agent's named slots (e.g. APP's `f` and `x`, LAM's `body`). Each port      |
|              | is realised as a cell at the corresponding offset from the agent's args base.        |
| **node**     | A vertex in the rendered heap graph (see [heap_graph.md](heap_graph.md)). Either an  |
|              | agent triangle or a small ERA circle. Not every cell is a node: VAR cells render     |
|              | as wires, not nodes.                                                                 |
| **wire**     | An edge in the rendered heap graph. May correspond to one cell (a `VAR` cell) or to  |
|              | a port-to-port connection inferred from a compound term value.                       |

The short version: a *term* lives in a *cell* at a *loc*. A
compound term defines an *agent* whose *ports* are the cells starting
at its *args base*. When you draw the heap, agents become *nodes* and
the relationships between them become *wires*; some terms (`VAR`)
disappear into wires rather than becoming nodes of their own.

## Bit layout

```
   bit  63        62..56     55..38         37..0
        [SUB:1]   [TAG:7]    [EXT:18]       [VAL:38]
```

| Field | Bits | Meaning                                                |
| ----- | ---- | ------------------------------------------------------ |
| SUB   | 1    | Substitution flag. When 1, the cell at this position   |
|       |      | holds a value that some VAR / DP0 / DP1 should pick    |
|       |      | up on its next entry. The reducer clears SUB after.    |
| TAG   | 7    | Term type. 128 distinct values; the minimal initial    |
|       |      | set uses 8.                                            |
| EXT   | 18   | Per-tag extension (label, opcode, arity).              |
| VAL   | 38   | Per-tag payload. For heap-backed nodes, the heap loc.  |
|       |      | 38 bits is 256 GiB worth of `u64` cells.               |

The SUB bit is at the top so a substitution-installed cell looks
"larger than usual" to a casual debugger, but otherwise this layout is
the same as HVM4's `Term`.

## Tag table

| Name      | Code | EXT meaning  | VAL meaning      | Heap layout                    |
| --------- | ---- | ------------ | ---------------- | ------------------------------ |
| `TAG_APP` | 0    | (unused)     | application loc  | `[fun, arg]` (2 cells)         |
| `TAG_LAM` | 1    | (unused)     | binder loc       | `[body]` (1 cell, also binder) |
| `TAG_VAR` | 2    | (unused)     | binder loc       | (no heap; resolves via SUB)    |
| `TAG_ERA` | 3    | (unused)     | (unused)         | (no heap)                      |
| `TAG_DP0` | 4    | dup label L  | dup loc          | shares `[body]` with DP1       |
| `TAG_DP1` | 5    | dup label L  | dup loc          | shares `[body]` with DP0       |
| `TAG_SUP` | 6    | sup label L  | sup loc          | `[a, b]` (2 cells)             |
| `TAG_DUP` | 7    | dup label L  | dup loc          | `[body]` (1 cell)              |

Labels (`L`) are 18-bit integers used to identify matching SUP/DUP
pairs. Two superpositions `&L{a,b}` and `&R{c,d}` annihilate when
`L == R` and commute otherwise.

The `TAG_DUP` cell itself only appears as a "carrier" for `body`;
nothing reduces against `DUP` directly. The reducer walks past it and
the projections (`DP0`, `DP1`) drive the interactions.

## Accessors

Pure bit-twiddling, all `static inline`:

| Function          | Source                               |
| ----------------- | ------------------------------------ |
| `term_new`        | [src/term/new.c](../src/term/new.c) |
| `term_tag`        | [src/term/tag.c](../src/term/tag.c) |
| `term_ext`        | [src/term/ext.c](../src/term/ext.c) |
| `term_val`        | [src/term/val.c](../src/term/val.c) |
| `term_sub_get`    | [src/term/sub/get.c](../src/term/sub/get.c) |
| `term_sub_set`    | [src/term/sub/set.c](../src/term/sub/set.c) |

`term_sub_set(t, b)` returns a copy of `t` with the SUB bit replaced
by `b`. It does not mutate; the caller writes the result back to the
heap if needed.

## Worked examples

Each example shows the heap state after construction, plus how the
terms relate to agents / ports. For the corresponding diagrams see
[heap_graph.md](heap_graph.md).

Identity lambda `(lam x. x)`:

```
HEAP[lam_loc] = VAR(val=lam_loc)
result term   = LAM(val=lam_loc)
```

The cell at `lam_loc` plays two roles for the LAM agent at args
base `lam_loc`:

- It is the agent's `body` port (the body term lives here).
- It is the agent's `binder` slot (the cell that APP-LAM writes the
  bound argument to, with SUB set).

The body term is `VAR(val=lam_loc)` -- a wire from the body port
back to the binder, which is the IC string-diagram form of the
identity lambda. APP-LAM works with one heap write (see
[interact/app_lam.md](interact/app_lam.md)).

Application `(f x)`:

```
HEAP[app_loc + 0] = f
HEAP[app_loc + 1] = x
result term       = APP(val=app_loc)
```

The APP agent at args base `app_loc` has two ports:
`f` at `HEAP[app_loc + 0]`, `x` at `HEAP[app_loc + 1]`. The result
term is the principal-port handle pointing to the agent.

Superposition `&7{a, b}`:

```
HEAP[sup_loc + 0] = a
HEAP[sup_loc + 1] = b
result term       = SUP(ext=7, val=sup_loc)
```

Two ports `L`, `R`. The label `7` lives in the term's `EXT` field,
not in any cell.

Duplication `! &7{x0, x1} = e`:

```
HEAP[dup_loc] = e
x0 term       = DP0(ext=7, val=dup_loc)
x1 term       = DP1(ext=7, val=dup_loc)
```

The DUP agent at args base `dup_loc` has one input port (`body`,
holding the term being duplicated) and two implicit output projections
returned to the caller as `DP0` / `DP1` term values. The first
projection that gets reduced takes the cell, descends into `e`, and
on apply fires whichever DUP-* interaction matches the WHNF of `e`.
The interaction substitutes the other side's value at `dup_loc`
(with SUB=1) so the second projection picks it up when it eventually
enters.
