# PDDL-XTS Test Suite

Each subdirectory contains a `domain.pddl` + `instance.pddl` pair.
Directories prefixed with **`X_`** are **error tests**: the domain or instance
encodes a violation that the compiler/validator should catch and reject.
All other directories are **valid tests** that should produce a plan.

---

## Valid Tests

### base
**Features:** arrays (1D), bounded integers, basic read/write  
**Tests:** Core array and bounded-integer features with two players and scoring.
Actions fill individual slots and promote best scores.  
**Initial state:** player boards all zeros, best scores all 0.  
**Goal:** player1 board slots 1,2 are (2,3); player2 board equals [1,0,4]; best scores 3,4.  
**Expected result:** SOLVED.

---

### bounded
**Features:** bounded integers, arithmetic, heat/cool actions  
**Tests:** Bounded-integer fluents with heat-up and cool-down actions that increment
or decrement temperature within a declared range.  
**Initial state:** kitchen temp=16 target=20; living-room temp=25 target=22.  
**Goal:** kitchen=20, living-room=22.  
**Expected result:** SOLVED — 7-step plan (heat kitchen 4 times, cool living-room 3 times).

---

### bounded_params
**Features:** bounded integers, action parameters, arithmetic assignment from parameter  
**Tests:** Two counter actions: `set-counter` directly assigns a bounded-int parameter value;
`inc` assigns `counter + param` (arithmetic on a parameter). `set-counter` is limited to
values ≤ 4, so reaching the goal (5) requires both actions.  
**Initial state:** counter=0.  
**Goal:** counter=5.  
**Expected result:** SOLVED — 2 steps (`set-counter(4)`, `inc(1)`).

---

### add_arithmetic
**Features:** sets, bounded integers, arithmetic in `add` AND `remove` element position  
**Tests:** Arithmetic expressions as the element argument of both `add` and `remove`.
`step(n)` adds `(n+1)`; `trim(n)` removes `(n+1)`. Both are required to reach the goal.  
**Initial state:** chain={0, 4}.  
**Goal:** 3 ∈ chain AND 4 ∉ chain.  
**Expected result:** SOLVED — 4 steps (`step(0)`, `step(1)`, `step(2)`, `trim(3)`).

---

### count
**Features:** sets, bounded integers, `count` function  
**Tests:** `count` applied to multiple boolean fluents to check how many are true.
Goal is satisfiable only after at least two lights are on.  
**Initial state:** l1 lit, l2 not lit, l3 not lit.  
**Goal:** `party-on` (requires `count(lit l1, lit l2, lit l3) >= 2`).  
**Expected result:** SOLVED — 1 step (`turn-on(l2)`).

---

### sets
**Features:** sets, add, remove, member  
**Tests:** Basic set operations: add and remove individual elements, membership in
preconditions, and set difference in effects.  
**Initial state:** basket={cherry, orange}.  
**Goal:** basket={apple, banana}.  
**Expected result:** SOLVED — 4 steps (remove cherry, remove orange, add apple, add banana).

---

### sets2
**Features:** sets, union, intersection, complement, subset, disjoint  
**Tests:** Complex set-algebraic operations across multiple set fluents.
Exercises `union`, `intersection`, `difference`, `subset`, `disjoint`,
`complement`, and bulk assignment in effects.  
**Initial state:** shelf1={a,b,c,f}, shelf2={a}, shelf3={b,c,d,e}.  
**Goal:** shelf1={a,d,e,f}, shelf2={a,b,c,d,e}.  
**Expected result:** SOLVED.

---

### sets3
**Features:** sets, bounded integers, set operations, conditions  
**Tests:** Set fluents combined with a bounded-integer temperature condition.
Actions are only applicable at certain temperature levels, mixing numeric
and set-membership reasoning.  
**Initial state:** lab={b,c,e}, archive={a,b,c}, temp=4.  
**Goal:** lab={a,b,c}, temp=1.  
**Expected result:** SOLVED.

---

### sets4
**Features:** sets, bounded integers, subset, disjoint, cardinality  
**Tests:** Comprehensive drone-corridor domain where altitude sets and congestion
are managed together. Exercises `subset`, `disjoint`, element add/remove,
and bounded arithmetic in the same plan.  
**Initial state:** active={1,3}, restricted={5,7}, charging={2,3,4}, ceiling=4, traffic=2.  
**Goal:** active ⊆ charging, disjoint(active, restricted), ceiling ≥ 6, traffic ≤ 3, 4 ∈ active.  
**Expected result:** SOLVED.

---

### sets_const
**Features:** sets, bounded integers, `set.mk` literal in precondition, cardinality  
**Tests:** A `set.mk` constant appears in a precondition equality check. Verifies
that literal set comparison and cardinality work together to gate an action.  
**Initial state:** active empty, allowed={2,4,6}.  
**Goal:** 2, 4, 6 all in active.  
**Expected result:** SOLVED — 4 steps (`activate_even` ×3, then `mark_complete`).

---

### sets_nested
**Features:** sets, nested set expressions in effects  
**Tests:** Effect assigns a set fluent to the union of two other set fluents
(union of unions), verifying multi-level set expression evaluation in effects.  
**Initial state:** bucket_a, bucket_b, bucket_c, result all empty.  
**Goal:** 1, 2, 3 all in result.  
**Expected result:** SOLVED — 4 steps (add to each bucket, then `merge_all`).

---

### sets_singleton
**Features:** sets, single-element invariant (token ring)  
**Tests:** A set always holds exactly one element. Actions release the current
element and claim a new one, modelling a token-ring handoff.  
**Initial state:** current={n0}.  
**Goal:** n2 ∈ current.  
**Expected result:** SOLVED — 4 steps (release n0 → claim n1 → release n1 → claim n2).

---

### setmk_effect
**Features:** sets, `set.mk` in effects, cardinality in precondition  
**Tests:** Action replaces an entire set fluent with a literal `set.mk` value in its
effect. The cardinality precondition ensures the action fires only once.  
**Initial state:** basket={item_c, item_d, item_a}.  
**Goal:** item_a ∈ basket, item_b ∈ basket, item_c ∉ basket.  
**Expected result:** SOLVED — 1 step (`reset_to_ab`).

---

### read_as_member
**Features:** arrays, sets, membership using array-read value  
**Tests:** The value at `tiles[pos]` is tested for membership in a set fluent.
Verifies that an array read result can appear as the element argument of `member`.  
**Initial state:** tiles=[7,3,9,2], called={3,9,5}, score=0.  
**Goal:** score=2.  
**Expected result:** SOLVED — 2 steps (`mark(1,3)`, `mark(2,9)`).

---

### setops_on_reads
**Features:** arrays, sets, set operations on 1D array-read elements  
**Tests:** `cardinality` and `subset` applied to elements read from a 1D array of sets.
Also writes the result of a set intersection back into an array cell.  
**Initial state:** cells=[{0,1,2,3}, {0,2}, {1,3,4}], big=0.  
**Goal:** big=2.  
**Expected result:** SOLVED — 2 steps (`count_big(0)`, `count_big(2)`).

---

### setops_on_2d_reads
**Features:** 2D arrays, sets, set operations on 2D array-read elements  
**Tests:** `cardinality` applied to elements read from a 2D array of integer sets.
Verifies that set-operation expressions compose with 2D indexing.  
**Initial state:** 2×3 grid of integer sets; big=0.  
**Goal:** big=4.  
**Expected result:** SOLVED — 4 steps (count cells with cardinality ≥ 2).

---

### write_setexpr
**Features:** arrays, sets, set expression in array write  
**Tests:** The value written into an array cell is the result of a set expression
(union of two other array-read sets).  
**Initial state:** cells=[{0,1}, {2,3}, {4}].  
**Goal:** 0 ∈ cells[2] and 1 ∈ cells[2].  
**Expected result:** SOLVED — 1 step (`merge_into(0,2)`, cells[2] becomes {0,1,4}).

---

### array_obj
**Features:** 1D arrays, user objects, swap actions  
**Tests:** A 1D array of user-defined objects manipulated by swap actions.
Verifies that object-typed array elements are correctly read and written.  
**Initial state:** canvas=[green, blue, red].  
**Goal:** canvas=[red, green, blue].  
**Expected result:** SOLVED — 2 steps (swap (0,2) then swap (1,2)).

---

### scalar_from_array
**Features:** arrays, objects, scalar assignment from array read  
**Tests:** A scalar object fluent is assigned the value read from a specific array index.
Verifies that an array read can appear on the right-hand side of an object assignment.  
**Initial state:** stock=[tok_b, tok_a, tok_c], picked=tok_a.  
**Goal:** picked=tok_c.  
**Expected result:** SOLVED — 1 step (`pick_2(tok_c)`).

---

### array_intsets
**Features:** 1D arrays, sets of bounded integers  
**Tests:** A 1D array whose elements are sets of bounded integers. An action copies
one element (a whole set) from one index to another.  
**Initial state:** assignments bay0={0,1,2}, bay1={3,4}, bay2={0}.  
**Goal:** bay2 contains 3 and 4.  
**Expected result:** SOLVED — 1 step (`copy_tasks(1, 2)`).

---

### 1d_array_objsets
**Features:** 1D arrays, sets of user objects, union  
**Tests:** A 1D array whose elements are sets of user objects. Actions propagate
set membership from one room to another via union.  
**Initial state:** room 0={red,blue}, room 1={green}, room 2={red}.  
**Goal:** green ∈ room 2.  
**Expected result:** SOLVED.

---

### multi_array
**Features:** 1D arrays, multiple independent array fluents  
**Tests:** Two independent 1D arrays in the same domain. Actions copy values from
the source array to the destination array, one cell at a time.  
**Initial state:** src=[3,1,4], dst=[0,0,0].  
**Goal:** dst=[3,1,4].  
**Expected result:** SOLVED — 3 steps (`copy_0`, `copy_1`, `copy_2`).

---

### fluent_index
**Features:** 1D arrays, read-modify-write  
**Tests:** An action reads an array cell and writes back an incremented value.
Verifies that `(write arr (i) (+ (read arr i) 1))` encodes correctly.  
**Initial state:** cells=[1,3,0].  
**Goal:** cells[1]=5.  
**Expected result:** SOLVED — 2 steps (`inc(1)`, `inc(1)`).

---

### 2d
**Features:** 2D arrays, bounded integers, read/write  
**Tests:** Basic 2D array read and write actions on a 3×4 grid.  
**Initial state:** board=all zeros (3×4).  
**Goal:** board[1][2]=2 and board[0][3]=1.  
**Expected result:** SOLVED — 2 steps.

---

### 2d_array_obj
**Features:** 2D arrays, user objects  
**Tests:** A 2D array of user objects (a seating plan). Actions swap two seats
within a row, testing object-typed 2D array read/write.  
**Initial state:** seats=[[alice, bob], [carol, dave]].  
**Goal:** seats[0][0]=bob and seats[0][1]=alice.  
**Expected result:** SOLVED — 1 step (row-0 swap).

---

### 2d_array_intsets
**Features:** 2D arrays, sets of bounded integers  
**Tests:** A 2D array whose elements are sets of bounded integers. Actions copy
set values between cells, gated by membership in an `active_c` set.  
**Initial state:** codes row0=[{1,2},{3},{4,0}], row1=[{0},{0},{0}], active_c={0,2}.  
**Goal:** 1 ∈ codes[1][0] and 4 ∈ codes[1][2].  
**Expected result:** SOLVED.

---

### 2d_array_objsets
**Features:** 2D arrays, sets of user objects, union  
**Tests:** A 2D array whose elements are sets of user objects. Actions propagate
object-set membership across rows using union.  
**Initial state:** labels row0=[{red,blue},{green},{blue,green}], row1=[{},{},{}], active_c={1,2}.  
**Goal:** green ∈ labels[1][1] and blue ∈ labels[1][2].  
**Expected result:** SOLVED.

---

### 2d_whole_goal
**Features:** 2D arrays, whole-array equality in goal  
**Tests:** The goal specifies exact equality for an entire 2D array, not just
individual cells. Verifies that whole-array comparison is encoded correctly.  
**Initial state:** board=all zeros (3×4).  
**Goal:** board = [[1,0,0,0],[0,2,0,0],[0,0,1,0]].  
**Expected result:** SOLVED — 3 steps (one per non-zero cell).

---

### 3d
**Features:** 3D arrays, bounded integers, basic read/write  
**Tests:** Individual cell fill and clear on a 2×2×2 integer tensor.
Verifies that 3-index read (`(read (tensor) ?d ?r ?c)`) and the matching
3-index write (`(write ((tensor) ?d ?r ?c) ?v)`) are encoded correctly.  
**Initial state:** tensor=all zeros (2×2×2).  
**Goal:** tensor[0][1][0]=3 and tensor[1][0][1]=2.  
**Expected result:** SOLVED — 2 steps (`set(0,1,0,3)`, `set(1,0,1,2)`).

---

### 3d_whole_goal
**Features:** 3D arrays, whole-array equality in goal  
**Tests:** The goal specifies exact equality for an entire 3D array via a
literal `array.mk` with three levels of nesting. Extends `2d_whole_goal`
to the 3D case.  
**Initial state:** tensor=all zeros (2×2×2).  
**Goal:** tensor = [[[1,2],[3,0]],[[0,4],[0,0]]].  
**Expected result:** SOLVED — 4 steps (one per non-zero cell).

---

### multi_3d_array
**Features:** 3D arrays, multiple independent 3D array fluents  
**Tests:** Two independent 3D array fluents (`src`, `dst`) in the same domain.
An action reads a value from `src[d][r][c]` and writes it to `dst[d][r][c]`.
Extends `multi_2d_array` to the 3D case.  
**Initial state:** src=[[[1,2],[3,4]],[[0,1],[2,3]]], dst=all zeros.  
**Goal:** dst=src (whole-array equality).  
**Expected result:** SOLVED — 7 steps (one per non-zero cell).

---

### 4d
**Features:** 4D arrays, bounded integers, read/write  
**Tests:** A 2×2×2×2 hypercube. Verifies that the N-dimensional array
infrastructure handles depth 4: 4-index read `(read (hypercube) ?a ?b ?c ?e)`
and 4-index write `(write ((hypercube) ?a ?b ?c ?e) ?v)`.  
**Initial state:** hypercube=all zeros (2×2×2×2).  
**Goal:** hypercube[0][1][0][1]=2 and hypercube[1][0][1][0]=1.  
**Expected result:** SOLVED — 2 steps.

---

### 4d_index_order
**Features:** 4D arrays, bounded integers, index ordering validation  
**Tests:** A 4×3×2×1 array where every dimension has a distinct size. Three goal
cells are chosen so that each one sits at the maximum valid index of a different
dimension:
- `arr[3][0][0][0]=1` — d0 maxed (index 3 ∉ d1's range 0–2)
- `arr[0][2][0][0]=2` — d1 maxed (index 2 ∉ d2's range 0–1)
- `arr[0][0][1][0]=3` — d2 maxed (index 1 ∉ d3's range {0})

If any two dimensions were transposed in the encoder the corresponding `set`
action would require an out-of-range index for the swapped type, meaning the
goal cell would never be written and the problem would be UNSOLVABLE.  
**Initial state:** arr=all zeros (24 cells).  
**Goal:** arr[3][0][0][0]=1, arr[0][2][0][0]=2, arr[0][0][1][0]=3.  
**Expected result:** SOLVED — 3 steps: `set(3,0,0,0,1)`, `set(0,2,0,0,2)`, `set(0,0,1,0,3)`.

---

### fluent_index_2d
**Features:** 2D arrays, read-modify-write  
**Tests:** Read-modify-write on a 2D array cell: `board[i][j] += 1`. Verifies
that double-index read/write composes with arithmetic.  
**Initial state:** board=[[1,3,0],[2,0,4]].  
**Goal:** board[1][1]=3.  
**Expected result:** SOLVED — 3 steps (`inc(1,1)` ×3).

---

### multi_2d_array
**Features:** 2D arrays, multiple independent 2D array fluents  
**Tests:** Two independent 2D arrays in the same domain; copy src→dst cell by cell.  
**Initial state:** src=[[3,1],[4,2]], dst=[[0,0],[0,0]].  
**Goal:** dst=[[3,1],[4,2]].  
**Expected result:** SOLVED — 4 steps (one copy per cell).

---

### forall_const_range
**Features:** forall, bounded integers, constant range  
**Tests:** A `forall` loop over the literal range [0..3] in both a precondition
(checking all cells are zero) and an effect (incrementing all cells).  
**Initial state:** cell(0..3)=[0,0,0,0].  
**Goal:** cell(0)=1 and cell(3)=1.  
**Expected result:** SOLVED — 1 step (`inc_all`).

---

### forall_array_body
**Features:** forall, arrays, constant range in loop body  
**Tests:** `forall` with a constant range whose body reads/writes array cells.
The loop is guarded by a boolean gate to ensure correct sequencing.  
**Initial state:** cells=[2,1,3,1], gate_open=false.  
**Goal:** cells[0]=0 and cells[3]=0.  
**Expected result:** SOLVED — 2 steps (`open_gate`, `reset_all`).

---

### forall_obj_param
**Features:** forall, arrays, object action parameter  
**Tests:** `forall` over a constant integer range inside an action whose own
parameter is a user object (not a bounded integer). Verifies that the
object parameter and the forall index coexist correctly.  
**Initial state:** board=[5,3,7,2], admin not authorized.  
**Goal:** all board cells=0.  
**Expected result:** SOLVED — 2 steps (`authorize(admin)`, `wipe(admin)`).

---

### forall_param_range
**Features:** forall, arrays, parameter as upper bound AND parameter as both bounds  
**Tests:** Two forall forms: `mark_prefix` uses `forall ?i in [0..?n]` (param upper bound);
`check_window` uses `forall ?i in [?lo..?hi]` (param lower AND upper bound). Both are
required to reach the goal.  
**Initial state:** cells=[0,5,7,0,0].  
**Goal:** prefix_done AND window_clear.  
**Expected result:** SOLVED — 4 steps (`zero(1)`, `zero(2)`, `mark_prefix(4)`, `check_window(1,2)`).

---

### forall_column_guard
**Features:** forall, arrays (2D), int action parameter shared with an earlier action  
**Tests:** A `forall` range variable and an int action parameter used as the two
indices of the same array read, `(read (board) ?k ?c)`, in an action whose `?c` is
declared with the same name and type by an earlier action. The two actions share one
interned parameter node, so a substitution cache that ignores the name→slot mapping
lets the earlier action's grounding answer for `?c`, and every `rotate_col_up_k`
guards the diagonal `board[j][j]` instead of column `k`. Regression test for that
capture (UP `IntParameterActionsRemover`).  
**Initial state:** board=[[0,1,2],[3,4,5],[6,7,8]], marker=4 (on the diagonal cell [1][1]), probes=0.  
**Goal:** board[0][0]=3.  
**Expected result:** SOLVED — 1 step (`rotate_col_up(0)`; column 0 is [0,3,6] and holds
no 4, so the guard admits it). Under the diagonal mis-expansion the guard reads
board[1][1]=4, every rotation is blocked and the run comes back UNSOLVABLE.

---

### exists_range
**Features:** exists, bounded integers, array reads in condition  
**Tests:** Existential quantification over a bounded-integer range where the body
reads an array cell and compares against a threshold.  
**Initial state:** cells=[3,7,1,0].  
**Goal:** found_high.  
**Expected result:** SOLVED — 1 step (`detect`, witnesses i=1 where cells[1]=7>5).

---

### conditional_array
**Features:** arrays, conditional effects  
**Tests:** A `when` conditional effect that writes to an array only when a boolean
predicate holds. One chip satisfies the condition, another does not.  
**Initial state:** cells=[0,0,0], ok_chip live, dead_chip not live.  
**Goal:** cells[1]=3.  
**Expected result:** SOLVED — 1 step (`activate(ok_chip, 1)`).

---

### conditional_set
**Features:** sets, conditional effects  
**Tests:** A `when` conditional effect that adds an element to a set only when
a quarantine predicate is not set on the element.  
**Initial state:** pool empty, gamma quarantined.  
**Goal:** alpha ∈ pool and beta ∈ pool.  
**Expected result:** SOLVED — 2 steps (`admit(alpha)`, `admit(beta)`).

---

### ipar_boundary_prune
**Features:** bounded integers, arrays, IPAR pruning  
**Tests:** After IPAR expansion, two action groundings (`step_right_0` and
`step_left_3`) are structurally out-of-bounds and must be pruned. Verifies
that the planner removes unreachable boundary groundings.  
**Initial state:** cursor=0, log=[9,9,9,9].  
**Goal:** cursor=3, log[2]=2.  
**Expected result:** SOLVED — 3 steps (`step_right(1)`, `step_right(2)`, `step_right(3)`).

---

### pancake_bounded
**Features:** forall, bounded integers, parameter-dependent range, sorting  
**Tests:** Pancake sorting with a `flip(k)` action that reverses the prefix of
length k using a `forall` loop. Non-trivial plan length; exercises large
parameter-bounded forall expansion.  
**Initial state:** val=[1,8,9,6,7,5,3,0,2,4].  
**Goal:** val=[0,1,2,3,4,5,6,7,8,9].  
**Expected result:** SOLVED.

---

### two_explicit_writes_2d
**Features:** arrays (2D), constant-index writes, IPAR cell SVs, frame axioms  
**Tests:** Writes with ALL indices constant on a 2D array, compiling to
multi-bracket IPAR cell SVs ("board[0][1]") — the path that broke when the
cell-name parser only handled single-bracket 1D names.  The second write's
precondition reads the first write's cell, forcing a 2-step plan so frame
axioms must carry 2D cell values across timesteps; an untouched cell appears
in the goal as a frame check.  
**Initial state:** board = 2×3 zeros.  
**Goal:** board[0][1]=5, board[1][2]=7, board[1][0]=0.  
**Expected result:** SOLVED — 2 steps (`write_a()`, `write_b()`).

---

### write_3d_const_cells
**Features:** arrays (3D), constant-index writes, IPAR cell SVs  
**Tests:** two_explicit_writes_2d at depth 3: cell SVs with three bracket
groups ("cube[0][1][0]").  The cell-name parser, nested store/select chains,
and frame axioms must all handle three index groups.  
**Initial state:** cube = 2×2×2 zeros.  
**Goal:** cube[0][1][0]=4, cube[1][0][1]=9, cube[1][1][1]=0.  
**Expected result:** SOLVED — 2 steps (`poke_a()`, `poke_b()`).

---

### whole_2d_replace_then_write
**Features:** arrays (2D), whole-array assign, nested array.mk effect value, cell write  
**Tests:** Whole-array ASSIGN of a 2D array with a nested `array.mk` value
(whole_array_replace covers 1D only), followed by a constant cell write on
the replaced array in the next step — the empty-indices replacement record
and a point-write record coexist on the same parent SV in the frame-axiom
ITE chain.  
**Initial state:** pad = 2×2 zeros.  
**Goal:** pad[0][0]=1, pad[0][1]=2, pad[1][0]=3, pad[1][1]=9.  
**Expected result:** SOLVED — 2 steps (`load()`, `touch()`).

---

## Stress / regression additions (June 2026)

These probe feature combinations that the rest of the suite did not exercise.
All were verified against the current backend.

### whole_sv_assign
**Features:** arrays (1D), whole-array assign with a *fluent-valued* RHS  
**Tests:** `(assign (dst) (src))` copies an entire array from one fluent to
another — the empty-indices replacement record whose value is a `STATE_VARIABLE`
(live array variable), not an `array.mk` literal. Complements `whole_array_replace`
(literal RHS) by exercising the SV-valued replacement path.  
**Initial state:** src=[4,5,6], dst=[0,0,0].  
**Goal:** dst[0]=4, dst[2]=6.  
**Expected result:** SOLVED — 1 step (`copy_all`).

---

### size1_array
**Features:** arrays, degenerate size-1 dimension  
**Tests:** `(array 1 val)` — the smallest possible array. Verifies the N-D
store/select, frame ITE chain, and `array.mk` default-fill never assume size ≥ 2.  
**Initial state:** a=[0].  
**Goal:** a[0]=5.  
**Expected result:** SOLVED — 1 step (`bump`).

---

### whole_array_precond
**Features:** arrays, whole-array equality in a **precondition**  
**Tests:** `(= (a) (array.mk (1 2 3)))` gating an action precondition (the rest
of the suite uses whole-array equality only in goals). The `ARRAY_CONSTANT` must
be built and equated against the live array variable at the action's timestep.  
**Initial state:** a=[1,2,0].  
**Goal:** `(done)`.  
**Expected result:** SOLVED — 2 steps (`set2`, `finish`).

---

### cond_array_read
**Features:** arrays, conditional effect whose `when` condition reads an array cell  
**Tests:** `(when (> (read (cells) ?i) 5) (increase (hits) 1))` — an array read
inside a conditional-effect guard (`conditional_array` gates on a plain boolean
predicate instead).  
**Initial state:** cells=[7,2,8], hits=0.  
**Goal:** hits=2.  
**Expected result:** SOLVED — 2 steps (scan two cells > 5).

---

### bnb_optimal_unsound  ⚠️ KNOWN-BUG REPRODUCER
**Features:** arrays (1D), whole-array assign — **B&B/abstract-suffix soundness**  
**Tests:** A trivially solvable 1-step copy domain. Exposes the documented array
soundness bug in the abstract-suffix encoder
(`encode_mod_v_equivalences` reads `epc_index_` only, so array writes — recorded
in `array_epc_index_` — are invisible and `dst` is asserted unmodifiable). See
docs/ChangesInPipeline.md §5.5.  
**Initial state:** src=[3,7], dst=[0,0].  
**Goal:** dst[1]=7.  
**Expected result (default seq satisficing — what the auto-runner uses):**
SOLVED — 1 step (`copy`).  
**Bug (run `--mode optimal`):** `UNSOLVABLE_PROVEN` at T0 — **false**; a 1-step
plan exists. Reproduce with:
`solve.py -d bnb_optimal_unsound/domain.pddl -p bnb_optimal_unsound/instance.pddl --mode optimal`

---

## Error Tests (`X_` prefix)

These domains encode violations. The expected result is a **compile-time or
semantic error** — the system should reject the domain/instance rather than
produce a plan.

---

### X_array_const_index_oob
**Violation:** Static out-of-bounds array access.  
**Detail:** Literal indices 4 and 7 used on a size-4 array (valid indices 0–3).
The out-of-bounds values are embedded as integer literals — no range analysis
needed, only static index checking.  
**Expected result:** Error — index out of bounds.

---

### X_array_mk_elem_out_of_range
**Violation:** `array.mk` initializer with elements outside the element type range.  
**Detail:** `(array.mk (0 99 -2))` for an element type `(number 0 5)`. Element
99 exceeds the upper bound and -2 is below the lower bound. A validator that
checks only element count (not values) misses this.  
**Expected result:** Error — element value out of range.

---

### X_array_mk_overcount
**Violation:** `array.mk` supplies more values than the declared array size.  
**Detail:** Array declared with size 3 but the `:init` provides 5 values.  
**Expected result:** Error — array size mismatch.

---

### X_bounded_arith_overflow
**Violation:** Arithmetic result exceeds the declared bounded-integer type.  
**Detail:** `a, b ∈ [0,5]`; effect `(assign (c) (+ (a) (b)))` can produce 10,
which exceeds the upper bound of 5. Also `(a * b)` can reach 25 and
`(- 0 (a))` goes negative. A naive checker that validates each operand's type
individually but not the result type misses this.  
**Expected result:** Error — arithmetic overflow of bounded type.

---

### X_bounded_init_overflow
**Violation:** Initial value of a bounded-integer fluent is outside its declared range.  
**Detail:** A fluent of type `(number 0 5)` is initialized to 99 and -3, both
outside [0,5].  
**Expected result:** Error — initial value out of bounds.

---

### X_computed_index_no_guard
**Violation:** Computed array index has no out-of-bounds guard.  
**Detail:** Expressions `(+ ?i 1)` and `(- ?i 1)` used as array indices
without guards. When `?i` is at the type maximum (3), `(+ 3 1) = 4` is
statically detectable as out of bounds. Similarly `(- 0 1)` at the minimum.  
**Expected result:** Error — potential index out of bounds.

---

### X_double_write_same_cell
**Violation:** Two writes to the same array cell in a single action effect.  
**Detail:** Two sequential `(write (cells) (1) …)` effects in one conjunction.
Under parallel semantics the second read should see the pre-action value, but
the two writes conflict. Also tests whether the system catches
order-dependent read-after-write bugs.  
**Expected result:** Error — conflicting effects on the same cell.

---

### X_exists_in_effect
**Violation:** `exists` quantifier used inside an `:effect` body.  
**Detail:** The spec permits `exists` only in preconditions. Using it in an
effect is a syntactic/semantic violation.  
**Expected result:** Error — existential quantifier in effect.

---

### X_fluent_as_array_index
**Violation:** A state fluent used as an array index.  
**Detail:** `(read (cells) (head))` where `(head)` is a scalar numeric fluent,
not a constant or bounded-integer parameter. Dynamic fluent-valued indices
are explicitly listed in the pitfalls table as unsupported.  
**Expected result:** Error — fluent-valued array index.

---

### X_forall_range_exceeds_array
**Violation:** `forall` loop range exceeds the array size.  
**Detail:** `(forall (?i - (number 0 7)) (write (cells) (?i) 0))` on a size-4
array. Indices 4–7 are out of bounds. The mismatch is between the forall
bound type and the array declaration.  
**Expected result:** Error — forall range out of bounds for array.

---

### X_member_on_array
**Violation:** `member` predicate applied to an array fluent instead of a set.  
**Detail:** `(member ?v (cells))` where `(cells)` is an array type. `member`
is defined only for set fluents.  
**Expected result:** Error — type mismatch, expected set got array.

---

### X_3d_write_wrong_dim
**Violation:** 3D array accessed with only two indices.  
**Detail:** `(read (tensor) ?d ?r)` supplies 2 indices for a `(array 2 2 2 val)` array;
the matching `(write ((tensor) ?d ?r) ?v)` uses the same 2-index write syntax.
After 2 reads the inner type is still an array (not a scalar), so the
equality `(= (read …) 0)` is ill-typed. Mirrors `X_read_2d_one_index` but
for the 3D case.  
**Expected result:** Error — type error at parse time (comparison of array
with integer is ill-formed).

---

### X_read_2d_one_index
**Violation:** 2D array accessed with only one index.  
**Detail:** `(read (board) ?i)` supplies one index for a 2D array;
`(write (board) (?i) 9)` uses 1D write syntax on a 2D array. Both
miscount the required index dimensions.  
**Expected result:** Error — dimension arity mismatch.

---

### X_read_as_array_index
**Violation:** An array read result used as the index into another array.  
**Detail:** `(read (data) (read (pointers) ?i))` — nested reads as dynamic
indices. A checker that looks only for plain function names as indices may
miss this variant.  
**Expected result:** Error — dynamic (read-valued) array index.

---

### X_2d_write_on_1d_array
**Violation:** Dimension arity mismatch in both directions.  
**Detail:** A 2D double-paren write applied to a 1D array, and a 1D
single-paren write applied to a 2D array, both in the same domain.  
**Expected result:** Error — dimension mismatch.

---

### X_set_mk_arithmetic_effect
**Violation:** Arithmetic expression inside `set.mk` in an effect.  
**Detail:** `(assign (chain) (set.mk ((+ ?n 1) ?n)))` — arithmetic inside
`set.mk` is listed in the pitfalls table as unsupported in effects.  
**Expected result:** Error — arithmetic not allowed inside `set.mk` effect.

---

### X_set_mk_elem_out_of_range
**Violation:** `set.mk` initializer contains elements outside the element type range.  
**Detail:** `(set.mk (1 3 15))` for element type `(number 0 9)` — element 15
exceeds the upper bound.  
**Expected result:** Error — set element out of range.

---

### X_set_mk_params_effect
**Violation:** Action parameters inside `set.mk` in an effect.  
**Detail:** `(assign (basket) (set.mk (?a ?b)))` — parameters inside `set.mk`
in effects are explicitly forbidden (pitfalls table). Only constants are allowed.  
**Expected result:** Error — parameters not allowed inside `set.mk` effect.

---

### X_set_of_set_type
**Violation:** Nested set type (set whose element type is itself a set).  
**Detail:** A fluent declared as `(set tagset)` where `tagset` is `(set tag)`.
The nesting matrix marks this as not compilable; only bounded integers and
user objects are valid set element types.  
**Expected result:** Error — nested set type not supported.

---

### X_set_ops_type_mismatch
**Violation:** Set operation applied to sets with incompatible element types.  
**Detail:** `(subset (obj_bag) (int_bag))` where one holds objects and the
other holds bounded integers. A checker that only verifies "both are set
types" without comparing element types silently accepts this.  
**Expected result:** Error — element type mismatch in set operation.

---

### X_two_svs_in_pred
**Violation:** Two array reads appear as separate arguments to the same boolean predicate.  
**Detail:** `(linked (read (board) ?i ?j) (read (board) ?k ?l))` — two nested
state-variable reads in the same predicate call. The spec's restrictions table
marks this as unsupported.  
**Expected result:** Error — multiple state-variable reads in predicate arguments.

---

### X_write_value_exceeds_elem_bound
**Violation:** Literal value written to an array cell exceeds the element type bounds.  
**Detail:** Literal constant 99 written into a cell with element type
`(number 0 5)`. Bonus: the matching precondition `(= (read …) 99)` is
statically unsatisfiable.  
**Expected result:** Error — write value out of element type range.

---

### X_sem_increase_overflow
**Violation:** `increase` effect guarantees overflow for every possible state.  
**Detail:** `(increase (counter) 100)` for `counter: (number 0 5)`.
The delta 100 > hi − lo = 5, so the post-increase value is always outside
[0,5]. No runtime analysis is needed — static interval arithmetic
([0+100, 5+100] = [100,105]) is entirely outside the declared range.  
**Expected result:** Error — guaranteed overflow on increase.

---

### X_sem_decrease_underflow
**Violation:** `decrease` effect guarantees underflow for every possible state.  
**Detail:** Two variants: delta=50 and delta=7 applied to `level: (number 3 9)`.
For delta=7: `hi − delta = 2 < lo = 3`, so the result is always below the
lower bound.  
**Expected result:** Error — guaranteed underflow on decrease.

---

### X_sem_assign_type_narrowing
**Violation:** Assignment from a wider bounded-integer type to a narrower one.  
**Detail:** `(assign (target ?r) (current ?r))` where `target: (number 18 23)`
and `current: (number 15 25)`. The source allows values 15–17 and 24–25,
which are outside the destination range. Widening (sub→super) is safe;
narrowing (super→sub) is not.  
**Expected result:** Error — source type is not a subrange of the destination type.

---

### X_sem_cardinality_of_scalar
**Violation:** `cardinality` applied to a scalar (non-set) fluent.  
**Detail:** `(cardinality (score))` where `score: (number 0 9)`. Cardinality
is defined only for set types. Also tests `(assign (bonus) (+ (cardinality (score)) 1))`
to verify that cardinality embedded in arithmetic is caught too.  
**Expected result:** Error — `cardinality` requires a set-typed argument.

---

### X_sem_add_wrong_elem_type
**Violation:** `add` applied with an element of the wrong type.  
**Detail:** `(add item_a (int_bag))` where `int_bag: (set level)` and
`item_a` is an object. Also `(add 5 (obj_bag))`. The element's static type
is known; a checker that validates only that the second argument is a set
fluent misses the element type mismatch.  
**Expected result:** Error — element type mismatch in `add`.

---

### X_sem_member_wrong_elem_type
**Violation:** `member` test with an incompatible element type.  
**Detail:** Three variants: object in int-set, integer in object-set, and a
parameter of object type checked against an int-set. A planner might treat
these as "always false" (silently killing the action) rather than rejecting
the domain.  
**Expected result:** Error — element type mismatch in `member`.

---

### X_sem_arith_elem_exceeds_type
**Violation:** Arithmetic set-element expression exceeds the element type range unconditionally.  
**Detail:** `(add (+ ?n 1) (chain))` where `?n: (number 8 9)` and the element
type is `(number 0 9)`. When `?n = 9`, `(+ 9 1) = 10 > 9`. Because the
parameter type makes the maximum value unconditional, the guard `(< ?n 9)`
can never be satisfied — the overflow is statically guaranteed for a specific grounding.  
**Expected result:** Error — arithmetic element value exceeds type bounds.

---

### X_sem_write_wrong_elem_type
**Violation:** Value of wrong type written into an array cell.  
**Detail:** `(write (int_arr) (?i) item_a)` writes an object into a numeric
array; `(write (obj_arr) (?i) 5)` writes an integer into an object array.
Encoding an object silently as its integer index would produce aliasing bugs.  
**Expected result:** Error — write value type does not match array element type.

---

### X_set_union_type_mismatch
**Violation:** `union` of two sets with incompatible element types.  
**Detail:** `(assign (res) (union (ob) (ib)))` where `ob: (set item)` and
`ib: (set lvl)` (bounded int). `X_set_ops_type_mismatch` covers the boolean
`subset` predicate; this covers a set-**valued** operation in an effect, a
different code path. (Confirmed: `intersect` and `difference` reject identically.)  
**Expected result:** Error — incompatible value type
(`set{item}` vs `set{integer[0,5]}`). *(Rejected at model-build time.)*

---

### X_nested_forall_effect
**Violation:** `forall` nested directly inside another `forall` in an `:effect`.  
**Detail:** A 2-D wipe written as `(forall (?i …) (forall (?j …) (write …)))`.
The reader rejects nested-forall effects outright; a 2-D loop must use a single
forall addressing both indices or separate effects. Locks in the rejection so a
future change cannot silently start mis-expanding it.  
**Expected result:** Error — "Nested forall on effects are not supported."

---

### X_set_mk_obj_in_domain
**Violation:** `set.mk` with **object** elements used in a **domain** expression.  
**Detail:** `(member ?x (set.mk (a b)))` in an action precondition. Objects are
problem-scoped, so `a`/`b` are unknown at domain-parse time; the reader then tries
to parse them as integers. Integer `set.mk` literals in domain expressions DO work
(`sets_const`).  
**Expected result:** Error — object `set.mk` literal cannot be resolved in the
domain. ⚠️ *Note: the current failure mode is an ungraceful Python `ValueError`
(`invalid literal for int(): 'a'`) rather than a clean domain-level message.*

---

### X_effect_array_mk_overcount
**Violation:** `array.mk` in an **effect** supplies more elements than the array size.  
**Detail:** `(assign (a) (array.mk (1 2 3 4 5)))` for a size-3 array.
`X_array_mk_overcount` covers the `:init` path; this covers the effect-value
construction site.  
**Expected result:** Error — element count does not match declared size.
⚠️ *Note: currently rejected with an empty error message (`Parse error:` with no
detail); a descriptive message would be an improvement.*

---

### X_neg_bounded_bounds  ⚠️ LIMITATION / FEATURE GAP
**Violation (today):** bounded-integer type with a **negative** lower bound.  
**Detail:** `(:types temp - (number -5 5))`. The grammar parses both bounds with
`Word(pyparsing.nums)` (digits only), so the leading `-` is a parse error. Negative
bounded integers are a reasonable modelling need (temperatures, balances, signed
offsets), so this is arguably a **feature gap** rather than a true user error: if
support is added (numeric token accepts an optional sign), this test should be
reclassified as a valid solving test (its instance has a real 5-step plan: `t`
from −3 to 2).  
**Expected result (today):** Parse error — `Expected ')', found '-'`.

---

### X_difference_type_mismatch
**Violation:** `difference` of two sets with incompatible element types.  
**Detail:** `(assign (result) (difference (obj-bag) (int-bag)))` where `obj-bag: (set item)`
and `int-bag: (set level)`. `X_set_ops_type_mismatch` covers union, intersection, subset, and
disjoint; this covers `difference`, the only set-valued operation previously missing a
type-mismatch test.  
**Expected result:** Error — incompatible element types for `difference`.

---

### X_read_3d_partial_index
**Violation:** 3D array accessed with only 2 indices — returns a 1D sub-array, not a scalar.  
**Detail:** `(assign (result) (read (tensor) ?d ?r))` where `tensor` is `(array 2 2 2 val)`.
With two indices, the expression refers to a `(array 2 val)` slice, not a scalar `val`.
Mirrors `X_read_2d_one_index` for the 3D case.  
**Expected result:** Error — type mismatch (array slice assigned to scalar, or scalar assigned to slice).

---

### X_sem_remove_arithmetic_exceeds_type
**Violation:** Arithmetic element expression in `remove` exceeds the set's element type range unconditionally.  
**Detail:** `(remove (+ ?n 3) (bag))` where `?n: (number 7 9)` and element type is `[0,9]`.
The minimum result is `7+3=10 > 9`. Mirrors `X_sem_arith_elem_exceeds_type` which covers `add`
and `member`; this locks in the same check for `remove`.  
**Expected result:** Error — arithmetic remove-element value unconditionally exceeds element type bounds.

---

### X_set_of_array_type
**Violation:** Set type whose element type is an array — not supported.  
**Detail:** `arr-set - (set arr)` where `arr - (array 3 val)`. The element type of a set
can be a bounded integer or a user object, not an array. Mirrors `X_set_of_set_type`.  
**Expected result:** Error — array type cannot be used as set element type.

---

## New Valid Tests

### bounded_multiply
**Features:** bounded integers, multiplication (`*`) of two bounded-int fluents  
**Tests:** `compute` assigns `area = width * height` — the first valid use of the `*`
operator between bounded-int fluents (previous multiplication only appeared in
`X_bounded_arith_overflow` as an overflow error).  
**Initial state:** width=2, height=1, area=0.  
**Goal:** area=6.  
**Expected result:** SOLVED — 2 steps (`set-height(3)`, `compute`).

---

### forall_when_effect
**Features:** forall, conditional effects (`when`), arrays  
**Tests:** A `forall` effect body with a `when` guard (Guide §5.5):
`(forall (?i - (number 0 3)) (when (= (active ?i) 1) (write (cells) (?i) 0)))`.
Cells flagged active are zeroed atomically.  
**Initial state:** cells=[5,7,3,9], active=[0,1,0,1].  
**Goal:** cells[0]=0, cells[1]=0, cells[3]=0.  
**Expected result:** SOLVED — 2 steps (`mark(0)`, `sweep`).

---

### whole_set_copy
**Features:** sets, whole-set assignment from another fluent (`assign set = set`)  
**Tests:** `(assign (backup) (original))` — copies an entire set fluent into another.
The `flush` action then clears the original, verifying the copy is independent.  
**Initial state:** original={1,2,3}, backup={}.  
**Goal:** 1,2,3 ∈ backup AND cardinality(original)=0.  
**Expected result:** SOLVED — 2 steps (`archive`, `flush`).

---

### empty_range_forall
**Features:** forall, empty integer range (`lo > hi`)  
**Tests:** A `forall (?i - (number 5 3))` over an empty range. The precondition is
vacuously true (no counter-example exists) and the effect is a no-op (no iterations).
PDDL-only test (UP API does not expose empty range variables directly).  
**Initial state:** cells=[3,1,4,1].  
**Goal:** done=True AND cells[0]=3 AND cells[2]=4 (unchanged).  
**Expected result:** SOLVED — 1 step (`noop-sweep`).

---

### exists_set_member
**Features:** exists, sets, set membership in exists body  
**Tests:** `(exists (?i - (number 3 7)) (member ?i (bag)))` — existential over
an integer range with set membership as the body condition.  
**Initial state:** bag={}.  
**Goal:** detected=True.  
**Expected result:** SOLVED — 2 steps (`fill`, `detect`).

---

### forall_set_member
**Features:** forall, sets, set membership in forall body  
**Tests:** `(forall (?i - (number 0 4)) (member ?i (bag)))` — universal quantifier
over an integer range with set membership as the body condition. `verify` fires only
when all of {0,1,2,3,4} are in the bag.  
**Initial state:** bag={0,2,4}.  
**Goal:** verified=True.  
**Expected result:** SOLVED — 3 steps (`fill(1)`, `fill(3)`, `verify`).

---

### cardinality_goal
**Features:** sets, cardinality equality (`= n`) in goal  
**Tests:** `(= (cardinality (bag)) 3)` used directly as a goal condition. Tests
the equality form of cardinality comparison (previous tests only used `>=` and `<`
in preconditions).  
**Initial state:** bag={1}.  
**Goal:** cardinality(bag) = 3.  
**Expected result:** SOLVED — 2 steps (`fill(2)`, `fill(3)`).

---

### 3d_obj
**Features:** 3D arrays, object element type  
**Tests:** A 3D `(array 2 2 2 person)` — the first test combining 3 dimensions with
an object (non-integer) element type (previous 3D tests used bounded-int elements;
object arrays only existed in 1D and 2D).  
**Initial state:** cube[d][r][c] = p{4d+2r+c}.  
**Goal:** two column-pairs swapped.  
**Expected result:** SOLVED — 2 steps (`swap-col(0,0,p0,p1)`, `swap-col(1,1,p6,p7)`).

---

### multi_when
**Features:** conditional effects, multiple independent `when` conditions in one action  
**Tests:** A single `process` action with two independent `when` guards: one fires
when `flag-a` is true, the other when `flag-b` is true. The planner must first
`activate` (set `flag-b`) so both guards hold when `process` fires.  
**Initial state:** flag-a=True, flag-b=False, cell-a=5, cell-b=5.  
**Goal:** cell-a=0, cell-b=0.  
**Expected result:** SOLVED — 2 steps (`activate`, `process`).

---

### when_numeric_guard
**Features:** conditional effects, bounded-integer comparison as `when` guard  
**Tests:** `(when (>= (counter) 5) (write (cells) (?i) 7))` — a `when` condition
whose guard is a numeric comparison between a fluent and a constant (not a predicate
or array-read comparison).  
**Initial state:** counter=3, cells=[0,0,0,0].  
**Goal:** cells[0]=7, cells[1]=7.  
**Expected result:** SOLVED — 4 steps (`inc`, `inc`, `stamp(0)`, `stamp(1)`).