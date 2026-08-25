#!/usr/bin/env python3
"""
bench.py — unified performance benchmark for RanTanPlan.

Replaces compare_versions.py, benchmark_frame_modes.py, ab_array_encoding.py
and benchmark/run.py, which had grown into three copies of the same
compile-once/measure-many-times machinery, each hardcoding its own instance
list and sweeping exactly one axis (binary version, array-encoding, or
frame-mode/pipeline). This tool generalizes that into two orthogonal sweep
axes over one shared instance-discovery layer:

  COMPILE axis   — changes what's IN the protobuf (requires a fresh compile):
                     --pipeline native,up         (up_compilers vs full UP chain)
                     --up-compilers "IPAR,QR,GR"  (only applies to 'native')
  RUNTIME axis   — same protobuf, different binary invocation (cheap, repeatable):
                     --binary PATH  / --binary-ref GIT_REF  (compare binaries)
                     --strategy, --array-encoding, --array-frame-mode

Every instance is compiled ONCE per compile-config (isolated subprocess, so
UP's global environment doesn't collide across the ~300 domains this can
scan), then the resulting .pb is fed directly to the binary for each
runtime-config, N times, to get status/plan stability and timing percentiles.
This is RTP-binary-only (no FastDownward/other solvers, no --source variation)
— that combinatorial-correctness concern belongs to test_PDDL-XTS.py, which
stays a separate script.

INSTANCE FAMILIES (--families, comma list; default: all)
    xts-unit          xts/benchmarks/unit/*/problem.py       (X_* excluded by default)
    xts-translation   xts/benchmarks/translations/*/{domain,problem}.pddl
    xts-native        xts/benchmarks/domains/<name>/instances/*.pddl for a fixed domain list
    classic           pddl/test/<name>/{domain,problem}.pddl  (non-XTS baseline)
    synthetic         generated scaling domains, explicit per-generator size list (see
                      SYNTHETIC_GENERATORS): pancake/labyrinth/setops/set_merge/warehouse
                      (array/set stress + one array+set+count+bounded-int combo), plus 5
                      paired classic-vs-array/set generators (delivery/expedition/gripper/
                      visit_all bool-or-classic vs sets-or-array, fo_counters fixed-vs-scaled
                      ranges) ported from 0benchmark0/benchmark/generators/
    scaling           xts/benchmarks/scaling/instances/<domain>/n<N>.py — 80 fixed
                      size-parametrized instances across 11 domains, each a stub calling
                      its generator in ../../generators/. Same domains 'synthetic'
                      builds in-process, but pinned to a checked-in size list, so a
                      sweep is reproducible instance-for-instance across runs.
    paper             ~/unified-planning/docs/extensions/domains/<domain>/*.py — the 9 UP-paper
                       comparison domains (pancake-sorting, 15-puzzle, labyrinth, plotting,
                       puzznic, rush-hour, sokoban, dump-trucks, storytellers), each domain's
                       real instances.txt where one exists; dump-trucks/storytellers have no
                       instances.txt so they're swept over a representative size list instead
    paper-hc          ~/unified-planning/docs/extensions/domains/<domain>/handcrafted/*.pddl —
                       the ORIGINAL PDDL model each 'paper' domain's Python get_problem() model
                       is compared against (FD/Symk/ENHSP's 'hc' column). 7 domains have one
                       (15-puzzle, pancake-sorting, labyrinth, plotting, puzznic, rush-hour,
                       sokoban); dump-trucks/storytellers don't (no original model exists).
                       Plain classical PDDL, no XTS features, so --pipeline native is enough —
                       no uti/int/up/etc. compiled variants apply here

USAGE
    # Sanity pass: every instance, current binary, default config, 5 iters each
    python bench.py

    # Just one family, more repeats for stable timing
    python bench.py --families xts-native --iterations 30

    # A/B two array encodings across the whole XTS corpus
    python bench.py --array-encoding theory,uf --families xts-unit,xts-translation,xts-native

    # A/B frame modes, full 2x2 matrix
    python bench.py --array-encoding theory,uf --array-frame-mode disequality,ite

    # A/B two binaries (git refs; each is built once via a throwaway worktree)
    python bench.py --binary-ref HEAD~5 --binary-ref HEAD --families classic

    # Compile-time axis: native vs full UP pipeline, scaling instances only
    python bench.py --pipeline native,up --families scaling

    # Narrow to specific instances, list before running
    python bench.py --filter pancake --list
    python bench.py --filter pancake --iterations 50 --csv out.csv --json out.json

    # Raw per-repetition data (every compile attempt + every solve run, not just
    # the aggregated summary row) for later statistical analysis
    python bench.py --families xts-native --raw-csv raw.csv --csv summary.csv

CLUSTER (optional — everything above works unchanged if you never touch these)
    A "job" is one (instance, compile-config, runtime-config) combo — exactly
    one summary row. --job-index runs exactly one job instead of the full
    sweep, so a SLURM job array can fan out one job per array task:

    # 1. See how big the array needs to be (uses the same selection flags
    #    you'll pass to every array task)
    python bench.py --families xts-native --array-encoding theory,uf --count-jobs

    # 2. Get a ready-to-edit sbatch script (partition/account/time are
    #    cluster-specific — edit those, then `sbatch` it)
    python bench.py --families xts-native --array-encoding theory,uf \\
        --csv xts/results/r.csv --raw-csv xts/results/raw.csv --emit-sbatch run.sbatch

    # 3. Or drive --job-index yourself (also reads $SLURM_ARRAY_TASK_ID
    #    automatically, so the sbatch script above doesn't need to pass it)
    python bench.py --families xts-native --array-encoding theory,uf \\
        --csv xts/results/r.csv --raw-csv xts/results/raw.csv --job-index 42

    Each job writes its OWN csv/raw-csv (suffixed ".jobNNNN") so concurrent
    array tasks never contend for the same file.

    # 4. Merge the shards when the array finishes. Verifies against the
    #    manifest --emit-sbatch/--write-manifest left in the run directory, so
    #    a sweep that lost tasks reports which indices to requeue instead of
    #    silently producing a short summary.csv. Idempotent.
        python bench.py --merge xts/results/my_sweep/2026-08-07

    Best done automatically, as a SLURM job that waits on the array — see
    xts/cluster/submit.sh, or the recipe --emit-sbatch prints.
"""

import argparse
import csv
import datetime
import fnmatch
import importlib.util
import json
import os
import random
import re
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
import traceback
from collections import Counter
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, field
from typing import Callable, Dict, FrozenSet, List, Optional, Tuple

# This file lives in xts/tools/. ROOT is the repo root — it holds the base
# project (the rantanplan package, its built binary, the .venv, the git dir).
# XTS_ROOT is the extension subtree this tool belongs to.
_HERE = os.path.dirname(os.path.abspath(__file__))
XTS_ROOT = os.path.dirname(_HERE)
ROOT = os.path.dirname(XTS_ROOT)
_UP_PATH = os.path.expanduser("~/unified-planning")
for _p in (_UP_PATH, ROOT, _HERE):
    if _p not in sys.path:
        sys.path.insert(0, _p)
import glob as _glob
_UP_VENV_SP = _glob.glob(os.path.join(_UP_PATH, "venv/lib/python*/site-packages"))
if _UP_VENV_SP and _UP_VENV_SP[0] not in sys.path:
    sys.path.insert(1, _UP_VENV_SP[0])
del _glob, _UP_VENV_SP

from unified_planning.shortcuts import (
    Problem, Fluent, InstantaneousAction, RangeVariable, UserType, Object,
    IntType, ArrayType, SetType, BoolType, Equals, Not, Plus, Minus, SetMember,
    LT, LE, GE, GT, SetAdd, SetRemove, SetUnion, SetIntersection, SetSubseteq,
    SetCardinality, FluentExp, Count,
)

import solve as _solve  # load_pddl, load_python, apply_pipeline
from rantanplan.planner_wrapper import RantanPlanPlanner

VENV_PY  = os.path.join(ROOT, ".venv", "bin", "python")
XTS      = os.path.join(XTS_ROOT, "benchmarks")
DEFAULT_BIN = os.path.join(ROOT, "rantanplan", "cpp", "build", "rantanplan")
if not os.path.isfile(DEFAULT_BIN):
    DEFAULT_BIN = os.path.join(ROOT, "rantanplan", "bin", "rantanplan")

SOLVED = {"SOLVED_SATISFICING", "SOLVED_OPTIMALLY"}

XTS_NATIVE_DOMAINS = [
    "labyrinth", "labyrinth_v2", "drone", "dump-trucks", "fo-counters",
    "pancake-sorting", "pancake-sorting-bounded", "petrobras", "rover",
    "15-puzzle", "15-puzzle_v2",
]

# Per-folder overrides for xts/benchmarks/unit: a folder may force a non-default
# strategy (which may be a Chained/R2E encoder that rejects --array-encoding
# uf outright — see config.cpp's validate()). Keeps those combos from being
# reported as benchmark failures when they're really just inapplicable.
# Both are empty since array/set folders are sequential-only: a forall/exists-step
# override is rejected by StrategyFactory::create_encoder.
FOLDER_EXTRA:          Dict[str, str]         = {}
FOLDER_SKIP_ENCODINGS: Dict[str, FrozenSet[str]] = {}


# ═════════════════════════════════════════════════════════════════════════
# 1. Instance model & discovery
# ═════════════════════════════════════════════════════════════════════════

@dataclass
class Instance:
    name: str
    family: str
    loader: Callable[[], "Problem"]          # zero-arg; returns a UP Problem
    forced_strategy: Optional[str] = None
    skip_encodings: FrozenSet[str] = field(default_factory=frozenset)

    def ref(self) -> "InstanceRef":
        return InstanceRef(self.name, self.family, self.forced_strategy, self.skip_encodings)


@dataclass
class InstanceRef:
    """Picklable stand-in for Instance, sent to worker processes.

    Carries no loader closure (lambdas aren't picklable) — worker processes
    never need to build the Problem themselves; the isolated `--_compile`
    subprocess re-derives the full Instance (with loader) by name instead.
    """
    name: str
    family: str
    forced_strategy: Optional[str] = None
    skip_encodings: FrozenSet[str] = field(default_factory=frozenset)


def _pddl_loader(domain, problem):
    return lambda: _solve.load_pddl(domain, problem)


def _python_loader(path):
    return lambda: _solve.load_python(path)


def discover_xts_units(include_x: bool = False) -> List[Instance]:
    tdir = os.path.join(XTS, "unit")
    out = []
    for name in sorted(os.listdir(tdir)):
        if name.startswith("X_") and not include_x:
            continue
        d = os.path.join(tdir, name)
        py = os.path.join(d, "problem.py")
        if os.path.isfile(py):
            out.append(Instance(
                name=f"xts-unit/{name}", family="xts-unit", loader=_python_loader(py),
                forced_strategy=FOLDER_EXTRA.get(name),
                skip_encodings=FOLDER_SKIP_ENCODINGS.get(name, frozenset()),
            ))
    return out


def discover_xts_translations() -> List[Instance]:
    tdir = os.path.join(XTS, "translations")
    out = []
    for name in sorted(os.listdir(tdir)):
        d = os.path.join(tdir, name)
        dom, prob = os.path.join(d, "domain.pddl"), os.path.join(d, "problem.pddl")
        if os.path.isfile(dom) and os.path.isfile(prob):
            out.append(Instance(name=f"xts-translation/{name}", family="xts-translation",
                                loader=_pddl_loader(dom, prob)))
    return out


def discover_xts_native(domains=XTS_NATIVE_DOMAINS) -> List[Instance]:
    out = []
    for dname in domains:
        d = os.path.join(XTS, "domains", dname)
        dom, instdir = os.path.join(d, "domain.pddl"), os.path.join(d, "instances")
        if not (os.path.isfile(dom) and os.path.isdir(instdir)):
            continue
        for inst in sorted(os.listdir(instdir)):
            if inst.endswith(".pddl"):
                stem = inst[:-5]
                out.append(Instance(name=f"xts-native/{dname}/{stem}", family="xts-native",
                                    loader=_pddl_loader(dom, os.path.join(instdir, inst))))
    return out


def discover_classic(tests_dir=os.path.join(ROOT, "pddl", "test")) -> List[Instance]:
    out = []
    for name in sorted(os.listdir(tests_dir)):
        d = os.path.join(tests_dir, name)
        dom, prob = os.path.join(d, "domain.pddl"), os.path.join(d, "problem.pddl")
        if os.path.isfile(dom) and os.path.isfile(prob):
            out.append(Instance(name=f"classic/{name}", family="classic",
                                loader=_pddl_loader(dom, prob)))
    return out


def discover_scaling(instances_dir=os.path.join(XTS_ROOT, "benchmarks", "scaling", "instances")) -> List[Instance]:
    out = []
    if not os.path.isdir(instances_dir):
        return out
    for domain in sorted(os.listdir(instances_dir)):
        ddir = os.path.join(instances_dir, domain)
        if not os.path.isdir(ddir):
            continue
        for fn in sorted(os.listdir(ddir)):
            if fn.endswith(".py"):
                path = os.path.join(ddir, fn)
                stem = fn[:-3]
                out.append(Instance(name=f"scaling/{domain}/{stem}", family="scaling",
                                    loader=_python_loader(path)))
    return out


# ── UP-paper comparison domains (9 domains patched with get_problem() in the
#    unified-planning fork; see ~/unified-planning/docs/extensions/domains/
#    instance_loading.py). Unlike the other python-source families, these load a
#    SPECIFIC instance by name/size — not the file's own zero-arg get_problem() —
#    so they need their own loader instead of _python_loader.

UP_DOMAINS_ROOT = os.path.join(_UP_PATH, "docs", "extensions", "domains")

# dirname -> script filename, for the 6 domains with a real instances.txt
PAPER_NAMED_DOMAINS: Dict[str, str] = {
    "pancake-sorting": "PancakeSorting.py",
    "15-puzzle":       "15Puzzle.py",
    "labyrinth":       "Labyrinth.py",
    "plotting":        "Plotting.py",
    "puzznic":         "Puzznic.py",
    "sokoban":         "Sokoban.py",
}
# instances.txt has no name field here (one raw board string per line) — both this
# and RushHour.py's own get_problem() synthesize rh0..rhN from the same file in the
# same order, so the names always line up.
PAPER_RUSHHOUR_DOMAIN, PAPER_RUSHHOUR_FILE = "rush-hour", "RushHour.py"
# No instances.txt exists for these two — parametrized directly by size, not looked
# up by name. Not the paper's exact (unrecoverable) instance set, just a
# representative spread across the paper's stated range.
PAPER_SIZED_DOMAINS: Dict[str, Tuple[str, List[int]]] = {
    "dump-trucks":  ("DumpTrucks.py", [10, 12, 15, 18, 20]),                       # paper: 10-20 packages
    "storytellers": ("Storytellers.py", [5, 6, 8, 9, 11, 12, 14, 15, 17, 18, 20]),  # paper: 5-20 stories
}


def _up_paper_loader(domain_file: str, instance_arg) -> Callable[[], "Problem"]:
    """Load one UP-paper domain script by file path (not by dotted module name —
    several of these directories have hyphens) and call get_problem(instance_arg)."""
    def _load():
        spec = importlib.util.spec_from_file_location("_up_paper_domain", domain_file)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)  # type: ignore[union-attr]
        return module.get_problem(instance_arg)
    return _load


def discover_paper_domains() -> List[Instance]:
    out: List[Instance] = []
    if not os.path.isdir(UP_DOMAINS_ROOT):
        return out

    # Only the instances.txt-driven domains need these helpers. The
    # size-parametrized ones below are independent of them, so a missing
    # instance_loading module must not take the whole family down with it —
    # that turned a broken UP checkout into an empty selection, and an empty
    # selection into a SLURM array where every task died out-of-range.
    try:
        from docs.extensions.domains.instance_loading import read_instances_txt, read_instances_txt_raw
    except ImportError as e:
        sys.stderr.write(
            f"[paper] docs.extensions.domains.instance_loading is unavailable ({e}).\n"
            f"[paper] That module lives in the UP fork at {UP_DOMAINS_ROOT}/instance_loading.py\n"
            f"[paper] and is imported by its own domain models too (15Puzzle, Labyrinth,\n"
            f"[paper] PancakeSorting, Plotting, Puzznic, RushHour, Sokoban). Until it is\n"
            f"[paper] restored, only {', '.join(sorted(PAPER_SIZED_DOMAINS))} are available.\n")
        read_instances_txt = read_instances_txt_raw = None

    if read_instances_txt is not None:
        for dirname, filename in PAPER_NAMED_DOMAINS.items():
            domain_file = os.path.join(UP_DOMAINS_ROOT, dirname, filename)
            if not os.path.isfile(domain_file):
                continue
            for name, _ in read_instances_txt(domain_file):
                out.append(Instance(name=f"paper/{dirname}/{name}", family="paper",
                                    loader=_up_paper_loader(domain_file, name)))

        rh_file = os.path.join(UP_DOMAINS_ROOT, PAPER_RUSHHOUR_DOMAIN, PAPER_RUSHHOUR_FILE)
        if os.path.isfile(rh_file):
            for i in range(len(read_instances_txt_raw(rh_file))):
                name = f"rh{i}"
                out.append(Instance(name=f"paper/{PAPER_RUSHHOUR_DOMAIN}/{name}", family="paper",
                                    loader=_up_paper_loader(rh_file, name)))

    for dirname, (filename, sizes) in PAPER_SIZED_DOMAINS.items():
        domain_file = os.path.join(UP_DOMAINS_ROOT, dirname, filename)
        if not os.path.isfile(domain_file):
            continue
        for n in sizes:
            out.append(Instance(name=f"paper/{dirname}/n{n}", family="paper",
                                loader=_up_paper_loader(domain_file, n)))

    return out


# dirnames with a docs/extensions/domains/<dir>/handcrafted/domain.pddl + instance
# *.pddl files -- the ORIGINAL model each paper domain's Python get_problem() model
# is compared against (FD/Symk/ENHSP's 'hc' column in the results table). Dump
# Trucks and Story-tellers have no handcrafted/ dir -- the paper's own text says no
# original model exists for those two, so there's nothing to load.
PAPER_HC_DOMAINS: List[str] = [
    "15-puzzle", "pancake-sorting", "labyrinth", "plotting", "puzznic", "rush-hour", "sokoban",
]


def discover_paper_hc_domains() -> List[Instance]:
    """Handcrafted-PDDL counterpart to discover_paper_domains(): runs RTP against
    the literal PDDL model FD/Symk/ENHSP's 'hc' scores are based on, instead of the
    XTS-extended Python get_problem() model 'paper' family instances use. This is
    the one config never exercised on the 'hc' row of the UP-benchmark results
    table -- everywhere else RTP only ever sees its own (or a UP-compiled) model,
    never the original handcrafted one."""
    out: List[Instance] = []
    for dirname in PAPER_HC_DOMAINS:
        hc_dir = os.path.join(UP_DOMAINS_ROOT, dirname, "handcrafted")
        domain_file = os.path.join(hc_dir, "domain.pddl")
        if not os.path.isfile(domain_file):
            continue
        for fn in sorted(os.listdir(hc_dir)):
            if fn == "domain.pddl" or not fn.endswith(".pddl"):
                continue
            prob_file = os.path.join(hc_dir, fn)
            out.append(Instance(name=f"paper-hc/{dirname}/{fn[:-5]}", family="paper-hc",
                                loader=_pddl_loader(domain_file, prob_file)))
    return out


# ── Synthetic generators (array/set stress domains, parametrized by size) ──
# Ported from benchmark_frame_modes.py — generalize existing PDDL-XTS unit
# domains (pancake-sorting, labyrinth_v2, setops_on_reads, sets_nested) to
# arbitrary N so encoding/frame-mode/binary comparisons can be run at a size
# that actually stresses frame axioms instead of the tiny fixed instances.

def make_pancake(n: int, seed: int = 42) -> Problem:
    rng = random.Random(seed)
    perm = list(range(n))
    while perm == list(range(n)):
        rng.shuffle(perm)
    p = Problem(f"pancake_gen_{n}")
    idx_t = IntType(0, n - 1)
    stack = Fluent("pancake_stack", ArrayType(n, idx_t))
    p.add_fluent(stack)
    p.set_initial_value(stack, perm)
    flip = InstantaneousAction("flip", f=idx_t)
    f = flip.parameter("f")
    i = RangeVariable("i", 0, f)
    flip.add_effect(stack[i], stack[f - i], forall=[i])
    p.add_action(flip)
    p.add_goal(Equals(stack, list(range(n))))
    return p


def make_labyrinth(n: int, extra_edges: int = 0, seed: int = 42) -> Problem:
    rng = random.Random(seed)
    p = Problem(f"labyrinth_gen_{n}")
    Direction = UserType("Direction")
    N_dir, S_dir, E_dir, W_dir = (Object(x, Direction) for x in "NSEW")
    p.add_objects([N_dir, S_dir, E_dir, W_dir])
    idx_t = IntType(0, n - 1)
    card_at = Fluent("card_at", ArrayType(n, ArrayType(n, SetType(Direction))))
    robot_row = Fluent("robot_row", idx_t)
    robot_col = Fluent("robot_col", idx_t)
    p.add_fluent(card_at); p.add_fluent(robot_row); p.add_fluent(robot_col)
    p.set_initial_value(robot_row, 0); p.set_initial_value(robot_col, 0)

    grid = [[set() for _ in range(n)] for _ in range(n)]

    def carve(r1, c1, r2, c2, d1, d2):
        grid[r1][c1].add(d1); grid[r2][c2].add(d2)

    for c in range(n - 1):
        carve(0, c, 0, c + 1, E_dir, W_dir)
    for r in range(n - 1):
        carve(r, n - 1, r + 1, n - 1, S_dir, N_dir)

    added, attempts = 0, 0
    dir_deltas = {"N": (-1, 0, N_dir, S_dir), "S": (1, 0, S_dir, N_dir),
                  "E": (0, 1, E_dir, W_dir), "W": (0, -1, W_dir, E_dir)}
    while added < extra_edges and attempts < max(extra_edges * 20, 1):
        attempts += 1
        r, c = rng.randrange(n), rng.randrange(n)
        dr, dc, d1, d2 = dir_deltas[rng.choice("NSEW")]
        r2, c2 = r + dr, c + dc
        if 0 <= r2 < n and 0 <= c2 < n and d1 not in grid[r][c]:
            carve(r, c, r2, c2, d1, d2)
            added += 1
    p.set_initial_value(card_at, grid)

    def _move(name, r_type, c_type, dr, dc, from_dir, to_dir):
        act = InstantaneousAction(name, r=r_type, c=c_type)
        r, col = act.parameter("r"), act.parameter("c")
        act.add_precondition(Equals(robot_row, r))
        act.add_precondition(Equals(robot_col, col))
        act.add_precondition(SetMember(from_dir, card_at[r][col]))
        act.add_precondition(SetMember(to_dir, card_at[r + dr][col + dc]))
        if dr == -1: act.add_effect(robot_row, Minus(robot_row, 1))
        if dr == 1:  act.add_effect(robot_row, Plus(robot_row, 1))
        if dc == -1: act.add_effect(robot_col, Minus(robot_col, 1))
        if dc == 1:  act.add_effect(robot_col, Plus(robot_col, 1))
        p.add_action(act)

    _move("move_north", IntType(1, n - 1), idx_t, -1, 0, N_dir, S_dir)
    _move("move_south", IntType(0, n - 2), idx_t, 1, 0, S_dir, N_dir)
    _move("move_east", idx_t, IntType(0, n - 2), 0, 1, E_dir, W_dir)
    _move("move_west", idx_t, IntType(1, n - 1), 0, -1, W_dir, E_dir)

    def _rotate_col(name, row_order):
        act = InstantaneousAction(name, col=idx_t)
        col = act.parameter("col")
        act.add_precondition(Not(Equals(robot_col, col)))
        for dst, src in enumerate(row_order):
            act.add_effect(card_at[dst][col], card_at[src][col])
        p.add_action(act)

    def _rotate_row(name, col_order):
        act = InstantaneousAction(name, row=idx_t)
        row = act.parameter("row")
        act.add_precondition(Not(Equals(robot_row, row)))
        for dst, src in enumerate(col_order):
            act.add_effect(card_at[row][dst], card_at[row][src])
        p.add_action(act)

    _rotate_col("rotate_col_up", list(range(1, n)) + [0])
    _rotate_col("rotate_col_down", [n - 1] + list(range(0, n - 1)))
    _rotate_row("rotate_row_left", list(range(1, n)) + [0])
    _rotate_row("rotate_row_right", [n - 1] + list(range(0, n - 1)))

    p.add_goal(Equals(robot_row, n - 1))
    p.add_goal(Equals(robot_col, n - 1))
    return p


def make_setops(n: int, seed: int = 42) -> Problem:
    rng = random.Random(seed)
    p = Problem(f"setops_gen_{n}")
    slot_t, val_t, count_t = IntType(0, n - 1), IntType(0, n - 1), IntType(0, n)
    store_t = ArrayType(n, SetType(val_t))
    cells = Fluent("cells", store_t)
    big = Fluent("big", count_t)
    p.add_fluent(cells); p.add_fluent(big, default_initial_value=0)
    threshold = max(2, n // 2)
    init = [{v for v in range(n) if rng.random() < 0.5} for _ in range(n)]
    p.set_initial_value(cells, init); p.set_initial_value(big, 0)
    reachable_big = sum(1 for s in init if len(s) >= threshold)

    count_big = InstantaneousAction("count_big", s=slot_t)
    s = count_big.parameter("s")
    count_big.add_precondition(LT(big, n))
    count_big.add_precondition(GE(SetCardinality(cells[s]), threshold))
    count_big.add_effect(big, Plus(big, 1))
    p.add_action(count_big)

    trim_to = InstantaneousAction("trim_to", src=slot_t, dst=slot_t)
    src, dst = trim_to.parameter("src"), trim_to.parameter("dst")
    trim_to.add_precondition(Not(Equals(src, dst)))
    trim_to.add_precondition(Not(SetSubseteq(cells[dst], cells[src])))
    trim_to.add_effect(cells[dst], SetIntersection(cells[dst], cells[src]))
    p.add_action(trim_to)

    p.add_goal(Equals(big, reachable_big))
    return p


def make_set_merge(n: int) -> Problem:
    p = Problem(f"set_merge_gen_{n}")
    level_t = IntType(0, n - 1)
    buckets = [Fluent(f"bucket_{i}", SetType(level_t)) for i in range(n)]
    result = Fluent("result", SetType(level_t))
    for f in buckets + [result]:
        p.add_fluent(f, default_initial_value=set())
        p.set_initial_value(f, set())

    merge_all = InstantaneousAction("merge_all")
    merged = buckets[0]
    for b in buckets[1:]:
        merged = SetUnion(merged, b)
    merge_all.add_effect(result, merged)
    p.add_action(merge_all)

    for i, b in enumerate(buckets):
        act = InstantaneousAction(f"add_to_{i}", x=level_t)
        x = act.parameter("x")
        act.add_precondition(Not(SetMember(x, b)))
        act.add_effect(b, SetAdd(x, b))
        p.add_action(act)

    targets = sorted({0, n // 4, n // 2, (3 * n) // 4, n - 1} & set(range(n)))
    for v in targets:
        p.add_goal(SetMember(v, result))
    return p


def make_warehouse(n: int, seed: int = 42) -> Problem:
    """Combo generator: array (bins) + set (bin contents) + bounded-int (capacity) +
    count (goal) together in one problem, to see whether stacking features costs more
    than the sum of testing them in isolation (none of the other generators here mix
    more than one or two features at once)."""
    rng = random.Random(seed)
    p = Problem(f"warehouse_gen_{n}")
    item_t = IntType(0, n - 1)
    cap_t = IntType(0, n)
    bins = Fluent("bins", ArrayType(n, SetType(item_t)))
    capacity = Fluent("capacity", ArrayType(n, cap_t))
    p.add_fluent(bins, default_initial_value=set())
    p.add_fluent(capacity, default_initial_value=n)

    init_bins = [set() for _ in range(n)]
    for item in range(n):
        init_bins[rng.randrange(n)].add(item)
    init_caps = [n if rng.random() < 0.7 else max(1, n // 4) for _ in range(n)]
    p.set_initial_value(bins, init_bins)
    p.set_initial_value(capacity, init_caps)

    move = InstantaneousAction("move", item=item_t, src=IntType(0, n - 1), dst=IntType(0, n - 1))
    item = move.parameter("item")
    src = move.parameter("src")
    dst = move.parameter("dst")
    move.add_precondition(Not(Equals(src, dst)))
    move.add_precondition(SetMember(item, bins[src]))
    move.add_precondition(LT(SetCardinality(bins[dst]), capacity[dst]))
    move.add_effect(bins[src], SetRemove(item, bins[src]))
    move.add_effect(bins[dst], SetAdd(item, bins[dst]))
    p.add_action(move)

    threshold = max(1, n // 2)
    full_bins = max(1, n // 3)
    rb = [GE(SetCardinality(bins[i]), threshold) for i in range(n)]
    p.add_goal(GE(Count(rb), full_bins))
    return p


# ── Paired classic-vs-feature generators, ported from 0benchmark0/benchmark/
#    generators/ — same problem modeled once with plain booleans/object fluents,
#    once with array/set fluents, to isolate encoding cost (not just feature cost
#    in isolation) at matched size. ──

def make_delivery_bool(n: int) -> Problem:
    """N items in room-a, goal: all items in room-b. Classic boolean-predicate
    encoding: (at ?i ?r) per item/room pair, goal has N atoms.
    Compare against make_delivery_sets (set partition encoding, 1 cardinality goal)."""
    p = Problem(f'delivery_bool_n{n}')
    Item = UserType('item')
    Room = UserType('room')

    items = [Object(f'item{i}', Item) for i in range(n)]
    room_a = Object('rooma', Room)
    room_b = Object('roomb', Room)
    p.add_objects(items + [room_a, room_b])

    at_robot = Fluent('at_robot', BoolType(), r=Room)
    at = Fluent('at', BoolType(), i=Item, r=Room)
    carrying = Fluent('carrying', BoolType(), i=Item)
    hand_free = Fluent('hand_free', BoolType())
    p.add_fluent(at_robot, default_initial_value=False)
    p.add_fluent(at, default_initial_value=False)
    p.add_fluent(carrying, default_initial_value=False)
    p.add_fluent(hand_free, default_initial_value=False)

    p.set_initial_value(at_robot(room_a), True)
    p.set_initial_value(hand_free, True)
    for i in items:
        p.set_initial_value(at(i, room_a), True)

    move = InstantaneousAction('move', f=Room, t=Room)
    f, t = move.parameter('f'), move.parameter('t')
    move.add_precondition(at_robot(f))
    move.add_effect(at_robot(t), True)
    move.add_effect(at_robot(f), False)
    p.add_action(move)

    pick = InstantaneousAction('pick', i=Item, r=Room)
    i, r = pick.parameter('i'), pick.parameter('r')
    pick.add_precondition(at_robot(r))
    pick.add_precondition(at(i, r))
    pick.add_precondition(hand_free)
    pick.add_effect(carrying(i), True)
    pick.add_effect(at(i, r), False)
    pick.add_effect(hand_free, False)
    p.add_action(pick)

    drop = InstantaneousAction('drop', i=Item, r=Room)
    i, r = drop.parameter('i'), drop.parameter('r')
    drop.add_precondition(at_robot(r))
    drop.add_precondition(carrying(i))
    drop.add_effect(at(i, r), True)
    drop.add_effect(carrying(i), False)
    drop.add_effect(hand_free, True)
    p.add_action(drop)

    for i in items:
        p.add_goal(at(i, room_b))
    return p


def make_delivery_sets(n: int) -> Problem:
    """Same problem as make_delivery_bool, XTS set-partition encoding: (items-at ?r)
    set fluent per room, goal is a single cardinality equality regardless of N."""
    p = Problem(f'delivery_sets_n{n}')
    Item = UserType('item')
    Room = UserType('room')
    ItemSet = SetType(Item)

    items = [Object(f'item{i}', Item) for i in range(n)]
    room_a = Object('rooma', Room)
    room_b = Object('roomb', Room)
    p.add_objects(items + [room_a, room_b])

    robot_at = Fluent('robot_at', Room)
    items_at = Fluent('items_at', ItemSet, r=Room)
    carrying = Fluent('carrying', ItemSet)
    hand_free = Fluent('hand_free', BoolType())
    p.add_fluent(robot_at)
    p.add_fluent(items_at)
    p.add_fluent(carrying)
    p.add_fluent(hand_free, default_initial_value=False)

    p.set_initial_value(robot_at, room_a)
    p.set_initial_value(items_at(room_a), set(items))
    p.set_initial_value(items_at(room_b), set())
    p.set_initial_value(carrying, set())
    p.set_initial_value(hand_free, True)

    move = InstantaneousAction('move', f=Room, t=Room)
    f, t = move.parameter('f'), move.parameter('t')
    move.add_precondition(Equals(robot_at, f))
    move.add_effect(robot_at, t)
    p.add_action(move)

    pick = InstantaneousAction('pick', i=Item, r=Room)
    i, r = pick.parameter('i'), pick.parameter('r')
    pick.add_precondition(Equals(robot_at, r))
    pick.add_precondition(SetMember(i, items_at(r)))
    pick.add_precondition(hand_free)
    pick.add_effect(items_at(r), SetRemove(i, items_at(r)))
    pick.add_effect(carrying, SetAdd(i, FluentExp(carrying)))
    pick.add_effect(hand_free, False)
    p.add_action(pick)

    drop = InstantaneousAction('drop', i=Item, r=Room)
    i, r = drop.parameter('i'), drop.parameter('r')
    drop.add_precondition(Equals(robot_at, r))
    drop.add_precondition(SetMember(i, carrying))
    drop.add_effect(carrying, SetRemove(i, FluentExp(carrying)))
    drop.add_effect(items_at(r), SetAdd(i, items_at(r)))
    drop.add_effect(hand_free, True)
    p.add_action(drop)

    p.add_goal(Equals(SetCardinality(items_at(room_b)), n))
    return p


def make_expedition_classic(n: int) -> Problem:
    """Linear track of N waypoints; sled retrieves supplies and walks to the end.
    Classic encoding: object fluent (sled_at) + static is_next adjacency predicate.
    Compare against make_expedition_array (1D array + bounded-int position, no
    waypoint objects or is_next predicate)."""
    p = Problem(f'expedition_classic_n{n}')

    Sled = UserType('sled')
    Waypoint = UserType('waypoint')
    supply_t = IntType(0, n + 2)

    sled = Object('s0', Sled)
    waypoints = [Object(f'w{i}', Waypoint) for i in range(n)]
    p.add_objects([sled] + waypoints)

    sled_at = Fluent('sled_at', Waypoint, s=Sled)
    sled_supplies = Fluent('sled_supplies', supply_t, s=Sled)
    sled_capacity = Fluent('sled_capacity', supply_t, s=Sled)
    wp_supplies = Fluent('wp_supplies', supply_t, w=Waypoint)
    is_next = Fluent('is_next', BoolType(), x=Waypoint, y=Waypoint)

    p.add_fluent(sled_at)
    p.add_fluent(sled_supplies, default_initial_value=0)
    p.add_fluent(sled_capacity, default_initial_value=0)
    p.add_fluent(wp_supplies, default_initial_value=0)
    p.add_fluent(is_next, default_initial_value=False)

    p.set_initial_value(sled_at(sled), waypoints[0])
    p.set_initial_value(sled_supplies(sled), 1)
    p.set_initial_value(sled_capacity(sled), n)
    for w in waypoints:
        p.set_initial_value(wp_supplies(w), 0)
    p.set_initial_value(wp_supplies(waypoints[0]), n)
    for i in range(n - 1):
        p.set_initial_value(is_next(waypoints[i], waypoints[i + 1]), True)

    move_fwd = InstantaneousAction('move_forward', s=Sled, w1=Waypoint, w2=Waypoint)
    s, w1, w2 = [move_fwd.parameter(x) for x in ['s', 'w1', 'w2']]
    move_fwd.add_precondition(Equals(sled_at(s), w1))
    move_fwd.add_precondition(is_next(w1, w2))
    move_fwd.add_precondition(GE(sled_supplies(s), 1))
    move_fwd.add_effect(sled_at(s), w2)
    move_fwd.add_effect(sled_supplies(s), Minus(sled_supplies(s), 1))
    p.add_action(move_fwd)

    retrieve = InstantaneousAction('retrieve', s=Sled, w=Waypoint)
    s, w = retrieve.parameter('s'), retrieve.parameter('w')
    retrieve.add_precondition(Equals(sled_at(s), w))
    retrieve.add_precondition(GE(wp_supplies(w), 1))
    retrieve.add_precondition(GT(sled_capacity(s), sled_supplies(s)))
    retrieve.add_effect(wp_supplies(w), Minus(wp_supplies(w), 1))
    retrieve.add_effect(sled_supplies(s), Plus(sled_supplies(s), 1))
    p.add_action(retrieve)

    p.add_goal(Equals(sled_at(sled), waypoints[n - 1]))
    return p


def make_expedition_array(n: int) -> Problem:
    """Same problem as make_expedition_classic, 1D-array encoding: track supplies as
    ArrayType(n, ...) and sled position as a bounded-int fluent — no waypoint objects,
    no is_next predicate, adjacency is implicit via +/-1 arithmetic on the index."""
    p = Problem(f'expedition_array_n{n}')

    Sled = UserType('sled')
    pos_t = IntType(0, n - 1)
    supply_t = IntType(0, n + 2)
    track_t = ArrayType(n, supply_t)

    sled = Object('s0', Sled)
    p.add_objects([sled])

    track = Fluent('track_supplies', track_t)
    sled_pos = Fluent('sled_pos', pos_t, s=Sled)
    sled_sup = Fluent('sled_supplies', supply_t, s=Sled)
    sled_cap = Fluent('sled_capacity', supply_t, s=Sled)
    p.add_fluent(track)
    p.add_fluent(sled_pos)
    p.add_fluent(sled_sup, default_initial_value=0)
    p.add_fluent(sled_cap, default_initial_value=0)

    p.set_initial_value(track, [n] + [0] * (n - 1))
    p.set_initial_value(sled_pos(sled), 0)
    p.set_initial_value(sled_sup(sled), 1)
    p.set_initial_value(sled_cap(sled), n)

    move_fwd = InstantaneousAction('move_forward', s=Sled, p=pos_t)
    s, ppar = move_fwd.parameter('s'), move_fwd.parameter('p')
    move_fwd.add_precondition(Equals(sled_pos(s), ppar))
    move_fwd.add_precondition(LT(ppar, n - 1))
    move_fwd.add_precondition(GE(sled_sup(s), 1))
    move_fwd.add_effect(sled_pos(s), Plus(ppar, 1))
    move_fwd.add_effect(sled_sup(s), Minus(sled_sup(s), 1))
    p.add_action(move_fwd)

    retrieve = InstantaneousAction('retrieve', s=Sled, pidx=pos_t, v=supply_t)
    s, pidx, v = [retrieve.parameter(x) for x in ['s', 'pidx', 'v']]
    retrieve.add_precondition(Equals(sled_pos(s), pidx))
    retrieve.add_precondition(Equals(track[pidx], v))
    retrieve.add_precondition(GE(v, 1))
    retrieve.add_precondition(GT(sled_cap(s), sled_sup(s)))
    retrieve.add_effect(track[pidx], Minus(v, 1))
    retrieve.add_effect(sled_sup(s), Plus(sled_sup(s), 1))
    p.add_action(retrieve)

    p.add_goal(Equals(sled_pos(sled), n - 1))
    return p


def make_gripper_bool(n: int) -> Problem:
    """N balls in room-a, goal: all in room-b, 2 rooms/2 grippers. Classic boolean
    encoding: (at ?b ?r) / (carry ?b ?g) predicates, goal has N atoms.
    Compare against make_gripper_sets (set partition, 1 cardinality goal)."""
    p = Problem(f'gripper_bool_n{n}')
    Ball = UserType('ball')
    Room = UserType('room')
    Gripper = UserType('gripper')

    balls = [Object(f'ball{i}', Ball) for i in range(n)]
    room_a = Object('rooma', Room)
    room_b = Object('roomb', Room)
    grippers = [Object('left', Gripper), Object('right', Gripper)]
    p.add_objects(balls + [room_a, room_b] + grippers)

    at_robby = Fluent('at_robby', BoolType(), r=Room)
    at = Fluent('at', BoolType(), b=Ball, r=Room)
    carry = Fluent('carry', BoolType(), b=Ball, g=Gripper)
    free = Fluent('free', BoolType(), g=Gripper)
    p.add_fluent(at_robby, default_initial_value=False)
    p.add_fluent(at, default_initial_value=False)
    p.add_fluent(carry, default_initial_value=False)
    p.add_fluent(free, default_initial_value=False)

    p.set_initial_value(at_robby(room_a), True)
    for b in balls:
        p.set_initial_value(at(b, room_a), True)
    for g in grippers:
        p.set_initial_value(free(g), True)

    move = InstantaneousAction('move', f=Room, t=Room)
    f, t = move.parameter('f'), move.parameter('t')
    move.add_precondition(at_robby(f))
    move.add_effect(at_robby(t), True)
    move.add_effect(at_robby(f), False)
    p.add_action(move)

    pick = InstantaneousAction('pick', b=Ball, r=Room, g=Gripper)
    b, r, g = [pick.parameter(x) for x in ['b', 'r', 'g']]
    pick.add_precondition(at_robby(r))
    pick.add_precondition(at(b, r))
    pick.add_precondition(free(g))
    pick.add_effect(carry(b, g), True)
    pick.add_effect(at(b, r), False)
    pick.add_effect(free(g), False)
    p.add_action(pick)

    drop = InstantaneousAction('drop', b=Ball, r=Room, g=Gripper)
    b, r, g = [drop.parameter(x) for x in ['b', 'r', 'g']]
    drop.add_precondition(at_robby(r))
    drop.add_precondition(carry(b, g))
    drop.add_effect(at(b, r), True)
    drop.add_effect(carry(b, g), False)
    drop.add_effect(free(g), True)
    p.add_action(drop)

    for b in balls:
        p.add_goal(at(b, room_b))
    return p


def make_gripper_sets(n: int) -> Problem:
    """Same problem as make_gripper_bool, XTS set-partition encoding: (balls-at ?r)
    / (carried ?g) set fluents, goal is a single cardinality equality."""
    p = Problem(f'gripper_sets_n{n}')
    Ball = UserType('ball')
    Room = UserType('room')
    Gripper = UserType('gripper')
    BallSet = SetType(Ball)

    balls = [Object(f'ball{i}', Ball) for i in range(n)]
    room_a = Object('rooma', Room)
    room_b = Object('roomb', Room)
    grippers = [Object('left', Gripper), Object('right', Gripper)]
    p.add_objects(balls + [room_a, room_b] + grippers)

    robby_at = Fluent('robby_at', Room)
    balls_at = Fluent('balls_at', BallSet, r=Room)
    carried = Fluent('carried', BallSet, g=Gripper)
    free = Fluent('free', BoolType(), g=Gripper)
    p.add_fluent(robby_at)
    p.add_fluent(balls_at)
    p.add_fluent(carried)
    p.add_fluent(free, default_initial_value=False)

    p.set_initial_value(robby_at, room_a)
    p.set_initial_value(balls_at(room_a), set(balls))
    p.set_initial_value(balls_at(room_b), set())
    for g in grippers:
        p.set_initial_value(carried(g), set())
        p.set_initial_value(free(g), True)

    move = InstantaneousAction('move', f=Room, t=Room)
    f, t = move.parameter('f'), move.parameter('t')
    move.add_precondition(Equals(robby_at, f))
    move.add_effect(robby_at, t)
    p.add_action(move)

    pick = InstantaneousAction('pick', b=Ball, r=Room, g=Gripper)
    b, r, g = [pick.parameter(x) for x in ['b', 'r', 'g']]
    pick.add_precondition(Equals(robby_at, r))
    pick.add_precondition(SetMember(b, balls_at(r)))
    pick.add_precondition(free(g))
    pick.add_effect(balls_at(r), SetRemove(b, balls_at(r)))
    pick.add_effect(carried(g), SetAdd(b, carried(g)))
    pick.add_effect(free(g), False)
    p.add_action(pick)

    drop = InstantaneousAction('drop', b=Ball, r=Room, g=Gripper)
    b, r, g = [drop.parameter(x) for x in ['b', 'r', 'g']]
    drop.add_precondition(Equals(robby_at, r))
    drop.add_precondition(SetMember(b, carried(g)))
    drop.add_effect(carried(g), SetRemove(b, carried(g)))
    drop.add_effect(balls_at(r), SetAdd(b, balls_at(r)))
    drop.add_effect(free(g), True)
    p.add_action(drop)

    p.add_goal(Equals(SetCardinality(balls_at(room_b)), n))
    return p


def _visit_all_neighbor_coords(n, x, y):
    coords = []
    for dx, dy in [(0, 1), (0, -1), (1, 0), (-1, 0)]:
        nx, ny = x + dx, y + dy
        if 0 <= nx < n and 0 <= ny < n:
            coords.append((nx, ny))
    return coords


def _visit_all_neighbors(locs, n, x, y):
    return {locs[nx][ny] for nx, ny in _visit_all_neighbor_coords(n, x, y)}


def make_visit_all_bool(n: int) -> Problem:
    """N x N grid (4-connected, no wrap), robot starts at (0,0), must visit every
    cell. TRUE zero-feature classic-PDDL baseline: position (at_robot) and adjacency
    (connected) are both boolean predicates, not an object fluent + set the way the
    previous version of this generator had them — matches the zero-feature bar
    delivery_bool_gen/gripper_bool_gen already set. Goal has N*N boolean atoms.
    Compare against make_visit_all_sets (object-fluent position, set adjacency, set
    covering goal)."""
    p = Problem(f'visit_all_bool_{n}x{n}')
    Place = UserType('place')

    locs = [[Object(f'x{x}y{y}', Place) for y in range(n)] for x in range(n)]
    flat = [locs[x][y] for x in range(n) for y in range(n)]
    p.add_objects(flat)

    at_robot = Fluent('at_robot', BoolType(), p=Place)
    connected = Fluent('connected', BoolType(), p1=Place, p2=Place)
    visited = Fluent('visited', BoolType(), wp=Place)
    p.add_fluent(at_robot, default_initial_value=False)
    p.add_fluent(connected, default_initial_value=False)
    p.add_fluent(visited, default_initial_value=False)

    start = locs[0][0]
    p.set_initial_value(at_robot(start), True)
    p.set_initial_value(visited(start), True)

    for x in range(n):
        for y in range(n):
            for nx, ny in _visit_all_neighbor_coords(n, x, y):
                p.set_initial_value(connected(locs[x][y], locs[nx][ny]), True)

    move = InstantaneousAction('move', curpos=Place, nextpos=Place)
    cur, nxt = move.parameter('curpos'), move.parameter('nextpos')
    move.add_precondition(at_robot(cur))
    move.add_precondition(connected(cur, nxt))
    move.add_effect(at_robot(nxt), True)
    move.add_effect(at_robot(cur), False)
    move.add_effect(visited(nxt), True)
    p.add_action(move)

    for wp in flat:
        p.add_goal(visited(wp))
    return p


def make_visit_all_sets(n: int) -> Problem:
    """Same problem as make_visit_all_bool, set covering-goal encoding: (visited)
    accumulates visited places, goal is a single cardinality equality."""
    p = Problem(f'visit_all_sets_{n}x{n}')
    Place = UserType('place')
    PlaceSet = SetType(Place)

    locs = [[Object(f'x{x}y{y}', Place) for y in range(n)] for x in range(n)]
    flat = [locs[x][y] for x in range(n) for y in range(n)]
    p.add_objects(flat)

    robot_at = Fluent('robot_at', Place)
    connects = Fluent('connects', PlaceSet, x=Place)
    visited = Fluent('visited', PlaceSet)
    p.add_fluent(robot_at)
    p.add_fluent(connects)
    p.add_fluent(visited)

    start = locs[0][0]
    p.set_initial_value(robot_at, start)
    p.set_initial_value(visited, {start})

    for x in range(n):
        for y in range(n):
            p.set_initial_value(connects(locs[x][y]), _visit_all_neighbors(locs, n, x, y))

    move = InstantaneousAction('move', curpos=Place, nextpos=Place)
    cur, nxt = move.parameter('curpos'), move.parameter('nextpos')
    move.add_precondition(Equals(robot_at, cur))
    move.add_precondition(SetMember(nxt, connects(cur)))
    move.add_effect(robot_at, nxt)
    move.add_effect(visited, SetAdd(nxt, FluentExp(visited)))
    p.add_action(move)

    p.add_goal(Equals(SetCardinality(FluentExp(visited)), n * n))
    return p


def _make_fo_counters(n: int, max_val: int, max_rate: int, name_suffix: str) -> Problem:
    p = Problem(f'fo_counters{name_suffix}_n{n}')
    Counter = UserType('Counter')
    counters = [Object(f'c{i}', Counter) for i in range(n)]
    p.add_objects(counters)

    val_t = IntType(0, max_val)
    rate_t = IntType(0, max_rate)
    value = Fluent('value', val_t, c=Counter)
    rate_value = Fluent('rate_value', rate_t, c=Counter)
    p.add_fluent(value, default_initial_value=0)
    p.add_fluent(rate_value, default_initial_value=0)

    for c in counters:
        p.set_initial_value(value(c), 0)
        p.set_initial_value(rate_value(c), 1)

    increment = InstantaneousAction('increment', c=Counter)
    c = increment.parameter('c')
    increment.add_precondition(LE(Plus(value(c), rate_value(c)), max_val))
    increment.add_effect(value(c), Plus(value(c), rate_value(c)))
    p.add_action(increment)

    decrement = InstantaneousAction('decrement', c=Counter)
    c = decrement.parameter('c')
    decrement.add_precondition(GE(Minus(value(c), rate_value(c)), 0))
    decrement.add_effect(value(c), Minus(value(c), rate_value(c)))
    p.add_action(decrement)

    increase_rate = InstantaneousAction('increase_rate', c=Counter)
    c = increase_rate.parameter('c')
    increase_rate.add_precondition(LT(rate_value(c), max_rate))
    increase_rate.add_effect(rate_value(c), Plus(rate_value(c), 1))
    p.add_action(increase_rate)

    decrement_rate = InstantaneousAction('decrement_rate', c=Counter)
    c = decrement_rate.parameter('c')
    decrement_rate.add_precondition(GT(rate_value(c), 0))
    decrement_rate.add_effect(rate_value(c), Minus(rate_value(c), 1))
    p.add_action(decrement_rate)

    for i in range(n - 1):
        p.add_goal(LE(Plus(value(counters[i]), 1), value(counters[i + 1])))
    return p


def make_fo_counters(n: int, max_val: int = 36, max_rate: int = 10) -> Problem:
    """N counters with FIXED value/rate ranges regardless of N — goal chains
    c[0]+1<=c[1]<=...<=c[n-1]. Fixed ranges mean UP-pipeline compilers that enumerate
    the domain (e.g. INTEGERS_REMOVING) blow up fast as N grows even though the
    ranges don't; compare against make_fo_counters_small (ranges scale with N)."""
    return _make_fo_counters(n, max_val, max_rate, name_suffix="")


def make_fo_counters_small(n: int) -> Problem:
    """Same problem as make_fo_counters, but value/rate ranges scale with N
    (val_t=[0,n+1], rate_t=[0,2]) instead of staying fixed — keeps compilers that
    enumerate the domain tractable much further out."""
    return _make_fo_counters(n, max_val=n + 1, max_rate=2, name_suffix="_small")


# ── Single-feature isolation pairs — each pair isolates exactly ONE XTS feature
#    against a classical-boolean baseline of the SAME problem, rounding out the
#    feature table so every row (array, bounded-int, object-fluent, range-var, count)
#    has a matched classical/XTS comparison point, not just set (already covered by
#    delivery/gripper/visit_all above). ──

def make_lights_bool(n: int) -> Problem:
    """N independent lights, goal: all on. Classical baseline: one boolean predicate
    per light object. Compare against make_lights_array (ArrayType(n, Bool), single-
    cell writes) — isolates ARRAY alone: no bounded-int-valued cells, no set, no
    range-var bulk effect, no count, no object fluent."""
    p = Problem(f'lights_bool_n{n}')
    Light = UserType('light')
    lights = [Object(f'l{i}', Light) for i in range(n)]
    p.add_objects(lights)

    on = Fluent('on', BoolType(), l=Light)
    p.add_fluent(on, default_initial_value=False)

    toggle = InstantaneousAction('toggle', l=Light)
    l = toggle.parameter('l')
    toggle.add_precondition(Not(on(l)))
    toggle.add_effect(on(l), True)
    p.add_action(toggle)

    for l in lights:
        p.add_goal(on(l))
    return p


def make_lights_array(n: int) -> Problem:
    """Same problem as make_lights_bool, ARRAY encoding: a single ArrayType(n, Bool)
    fluent, one cell per light, written by index instead of by object identity."""
    p = Problem(f'lights_array_n{n}')
    idx_t = IntType(0, n - 1)
    lights = Fluent('lights', ArrayType(n, BoolType()))
    p.add_fluent(lights, default_initial_value=False)
    p.set_initial_value(lights, [False] * n)

    toggle = InstantaneousAction('toggle', i=idx_t)
    i = toggle.parameter('i')
    toggle.add_precondition(Not(lights[i]))
    toggle.add_effect(lights[i], True)
    p.add_action(toggle)

    for idx in range(n):
        p.add_goal(lights[idx])
    return p


def make_counter_bool(k: int) -> Problem:
    """Reach level k-1 from level 0. Classical thermometer/unary encoding: k Level
    objects and a boolean 'reached' predicate — advancing needs the current max level
    reached as a precondition, so the plan is forced to walk every level in order.
    Compare against make_counter_int (single bounded-int fluent, +1 each step) —
    isolates BOUNDED-INT alone: no array, no set, no range-var, no count, and no
    object-valued fluent (levels are only used as predicate parameters here, not as a
    fluent's value type)."""
    p = Problem(f'counter_bool_k{k}')
    Level = UserType('level')
    levels = [Object(f'lvl{i}', Level) for i in range(k)]
    p.add_objects(levels)

    reached = Fluent('reached', BoolType(), l=Level)
    p.add_fluent(reached, default_initial_value=False)
    p.set_initial_value(reached(levels[0]), True)

    for i in range(k - 1):
        adv = InstantaneousAction(f'advance_{i}')
        adv.add_precondition(reached(levels[i]))
        adv.add_effect(reached(levels[i + 1]), True)
        p.add_action(adv)

    p.add_goal(reached(levels[k - 1]))
    return p


def make_counter_int(k: int) -> Problem:
    """Same problem as make_counter_bool, BOUNDED-INT encoding: a single
    IntType(0, k-1) fluent incremented by one action instead of k-1 distinct
    per-level advance actions."""
    p = Problem(f'counter_int_k{k}')
    counter = Fluent('counter', IntType(0, k - 1))
    p.add_fluent(counter, default_initial_value=0)
    p.set_initial_value(counter, 0)

    increment = InstantaneousAction('increment')
    increment.add_precondition(LT(counter, k - 1))
    increment.add_effect(counter, Plus(counter, 1))
    p.add_action(increment)

    p.add_goal(Equals(counter, k - 1))
    return p


def make_walk_bool(n: int) -> Problem:
    """Robot walks a line of N locations, start to end. Classical boolean position:
    (at ?l) predicate per location, plus a static 'next' adjacency predicate (shared
    with make_walk_obj so ONLY the position representation differs between the two).
    Compare against make_walk_obj (single object-valued position fluent) — isolates
    OBJECT-FLUENT alone: no array, no set, no bounded-int, no range-var, no count."""
    p = Problem(f'walk_bool_n{n}')
    Loc = UserType('loc')
    locs = [Object(f'loc{i}', Loc) for i in range(n)]
    p.add_objects(locs)

    at = Fluent('at', BoolType(), l=Loc)
    nxt = Fluent('next', BoolType(), a=Loc, b=Loc)
    p.add_fluent(at, default_initial_value=False)
    p.add_fluent(nxt, default_initial_value=False)
    p.set_initial_value(at(locs[0]), True)
    for i in range(n - 1):
        p.set_initial_value(nxt(locs[i], locs[i + 1]), True)

    move = InstantaneousAction('move', cur=Loc, dst=Loc)
    cur, dst = move.parameter('cur'), move.parameter('dst')
    move.add_precondition(at(cur))
    move.add_precondition(nxt(cur, dst))
    move.add_effect(at(dst), True)
    move.add_effect(at(cur), False)
    p.add_action(move)

    p.add_goal(at(locs[n - 1]))
    return p


def make_walk_obj(n: int) -> Problem:
    """Same problem as make_walk_bool, OBJECT-FLUENT encoding: a single Loc-valued
    'robot_at' fluent instead of a boolean predicate per location. The static 'next'
    adjacency predicate is left as-is (boolean, same in both) so this isolates
    exactly the position-representation difference."""
    p = Problem(f'walk_obj_n{n}')
    Loc = UserType('loc')
    locs = [Object(f'loc{i}', Loc) for i in range(n)]
    p.add_objects(locs)

    robot_at = Fluent('robot_at', Loc)
    nxt = Fluent('next', BoolType(), a=Loc, b=Loc)
    p.add_fluent(robot_at)
    p.add_fluent(nxt, default_initial_value=False)
    p.set_initial_value(robot_at, locs[0])
    for i in range(n - 1):
        p.set_initial_value(nxt(locs[i], locs[i + 1]), True)

    move = InstantaneousAction('move', cur=Loc, dst=Loc)
    cur, dst = move.parameter('cur'), move.parameter('dst')
    move.add_precondition(Equals(robot_at, cur))
    move.add_precondition(nxt(cur, dst))
    move.add_effect(robot_at, dst)
    p.add_action(move)

    p.add_goal(Equals(robot_at, locs[n - 1]))
    return p


def make_pancake_bool(n: int, seed: int = 42) -> Problem:
    """Classical baseline for make_pancake: same 'sort the stack by prefix-flip'
    problem, but the stack is N Pancake objects placed via a boolean 'at_position'
    predicate (N*N atoms) instead of an array, and the flip action needs one
    conditional effect per position in the flipped prefix instead of a single
    RangeVariable forall-effect. Isolates ARRAY+RANGE-VAR together (the two are
    naturally paired — a range-var forall-effect is what makes a bulk array update
    a single action instead of one action per position)."""
    rng = random.Random(seed)
    perm = list(range(n))
    while perm == list(range(n)):
        rng.shuffle(perm)
    p = Problem(f"pancake_bool_n{n}")
    Pancake = UserType('pancake')
    pancakes = [Object(f'p{i}', Pancake) for i in range(n)]
    p.add_objects(pancakes)

    pos_t = IntType(0, n - 1)
    at_position = Fluent('at_position', BoolType(), pc=Pancake, pos=pos_t)
    p.add_fluent(at_position, default_initial_value=False)
    for pos, pc_id in enumerate(perm):
        p.set_initial_value(at_position(pancakes[pc_id], pos), True)

    # Flipping the top f+1 pancakes swaps whoever occupies position pos with
    # whoever occupies position (f-pos), for every pos < f-pos. Since which pancake
    # is at a position is state (not known statically), each pancake gets its own
    # pair of conditional effects per position-pair, independent of every other
    # pancake — no need to reason about pairs of pancakes at all.
    for f in range(1, n):
        flip = InstantaneousAction(f'flip_{f}')
        for pos in range((f + 1) // 2):
            other = f - pos
            for pc in pancakes:
                flip.add_effect(at_position(pc, other), True, condition=at_position(pc, pos))
                flip.add_effect(at_position(pc, pos), False, condition=at_position(pc, pos))
                flip.add_effect(at_position(pc, pos), True, condition=at_position(pc, other))
                flip.add_effect(at_position(pc, other), False, condition=at_position(pc, other))
        p.add_action(flip)

    for pos in range(n):
        p.add_goal(at_position(pancakes[pos], pos))
    return p


def make_count_tally_bool(n: int) -> Problem:
    """N independent tasks, goal: at least K done (K = n//2, any K of N — NOT a
    fixed subset). Classical mechanism for 'K of N' without Count(): an explicit
    bounded-int tally fluent, incremented as an extra effect on every completion.
    Compare against make_count_agg_xts (identical problem, Count() aggregate goal
    instead of a maintained tally) — isolates COUNT alone: the tally fluent is
    bounded-int, but that's the classical MECHANISM being compared against, not an
    XTS feature under test on this side of the pair."""
    p = Problem(f'count_tally_bool_n{n}')
    Task = UserType('task')
    tasks = [Object(f't{i}', Task) for i in range(n)]
    p.add_objects(tasks)
    k = max(1, n // 2)

    done = Fluent('done', BoolType(), t=Task)
    tally = Fluent('tally', IntType(0, n))
    p.add_fluent(done, default_initial_value=False)
    p.add_fluent(tally, default_initial_value=0)
    p.set_initial_value(tally, 0)

    complete = InstantaneousAction('complete', t=Task)
    t = complete.parameter('t')
    complete.add_precondition(Not(done(t)))
    complete.add_effect(done(t), True)
    complete.add_effect(tally, Plus(tally, 1))
    p.add_action(complete)

    p.add_goal(GE(tally, k))
    return p


def make_count_agg_xts(n: int) -> Problem:
    """Same problem as make_count_tally_bool, COUNT encoding: no tally fluent and no
    extra effect on completion — the 'K of N' goal is a single Count() aggregate over
    the done() atoms, evaluated once rather than maintained incrementally."""
    p = Problem(f'count_agg_xts_n{n}')
    Task = UserType('task')
    tasks = [Object(f't{i}', Task) for i in range(n)]
    p.add_objects(tasks)
    k = max(1, n // 2)

    done = Fluent('done', BoolType(), t=Task)
    p.add_fluent(done, default_initial_value=False)

    complete = InstantaneousAction('complete', t=Task)
    t = complete.parameter('t')
    complete.add_precondition(Not(done(t)))
    complete.add_effect(done(t), True)
    p.add_action(complete)

    p.add_goal(GE(Count([done(t) for t in tasks]), k))
    return p


# name -> (generator, sizes). Sizes are starting points chosen from each generator's
# structure (linear-goal domains like delivery/gripper/visit_all scale further before
# breaking down than combinatorial-search ones like pancake/labyrinth/warehouse) —
# expect to trim or extend per-generator once real timing data comes back.
SYNTHETIC_GENERATORS: List[Tuple[str, Callable[[int], Problem], List[int]]] = [
    ("pancake_gen",            make_pancake,                       [6, 8, 10, 12, 14, 16, 18, 20, 24, 28]),
    ("pancake_bool_gen",       make_pancake_bool,                  [6, 8, 10, 12, 14, 16, 18, 20, 24, 28]),
    ("lights_bool_gen",        make_lights_bool,                   [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("lights_array_gen",       make_lights_array,                  [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("counter_bool_gen",       make_counter_bool,                  [4, 6, 8, 10, 15, 20, 25, 32, 40, 50]),
    ("counter_int_gen",        make_counter_int,                   [4, 6, 8, 10, 15, 20, 25, 32, 40, 50]),
    ("walk_bool_gen",          make_walk_bool,                     [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("walk_obj_gen",           make_walk_obj,                      [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("count_tally_bool_gen",   make_count_tally_bool,              [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("count_agg_xts_gen",      make_count_agg_xts,                 [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("labyrinth_gen",          lambda n: make_labyrinth(n, extra_edges=max(0, n - 3)),
                                                                    [3, 4, 5, 6, 7, 8, 10, 12]),
    ("setops_gen",             make_setops,                        [6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("set_merge_gen",          make_set_merge,                     [8, 12, 16, 20, 24, 32, 40, 48, 64]),
    ("warehouse_gen",          make_warehouse,                     [6, 8, 10, 12, 16, 20, 24, 28]),
    ("delivery_bool_gen",      make_delivery_bool,                 [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("delivery_sets_gen",      make_delivery_sets,                 [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("expedition_classic_gen", make_expedition_classic,            [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("expedition_array_gen",   make_expedition_array,              [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("gripper_bool_gen",       make_gripper_bool,                  [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("gripper_sets_gen",       make_gripper_sets,                  [4, 6, 8, 10, 12, 16, 20, 24, 28, 32]),
    ("visit_all_bool_gen",     make_visit_all_bool,                [2, 3, 4, 5, 6, 7, 8, 10]),
    ("visit_all_sets_gen",     make_visit_all_sets,                [2, 3, 4, 5, 6, 7, 8, 10]),
    ("fo_counters_gen",        make_fo_counters,                   [2, 3, 4, 5, 6, 8, 10, 15, 20, 25]),
    ("fo_counters_small_gen",  make_fo_counters_small,             [2, 3, 4, 5, 6, 8, 10, 12, 16, 20]),
]


def discover_synthetic() -> List[Instance]:
    out = []
    for prefix, gen, sizes in SYNTHETIC_GENERATORS:
        for n in sizes:
            out.append(Instance(name=f"synthetic/{prefix}_{n}", family="synthetic",
                                loader=(lambda gen=gen, n=n: gen(n))))
    return out


FAMILY_DISCOVERERS = {
    "xts-unit":        lambda args: discover_xts_units(),
    "xts-translation": lambda args: discover_xts_translations(),
    "xts-native":      lambda args: discover_xts_native(),
    "classic":         lambda args: discover_classic(),
    "synthetic":       lambda args: discover_synthetic(),
    "scaling":         lambda args: discover_scaling(),
    "paper":           lambda args: discover_paper_domains(),
    "paper-hc":        lambda args: discover_paper_hc_domains(),
}


def discover_all(families: List[str], args) -> List[Instance]:
    fams = list(FAMILY_DISCOVERERS) if "all" in families else families
    out = []
    for fam in fams:
        out += FAMILY_DISCOVERERS[fam](args)
    return out


def _matches_filter(name, pattern):
    if any(c in pattern for c in ("*", "?", "[")):
        return fnmatch.fnmatch(name, pattern)
    return pattern.lower() in name.lower()


# ═════════════════════════════════════════════════════════════════════════
# 2. Compile configs (compile-time axis) & runtime configs (invocation axis)
# ═════════════════════════════════════════════════════════════════════════

@dataclass
class CompileConfig:
    label: str
    pipeline: Optional[str]       # None/'native', or a COMPILATION_PIPELINES name
    up_compilers: str = "IPAR,QR,GR"


def parse_compile_configs(args) -> List[CompileConfig]:
    values = [v.strip() for v in args.pipeline.split(",") if v.strip()]
    out = []
    for v in values:
        if v == "native":
            out.append(CompileConfig(label="native", pipeline=None, up_compilers=args.up_compilers))
        else:
            out.append(CompileConfig(label=f"up:{v}", pipeline=v))
    return out


@dataclass
class RuntimeConfig:
    label: str
    binary: str
    strategy: str = "seq"
    array_encoding: Optional[str] = None
    array_frame_mode: Optional[str] = None


_BINARY_BUILD_CACHE: Dict[str, str] = {}


def build_binary_from_ref(ref: str, jobs: int) -> str:
    """Build the C++ binary from a git ref in a throwaway worktree; cache per ref."""
    if ref in _BINARY_BUILD_CACHE:
        return _BINARY_BUILD_CACHE[ref]
    out_path = os.path.join(tempfile.gettempdir(), f"rantanplan_ref_{ref.replace('/', '_')}")
    wt = tempfile.mkdtemp(prefix="rtp_bench_"); os.rmdir(wt)
    try:
        print(f"[build] worktree at {wt}  (ref={ref}) — building once…")
        subprocess.run(["git", "-C", ROOT, "worktree", "add", "--detach", wt, ref],
                       check=True, stdout=subprocess.DEVNULL)
        src, build = os.path.join(wt, "rantanplan", "cpp"), os.path.join(wt, "rantanplan", "cpp", "build")
        extra = _cmake_cache_vars(
            os.path.join(ROOT, "rantanplan", "cpp", "build", "CMakeCache.txt"),
            ["Z3_INCLUDE_DIR", "Z3_LIB_DIR", "Z3_LIBRARY", "Protobuf_INCLUDE_DIR"])
        subprocess.run(["cmake", "-S", src, "-B", build, "-DCMAKE_BUILD_TYPE=Release", *extra],
                       check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["cmake", "--build", build, f"-j{jobs}"], check=True, stdout=subprocess.DEVNULL)
        built = os.path.join(build, "rantanplan")
        if not os.path.isfile(built):
            sys.exit(f"[build] build did not produce {built}")
        shutil.copy2(built, out_path)
        print(f"[build] {ref} → {out_path}")
    finally:
        subprocess.run(["git", "-C", ROOT, "worktree", "remove", "--force", wt], check=False)
        shutil.rmtree(wt, ignore_errors=True)
    _BINARY_BUILD_CACHE[ref] = out_path
    return out_path


def _cmake_cache_vars(cache_path, names):
    if not os.path.isfile(cache_path):
        return []
    wanted = {n: None for n in names}
    with open(cache_path) as f:
        for line in f:
            line = line.strip()
            if ":" not in line or "=" not in line or line.startswith(("//", "#")):
                continue
            key = line.split(":", 1)[0]
            if key in wanted:
                wanted[key] = line.split("=", 1)[1]
    return [f"-D{n}={v}" for n, v in wanted.items() if v and "NOTFOUND" not in v]


def parse_runtime_configs(args) -> List[RuntimeConfig]:
    binaries: List[Tuple[str, str]] = []          # (label, path)
    if args.binary:
        binaries += [(os.path.basename(b), os.path.abspath(b)) for b in args.binary]
    if args.binary_ref:
        binaries += [(f"ref:{r}", build_binary_from_ref(r, args.jobs)) for r in args.binary_ref]
    if not binaries:
        binaries = [("current", DEFAULT_BIN)]
    # Disambiguate binaries that share a basename (e.g. two different
    # rantanplan/.../rantanplan builds) by qualifying with the parent dir.
    label_counts = Counter(lbl for lbl, _ in binaries)
    if any(c > 1 for c in label_counts.values()):
        binaries = [(os.path.join(os.path.basename(os.path.dirname(path)), lbl), path)
                    if label_counts[lbl] > 1 else (lbl, path)
                    for lbl, path in binaries]

    strategies = [s.strip() for s in args.strategy.split(",") if s.strip()]
    encodings  = [e.strip() for e in args.array_encoding.split(",") if e.strip()]
    frame_modes = [f.strip() for f in args.array_frame_mode.split(",") if f.strip()]

    out = []
    for bin_label, bin_path in binaries:
        for strat in strategies:
            for enc in encodings:
                # [XTS] Frame mode is meaningful under BOTH encodings: uf implements
                # the same two shapes pointwise, per enumerated cell (see
                # GroundedEncoder::encode_frames), so sweep the full matrix.
                for fm in frame_modes:
                    label = f"{bin_label}|{strat}|{enc}|{fm}"
                    out.append(RuntimeConfig(label, bin_path, strat, enc, fm))
    return out


# ═════════════════════════════════════════════════════════════════════════
# 3. Compilation (isolated subprocess) — one .pb per (instance, compile-config)
# ═════════════════════════════════════════════════════════════════════════

def compile_worker(name: str, pipeline: str, up_compilers: str, out_pb: str) -> int:
    try:
        registry = {i.name: i for i in discover_all(["all"], _default_args())}
        inst = registry[name]
        problem = inst.loader()
        if pipeline and pipeline not in ("", "native"):
            problem, _ = _solve.apply_pipeline(problem, pipeline)
            up_compilers = "none"
        planner = RantanPlanPlanner(up_compilers=up_compilers, strategy="seq",
                                    verbosity="silent", executable_path=DEFAULT_BIN)
        _, _, prob_pb, sol_pb = planner._prepare_and_serialize(problem)
        if os.path.exists(sol_pb):
            os.remove(sol_pb)
        shutil.move(prob_pb, out_pb)
        return 0
    except BaseException as e:
        # Full traceback first (for the .err file / logs), then a single-line
        # marker the parent can pick out reliably. Without the marker,
        # compile_instance had to guess at "the last stderr line", which UP's
        # deprecation warnings routinely won, leaving a bare "compile failed".
        traceback.print_exc()
        first = str(e).strip().splitlines()
        sys.stderr.write(
            f"{_COMPILE_ERROR_MARKER}{type(e).__name__}: {first[0] if first else ''}\n")
        sys.stderr.flush()
        return 3


def _default_args():
    """Minimal args namespace for discover_all() defaults used by subprocess workers."""
    import types
    return types.SimpleNamespace()


def compile_instance(instance: "InstanceRef", cc: CompileConfig, timeout: float = 180) -> Tuple[Optional[str], str, float]:
    out_pb = tempfile.NamedTemporaryFile(suffix=".pb", delete=False).name
    cmd = [sys.executable, os.path.abspath(__file__), "--_compile",
           instance.name, cc.pipeline or "native", cc.up_compilers, out_pb]
    t0 = time.perf_counter()
    try:
        cp = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        _rm(out_pb)
        return None, "compile timeout", time.perf_counter() - t0
    elapsed = time.perf_counter() - t0
    if cp.returncode != 0 or not os.path.getsize(out_pb):
        _rm(out_pb)
        return None, _compile_error_detail(cp), elapsed
    return out_pb, "", elapsed


_COMPILE_ERROR_MARKER = "RTP-COMPILE-ERROR: "


def _compile_error_detail(cp) -> str:
    """Turn a failed compile subprocess into one informative line.

    Prefers the marker compile_worker emits, because UP's own warnings are
    written to stderr *after* the traceback and would otherwise win a naive
    "last line" heuristic — which is how 24 rows in the August sweeps ended up
    recording nothing but "compile failed"."""
    lines = [l.strip() for l in (cp.stderr or "").splitlines() if l.strip()]
    for line in reversed(lines):
        if _COMPILE_ERROR_MARKER in line:
            return line.split(_COMPILE_ERROR_MARKER, 1)[1][:200]
    if cp.returncode < 0:
        return f"compile killed by signal {-cp.returncode}"
    # No marker: the worker died before its handler ran (OOM, os._exit, a
    # native crash). Fall back to the last line that isn't a warning.
    for line in reversed(lines):
        if "Warning" not in line and not line.startswith(("warn(", "  ")):
            return f"rc={cp.returncode}: {line[:200]}"
    return f"compile failed (rc={cp.returncode}, no stderr)"


def _rm(p):
    try:
        os.remove(p)
    except OSError:
        pass


def _summ(vals: List[float]) -> Tuple[Optional[float], Optional[float], Optional[float], Optional[float]]:
    """(min, median, stddev, max) — same percentile-free treatment as compile time gets,
    since K is small (3-5) and a stddev is more informative than a p90 at that sample size."""
    if not vals:
        return None, None, None, None
    return (round(min(vals), 4), round(statistics.median(vals), 4),
            round(statistics.stdev(vals), 4) if len(vals) > 1 else 0.0,
            round(max(vals), 4))


@dataclass
class CompileStats:
    pb: Optional[str]              # retained artifact (last successful compile), or None if all K failed
    k: int
    n_success: int
    c_min: Optional[float]
    c_med: Optional[float]
    c_stddev: Optional[float]
    c_max: Optional[float]
    last_error: str
    raw: List[dict]                # one dict per attempt: rep, status, detail, plan(None), time, ok


def compile_k_times(instance: "InstanceRef", cc: CompileConfig, k: int, timeout: float) -> CompileStats:
    """Compile the same (instance, compile-config) K times to measure compile-time variance,
    then keep exactly one .pb (the last successful compile) as the artifact fed to the
    runtime-config sweep. K stays small (3-5) — this isn't a search for a fast outlier, it's
    giving compile time the same min/median/stddev treatment solve time already gets.
    """
    pbs, times, raw = [], [], []
    last_error = ""
    for rep in range(k):
        pb, err, elapsed = compile_instance(instance, cc, timeout=timeout)
        ok = pb is not None
        raw.append({"rep": rep, "status": "OK" if ok else "COMPILE_ERROR",
                     "detail": "" if ok else err, "plan": None,
                     "time": round(elapsed, 4), "ok": ok})
        if ok:
            pbs.append(pb)
            times.append(elapsed)
        else:
            last_error = err
    if not pbs:
        return CompileStats(None, k, 0, None, None, None, None, last_error, raw)
    keep = pbs[-1]
    for p in pbs[:-1]:
        _rm(p)
    c_min, c_med, c_stddev, c_max = _summ(times)
    return CompileStats(keep, k, len(pbs), c_min, c_med, c_stddev, c_max, last_error, raw)


# ═════════════════════════════════════════════════════════════════════════
# 4. Binary execution & measurement
# ═════════════════════════════════════════════════════════════════════════

def read_solution(sol_pb: str) -> Tuple[str, Optional[int]]:
    import unified_planning.grpc.generated.unified_planning_pb2 as up_pb2
    msg = up_pb2.PlanGenerationResult()
    with open(sol_pb, "rb") as f:
        msg.ParseFromString(f.read())
    status = msg.DESCRIPTOR.fields_by_name["status"].enum_type.values_by_number[msg.status].name
    plan = len(msg.plan.actions) if msg.HasField("plan") else None
    return status, plan


def run_binary_once(rc: RuntimeConfig, pb: str, timeout: float, instance: "InstanceRef") -> dict:
    strategy = instance.forced_strategy or rc.strategy
    sol = tempfile.NamedTemporaryFile(suffix=".pb", delete=False).name
    cmd = [rc.binary, pb, sol, "--strategy", strategy, "--timeout", str(int(timeout))]
    if rc.array_encoding:
        cmd += ["--array-encoding", rc.array_encoding]
    if rc.array_frame_mode:
        cmd += ["--array-frame-mode", rc.array_frame_mode]
    t0 = time.perf_counter()
    try:
        cp = subprocess.run(cmd, cwd=ROOT, capture_output=True, timeout=timeout + 15)
    except subprocess.TimeoutExpired:
        _rm(sol)
        return {"status": "TIMEOUT", "plan": None, "time": timeout + 15, "ok": False}
    wall = time.perf_counter() - t0
    try:
        if cp.returncode < 0:
            return {"status": f"CRASH(sig{-cp.returncode})", "plan": None, "time": wall, "ok": False}
        if cp.returncode != 0 or not os.path.getsize(sol):
            tail = (cp.stderr or b"").decode(errors="replace").strip().splitlines()
            detail = tail[-1][:120] if tail else ""
            return {"status": "INTERNAL_ERROR", "plan": None, "time": wall, "ok": False, "detail": detail}
        status, plan = read_solution(sol)
        return {"status": status, "plan": plan, "time": wall, "ok": status in SOLVED}
    finally:
        _rm(sol)


def measure(rc: RuntimeConfig, pb: str, instance: "InstanceRef", iterations: int,
           timeout: float, budget: float, fail_cap: int) -> Tuple[dict, List[dict]]:
    times, statuses, plans = [], Counter(), Counter()
    start, n = time.time(), 0
    last_detail = ""
    raw = []
    while n < iterations and (time.time() - start) < budget:
        r = run_binary_once(rc, pb, timeout, instance)
        times.append(r["time"]); statuses[r["status"]] += 1
        if r["plan"] is not None:
            plans[r["plan"]] += 1
        if r.get("detail"):
            last_detail = r["detail"]
        raw.append({"rep": n, "status": r["status"], "detail": r.get("detail", ""),
                     "plan": r["plan"], "time": round(r["time"], 4), "ok": r["ok"]})
        n += 1
        if not r["ok"] and n >= fail_cap:
            break
    rep_status = statuses.most_common(1)[0][0]
    rep_plan = plans.most_common(1)[0][0] if plans else None
    summary = {
        "n": n, "status": rep_status, "status_stable": len(statuses) == 1,
        "plan": rep_plan, "plan_stable": len(plans) <= 1,
        "ok": rep_status in SOLVED, "detail": last_detail,
        "t_min": min(times), "t_med": statistics.median(times),
        "t_p90": sorted(times)[min(len(times) - 1, int(0.9 * len(times)))],
        "t_max": max(times),
    }
    return summary, raw


# ═════════════════════════════════════════════════════════════════════════
# 5. Sweep engine — per instance, per (compile-config x runtime-config)
# ═════════════════════════════════════════════════════════════════════════

def run_instance(instance: "InstanceRef", compile_cfgs: List[CompileConfig],
                 runtime_cfgs: List[RuntimeConfig], args) -> dict:
    rows, raw_rows = [], []
    for cc in compile_cfgs:
        # Applied uniformly (success or failure) so the row count here always matches
        # enumerate_jobs()'s count for the same selection.
        applicable_rcs = [rc for rc in _runtime_cfgs_for_compile(cc, runtime_cfgs)
                          if rc.array_encoding not in instance.skip_encodings]

        cs = compile_k_times(instance, cc, args.compile_repeats, args.compile_timeout)
        raw_rows += [{"job_index": "", "instance": instance.name, "family": instance.family,
                      "compile": cc.label, "runtime": "", "phase": "compile", **r} for r in cs.raw]
        compile_stats = {
            "compile_n": cs.k, "compile_ok_n": cs.n_success,
            "compile_min": cs.c_min, "compile_med": cs.c_med,
            "compile_stddev": cs.c_stddev, "compile_max": cs.c_max,
        }

        if cs.pb is None:
            for rc in applicable_rcs:
                rows.append({
                    "instance": instance.name, "family": instance.family,
                    "compile": cc.label, "runtime": rc.label,
                    "status": "COMPILE_ERROR", "detail": cs.last_error,
                    **compile_stats,
                    "status_stable": True, "plan": None, "plan_stable": True,
                    "t_min": None, "t_med": None, "t_p90": None, "t_max": None, "n": 0,
                })
            continue
        try:
            for rc in applicable_rcs:
                m, raw = measure(rc, cs.pb, instance, args.iterations, args.timeout, args.budget, args.fail_cap)
                raw_rows += [{"job_index": "", "instance": instance.name, "family": instance.family,
                              "compile": cc.label, "runtime": rc.label, "phase": "solve", **r} for r in raw]
                rows.append({
                    "instance": instance.name, "family": instance.family,
                    "compile": cc.label, "runtime": rc.label,
                    "status": m["status"], "detail": m.get("detail", ""),
                    **compile_stats,
                    "status_stable": m["status_stable"], "plan": m["plan"], "plan_stable": m["plan_stable"],
                    "t_min": round(m["t_min"], 4), "t_med": round(m["t_med"], 4),
                    "t_p90": round(m["t_p90"], 4), "t_max": round(m["t_max"], 4), "n": m["n"],
                })
        finally:
            _rm(cs.pb)
    return {"instance": instance.name, "family": instance.family, "rows": rows, "raw_rows": raw_rows}


# ═════════════════════════════════════════════════════════════════════════
# 6. Reporting
# ═════════════════════════════════════════════════════════════════════════

def _rank_time(row) -> Optional[float]:
    """Solve time alone (excludes the one-time compile cost).

    Compile time is shared across every runtime-config under the same
    compile-config, so folding it into the ranking metric would dilute
    encoding/frame-mode/binary speedups by a constant that has nothing to
    do with what's being compared. It's still reported as its own column —
    that's exactly the number that matters when the COMPILE axis itself
    varies (e.g. --pipeline native,up).
    """
    return row["t_med"]


def summarize_instance(result: dict) -> dict:
    rows = result["rows"]
    keyed = [(r["status"], r["plan"]) for r in rows]
    agree = len(set(keyed)) <= 1
    solved = [r for r in rows if r["status"] in SOLVED]
    winner, speedup = None, None
    if solved:
        winner_row = min(solved, key=lambda r: _rank_time(r) or float("inf"))
        winner = f"{winner_row['compile']}/{winner_row['runtime']}"
        others = [_rank_time(r) for r in solved if r is not winner_row]
        if others and _rank_time(winner_row):
            speedup = min(others) / _rank_time(winner_row)
    verdict = None
    if len(rows) == 2:
        a, b = rows
        if a["status"] in SOLVED and b["status"] not in SOLVED:
            verdict = "REGRESSED"
        elif a["status"] not in SOLVED and b["status"] in SOLVED:
            verdict = "FIXED"
        elif a["status"] in SOLVED and b["status"] in SOLVED and a["plan"] != b["plan"]:
            verdict = "PLAN-DIFF"
        elif a["status"] != b["status"]:
            verdict = "STATUS-DIFF"
        else:
            ta, tb = _rank_time(a), _rank_time(b)
            if ta and tb and ta > 0:
                d = (tb - ta) / ta * 100
                if abs(d) >= 15 and abs(tb - ta) >= 0.005:
                    verdict = "SLOWER" if d > 0 else "FASTER"
            if verdict is None:
                verdict = "same"
    return {"agree": agree, "winner": winner, "speedup": speedup, "verdict": verdict}


def _fmt_row(r):
    if r["status"] == "COMPILE_ERROR":
        return f"COMPILE_ERROR: {r['detail'][:40]}"
    stab = "" if r["status_stable"] else "*"
    plan_s = f" [{r['plan']}steps]" if r["plan"] is not None else ""
    plan_stab = "" if r["plan_stable"] else "!"
    t = f"{r['t_med']*1000:.0f}ms" if r["t_med"] is not None else "  -  "
    c = ""
    if r.get("compile_med") is not None:
        c = f"  (compile={r['compile_med']*1000:.0f}ms±{r['compile_stddev']*1000:.0f})"
    return f"{r['status']}{stab}{plan_s}{plan_stab} {t}{c}"


def print_instance(result: dict, summary: dict):
    rows = result["rows"]
    print(f"\n[{result['family']:<15}] {result['instance']}")
    for r in rows:
        print(f"    {r['compile']:<12} {r['runtime']:<28} {_fmt_row(r)}")
    tag = ""
    if summary["verdict"] and summary["verdict"] != "same":
        tag = f"  <<< {summary['verdict']}"
    elif not summary["agree"]:
        tag = "  <<< MISMATCH"
    if summary["winner"]:
        sp = f"  ({summary['speedup']:.2f}x)" if summary["speedup"] else ""
        print(f"    → winner: {summary['winner']}{sp}{tag}")
    elif tag:
        print(f"   {tag}")


SUMMARY_FIELDS = ["job_index", "instance", "family", "compile", "runtime", "status", "detail",
                  "status_stable", "plan", "plan_stable",
                  "compile_n", "compile_ok_n", "compile_min", "compile_med", "compile_stddev",
                  "compile_max",
                  "t_min", "t_med", "t_p90", "t_max", "n"]

# phase: "compile" or "solve"; rep: 0-indexed attempt/iteration number within that phase.
# runtime/job_index are blank for compile-phase rows produced during a local full sweep
# (one K-repeat compile is shared across every runtime-config for that instance x
# compile-config); --job-index runs fill in a real job_index since there's exactly one.
RAW_FIELDS = ["job_index", "instance", "family", "compile", "runtime", "phase", "rep",
              "status", "detail", "plan", "time", "ok"]


def _ensure_parent(path: str) -> None:
    """Create the output directory if needed. Every CSV write goes through here:
    under --job-index the write happens *after* the compile and the solve, so a
    missing directory would throw away a job's worth of work at the last step."""
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)


def write_summary_csv(rows: List[dict], path: str) -> None:
    _ensure_parent(path)
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=SUMMARY_FIELDS)
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, "") for k in SUMMARY_FIELDS})


def write_raw_csv(rows: List[dict], path: str) -> None:
    _ensure_parent(path)
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=RAW_FIELDS)
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, "") for k in RAW_FIELDS})


def write_outputs(all_rows: List[dict], summaries: Dict[str, dict], args):
    if args.csv:
        write_summary_csv(all_rows, args.csv)
        print(f"\nCSV written to: {args.csv}")
    if args.json:
        _ensure_parent(args.json)
        with open(args.json, "w") as f:
            json.dump({"rows": all_rows, "summaries": summaries}, f, indent=2, default=str)
        print(f"JSON written to: {args.json}")


# ═════════════════════════════════════════════════════════════════════════
# 6b. Cluster job selection — a "job" is one (instance, compile-config,
#     runtime-config) combo, i.e. exactly one summary row. Optional: every
#     flag below defaults to off, so a plain `python bench.py ...` is
#     unaffected.
# ═════════════════════════════════════════════════════════════════════════

def _runtime_cfgs_for_compile(cc: CompileConfig, runtime_cfgs: List[RuntimeConfig]) -> List[RuntimeConfig]:
    """Non-native compile-configs already ran ARRAYS_REMOVING/ARRAYS_LOGARITHMIC_REMOVING
    or SETS_REMOVING before RTP ever sees the problem (every UP pipeline we actually use
    — up/uti/int/log/c/ci/cin/sc/sci/scin — includes one of these), so
    --array-encoding/--array-frame-mode have nothing left to act on: empirically confirmed
    identical results across all 3 runtime-configs for up:uti/up:int on pancake. Sweeping
    the full runtime-config list against them just re-runs the same solve N times. native
    keeps the full sweep (the array/set representation is still there); every other
    compile-config collapses to a single representative runtime-config (the first one,
    conventionally uf/disequality — RTP's own default)."""
    if cc.pipeline is None:  # native
        return runtime_cfgs
    return runtime_cfgs[:1]


def enumerate_jobs(instances: List[Instance], compile_cfgs: List[CompileConfig],
                   runtime_cfgs: List[RuntimeConfig]) -> List[Tuple[Instance, CompileConfig, RuntimeConfig]]:
    """Deterministic flat (instance, compile-config, runtime-config) list — the same
    selection flags (families/filter/pipeline/binary/strategy/array-encoding/frame-mode)
    must be passed to every --job-index invocation for indices to line up.
    """
    jobs = []
    for inst in instances:
        for cc in compile_cfgs:
            for rc in _runtime_cfgs_for_compile(cc, runtime_cfgs):
                if rc.array_encoding in inst.skip_encodings:
                    continue
                jobs.append((inst, cc, rc))
    return jobs


def _job_suffixed_path(path: str, idx: int, total: int) -> str:
    """Give each job its own output file (e.g. results.job007.csv) so concurrent
    SLURM array tasks never contend for the same file; merge them afterward."""
    root, ext = os.path.splitext(path)
    width = max(len(str(max(total - 1, 0))), 1)
    return f"{root}.job{idx:0{width}d}{ext}"


def _fmt_hms(total_seconds: float) -> str:
    total_seconds = max(60, int(round(total_seconds)))
    h, rem = divmod(total_seconds, 3600)
    m, s = divmod(rem, 60)
    return f"{h:02d}:{m:02d}:{s:02d}"


def _default_sbatch_time(args) -> str:
    """Worst case for one array task: K sequential compiles (each up to
    --compile-timeout) plus up to --iterations sequential solves (each up to
    --timeout), with a 20% + 2min margin for python/UP import and I/O overhead.
    This is what actually caught the old hardcoded 00:30:00 being wrong for a
    1800s solve timeout — always derive it from the real per-job cost inputs.
    """
    worst = args.compile_repeats * args.compile_timeout + args.iterations * args.timeout
    return _fmt_hms(worst * 1.2 + 120)


def emit_sbatch_script(path: str, n_jobs: int, args, jobs_list=None) -> None:
    """Write a template array-job script that reruns this exact invocation (minus
    --emit-sbatch/--count-jobs/--job-index/--sbatch-* themselves) once per array index."""
    strip_with_value = {"--emit-sbatch", "--job-index", "--sbatch-time", "--sbatch-mem",
                        "--sbatch-cpus", "--sbatch-array-limit", "--write-manifest", "--merge"}
    strip_flag = {"--count-jobs"}
    cleaned, skip_next = [], False
    for a in sys.argv[1:]:
        if skip_next:
            skip_next = False
            continue
        bare = a.split("=", 1)[0]
        if bare in strip_with_value:
            if "=" not in a:
                skip_next = True
            continue
        if bare in strip_flag:
            continue
        cleaned.append(a)
    cmd = " ".join(shlex.quote(a) for a in cleaned)

    time_limit = args.sbatch_time or _default_sbatch_time(args)
    array_spec = f"0-{n_jobs - 1}" + (f"%{args.sbatch_array_limit}" if args.sbatch_array_limit else "")

    # The run directory is wherever the shards land; the manifest, the merged
    # output and the logs all belong one level up from them.
    run_dir = None
    if args.csv:
        shard_dir = os.path.dirname(args.csv)
        run_dir = (os.path.dirname(shard_dir)
                   if os.path.basename(shard_dir) == "shards" else shard_dir)
    log_dir = os.path.join(run_dir, "logs") if run_dir else "slurm_logs"

    # mkdir every output dir up front — a job that's first to run in the array
    # would otherwise crash on open() before any file exists. The log directory
    # is created here at emit time rather than in the script: SLURM opens the
    # log files before the script body ever runs.
    mkdirs = {log_dir}
    for p_ in (args.csv, args.raw_csv, args.json):
        if p_:
            d = os.path.dirname(p_)
            if d:
                mkdirs.add(d)
    os.makedirs(log_dir, exist_ok=True)
    mkdir_cmd = " && ".join(f"mkdir -p {shlex.quote(d)}" for d in sorted(mkdirs))

    script = f"""#!/bin/bash
#SBATCH --job-name=rtp-bench
#SBATCH --array={array_spec}
#SBATCH --cpus-per-task={args.sbatch_cpus}
#SBATCH --mem={args.sbatch_mem}
#SBATCH --time={time_limit}
#SBATCH --output={log_dir}/%A_%a.log
#SBATCH --error={log_dir}/%A_%a.err
# EDIT ME — partition/account are cluster-specific:
# #SBATCH --partition=CHANGE_ME
# #SBATCH --account=CHANGE_ME

{mkdir_cmd}
source .venv/bin/activate
python3 {shlex.quote(os.path.abspath(__file__))} {cmd} --workers 1
"""
    script_dir = os.path.dirname(path)
    if script_dir:
        os.makedirs(script_dir, exist_ok=True)
    with open(path, "w") as f:
        f.write(script)
    os.chmod(path, 0o755)
    print(f"Wrote {path}  ({n_jobs} jobs, --array={array_spec}, --time={time_limit}, --mem={args.sbatch_mem})")
    print(f"$SLURM_ARRAY_TASK_ID is picked up automatically as --job-index.")

    if run_dir and jobs_list is not None:
        write_manifest(os.path.join(run_dir, MANIFEST_NAME), jobs_list, args)

    print("\nEdit partition/account, then submit the array and a merge job that "
          "waits on it — the merge is then automatic:")
    print(f"    ARRAY_ID=$(sbatch --parsable {shlex.quote(path)})")
    merge_target = shlex.quote(run_dir) if run_dir else "<run-dir>"
    print(f"    sbatch --dependency=afterany:$ARRAY_ID --wrap \\")
    print(f"        \"python3 {shlex.quote(os.path.abspath(__file__))} --merge {merge_target}\"")
    print("(afterany, not afterok: a few dead tasks should still produce a merge "
          "plus a report of which indices to requeue.)")


def run_single_job(job_idx: int, jobs: List[Tuple[Instance, CompileConfig, RuntimeConfig]], args) -> int:
    if not jobs:
        sys.exit("No jobs available for the given selection.")
    if job_idx < 0 or job_idx >= len(jobs):
        sys.exit(f"--job-index {job_idx} out of range (0..{len(jobs) - 1})")
    inst, cc, rc = jobs[job_idx]
    ref = inst.ref()

    cs = compile_k_times(ref, cc, args.compile_repeats, args.compile_timeout)
    raw_rows = [{"job_index": job_idx, "instance": inst.name, "family": inst.family,
                 "compile": cc.label, "runtime": "", "phase": "compile", **r} for r in cs.raw]
    base = {"job_index": job_idx, "instance": inst.name, "family": inst.family,
            "compile": cc.label, "runtime": rc.label}
    compile_stats = {
        "compile_n": cs.k, "compile_ok_n": cs.n_success,
        "compile_min": cs.c_min, "compile_med": cs.c_med,
        "compile_stddev": cs.c_stddev, "compile_max": cs.c_max,
    }

    if cs.pb is None:
        row = {**base, "status": "COMPILE_ERROR", "detail": cs.last_error, **compile_stats,
               "status_stable": True, "plan": None, "plan_stable": True,
               "t_min": None, "t_med": None, "t_p90": None, "t_max": None, "n": 0}
    else:
        try:
            m, raw = measure(rc, cs.pb, ref, args.iterations, args.timeout, args.budget, args.fail_cap)
            raw_rows += [{**base, "phase": "solve", **r} for r in raw]
            row = {**base, "status": m["status"], "detail": m.get("detail", ""), **compile_stats,
                   "status_stable": m["status_stable"], "plan": m["plan"], "plan_stable": m["plan_stable"],
                   "t_min": round(m["t_min"], 4), "t_med": round(m["t_med"], 4),
                   "t_p90": round(m["t_p90"], 4), "t_max": round(m["t_max"], 4), "n": m["n"]}
        finally:
            _rm(cs.pb)

    print(f"[job {job_idx}/{len(jobs) - 1}] [{inst.family:<15}] {inst.name}  "
          f"{cc.label}/{rc.label}  {_fmt_row(row)}")

    if args.csv:
        out = _job_suffixed_path(args.csv, job_idx, len(jobs))
        write_summary_csv([row], out)
        print(f"CSV written to: {out}")
    if args.raw_csv:
        out = _job_suffixed_path(args.raw_csv, job_idx, len(jobs))
        write_raw_csv(raw_rows, out)
        print(f"Raw CSV written to: {out}")

    return 0 if row["status"] in SOLVED else 1


# ═════════════════════════════════════════════════════════════════════════
# 6c. Cluster run manifest + shard merge — the other half of --job-index.
#
#     An array run scatters one CSV pair per task. Without a record of what
#     the sweep was *supposed* to produce, a merge cannot tell "750 shards,
#     all present" from "750 shards, task 431 died and 749 are here" — and a
#     merge run before the last tasks land silently produces a short file.
#     Both have already happened in xts/results/ (a 280-row summary against 281
#     shards, a 3498-row raw against 3499). So: write a manifest at submit
#     time, and verify against it at merge time.
# ═════════════════════════════════════════════════════════════════════════

MANIFEST_NAME = "run.json"
_SHARD_RE = re.compile(r"^(?P<base>.+)\.job(?P<idx>\d+)\.csv$")

# Flags that describe *this* invocation rather than the sweep, so they must not
# be recorded as part of the selection that array tasks have to reproduce.
_NON_SELECTION_FLAGS = {"--emit-sbatch", "--job-index", "--sbatch-time", "--sbatch-mem",
                        "--sbatch-cpus", "--sbatch-array-limit", "--write-manifest",
                        "--merge", "--csv", "--raw-csv", "--json"}
_NON_SELECTION_BARE = {"--count-jobs", "--list"}


def _selection_argv() -> List[str]:
    """This invocation's argv with the non-selection flags stripped — i.e. exactly
    what every array task must be given for --job-index values to line up."""
    out, skip = [], False
    for a in sys.argv[1:]:
        if skip:
            skip = False
            continue
        bare = a.split("=", 1)[0]
        if bare in _NON_SELECTION_FLAGS:
            if "=" not in a:
                skip = True
            continue
        if bare in _NON_SELECTION_BARE:
            continue
        out.append(a)
    return out


def _git_sha() -> Optional[str]:
    try:
        cp = subprocess.run(["git", "-C", ROOT, "rev-parse", "HEAD"],
                            capture_output=True, text=True, timeout=10)
        return cp.stdout.strip() or None
    except Exception:
        return None


def write_manifest(path: str, jobs: List[Tuple[Instance, CompileConfig, RuntimeConfig]],
                   args) -> None:
    """Record what this sweep is supposed to produce, next to where it produces it."""
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    manifest = {
        "created": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
        "git_sha": _git_sha(),
        "selection_argv": _selection_argv(),
        "n_jobs": len(jobs),
        "timeout": args.timeout,
        "compile_timeout": args.compile_timeout,
        "iterations": args.iterations,
        "jobs": [{"index": i, "instance": inst.name, "family": inst.family,
                  "compile": cc.label, "runtime": rc.label}
                 for i, (inst, cc, rc) in enumerate(jobs)],
    }
    with open(path, "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"Manifest written to: {path}  ({len(jobs)} jobs)")


def _compact_ranges(indices: List[int]) -> str:
    """[3,7,8,9,42] -> '3,7-9,42' — SLURM --array syntax, so a requeue of exactly
    the missing tasks is copy-pasteable."""
    if not indices:
        return ""
    parts, start, prev = [], indices[0], indices[0]
    for i in indices[1:]:
        if i == prev + 1:
            prev = i
            continue
        parts.append(f"{start}" if start == prev else f"{start}-{prev}")
        start = prev = i
    parts.append(f"{start}" if start == prev else f"{start}-{prev}")
    return ",".join(parts)


def _discover_shards(shard_dir: str) -> Dict[str, List[Tuple[int, str]]]:
    """Group <base>.jobNNN.csv files by base name, each as [(job_index, path)]."""
    groups: Dict[str, List[Tuple[int, str]]] = {}
    for name in sorted(os.listdir(shard_dir)):
        m = _SHARD_RE.match(name)
        if m:
            groups.setdefault(m.group("base"), []).append(
                (int(m.group("idx")), os.path.join(shard_dir, name)))
    for v in groups.values():
        v.sort()
    return groups


def _concat_shards(shards: List[Tuple[int, str]], fields: List[str], dest: str):
    """Concatenate shard CSVs through the canonical writer.

    Going via DictReader/DictWriter rather than `tail -n +2` normalizes column
    order and surfaces a truncated shard (a task killed mid-write) as a short
    row instead of silently corrupting the merged file.
    """
    rows, seen, empty = [], set(), []
    for idx, path in shards:
        with open(path, newline="") as f:
            got = list(csv.DictReader(f))
        if not got:
            empty.append(idx)
            continue
        rows.extend(got)
        seen.add(idx)
    with open(dest, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, "") for k in fields})
    return rows, seen, empty


def merge_run(run_dir: str) -> int:
    """Merge one run directory's shards into summary.csv / raw.csv, and verify.

    Idempotent: re-running after requeuing the missing tasks just picks up the
    new shards and rewrites both files.
    """
    if not os.path.isdir(run_dir):
        sys.exit(f"--merge: not a directory: {run_dir}")

    shard_dir = os.path.join(run_dir, "shards")
    if not os.path.isdir(shard_dir):
        shard_dir = run_dir  # tolerate shards sitting loose in the run dir
    groups = _discover_shards(shard_dir)
    if not groups:
        sys.exit(f"--merge: no <base>.jobNNN.csv shards found under {shard_dir}")

    manifest_path = os.path.join(run_dir, MANIFEST_NAME)
    manifest = None
    if os.path.isfile(manifest_path):
        with open(manifest_path) as f:
            manifest = json.load(f)

    raw_bases = [b for b in groups if b.endswith("_raw")]
    sum_bases = [b for b in groups if not b.endswith("_raw")]
    if len(sum_bases) > 1:
        sys.exit(f"--merge: ambiguous, found several shard sets: {', '.join(sorted(sum_bases))}")

    print(f"Merging {run_dir}")
    if manifest:
        print(f"  manifest: {manifest['n_jobs']} jobs, created {manifest['created']}"
              + (f", git {manifest['git_sha'][:9]}" if manifest.get("git_sha") else ""))
    else:
        print("  manifest: none (completeness inferred from the highest job index present)")

    seen_all, empty_all = set(), []
    for bases, fields, out_name in ((sum_bases, SUMMARY_FIELDS, "summary.csv"),
                                    (raw_bases, RAW_FIELDS, "raw.csv")):
        if not bases:
            print(f"  {out_name}: no shards, skipped")
            continue
        shards = groups[bases[0]]
        dest = os.path.join(run_dir, out_name)
        rows, seen, empty = _concat_shards(shards, fields, dest)
        print(f"  {out_name}: {len(shards)} shards -> {len(rows)} rows")
        if out_name == "summary.csv":
            seen_all, empty_all = seen, empty

    expected = manifest["n_jobs"] if manifest else (max(seen_all) + 1 if seen_all else 0)
    missing = sorted(set(range(expected)) - seen_all)

    print()
    if empty_all:
        print(f"  {len(empty_all)} shard(s) had a header but no data row "
              f"(task started then died): {_compact_ranges(empty_all)}")
    if missing:
        print(f"  INCOMPLETE — {len(missing)} of {expected} jobs are missing.")
        print(f"  Requeue exactly those, then re-run --merge:")
        print(f"      sbatch --array={_compact_ranges(missing)} <your-array-script>")
        return 1
    if not manifest:
        print(f"  {len(seen_all)} jobs present, indices 0-{expected - 1} contiguous. "
              f"Without a manifest this cannot rule out tasks missing off the end.")
        return 0
    print(f"  COMPLETE — all {expected} jobs present.")
    return 0


# ═════════════════════════════════════════════════════════════════════════
# 7. CLI
# ═════════════════════════════════════════════════════════════════════════

def parse_args():
    ap = argparse.ArgumentParser(
        description="Unified RanTanPlan performance benchmark.",
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("--_compile", nargs=4, default=None, help=argparse.SUPPRESS)

    ap.add_argument("--families", default="all",
                    help="Comma list: xts-unit,xts-translation,xts-native,classic,synthetic,scaling,"
                         "paper,paper-hc,all (default: all).")
    ap.add_argument("--filter", default=None, metavar="PATTERN",
                    help="Only instances whose name matches PATTERN (glob or substring).")
    ap.add_argument("--only", nargs="+", default=[], help="Alias for --filter with multiple substrings (OR).")
    ap.add_argument("--list", action="store_true", help="List discovered instances and exit.")

    ap.add_argument("--pipeline", default="native",
                    help="Compile-time axis, comma list: 'native' plus/or any COMPILATION_PIPELINES name "
                         "(e.g. 'up' -> --list-pipelines in solve.py names it 'iasciu'; pass that name "
                         "directly, e.g. --pipeline native,iasciu). Default: native.")
    ap.add_argument("--up-compilers", default="IPAR,QR,GR",
                    help="Compiler list used for 'native' compile-configs (default: IPAR,QR,GR).")

    ap.add_argument("--binary", action="append", default=[], metavar="PATH",
                    help="Binary to invoke (repeatable). Default: current build.")
    ap.add_argument("--binary-ref", action="append", default=[], metavar="GIT_REF",
                    help="Git ref to build (via throwaway worktree) and invoke (repeatable).")
    ap.add_argument("--strategy", default="seq", help="Comma list of --strategy values (default: seq).")
    ap.add_argument("--array-encoding", default="uf",
                    help="Comma list: theory,uf (default: uf).")
    ap.add_argument("--array-frame-mode", default="disequality",
                    help="Comma list: disequality,ite (default: disequality). Applies to "
                         "both the theory and uf encodings.")

    ap.add_argument("--iterations", type=int, default=5, help="Repeats per (instance,compile,runtime) combo.")
    ap.add_argument("--timeout", type=float, default=10.0, help="Per-solve timeout, seconds.")
    ap.add_argument("--compile-timeout", type=float, default=180.0, help="Per-compile timeout, seconds.")
    ap.add_argument("--compile-repeats", type=int, default=3,
                    help="Times to compile each (instance,compile-config) to measure compile-time "
                         "variance with the same min/median/stddev treatment solve time gets "
                         "(default: 3; keep small, 3-5 — this is not a search for a fast outlier).")
    ap.add_argument("--budget", type=float, default=60.0, help="Max seconds per combo across iterations.")
    ap.add_argument("--fail-cap", type=int, default=3, help="Stop repeating after this many non-SOLVED runs.")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4, help="Build parallelism for --binary-ref.")
    ap.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 2) // 2),
                    help="Parallel instances (ProcessPoolExecutor workers). Ignored under --job-index.")

    ap.add_argument("--csv", default=None, help="Aggregated summary, one row per (instance,compile,runtime).")
    ap.add_argument("--json", default=None)
    ap.add_argument("--raw-csv", default=None,
                    help="Raw data, one row per individual repetition (every compile attempt and every "
                         "solve run) instead of just the aggregated summary.")

    ap.add_argument("--job-index", type=int, default=None,
                    help="Run only this 0-indexed job (one instance x compile-config x runtime-config "
                         "combo) instead of the full sweep. Falls back to $SLURM_ARRAY_TASK_ID if that's "
                         "set and this is omitted. Omit both for a normal local run (default).")
    ap.add_argument("--count-jobs", action="store_true",
                    help="Print the total number of jobs for the current selection and exit "
                         "(use to size a SLURM --array=0-N-1).")
    ap.add_argument("--emit-sbatch", default=None, metavar="FILE",
                    help="Write a template SLURM array-job script for the current selection and exit "
                         "(edit partition/account before submitting).")
    ap.add_argument("--write-manifest", default=None, metavar="FILE",
                    help="Write a run manifest (job count + the full instance x config list + git SHA) "
                         "for the current selection and exit. Run once at submit time; --merge reads it "
                         "to tell a complete sweep from one that lost tasks.")
    ap.add_argument("--merge", default=None, metavar="RUN_DIR",
                    help="Merge a finished array run's shards into RUN_DIR/summary.csv and "
                         "RUN_DIR/raw.csv, verify against RUN_DIR/run.json, and report any missing "
                         "job indices as a ready-to-submit --array spec. Idempotent.")
    ap.add_argument("--sbatch-time", default=None, metavar="HH:MM:SS",
                    help="SLURM --time for --emit-sbatch. Default: auto-computed from "
                         "compile-repeats x compile-timeout + iterations x timeout, plus margin — "
                         "not a fixed guess, so it tracks whatever --timeout you actually set.")
    ap.add_argument("--sbatch-mem", default="4G", metavar="MEM",
                    help="SLURM --mem for --emit-sbatch (default: 4G — Z3 can be memory-hungry "
                         "on the bigger domains; raise if you see OOM kills).")
    ap.add_argument("--sbatch-cpus", type=int, default=1, metavar="N",
                    help="SLURM --cpus-per-task for --emit-sbatch (default: 1).")
    ap.add_argument("--sbatch-array-limit", type=int, default=None, metavar="K",
                    help="Cap concurrent array tasks (--array=0-N-1%%K) for --emit-sbatch — check your "
                         "cluster's per-user job/fairshare limits before submitting hundreds unthrottled.")
    return ap.parse_args()


def _worker(item):
    instance, compile_cfgs, runtime_cfgs, args = item
    return run_instance(instance, compile_cfgs, runtime_cfgs, args)


def main():
    args = parse_args()
    if args._compile:
        sys.exit(compile_worker(*args._compile))

    # Merging reads finished CSVs only — no corpus discovery, no planner binary,
    # so it is I/O-bound and needs neither a compute node nor a built backend.
    # (It still needs this file's imports, hence the venv.)
    if args.merge:
        return merge_run(args.merge)

    if not os.path.isfile(DEFAULT_BIN) and not args.binary and not args.binary_ref:
        sys.exit(f"No RanTanPlan binary found at {DEFAULT_BIN}; build it or pass --binary/--binary-ref.")

    families = [f.strip() for f in args.families.split(",") if f.strip()]
    instances = discover_all(families, args)

    patterns = ([args.filter] if args.filter else []) + args.only
    if patterns:
        instances = [i for i in instances if any(_matches_filter(i.name, p) for p in patterns)]

    if args.list:
        for i in instances:
            tag = f"  (strategy={i.forced_strategy})" if i.forced_strategy else ""
            print(f"  [{i.family:<15}] {i.name}{tag}")
        print(f"\n{len(instances)} instances.")
        return 0

    if not instances:
        print("No instances matched.")
        return 1

    compile_cfgs = parse_compile_configs(args)
    runtime_cfgs = parse_runtime_configs(args)
    jobs = enumerate_jobs(instances, compile_cfgs, runtime_cfgs)

    if args.count_jobs:
        per_cc = ", ".join(f"{cc.label}={len(_runtime_cfgs_for_compile(cc, runtime_cfgs))}" for cc in compile_cfgs)
        print(f"{len(jobs)} jobs  ({len(instances)} instances x runtime-configs per compile-config: {per_cc}"
              f" — non-native collapsed to 1, array/set already removed before RTP sees it —"
              f" minus skipped encoding combos)")
        if jobs:
            print(f"--array=0-{len(jobs) - 1}")
        return 0

    if args.write_manifest:
        if not jobs:
            sys.exit("No jobs to record — check your family/filter/config selection.")
        write_manifest(args.write_manifest, jobs, args)
        return 0

    if args.emit_sbatch:
        if not jobs:
            sys.exit("No jobs to schedule — check your family/filter/config selection.")
        emit_sbatch_script(args.emit_sbatch, len(jobs), args, jobs_list=jobs)
        return 0

    job_index = args.job_index
    if job_index is None and os.environ.get("SLURM_ARRAY_TASK_ID") is not None:
        job_index = int(os.environ["SLURM_ARRAY_TASK_ID"])
    if job_index is not None:
        return run_single_job(job_index, jobs, args)

    print(f"{len(instances)} instances  x  {len(compile_cfgs)} compile-config(s)  "
          f"(runtime-configs non-native-collapsed)  =  {len(jobs)} combos  "
          f"({args.iterations} iters each)\n")
    print("compile configs: " + ", ".join(c.label for c in compile_cfgs))
    print("runtime configs: " + ", ".join(r.label for r in runtime_cfgs))

    all_rows: List[dict] = []
    all_raw_rows: List[dict] = []
    summaries: Dict[str, dict] = {}
    n_mismatch = n_regressed = n_fixed = 0

    with ProcessPoolExecutor(max_workers=args.workers) as ex:
        futures = {ex.submit(_worker, (inst.ref(), compile_cfgs, runtime_cfgs, args)): inst for inst in instances}
        for fut in as_completed(futures):
            result = fut.result()
            summary = summarize_instance(result)
            print_instance(result, summary)
            all_rows += result["rows"]
            all_raw_rows += result["raw_rows"]
            summaries[result["instance"]] = summary
            if not summary["agree"]:
                n_mismatch += 1
            if summary["verdict"] == "REGRESSED":
                n_regressed += 1
            if summary["verdict"] == "FIXED":
                n_fixed += 1

    print(f"\n{'='*70}\nSUMMARY\n{'='*70}")
    print(f"  instances run:     {len(instances)}")
    if n_mismatch:
        print(f"  mismatches:        {n_mismatch}  (status/plan disagree across configs)")
    if n_regressed:
        print(f"  REGRESSED:         {n_regressed}  (solved in one config, not another)")
    if n_fixed:
        print(f"  FIXED:             {n_fixed}")

    write_outputs(all_rows, summaries, args)
    if args.raw_csv:
        write_raw_csv(all_raw_rows, args.raw_csv)
        print(f"Raw CSV written to: {args.raw_csv}")
    return 1 if (n_mismatch or n_regressed) else 0


if __name__ == "__main__":
    sys.exit(main())