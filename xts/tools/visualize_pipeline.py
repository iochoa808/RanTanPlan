#!/usr/bin/env python3
"""
Visualize UP compiler × remaining-feature compatibility and viable pipeline combinations.

IPAR (INT_PARAMETER_ACTIONS_REMOVING) always runs first and is not part of the
skip/keep decision — it is listed as a fixed prerequisite in §0.
"""

# ── ANSI colours ──────────────────────────────────────────────────────────────
GRN  = "\033[32m"; YLW = "\033[33m"; RED  = "\033[31m"
BLU  = "\033[34m"; CYN = "\033[36m"; MAG  = "\033[35m"
WHT  = "\033[37m"; DIM = "\033[2m";  BLD  = "\033[1m"
RST  = "\033[0m"

def clr(text, *codes): return "".join(codes) + text + RST

# ── Data ──────────────────────────────────────────────────────────────────────

# Features that can be "left" for RTP to handle natively (IPAR excluded — always runs)
FEATURES   = ["arrays", "sets", "count", "bounded", "usertype"]
FEAT_SHORT = {"arrays": "ARR", "sets": "SET", "count": "CNT",
              "bounded": "BND", "usertype": "UTYP"}

# UP compilers in pipeline order (after the mandatory IPAR prefix)
COMPILERS = ["ARRAYS", "SETS", "COUNT", "INTEGERS", "USERTYPE"]

# Compiler × feature cell values:
#   "own"   = compiler's primary job — removes this feature
#   "pass"  = passes through unchanged (empirically confirmed)
#   "crash" = confirmed runtime crash
#   "cond"  = conditional — domain-pattern-dependent
COMPAT = {
    # compiler      arrays    sets      count     bounded   usertype
    "ARRAYS":   ["own",    "pass",   "pass",   "pass",   "pass"  ],
    "SETS":     ["cond",   "own",    "pass",   "pass",   "pass"  ],
    "COUNT":    ["pass",   "pass",   "own",    "pass",   "pass"  ],
    "INTEGERS": ["crash",  "crash",  "crash",  "own",    "pass"  ],
    "USERTYPE": ["crash",  "crash",  "crash",  "pass",   "own"   ],
}

NOTES = {
    ("SETS",     "arrays"):  "crashes only when a set expression IS an array read\n"
                             "    (`x in arr[i]` pattern); simple array fluents pass through",
    ("INTEGERS", "arrays"):  "type conflict: array read returns int, but INTEGERS replaces\n"
                             "    bounded ints with a Number user-type →\n"
                             "    `(read arr i) == n0` becomes ill-typed",
    ("INTEGERS", "sets"):    "crashes when `len(set)` is compared with a bounded int —\n"
                             "    same Number-type mismatch as with arrays",
    ("INTEGERS", "count"):   "crashes when a Count expression appears in an integer comparison:\n"
                             "    `n2 <= Count(...)` is ill-typed after Number replacement",
    ("USERTYPE", "arrays"):  "NotImplementedError: walker has no handler for `read` expressions",
    ("USERTYPE", "sets"):    "NotImplementedError: walker has no handler for `member`, `len`,\n"
                             "    or set literals",
    ("USERTYPE", "count"):   "NotImplementedError: walker has no handler for `Count` nodes",
}

# RTP native capabilities (confirmed: can handle without UP pre-compilation)
RTP_NATIVE = {
    "arrays":   ("✓ native", "Z3 native Array theory"),
    "sets":     ("✓ native", "Z3 native Set theory"),
    "count":    ("✓ native", "Z3 cardinality / Count via Set theory"),
    "bounded":  ("✓ native", "Z3 bounded Int (current approach)"),
    "usertype": ("✓ native", "grounded object encoding"),
}

FEAT_TO_COMPILER = {
    "arrays":   "ARRAYS",
    "sets":     "SETS",
    "count":    "COUNT",
    "bounded":  "INTEGERS",
    "usertype": "USERTYPE",
}

# ── Rendering helpers ─────────────────────────────────────────────────────────

STATUS_STYLE = {
    "own":   (GRN, BLD, "  COMPILE "),
    "pass":  (GRN,      "   pass   "),
    "crash": (RED, BLD, "  CRASH  "),
    "cond":  (YLW, BLD, "   COND  "),
}

def render_cell(status):
    codes, label = STATUS_STYLE[status][:-1], STATUS_STYLE[status][-1]
    return clr(label, *codes)

# ── §0: Fixed prerequisite ────────────────────────────────────────────────────

def print_prerequisite():
    print(clr(f"\n{'━'*72}", BLD))
    print(clr("  §0  FIXED PREREQUISITE", BLD, CYN))
    print(clr(f"{'━'*72}", BLD))
    print()
    print(f"  {clr('IPAR', BLD, CYN)}  {clr('(INT_PARAMETER_ACTIONS_REMOVING)', DIM)}")
    print(f"  {clr('Always runs first — not part of the skip/keep decision.', DIM)}")
    print()
    print(clr("  Why: SETS, INTEGERS, and USERTYPE all require bounded int action", DIM))
    print(clr("  parameters to be already unrolled before they can run. ARRAYS is", DIM))
    print(clr("  the only compiler that handles them natively (tested OK), but", DIM))
    print(clr("  running IPAR first is zero cost and avoids all downstream issues.", DIM))
    print()

# ── §1: Compatibility Matrix ──────────────────────────────────────────────────

def print_matrix():
    CW = 11
    LW = 11
    sep = "─" * (LW + 1 + CW * len(FEATURES) + len(FEATURES))

    print(clr(f"{'━'*72}", BLD))
    print(clr("  §1  COMPILER × FEATURE COMPATIBILITY", BLD, CYN))
    print(clr(f"{'━'*72}", BLD))
    print()
    print(clr("  What happens when a compiler runs while a feature is still present:", DIM))
    print(clr("  (IPAR has already run; these are the remaining pipeline decisions)", DIM))
    print()

    for status in STATUS_STYLE:
        desc = {
            "own":   "compiler's primary job — removes this feature",
            "pass":  "passes feature through unchanged (empirically confirmed)",
            "crash": "confirmed runtime crash — cannot run",
            "cond":  "conditional — depends on usage pattern in domain",
        }[status]
        print(f"    {render_cell(status)}  {clr(desc, DIM)}")
    print()

    print(f"  {'Compiler':<{LW}}", end="")
    for f in FEATURES:
        print(clr(f"{FEAT_SHORT[f]:^{CW}}", BLD), end="")
    print()
    print(f"  {sep}")

    for compiler in COMPILERS:
        row = COMPAT[compiler]
        print(f"  {clr(compiler, BLD):<{LW+len(BLD+RST)}}", end="")
        for feat, status in zip(FEATURES, row):
            print(render_cell(status), end="")
        print()

    print(f"\n  {clr('Footnotes:', BLD, DIM)}")
    for (comp, feat), note in NOTES.items():
        lines = note.split("\n")
        print(f"  {clr(f'{comp} × {FEAT_SHORT[feat]}:', BLD)}  {lines[0]}")
        for l in lines[1:]:
            print(f"    {clr(l, DIM)}")
    print()


# ── §2: RTP Native Capabilities ──────────────────────────────────────────────

def print_rtp_native():
    print(clr(f"{'━'*72}", BLD))
    print(clr("  §2  RTP NATIVE CAPABILITIES", BLD, CYN))
    print(clr(f"{'━'*72}", BLD))
    print()
    print(clr("  Features RTP handles natively (no UP pre-compilation needed):", DIM))
    print()
    for feat in FEATURES:
        status, detail = RTP_NATIVE[feat]
        print(f"  {clr(f'{FEAT_SHORT[feat]:<6}', BLD)}  {clr(status, GRN, BLD)}  {clr(detail, DIM)}")
    print()


# ── §3: Skip-feature pipeline analysis ───────────────────────────────────────

def viable_pipeline(skip_feat):
    """
    Simulate running the pipeline with one feature left for RTP.
    Returns list of (compiler, status) and the set of features left for RTP.
    """
    skip_compiler = FEAT_TO_COMPILER[skip_feat]
    active_remaining = {skip_feat}
    result = []

    for comp in COMPILERS:
        if comp == skip_compiler:
            result.append((comp, "skipped"))
            continue
        row = COMPAT[comp]
        blocker = next(
            ((feat, status) for feat, status in zip(FEATURES, row)
             if feat in active_remaining and status == "crash"),
            None
        )
        if blocker is None:
            # compiler runs; remove any features it compiles away
            for feat, status in zip(FEATURES, row):
                if status == "own":
                    active_remaining.discard(feat)
            result.append((comp, "run"))
        else:
            result.append((comp, f"drop:{blocker[0]}"))
            # feature stays — compiler didn't run

    return result, active_remaining


def print_pipeline_analysis():
    print(clr(f"{'━'*72}", BLD))
    print(clr("  §3  SKIP-FEATURE CASCADE ANALYSIS", BLD, CYN))
    print(clr(f"{'━'*72}", BLD))
    print()
    print(clr("  For each feature left to RTP: which UP steps can still run,", DIM))
    print(clr("  and which are forced to drop because of the remaining feature?", DIM))
    print()

    for skip_feat in FEATURES:
        pipeline, leftover = viable_pipeline(skip_feat)
        skip_comp = FEAT_TO_COMPILER[skip_feat]
        dropped = [(comp, feat) for comp, status in pipeline
                   if status.startswith("drop:")
                   for feat in [status.split(":")[1]]
                   for comp in [comp]]

        print(f"  {clr('─'*68, DIM)}")
        cascade = clr(" → cascade drops: " + ", ".join(c for c, _ in dropped), YLW) if dropped else clr(" → clean", GRN)
        print(f"  {clr('Skip', BLD)} {clr(FEAT_SHORT[skip_feat], CYN, BLD)}"
              f"  {clr(f'({skip_comp})', DIM)}{cascade}")
        print()

        for comp, status in pipeline:
            if status == "run":
                print(f"    {clr('  ✓ RUN   ', GRN)}  {clr(comp, BLD)}")
            elif status == "skipped":
                print(f"    {clr('  ↓ SKIP  ', CYN, BLD)}  {clr(comp, BLD)}"
                      f"  {clr(f'← RTP handles {FEAT_SHORT[skip_feat]}', CYN)}")
            else:
                feat = status.split(":")[1]
                print(f"    {clr('  ✗ DROP  ', RED)}  {clr(comp, BLD)}"
                      f"  {clr(f'← crashes on {FEAT_SHORT[feat]} (not yet compiled away)', YLW)}")

        left_rtp = sorted(leftover, key=lambda f: FEATURES.index(f))
        print()
        if dropped:
            print(f"    {clr('→ RTP must handle:', YLW)} "
                  f"{clr(' + '.join(FEAT_SHORT[f] for f in left_rtp), BLD, YLW)}")
        else:
            print(f"    {clr('→ RTP handles only:', GRN)} "
                  f"{clr(FEAT_SHORT[skip_feat], BLD, GRN)}")
        print()

    print()


# ── §4: Viable complete pipelines ────────────────────────────────────────────

def print_viable_pipelines():
    print(clr(f"{'━'*72}", BLD))
    print(clr("  §4  VIABLE PARTIAL PIPELINES → RTP", BLD, CYN))
    print(clr(f"{'━'*72}", BLD))
    print()
    print(clr("  IPAR → [selected UP steps] → RTP(native features)", DIM))
    print(clr("  All pipelines begin with IPAR (omitted for brevity).", DIM))
    print()

    step_colour = {
        "ARRAYS": BLU, "SETS": MAG, "COUNT": WHT, "INTEGERS": YLW, "USERTYPE": GRN,
    }

    scenarios = [
        {
            "name":       "Full pipeline — baseline  (iasciu)",
            "up_steps":   ["ARRAYS", "SETS", "COUNT", "INTEGERS", "USERTYPE"],
            "rtp_native": [],
            "note":       "UP handles everything; RTP sees only bool/int/object fluents",
        },
        {
            "name":       "Skip INTEGERS  — RTP handles bounded  ★ current approach",
            "up_steps":   ["ARRAYS", "SETS", "COUNT", "USERTYPE"],
            "rtp_native": ["bounded"],
            "note":       "Clean cascade: USERTYPE runs fine (no arrays/sets/count remain)",
        },
        {
            "name":       "Skip USERTYPE  — RTP handles usertype",
            "up_steps":   ["ARRAYS", "SETS", "COUNT", "INTEGERS"],
            "rtp_native": ["usertype"],
            "note":       "Clean cascade: zero drops. Unexploited free win (symmetric to bounded)",
        },
        {
            "name":       "Skip INTEGERS + USERTYPE  — RTP handles bounded + usertype",
            "up_steps":   ["ARRAYS", "SETS", "COUNT"],
            "rtp_native": ["bounded", "usertype"],
            "note":       "Both clean skips combined. COUNT still compiles count conditions away.",
        },
        {
            "name":       "Skip COUNT + INTEGERS + USERTYPE  — RTP handles count + bounded + usertype",
            "up_steps":   ["ARRAYS", "SETS"],
            "rtp_native": ["count", "bounded", "usertype"],
            "note":       "COUNT conditions left for RTP. INTEGERS/USERTYPE already dropped\n"
                          "    (both crash on Count nodes), so skipping COUNT costs nothing extra.",
        },
        {
            "name":       "Skip ARRAYS + INTEGERS + USERTYPE  — RTP handles arrays + bounded + usertype",
            "up_steps":   ["SETS", "COUNT"],
            "rtp_native": ["arrays", "bounded", "usertype"],
            "note":       "SETS runs if domain has no `x in arr[i]` pattern (cond).\n"
                          "    COUNT passes arrays through cleanly.",
            "caveat":     "SETS crashes on array-of-set domains (`x in arr[i]`)",
        },
        {
            "name":       "Skip SETS + COUNT + INTEGERS + USERTYPE  — RTP handles sets + count + bounded + usertype",
            "up_steps":   ["ARRAYS"],
            "rtp_native": ["sets", "count", "bounded", "usertype"],
            "note":       "Only ARRAYS runs in UP (removes array fluents before RTP).\n"
                          "    All other features handled natively by RTP.",
        },
        {
            "name":       "Pure RTP — no UP compilation",
            "up_steps":   [],
            "rtp_native": ["arrays", "sets", "count", "bounded", "usertype"],
            "note":       "RTP handles everything via Z3. No UP pre-compilation at all.",
        },
    ]

    for i, s in enumerate(scenarios):
        print(f"  {clr(f'[{i}]', BLD, DIM)} {clr(s['name'], BLD)}")

        parts = [clr(step, step_colour.get(step, WHT), BLD) for step in s["up_steps"]]
        rtp_suffix = (clr(f"({'+'.join(FEAT_SHORT[f] for f in s['rtp_native'])})", GRN)
                      if s["rtp_native"] else "")
        parts.append(clr("RTP", GRN, BLD) + rtp_suffix)
        print(f"      {clr('→ ', DIM).join(parts)}")

        for line in s["note"].split("\n"):
            print(f"      {clr(line, DIM)}")
        if "caveat" in s:
            for line in s["caveat"].split("\n"):
                print(f"      {clr('⚠ ' + line, YLW)}")
        print()


# ── Main ──────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print_prerequisite()
    print_matrix()
    print_rtp_native()
    print_pipeline_analysis()
    print_viable_pipelines()