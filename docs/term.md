# Term layout

Every node in the runtime is a single 64-bit word called a `Term`.
Headers, pointers, labels, and leaves all share this representation.
Defined in [src/thvm.h](../src/thvm.h), packed by
[src/term/new.c](../src/term/new.c), unpacked by the four accessors
under [src/term/](../src/term/).

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

Identity lambda `(lam x. x)`:

```
heap[lam_loc] = VAR(loc=lam_loc)
result        = LAM(val=lam_loc)
```

The body `x` is a `VAR` whose `val` field points at the binder loc.
The same loc is the cell that holds the body and the slot that
substitution writes to when an argument is bound. This lets APP-LAM
work with one heap write (see
[interact/app_lam.md](interact/app_lam.md)).

Application `(f x)`:

```
heap[app_loc + 0] = f
heap[app_loc + 1] = x
result            = APP(val=app_loc)
```

Superposition `&7{a, b}`:

```
heap[sup_loc + 0] = a
heap[sup_loc + 1] = b
result            = SUP(ext=7, val=sup_loc)
```

Duplication `! &7{x0, x1} = e`:

```
heap[dup_loc] = e
x0            = DP0(ext=7, val=dup_loc)
x1            = DP1(ext=7, val=dup_loc)
```

Both projections share the same `dup_loc`. The first projection to
enter takes the cell, descends into `e`, and on apply fires whichever
DUP-* interaction matches the WHNF of `e`. The interaction substitutes
the other side's value at `dup_loc` (with SUB=1) so the second
projection picks it up when it eventually enters.
