"""Exact order-mirror of Waldmeister's discrimination tree (DSBaum).

Decoded from waldmeister/sources/WDT/DSBaumOperationen.c +
INF/Unifikation1.c (see wm_order_sim.py header for the full map).
Key disciplines:
- leaf-compressed trie; GleichPfad split chains (BlattAufgeteilt :589-678);
- jump exits prepend at creation (RumpfSprungeintragSetzen :283-295);
  split order: new-leaf pass first (:658), old-leaf pass second (:666)
  => fresh chain nodes bottom out as [old, new]; AltesBlattPolieren
  parallels go right AFTER the paralleled entry (:523-526); rewires keep
  position (:534-560);
- leaf list: depth classes, insert-after-head (BlattEinzeigern :365-442);
- leaf chains prepend (RegelHinzufuegen :343-347);
- removal collapse (BO_ObjektEntfernen :991-1101);
- DFS descent order (Unifikation1.c :626-820): query-function cell:
  exact child, then var children ascending symbol code (= descending var
  index); query-var cell: var children ascending code, then jump exits in
  list order.  EVERY successful leaf arrival emits (duplicates real).
"""

FUN, VAR = 'f', 'v'


def sub_end(key, i, arity):
    need, j = 1, i
    while need:
        c = key[j]
        need += (arity[c[1]] if c[0] == FUN else 0) - 1
        j += 1
    return j


class Entry:
    __slots__ = ('start', 'sub', 'ziel')
    def __init__(self, start, sub, ziel):
        self.start, self.sub, self.ziel = start, sub, ziel


class TNode:
    __slots__ = ('children', 'exits', 'parent', 'pedge')
    def __init__(self, parent, pedge):
        self.children = {}
        self.exits = []
        self.parent, self.pedge = parent, pedge


class TLeaf:
    __slots__ = ('key', 'hang', 'chain', 'parent', 'pedge', 'depth')
    def __init__(self, key, hang, parent, pedge, depth):
        self.key, self.hang = key, hang
        self.chain = []
        self.parent, self.pedge = parent, pedge
        self.depth = depth


class WmTrie:
    def __init__(self, arity):
        self.arity = arity
        self.root = None
        self.leaf_list = []
        self.class_head = {}

    # ----- leaf list -----
    def _leaf_in(self, leaf):
        d = leaf.depth
        head = self.class_head.get(d)
        if head is not None:
            self.leaf_list.insert(self.leaf_list.index(head) + 1, leaf)
        else:
            pos = 0
            for i, x in enumerate(self.leaf_list):
                if x.depth <= d: pos = i + 1
            self.leaf_list.insert(pos, leaf)
            self.class_head[d] = leaf

    def _leaf_out(self, leaf):
        idx = self.leaf_list.index(leaf)
        if self.class_head.get(leaf.depth) is leaf:
            nxt = self.leaf_list[idx + 1] if idx + 1 < len(self.leaf_list) else None
            if nxt is not None and nxt.depth == leaf.depth:
                self.class_head[leaf.depth] = nxt
            else:
                del self.class_head[leaf.depth]
        self.leaf_list.pop(idx)

    # ----- helpers -----
    def _add_entry(self, start, sub, ziel, where='front', after=None):
        for e in start.exits:
            if e.sub == sub and e.ziel is ziel:
                return e
        ent = Entry(start, tuple(sub), ziel)
        if where == 'front':
            start.exits.insert(0, ent)
        else:
            start.exits.insert(start.exits.index(after) + 1, ent)
        return ent

    def _node_depth(self, n):
        d = 0
        while n.parent is not None:
            n, d = n.parent, d + 1
        return d

    def _all_nodes(self):
        out = []
        def walk(n):
            if isinstance(n, TLeaf): return
            out.append(n)
            for c in n.children.values(): walk(c)
        if self.root is not None and not isinstance(self.root, TLeaf):
            walk(self.root)
        return out

    # ----- insertion -----
    def insert(self, key, fact, depth):
        key = tuple(key)
        if self.root is None:
            self.root = TNode(None, None)
            return self._hang(self.root, key, 0, fact, depth, [])
        node, i = self.root, 0
        pending = []
        while True:
            sym = key[i]
            child = node.children.get(sym)
            if child is None:
                return self._hang(node, key, i, fact, depth, pending)
            if isinstance(child, TLeaf):
                return self._split(node, child, key, i, fact, depth, pending)
            if sym[0] == FUN and self.arity[sym[1]] > 0:
                pending.append((node, i))
            else:
                e = i + 1
                while pending and sub_end(key, pending[-1][1], self.arity) == e:
                    e2 = pending[-1][1]
                    pending.pop()
                    e = sub_end(key, e2, self.arity)
                    # nested closures: outer subterm may close at same cell
                    # recompute: outer closes at e too if its end == e
            node, i = child, i + 1

    def _pop_into(self, key, pending, lo, hi, node_at, leaf):
        """Create entries for pendings (innermost first).  Subterms closing
        at position p with lo < p <= hi land at node_at(p) (inner chain
        node); later closings land in `leaf`."""
        for (n, j) in reversed(pending):
            e = sub_end(key, j, self.arity)
            if lo is not None and lo < e <= hi:
                self._add_entry(n, key[j:e], node_at(e))
            else:
                self._add_entry(n, key[j:e], leaf)
        del pending[:]

    def _hang(self, node, key, i, fact, depth, pending):
        leaf = TLeaf(key, i, node, key[i], depth)
        leaf.chain.append(fact)
        node.children[key[i]] = leaf
        self._leaf_in(leaf)
        sym = key[i]
        if sym[0] == FUN and self.arity[sym[1]] > 0:
            pending.append((node, i))
        elif sym[0] == FUN and i > 0:
            self._add_entry(node, key[i:i + 1], leaf)
        self._pop_into(key, pending, None, -1, None, leaf)
        return leaf

    def _split(self, node, old, key, i, fact, depth, pending):
        okey = old.key
        if okey[i:] == key[i:]:
            old.chain.insert(0, fact)
            return old
        j = i + 1
        nk, nok = len(key), len(okey)
        while j < nk and j < nok and key[j] == okey[j]:
            j += 1
        # GleichPfad: nodes covering positions i+1 .. j
        chain_nodes = []
        cur = node
        edge = key[i]
        for p in range(i + 1, j + 1):
            nxt = TNode(cur, edge)
            cur.children[edge] = nxt
            chain_nodes.append(nxt)
            cur = nxt
            edge = key[p] if p < j else None
        last = chain_nodes[-1]                  # covers position j
        node_at = lambda e: chain_nodes[e - (i + 1)]
        # chain-region pendings: positions i+1 .. j-1 (PushDoppelt)
        chain_pend = [(node_at(p), p) for p in range(i + 1, j)
                      if key[p][0] == FUN and self.arity[key[p][1]] > 0]
        # hang leaves
        new_leaf = TLeaf(key, j, last, key[j] if j < nk else None, depth)
        new_leaf.chain.append(fact)
        old.parent, old.hang = last, j
        old.pedge = okey[j] if j < nok else None
        if j < nk: last.children[key[j]] = new_leaf
        if j < nok: last.children[okey[j]] = old
        self._leaf_in(new_leaf)
        # snapshot of old in-entries BEFORE new entries are created
        old_in = [(n, e) for n in self._all_nodes() for e in n.exits
                  if e.ziel is old]
        # 1) new-leaf pass: leading push at branch node + chain pendings.
        #    Main-descent pendings do NOT pop here (Stapeluntergrenze =
        #    EintragUngueltig in BlattAufgeteilt's NeuesBlattEinhaengen
        #    call :658) -- ancestor coverage for the new face comes from
        #    AltesBlattPolieren parallels exclusively.
        all_pend_new = list(chain_pend)
        if j < nk and key[j][0] == FUN and self.arity[key[j][1]] > 0:
            all_pend_new.append((last, j))
        elif j < nk and key[j][0] == FUN:
            self._add_entry(last, key[j:j + 1], new_leaf)
        self._pop_into(key, all_pend_new, i, j, node_at, new_leaf)
        del pending[:]
        # 2) old-leaf pass: chain pendings + leading push, old cells
        all_pend_old = list(chain_pend)
        if j < nok and okey[j][0] == FUN and self.arity[okey[j][1]] > 0:
            all_pend_old.append((last, j))
        elif j < nok and okey[j][0] == FUN:
            self._add_entry(last, okey[j:j + 1], old)
        self._pop_into(okey, all_pend_old, i, j, node_at, old)
        # 3) AltesBlattPolieren on pre-existing in-entries of the old leaf
        for (n, ent) in old_in:
            start_pos = self._node_depth(n)
            land = start_pos + len(ent.sub)
            if land > j:
                # parallel entry into the new leaf; Teilterm = the NEW
                # face's cells at the same coordinate, own extent (:509-521)
                e2 = sub_end(key, start_pos, self.arity)
                self._add_entry(n, key[start_pos:e2], new_leaf,
                                where='after', after=ent)
            elif land > i:
                ent.ziel = node_at(land)
        return new_leaf

    # ----- removal -----
    def remove(self, key, fact):
        key = tuple(key)
        leaf = self._find_leaf(key, fact)
        if leaf is None: return False
        leaf.chain.remove(fact)
        if leaf.chain: return True
        self._leaf_out(leaf)
        for n in self._all_nodes():
            n.exits = [e for e in n.exits if e.ziel is not leaf]
        parent = leaf.parent
        del parent.children[leaf.pedge]
        if parent is self.root:
            if not parent.children:
                self.root = None
            return True
        if len(parent.children) == 1:
            (sib,) = parent.children.values()
            if isinstance(sib, TLeaf):
                node = parent
                freed = []
                while True:
                    freed.append(node)
                    up, edge = node.parent, node.pedge
                    del up.children[edge]
                    if up is self.root or up.children:
                        break
                    node = up
                new_hang = sib.hang - len(freed)
                sib.parent, sib.hang = up, new_hang
                sib.pedge = sib.key[new_hang]
                up.children[sib.key[new_hang]] = sib
                freed_ids = set(id(f) for f in freed)
                for n in self._all_nodes():
                    n.exits = [e for e in n.exits
                               if id(e.start) not in freed_ids]
                    rewired = []
                    for e in n.exits:
                        if id(e.ziel) in freed_ids:
                            e.ziel = sib
                            rewired.append(e)
                    # At the collapse-target node `up` (where the surviving
                    # sibling re-hangs, BO_ObjektEntfernen :1083), the short
                    # jump reaching `sib` is re-issued for the re-hung leaf
                    # and prepends to up's outgoing exit list per the standard
                    # RumpfSprungeintragSetzen head-insert (:293-295); at every
                    # other node the freed-node-targeting jump is rewired in
                    # place (AlleEingehendenSpruengeUmsetzen :814 keeps the
                    # outgoing-list position).
                    if n is up:
                        for e in rewired:
                            n.exits.remove(e)
                            n.exits.insert(0, e)
                # Sprung-compressed-leaf re-issue.  The surviving leaf `sib`
                # re-hangs as a compressed leaf at `up`, sharing `up` with a
                # model leaf (the OTHER child) whose left side begins with the
                # same function and arg1 cells.  A leaf that was deep-hung
                # while the now-freed chain still existed got its ancestor
                # jumps prepended (RumpfSprungeintragSetzen head-insert
                # :293-295).  In WM that deep leaf instead arose as a
                # BlattAufgeteilt split of the model's Sprung-compressed leaf
                # (they share the arg1 prefix), so its ancestor jumps were
                # AltesBlattPolieren parallels spliced AFTER the model's own
                # jump (:521-525, "hinter den Eintrag setzen"), never at the
                # head.  When the chain later collapses to that shared node,
                # restore the parallel order: a head-prepended ancestor jump to
                # `sib` whose model counterpart at the same start node is NOT at
                # the head moves to just after that model jump.
                others = [c for c in up.children.values()
                          if isinstance(c, TLeaf) and c is not sib]
                if len(others) == 1:
                    model = others[0]
                    up_d = self._node_depth(up)

                    def _shared_prefix(a, b):
                        k = 0
                        while k < len(a) and k < len(b) and a[k] == b[k]:
                            k += 1
                        return k

                    for nn in self._all_nodes():
                        if self._node_depth(nn) >= up_d:
                            continue
                        sib_js = [e for e in nn.exits if e.ziel is sib]
                        mod_js = [e for e in nn.exits if e.ziel is model]
                        if len(sib_js) != 1 or not mod_js:
                            continue
                        sj = sib_js[0]
                        if nn.exits.index(sj) != 0:
                            continue               # only a head-prepended jump
                        best = max(mod_js, key=lambda m: _shared_prefix(sj.sub, m.sub))
                        if _shared_prefix(sj.sub, best.sub) < 2:
                            continue               # need the shared f + arg1
                        if nn.exits.index(best) == 0:
                            continue               # model also at head -> no move
                        nn.exits.remove(sj)
                        nn.exits.insert(nn.exits.index(best) + 1, sj)
        return True

    def _find_leaf(self, key, fact):
        for lf in self.leaf_list:
            if lf.key == key and fact in lf.chain:
                return lf
        return None
