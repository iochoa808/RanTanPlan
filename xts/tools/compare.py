#!/usr/bin/env python3
"""
compare.py — compare RanTanPlan compilation paths.

Subcommands
  pipeline   Native RTP vs UP-pre-compiled, both solved by RanTanPlan.
             Answers: does RTP handle a feature natively as well as when UP pre-compiles it away?
  up         Native RTP vs UP compilation pipeline + external planner.
             Answers: does RTP agree with an external planner (e.g. fast-downward)?
  array      Same problem solved once per array-axiom config (encoding backend /
             frame-axiom shape). Answers: does RTP agree with itself across
             --array-encoding (theory|uf) and --array-frame-mode (disequality|ite)?

Usage:
    python compare.py --list-pipelines
    python compare.py pipeline -d domain.pddl -p problem.pddl
    python compare.py pipeline --source problem.py --feature arrays sets --run --diff
    python compare.py pipeline -d domain.pddl -p problem.pddl --precompile sets --run
    python compare.py up -d domain.pddl -p problem.pddl
    python compare.py up -d domain.pddl -p problem.pddl --feature arrays
    python compare.py up -d domain.pddl -p problem.pddl --run --solver fast-downward
    python compare.py array -d domain.pddl -p problem.pddl
    python compare.py array -d domain.pddl -p problem.pddl --configs theory:ite uf:ite
"""

import argparse
import os
import shutil
import sys
import time
from typing import Dict, List, Optional, Tuple

# ── UP extension path — must precede all unified_planning imports ──────────────
_ep = argparse.ArgumentParser(add_help=False)
_ep.add_argument('--up-path', default=os.path.expanduser('~/unified-planning'))
_UP_PATH = _ep.parse_known_args()[0].up_path
if _UP_PATH not in sys.path:
    sys.path.insert(0, _UP_PATH)

# ── Pull in UP venv site-packages ─────────────────────────────────────────────
import glob as _glob
_UP_VENV_SP = _glob.glob(os.path.join(_UP_PATH, 'venv/lib/python*/site-packages'))
if _UP_VENV_SP:
    _sp = _UP_VENV_SP[0]
    if _sp not in sys.path:
        sys.path.insert(1, _sp)
del _glob, _UP_VENV_SP

# ── UP imports ────────────────────────────────────────────────────────────────
from unified_planning.engines import CompilationKind
from unified_planning.grpc.proto_writer import ProtobufWriter
from unified_planning.io.up_pddl_reader import UPPDDLReader
from unified_planning.shortcuts import get_environment, Fraction, Compiler, OneshotPlanner
from unified_planning.model import Problem
from unified_planning.model.fluent import get_all_fluent_exp
from docs.extensions.domains.compilation_solving import COMPILATION_PIPELINES as PIPELINES

# ── RanTanPlan imports ────────────────────────────────────────────────────────
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rantanplan import RantanPlanPlanner
from solve import solve_rtp, load_python, _format_sidebyside

BAR = "=" * 70
DEFAULT_PIPELINE = 'iasciu'

# ── Feature maps ──────────────────────────────────────────────────────────────
FEATURES = {
    'arrays':   (CompilationKind.ARRAYS_REMOVING,                'fluents_array',    'array fluents → 0 after ARRAYS_REMOVING'),
    'params':   (CompilationKind.INT_PARAMETER_ACTIONS_REMOVING, 'actions',          'actions unrolled by INT_PARAMETER_ACTIONS_REMOVING'),
    'sets':     (CompilationKind.SETS_REMOVING,                  'fluents_set',      'set fluents → 0 after SETS_REMOVING'),
    'count':    (CompilationKind.COUNT_REMOVING,                 'actions',          'count conditions removed by COUNT_REMOVING'),
    'bounded':  (CompilationKind.INTEGERS_REMOVING,              'fluents_bounded',  'bounded types removed by INTEGERS_REMOVING'),
    'usertype': (CompilationKind.USERTYPE_FLUENTS_REMOVING,      'fluents_usertype', 'usertype fluents → 0 after USERTYPE_FLUENTS_REMOVING'),
}

# When a feature is left for RTP, downstream UP compilers that crash on that
# feature must also be dropped.  Empirically confirmed; see visualize_pipeline.py.
# Empirically confirmed crash matrix — see visualize_pipeline.py for details.
# 'sets' also forces COUNT to drop: CountRemover crashes on SET_MEMBER nodes
# that remain when SETS_REMOVING is skipped (walker has no SET_MEMBER handler).
_CASCADE_DROPS: Dict[str, set] = {
    'arrays':   {CompilationKind.INTEGERS_REMOVING, CompilationKind.USERTYPE_FLUENTS_REMOVING},
    'sets':     {CompilationKind.COUNT_REMOVING, CompilationKind.INTEGERS_REMOVING,
                 CompilationKind.USERTYPE_FLUENTS_REMOVING, CompilationKind.QUANTIFIERS_REMOVING},
    'count':    {CompilationKind.INTEGERS_REMOVING, CompilationKind.USERTYPE_FLUENTS_REMOVING},
    'params':   set(),
    'bounded':  set(),
    'usertype': set(),
}

PRECOMPILE_KINDS = {
    'arrays':   (CompilationKind.ARRAYS_REMOVING,           {'mode': 'permissive'}),
    'sets':     (CompilationKind.SETS_REMOVING,             {}),
    'count':    (CompilationKind.COUNT_REMOVING,            {}),
    'bounded':  (CompilationKind.INTEGERS_REMOVING,         {}),
    'usertype': (CompilationKind.USERTYPE_FLUENTS_REMOVING, {}),
}
_PRECOMPILE_ORDER = ['arrays', 'sets', 'count', 'bounded', 'usertype']

# ── Type predicates ───────────────────────────────────────────────────────────
def _is_array(t):    return hasattr(t, 'is_array_type') and t.is_array_type()
def _is_set(t):      return hasattr(t, 'is_set_type')   and t.is_set_type()
def _is_bounded(t):  return (t.is_int_type() or t.is_real_type()) and getattr(t, 'lower_bound', None) is not None
def _is_usertype(t): return t.is_user_type()


# ── Direct compiler access (bypasses UP engine selector) ──────────────────────
# Used by the research pipeline to call compilers even when the selector would
# reject them due to conservative supported_kind declarations.
def _get_direct_compiler_map():
    from unified_planning.engines.compilers.int_parameter_actions_remover import IntParameterActionsRemover
    from unified_planning.engines.compilers.arrays_remover import ArraysRemover
    from unified_planning.engines.compilers.sets_remover import SetsRemover
    from unified_planning.engines.compilers.count_remover import CountRemover
    from unified_planning.engines.compilers.integers_remover import IntegersRemover
    from unified_planning.engines.compilers.usertype_fluents_remover import UsertypeFluentsRemover
    from unified_planning.engines.compilers.quantifiers_remover import QuantifiersRemover
    return {
        CompilationKind.INT_PARAMETER_ACTIONS_REMOVING: (IntParameterActionsRemover, {}),
        CompilationKind.ARRAYS_REMOVING:                (ArraysRemover,              {'mode': 'permissive'}),
        CompilationKind.SETS_REMOVING:                  (SetsRemover,                {}),
        CompilationKind.COUNT_REMOVING:                 (CountRemover,               {}),
        CompilationKind.INTEGERS_REMOVING:              (IntegersRemover,            {}),
        CompilationKind.USERTYPE_FLUENTS_REMOVING:      (UsertypeFluentsRemover,     {}),
        CompilationKind.QUANTIFIERS_REMOVING:           (QuantifiersRemover,         {}),
    }


def run_research_pipeline(problem: Problem, pipeline_steps: List[CompilationKind]) -> Tuple[Problem, List, List[str]]:
    """Like run_up_pipeline but calls compiler classes directly, bypassing the
    UP engine selector.  Used for research experiments where compilers are run
    out of their declared supported_kind order."""
    direct_map = _get_direct_compiler_map()
    results, steps = [], []
    for ck in pipeline_steps:
        if ck in direct_map:
            cls, params = direct_map[ck]
            compiler = cls()
            r = compiler._compile(problem, ck)
        else:
            params = {'mode': 'permissive'} if ck == CompilationKind.ARRAYS_REMOVING else {}
            with Compiler(problem_kind=problem.kind, compilation_kind=ck, params=params) as c:
                r = c.compile(problem, ck)
        results.append(r)
        problem = r.problem
        steps.append(ck.name)
    return problem, results, steps


# ── Shared pipeline runners ───────────────────────────────────────────────────
def run_native_pipeline(problem: Problem) -> Tuple[Problem, List[str]]:
    planner = RantanPlanPlanner(executable_path="/dev/null", verbosity="silent")
    compiled_problem, combined_result = planner._compile_problem(problem)
    return compiled_problem, combined_result.step_names


def run_up_pipeline(problem: Problem, pipeline_steps: List[CompilationKind]) -> Tuple[Problem, List, List[str]]:
    """Returns (compiled_problem, compilation_results, step_names)."""
    results, steps = [], []
    for ck in pipeline_steps:
        params = {'mode': 'permissive'} if ck == CompilationKind.ARRAYS_REMOVING else {}
        with Compiler(problem_kind=problem.kind, compilation_kind=ck, params=params) as c:
            r = c.compile(problem, ck)
            results.append(r)
            problem = r.problem
            steps.append(ck.name)
    return problem, results, steps


def _run_ck(problem: Problem, ck: CompilationKind, params: dict = {}) -> Tuple[Problem, str]:
    with Compiler(problem_kind=problem.kind, compilation_kind=ck, params=params) as c:
        return c.compile(problem, ck).problem, ck.name


def precompile_features(problem: Problem, features: List[str]) -> Tuple[Problem, List[str]]:
    steps = []
    problem, name = _run_ck(problem, CompilationKind.INT_PARAMETER_ACTIONS_REMOVING)
    steps.append(name)
    for feat in _PRECOMPILE_ORDER:
        if feat not in features:
            continue
        if feat in ('sets', 'count') and 'arrays' not in features and problem.kind.has_array_fluents():
            problem, name = _run_ck(problem, CompilationKind.ARRAYS_REMOVING, {'mode': 'permissive'})
            steps.append(f"{name}(auto)")
        ck, params = PRECOMPILE_KINDS[feat]
        problem, name = _run_ck(problem, ck, params)
        steps.append(name)
    return problem, steps


# ── Shared stats / serialization ──────────────────────────────────────────────
def problem_stats(problem: Problem) -> Dict[str, int]:
    fl, ac = list(problem.fluents), list(problem.actions)
    return {
        'fluents_total':    len(fl),
        'fluents_bool':     sum(1 for f in fl if f.type.is_bool_type()),
        'fluents_int':      sum(1 for f in fl if f.type.is_int_type()),
        'fluents_real':     sum(1 for f in fl if f.type.is_real_type()),
        'fluents_array':    sum(1 for f in fl if _is_array(f.type)),
        'fluents_set':      sum(1 for f in fl if _is_set(f.type)),
        'fluents_bounded':  sum(1 for f in fl if _is_bounded(f.type)),
        'fluents_usertype': sum(1 for f in fl if _is_usertype(f.type)),
        'actions':          len(ac),
        'total_effects':    sum(len(a.effects) for a in ac),
        'cond_effects':     sum(sum(1 for e in a.effects if e.is_conditional()) for a in ac),
        'total_preconds':   sum(len(a.preconditions) for a in ac),
        'objects':          len(list(problem.all_objects)),
    }


def initialize_fluents(problem: Problem) -> None:
    env = get_environment()
    tm, em = env.type_manager, env.expression_manager
    problem.initial_defaults.update({
        tm.RealType(): em.Real(Fraction(0)),
        tm.IntType():  em.Int(0),
        tm.BoolType(): em.Bool(False),
    })
    initialized = set(problem.explicit_initial_values.keys())
    for fluent in problem.fluents:
        for fe in get_all_fluent_exp(problem, fluent):
            if fe not in initialized and fe.type in problem.initial_defaults:
                problem.set_initial_value(fe, problem.initial_defaults[fe.type])


def serialize(problem: Problem) -> Optional[bytes]:
    try:
        return ProtobufWriter().convert(problem).SerializeToString()
    except Exception as e:
        print(f"    [serialization failed: {e}]")
        return None


# ── Shared structural diff ────────────────────────────────────────────────────
def print_diff(a_prob: Problem, b_prob: Problem,
               a_label: str = "A", b_label: str = "B") -> None:
    def _sig(fl):
        return f"({', '.join(f'{p.name}:{p.type}' for p in fl.signature)}) -> {fl.type}"

    print(f"\n  {'── Fluents ':─<64}")
    a_f = {f.name: f for f in a_prob.fluents}
    b_f = {f.name: f for f in b_prob.fluents}
    any_diff = False
    for name in sorted(set(a_f) | set(b_f)):
        af, bf = a_f.get(name), b_f.get(name)
        if af is None:
            print(f"    + {name}{_sig(bf)}  (only in {b_label})"); any_diff = True
        elif bf is None:
            print(f"    - {name}{_sig(af)}  (only in {a_label})"); any_diff = True
        elif _sig(af) != _sig(bf):
            print(f"    ~ {name}")
            print(f"        {a_label}: {_sig(af)}")
            print(f"        {b_label}: {_sig(bf)}")
            any_diff = True
    if not any_diff:
        print("    (identical)")

    print(f"\n  {'── Actions ':─<64}")
    a_a = {a.name: a for a in a_prob.actions}
    b_a = {a.name: a for a in b_prob.actions}
    for name in sorted(set(a_a) | set(b_a)):
        aa, ba = a_a.get(name), b_a.get(name)
        if aa is None:
            print(f"    + [{name}]  (only in {b_label})")
        elif ba is None:
            print(f"    - [{name}]  (only in {a_label})")
        else:
            a_prec = [str(p) for p in aa.preconditions]
            b_prec = [str(p) for p in ba.preconditions]
            a_eff  = [(str(e.fluent), str(e.value)) for e in aa.effects]
            b_eff  = [(str(e.fluent), str(e.value)) for e in ba.effects]
            if a_prec == b_prec and a_eff == b_eff:
                print(f"    [{name}]  identical")
            else:
                print(f"    [{name}]  preconds {len(aa.preconditions)}→{len(ba.preconditions)}"
                      f"  effects {len(aa.effects)}→{len(ba.effects)}")
                added = [p for p in b_prec if p not in a_prec]
                for p in added[:5]:
                    print(f"      + prec: {p}")
                if len(added) > 5:
                    print(f"      ... ({len(added) - 5} more preconds)")

    print(f"\n  {'── Goals ':─<64}")
    ag = [str(g) for g in a_prob.goals]
    bg = [str(g) for g in b_prob.goals]
    if ag == bg:
        print("    (identical)")
    else:
        print(f"    {a_label} ({len(ag)}):")
        for g in ag: print(f"      {g}")
        print(f"    {b_label} ({len(bg)}):")
        for g in bg: print(f"      {g}")


# ── Shared pre-compile step (printed inline) ──────────────────────────────────
def run_precompile(base_problem: Problem, features: List[str]) -> Problem:
    print(f"\n{BAR}\n  Pre-compilation  (applied before both paths)\n{BAR}")
    try:
        base_problem, steps = precompile_features(base_problem, features)
        print(f"  Applied: {' → '.join(steps)}")
        s = problem_stats(base_problem)
        print(f"  Fluents after: {s['fluents_total']}  "
              f"(arrays={s['fluents_array']}, sets={s['fluents_set']}, "
              f"bounded={s['fluents_bounded']}, usertype={s['fluents_usertype']})")
    except Exception as e:
        print(f"  FAILED — {type(e).__name__}: {e}")
        print("  Continuing with original problem.")
    return base_problem


# ══════════════════════════════════════════════════════════════════════════════
#  PIPELINE subcommand
#  Path A: original → [UP pipeline − features] → RTP
#  Path B: original → [full UP pipeline]        → RTP
# ══════════════════════════════════════════════════════════════════════════════

_INLINE_ACTION_MAX = 8


def _problem_text(problem: Problem, steps: List[str], pb_size: Optional[int]) -> str:
    buf: List[str] = []

    def w(s: str = "") -> None:
        buf.append(s)

    fl = list(problem.fluents)
    ac = list(problem.actions)

    w(f"compilers: {' → '.join(steps) or '(none)'}")
    w(f"{len(ac)} actions, {len(fl)} fluents, "
      f"{len(list(problem.all_objects))} objects"
      + (f"  |  {pb_size} B" if pb_size else ""))
    w()
    w("fluents = [")
    for f in fl:
        sig = ("[" + ", ".join(f"{p.name}={p.type}" for p in f.signature) + "]"
               if f.signature else "")
        w(f"  {f.type} {f.name}{sig}")
    w("]")
    w()
    w("actions = [")
    for a in ac:
        params = ", ".join(f"{p.type} {p.name}" for p in a.parameters)
        if len(ac) <= _INLINE_ACTION_MAX:
            w(f"  action {a.name}({params}) {{")
            w("    preconditions = [")
            for prec in a.preconditions:
                w(f"      {prec}")
            w("    ]")
            w("    effects = [")
            for e in a.effects:
                cond = f"if {e.condition}: " if e.is_conditional() else ""
                w(f"      {cond}{e.fluent} := {e.value}")
            w("    ]")
            w("  }")
        else:
            w(f"  {a.name}({params})  [{len(a.preconditions)} prec, {len(a.effects)} eff]")
    w("]")
    w()
    w("goals = [")
    for g in problem.goals:
        w(f"  {g}")
    w("]")
    return "\n".join(buf)


def _solve_text(result: Optional[Dict], error: Optional[str] = None) -> str:
    if error:
        return f"ERROR — {error}"
    if result is None:
        return "SKIPPED"
    lines = [f"Status: {result['status']}"]
    plan = result.get('plan')
    if plan is not None:
        lines.append(f"Plan ({len(plan)} steps):")
        for i, step in enumerate(plan, 1):
            lines.append(f"  {i}.  {step}")
    elif result['plan_len'] is not None:
        lines.append(f"Plan: {result['plan_len']} steps")
    lines.append(f"Time: {result['time_s']:.2f}s")
    for msg in result.get('logs', []):
        lines.append(f"[log] {msg}")
    return "\n".join(lines)


def main_pipeline(args: argparse.Namespace) -> None:
    # ── Step 1: Parse ──────────────────────────────────────────────────────
    print(f"\n{BAR}\n  Step 1 — Parse\n{BAR}")

    if args.source and args.domain:
        print("Error: --source and -d/--domain are mutually exclusive.")
        sys.exit(1)
    if not args.source and not args.domain:
        print("Error: provide either --source FILE.py or -d domain.pddl [-p problem.pddl]")
        sys.exit(1)

    if args.source:
        print(f"  source:      {args.source}  (Python UP problem)")
        print(f"  feature:     {', '.join(args.feature) if args.feature else '(none)'}")
        base_problem = load_python(args.source)
    else:
        print(f"  domain:      {args.domain}")
        print(f"  problem:     {args.problem or '(none)'}")
        print(f"  feature:     {', '.join(args.feature) if args.feature else '(none)'}")
        base_problem = UPPDDLReader().parse_problem(args.domain, args.problem)

    # ── Resolve pipeline steps ─────────────────────────────────────────────
    # Path A = full pipeline MINUS the listed features (RTP handles those natively)
    # Path B = full pipeline (UP pre-compiles the features away before RTP)
    a_pipeline_steps: Optional[List[CompilationKind]] = None
    if args.pipeline:
        pipeline_label   = args.pipeline
        pipeline_steps   = list(PIPELINES[args.pipeline])
    elif args.feature:
        target_kinds     = {FEATURES[f][0] for f in args.feature}
        cascade_kinds    = set().union(*(_CASCADE_DROPS.get(f, set()) for f in args.feature))
        drop_kinds       = target_kinds | cascade_kinds
        default_steps    = list(PIPELINES[DEFAULT_PIPELINE])
        a_pipeline_steps = [s for s in default_steps if s not in drop_kinds]
        pipeline_steps   = default_steps
        pipeline_label   = DEFAULT_PIPELINE
    else:
        pipeline_label   = DEFAULT_PIPELINE
        pipeline_steps   = list(PIPELINES[DEFAULT_PIPELINE])

    if args.feature:
        also_dropped = cascade_kinds if args.feature else set()
        also_str     = (f"  [also drops: {', '.join(k.name for k in also_dropped)}]"
                        if also_dropped else "")
        a_label = f"UP(¬{','.join(args.feature)}) → RTP"
        print(f"  Path A:      {a_label}{also_str}")
    else:
        a_label = "RTP native"
        print(f"  Path A:      RanTanPlan native  (no UP pre-compilation)")
    b_label = f"UP({pipeline_label}) → RTP"
    print(f"  Path B:      {b_label}  →  "
          f"{' → '.join(k.name for k in pipeline_steps) or '(none)'}")
    if args.precompile:
        print(f"  precompile:  {', '.join(args.precompile)}")

    if args.precompile:
        base_problem = run_precompile(base_problem, args.precompile)

    # ── Layout ─────────────────────────────────────────────────────────────
    term_w = shutil.get_terminal_size((200, 50)).columns
    col_w  = max(50, (term_w - 4) // 2)
    full_w = col_w * 2 + 4

    # ── Run Path A ─────────────────────────────────────────────────────────
    a_problem = a_bytes = a_steps = None
    a_error: Optional[str] = None
    try:
        if a_pipeline_steps is not None:
            # Use direct compiler calls when features are skipped: the UP engine
            # selector rejects compilers running on out-of-order feature mixes
            # even though the compilers handle them correctly.
            a_problem, _, a_steps = run_research_pipeline(base_problem, a_pipeline_steps)
        else:
            a_problem, a_steps = run_native_pipeline(base_problem)
        initialize_fluents(a_problem)
        a_bytes = serialize(a_problem)
    except Exception as e:
        a_error = f"{type(e).__name__}: {e}"

    # ── Run Path B ─────────────────────────────────────────────────────────
    b_problem = b_bytes = b_steps = None
    b_error: Optional[str] = None
    try:
        b_problem, _, b_steps = run_up_pipeline(base_problem, pipeline_steps)
        initialize_fluents(b_problem)
        b_bytes = serialize(b_problem)
    except Exception as e:
        b_error = f"{type(e).__name__}: {e}"

    # ── Compilation side-by-side ───────────────────────────────────────────
    print(f"\n{'═' * full_w}")
    print(f"  COMPILATION")
    print(f"{'═' * full_w}\n")

    a_text = (_problem_text(a_problem, a_steps, len(a_bytes) if a_bytes else None)
              if a_problem is not None else f"FAILED — {a_error}")
    b_text = (_problem_text(b_problem, b_steps, len(b_bytes) if b_bytes else None)
              if b_problem is not None else f"FAILED — {b_error}")

    for line in _format_sidebyside(a_text, b_text, a_label, b_label, col_w):
        print(line)

    # ── Diff ───────────────────────────────────────────────────────────────
    if a_problem is None:
        print(f"\n  Path A failed: {a_error}")
    elif b_problem is None:
        print(f"\n  Path B failed: {b_error}")
    elif args.diff:
        print(f"\n{'═' * full_w}")
        print(f"  DIFF")
        print(f"{'═' * full_w}\n")
        print_diff(a_problem, b_problem, a_label, b_label)
    else:
        print(f"\n  (use --diff for structural action/fluent/goal diff)")

    # ── Solve ──────────────────────────────────────────────────────────────
    if args.run:
        print(f"\n{'═' * full_w}")
        print(f"  SOLVE")
        print(f"{'═' * full_w}\n")

        a_r = b_r = None
        a_err = b_err = None

        # Feature mode: Path A solves the partially UP-compiled problem so RTP only
        # handles the isolated features natively. Default mode: give original to RTP.
        a_solve_problem = a_problem if a_pipeline_steps is not None else base_problem

        if a_problem is not None:
            try:
                a_r = solve_rtp(a_solve_problem, verbosity='silent',
                                executable=args.executable or None,
                                array_frame_mode=args.array_frame_mode,
                                array_encoding=args.array_encoding)
            except Exception as e:
                a_err = str(e)
        else:
            a_err = f"Path A failed: {a_error}"

        if b_problem is not None:
            try:
                b_r = solve_rtp(b_problem, verbosity='silent',
                                executable=args.executable or None,
                                array_frame_mode=args.array_frame_mode,
                                array_encoding=args.array_encoding)
            except Exception as e:
                b_err = str(e)
        else:
            b_err = f"Path B failed: {b_error}"

        for line in _format_sidebyside(
            _solve_text(a_r, a_err),
            _solve_text(b_r, b_err),
            a_label, b_label, col_w,
        ):
            print(line)

        if a_r is not None and b_r is not None:
            status_match = a_r['status'] == b_r['status']
            print(f"\n  Status {'match' if status_match else 'DIFFER'}  "
                  f"(plan lengths not compared: different action spaces)")

    print()


# ══════════════════════════════════════════════════════════════════════════════
#  UP subcommand
#  Path A: original → RTP native pipeline → C++ backend
#  Path B: original → UP pipeline         → external planner
# ══════════════════════════════════════════════════════════════════════════════

_STAT_ROWS = [
    ("Fluents total",       'fluents_total'),
    ("  array",             'fluents_array'),
    ("  set",               'fluents_set'),
    ("  bounded",           'fluents_bounded'),
    ("  usertype",          'fluents_usertype'),
    ("  int",               'fluents_int'),
    ("  real",              'fluents_real'),
    ("  bool",              'fluents_bool'),
    ("Actions",             'actions'),
    ("Total effects",       'total_effects'),
    ("  conditional",       'cond_effects'),
    ("Total preconditions", 'total_preconds'),
    ("Objects",             'objects'),
]


def print_stats_table(nat: Dict, up: Dict, nat_pb: Optional[int], up_pb: Optional[int],
                      key_metric: str, key_label: str) -> None:
    print(f"\n  {'Metric':<26}  {'Native':>10}  {'UP':>10}  {'Diff':>8}")
    print(f"  {'-'*26}  {'-'*10}  {'-'*10}  {'-'*8}")
    for label, key in _STAT_ROWS:
        nv, uv = nat.get(key, 0), up.get(key, 0)
        diff = uv - nv
        ds   = f"+{diff}" if diff > 0 else (str(diff) if diff != 0 else "—")
        mark = "  ◄" if key == key_metric and diff != 0 else ""
        print(f"  {label:<26}  {nv:>10}  {uv:>10}  {ds:>8}{mark}")
    if nat_pb is not None and up_pb is not None:
        diff = up_pb - nat_pb
        ds   = f"+{diff}" if diff > 0 else (str(diff) if diff != 0 else "—")
        print(f"  {'Protobuf bytes':<26}  {nat_pb:>10}  {up_pb:>10}  {ds:>8}")
    print(f"\n  ◄ = {key_label}")


def run_up_planner(problem: Problem, solver: str, comp_results: List) -> Dict:
    get_environment().credits_stream = None
    with OneshotPlanner(name=solver) as planner:
        t0 = time.perf_counter()
        result = planner.solve(problem)
        elapsed = time.perf_counter() - t0
    plan = result.plan
    if plan is not None:
        for cr in reversed(comp_results):
            plan = plan.replace_action_instances(cr.map_back_action_instance)
    return {
        "status":   result.status.name,
        "plan_len": len(plan.actions) if plan else None,
        "time_s":   elapsed,
        "logs":     [m.message for m in (result.log_messages or [])],
    }


def main_up(args: argparse.Namespace) -> None:
    # ── Step 1: Parse ──────────────────────────────────────────────────────
    print(f"\n{BAR}\n  Step 1 — Parse\n{BAR}")
    print(f"  domain:     {args.domain}")
    print(f"  problem:    {args.problem or '(none)'}")
    print(f"  feature:    {args.feature or '(none)'}")

    base_problem = UPPDDLReader().parse_problem(args.domain, args.problem)

    # ── Resolve UP pipeline ────────────────────────────────────────────────
    # --feature removes one step from the UP pipeline so that feature stays
    # uncompiled on the UP side, isolating what UP's compiler contributes.
    if args.pipeline:
        pipeline_label = args.pipeline
        pipeline_steps = list(PIPELINES[args.pipeline])
        key_metric, key_label = 'fluents_total', 'total fluents'
    elif args.feature:
        ck_remove, key_metric, key_label = FEATURES[args.feature]
        default_steps = PIPELINES[DEFAULT_PIPELINE]
        if ck_remove not in default_steps:
            print(f"  Error: --feature '{args.feature}' maps to {ck_remove.name} "
                  f"which is not present in the default pipeline '{DEFAULT_PIPELINE}'. "
                  f"Use --pipeline to specify a pipeline manually.")
            sys.exit(1)
        pipeline_steps = [ck for ck in default_steps if ck != ck_remove]
        pipeline_label = f"{DEFAULT_PIPELINE} − {ck_remove.name}"
    else:
        pipeline_label = DEFAULT_PIPELINE
        pipeline_steps = list(PIPELINES[DEFAULT_PIPELINE])
        key_metric, key_label = 'fluents_total', 'total fluents'

    print(f"  pipeline:   {pipeline_label}  →  "
          f"{' → '.join(k.name for k in pipeline_steps) or '(none)'}")
    if args.precompile:
        print(f"  precompile: {', '.join(args.precompile)}")

    if args.precompile:
        base_problem = run_precompile(base_problem, args.precompile)

    # ── Step 2: Native pipeline ────────────────────────────────────────────
    print(f"\n{BAR}\n  Step 2 — Native pipeline  (→ C++ backend)\n{BAR}")
    nat_problem, nat_steps = run_native_pipeline(base_problem)
    print(f"  Pipeline: {' → '.join(nat_steps) or '(no steps applied)'}")
    initialize_fluents(nat_problem)
    nat_bytes = serialize(nat_problem)
    nat_stats = problem_stats(nat_problem)

    # ── Step 3: UP pipeline ────────────────────────────────────────────────
    print(f"\n{BAR}\n  Step 3 — UP pipeline  ({pipeline_label})\n{BAR}")
    up_problem = up_results = up_bytes = None
    up_error: Optional[str] = None
    try:
        up_problem, up_results, up_steps = run_up_pipeline(base_problem, pipeline_steps)
        print(f"  Pipeline: {' → '.join(up_steps) or '(no steps applied)'}")
        initialize_fluents(up_problem)
        up_bytes = serialize(up_problem)
    except Exception as e:
        up_error = f"{type(e).__name__}: {e}"
        print(f"  FAILED — {up_error}")

    # ── Step 4: Structural comparison ──────────────────────────────────────
    print(f"\n{BAR}\n  Step 4 — Structural comparison\n{BAR}")
    if up_problem is not None:
        print_stats_table(
            nat_stats, problem_stats(up_problem),
            len(nat_bytes) if nat_bytes else None,
            len(up_bytes)  if up_bytes  else None,
            key_metric, key_label,
        )
    else:
        print(f"  UP pipeline failed: {up_error}")

    # ── Step 5: Diff ───────────────────────────────────────────────────────
    if up_problem is None:
        print(f"\n  Step 5 — Diff skipped (UP pipeline failed)")
    elif args.diff:
        print(f"\n{BAR}\n  Step 5 — Structural diff\n{BAR}")
        print_diff(nat_problem, up_problem, "native", f"UP({pipeline_label})")
    else:
        print(f"\n  (use --diff for full structural diff)")

    # ── Step 6: Planner run ────────────────────────────────────────────────
    if args.run:
        print(f"\n{BAR}\n  Step 6 — Planner run\n{BAR}")
        _SOLVED = {'SOLVED_SATISFICING', 'SOLVED_OPTIMALLY'}

        nat_r = None
        try:
            nat_r = solve_rtp(base_problem, verbosity='silent',
                              executable=args.executable or None,
                              array_frame_mode=args.array_frame_mode,
                              array_encoding=args.array_encoding)
            print(f"  native:              status={nat_r['status']:<20}  "
                  f"plan_len={nat_r['plan_len']}  time={nat_r['time_s']:.2f}s")
            if nat_r['status'] not in _SOLVED and nat_r.get('logs'):
                for msg in nat_r['logs']:
                    print(f"    [log] {msg}")
        except Exception as e:
            print(f"  native:              ERROR — {e}")

        up_r = None
        if up_problem is not None:
            try:
                up_r = run_up_planner(up_problem, args.solver, up_results)
                print(f"  UP ({args.solver}):  status={up_r['status']:<20}  "
                      f"plan_len={up_r['plan_len']}  time={up_r['time_s']:.2f}s")
                if up_r['status'] not in _SOLVED and up_r.get('logs'):
                    for msg in up_r['logs']:
                        print(f"    [log] {msg}")
            except Exception as e:
                print(f"  UP ({args.solver}):  ERROR — {e}")
        else:
            print(f"  UP:                  SKIPPED — pipeline failed")

        if nat_r is not None and up_r is not None:
            match = nat_r['status'] == up_r['status'] and nat_r['plan_len'] == up_r['plan_len']
            print(f"\n  Results {'match' if match else 'DIFFER'}")

    print()


# ══════════════════════════════════════════════════════════════════════════════
#  ARRAY subcommand
#  Solves the SAME problem once per array-axiom config (encoding backend /
#  frame-axiom shape) and checks that RanTanPlan agrees with itself.
#  A status or plan-length mismatch signals a soundness bug in one of the
#  array encodings.
# ══════════════════════════════════════════════════════════════════════════════

_ARRAY_CONFIG_PRESETS = {
    'theory-disequality': dict(array_encoding='theory', array_frame_mode='disequality'),
    'theory-ite':         dict(array_encoding='theory', array_frame_mode='ite'),
    'uf-disequality':     dict(array_encoding='uf',      array_frame_mode='disequality'),
    'uf-ite':             dict(array_encoding='uf',      array_frame_mode='ite'),
}
_DEFAULT_ARRAY_CONFIGS = ['theory-disequality', 'theory-ite',
                          'uf-disequality', 'uf-ite']


def _parse_array_config(spec: str) -> Tuple[str, Dict]:
    """Parse one --configs entry into (label, solve_rtp kwargs).

    Accepts a preset name (see _ARRAY_CONFIG_PRESETS) or an explicit
    'encoding[:frame-mode]' spec, e.g. 'theory:ite' or 'uf:ite'.  Frame mode
    defaults to disequality and is meaningful under both encodings.
    """
    if spec in _ARRAY_CONFIG_PRESETS:
        return spec, dict(_ARRAY_CONFIG_PRESETS[spec])

    parts = spec.split(':')
    encoding = parts[0]
    frame_mode = parts[1] if len(parts) > 1 else 'disequality'
    if encoding not in ('theory', 'uf'):
        raise ValueError(f"unknown array-encoding '{encoding}' in --configs spec '{spec}' "
                         f"(valid: theory, uf)")
    if frame_mode not in ('disequality', 'ite'):
        raise ValueError(f"unknown array-frame-mode '{frame_mode}' in --configs spec '{spec}' "
                         f"(valid: disequality, ite)")
    label = f"{encoding}-{frame_mode}"
    return label, dict(array_encoding=encoding, array_frame_mode=frame_mode)


def _array_config_tag(kwargs: Dict) -> str:
    enc = kwargs['array_encoding']
    fm  = kwargs['array_frame_mode']
    return f"{enc}/{fm}"


def main_array(args: argparse.Namespace) -> None:
    print(f"\n{BAR}\n  Step 1 — Parse\n{BAR}")

    if args.source and args.domain:
        print("Error: --source and -d/--domain are mutually exclusive.")
        sys.exit(1)
    if not args.source and not args.domain:
        print("Error: provide either --source FILE.py or -d domain.pddl [-p problem.pddl]")
        sys.exit(1)

    if args.source:
        print(f"  source:  {args.source}  (Python UP problem)")
        base_problem = load_python(args.source)
    else:
        print(f"  domain:  {args.domain}")
        print(f"  problem: {args.problem or '(none)'}")
        base_problem = UPPDDLReader().parse_problem(args.domain, args.problem)

    try:
        configs = [_parse_array_config(spec) for spec in args.configs]
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)

    print(f"  configs: {', '.join(_array_config_tag(kw) for _, kw in configs)}")

    print(f"\n{BAR}\n  Step 2 — Solve under each config\n{BAR}")
    results: List[Tuple[str, Optional[Dict], Optional[str]]] = []
    for label, kwargs in configs:
        tag = _array_config_tag(kwargs)
        try:
            r = solve_rtp(base_problem, strategy=args.strategy, verbosity='silent',
                         timeout=args.timeout, up_compilers=args.up_compilers,
                         executable=args.executable or None, **kwargs)
            print(f"  [{tag:<20}]  status={r['status']:<20}  "
                  f"plan_len={str(r['plan_len']):<6}  time={r['time_s']:.2f}s")
            results.append((label, r, None))
        except Exception as e:
            err = f"{type(e).__name__}: {e}"
            print(f"  [{tag:<20}]  ERROR — {err}")
            results.append((label, None, err))

    print(f"\n{BAR}\n  Step 3 — Agreement\n{BAR}")
    ok = [r for _, r, err in results if r is not None]
    if len(ok) < 2:
        print("  Fewer than 2 configs solved successfully — nothing to compare.")
    else:
        statuses  = {r['status'] for r in ok}
        plan_lens = {r['plan_len'] for r in ok}
        if len(statuses) == 1 and len(plan_lens) == 1:
            print(f"  ALL MATCH — status={next(iter(statuses))}  "
                  f"plan_len={next(iter(plan_lens))}")
        else:
            print("  MISMATCH:")
            for label, r, err in results:
                if err:
                    print(f"    {label:<20}  ERROR — {err}")
                else:
                    print(f"    {label:<20}  status={r['status']:<20}  plan_len={r['plan_len']}")

    if args.diff:
        print(f"\n{BAR}\n  Plans\n{BAR}")
        for label, r, err in results:
            print(f"\n  [{label}]")
            print(f"  {_solve_text(r, err)}")

    print()


# ══════════════════════════════════════════════════════════════════════════════
#  Argument parsing
# ══════════════════════════════════════════════════════════════════════════════

def _add_common_args(p: argparse.ArgumentParser) -> None:
    """Args shared by both subcommands."""
    p.add_argument('--pipeline', choices=list(PIPELINES), default=None,
                   help="Override the UP pipeline entirely (ignores --feature).")
    p.add_argument('--precompile', nargs='+', choices=list(PRECOMPILE_KINDS),
                   default=None, metavar='FEATURE',
                   help=f"Pre-compile features before either path runs. "
                        f"Choices: {', '.join(PRECOMPILE_KINDS)}.")
    p.add_argument('--diff',       action='store_true', help="Show full structural diff.")
    p.add_argument('--run',        action='store_true', help="Run planners and compare results.")
    p.add_argument('--executable', default=None, help="RanTanPlan binary path.")
    p.add_argument('--array-frame-mode', default=None, choices=["disequality", "ite"],
                   help="Frame axiom shape for Z3 array/set fluents (--run only). "
                        "Applies under both --array-encoding backends.")
    p.add_argument('--array-encoding', default=None, choices=["theory", "uf"],
                   help="Z3 backend for array/set fluents: uf (uninterpreted functions, "
                        "default) or theory (Z3 Array sort). (--run only)")
    p.add_argument('--up-path',    default=os.path.expanduser('~/unified-planning'),
                   help="Path to unified-planning extension (default: ~/unified-planning).")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Compare RanTanPlan compilation paths.",
        epilog=(
            "Subcommands:\n"
            "  pipeline  Native RTP vs UP-pre-compiled, both solved by RanTanPlan.\n"
            "  up        Native RTP vs UP pipeline + external planner.\n\n"
            "Use 'compare.py <subcommand> --help' for subcommand options.\n"
            "Use 'compare.py --list-pipelines' to list available UP pipelines."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument('--list-pipelines', action='store_true',
                   help="Print available UP pipelines and exit.")
    p.add_argument('--up-path', default=os.path.expanduser('~/unified-planning'),
                   help="Path to unified-planning extension (default: ~/unified-planning).")

    sub = p.add_subparsers(dest='mode')

    # ── pipeline subcommand ────────────────────────────────────────────────
    sp = sub.add_parser(
        'pipeline',
        help="Native RTP vs UP-pre-compiled, both solved by RanTanPlan.",
        description=(
            "Path A: original → [UP pipeline − features] → RanTanPlan\n"
            "Path B: original → [full UP pipeline]        → RanTanPlan\n\n"
            "Answers: does RTP handle a feature natively as well as when UP pre-compiles it away?"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sp.add_argument('--source', metavar='FILE.py', default=None,
                    help="Python UP problem file (.py exposing get_problem()). "
                         "Mutually exclusive with -d/-p.")
    sp.add_argument('-d', '--domain',  default=None, help="PDDL domain file.")
    sp.add_argument('-p', '--problem', default=None, help="PDDL problem file.")
    sp.add_argument('--feature', nargs='+', choices=list(FEATURES), default=None,
                    metavar='FEATURE',
                    help=f"Feature(s) RTP should handle natively on Path A (UP skips those steps). "
                         f"Path B always uses the full pipeline. "
                         f"Choices: {', '.join(FEATURES)}.")
    _add_common_args(sp)

    # ── up subcommand ──────────────────────────────────────────────────────
    su = sub.add_parser(
        'up',
        help="Native RTP vs UP pipeline + external planner.",
        description=(
            "Path A (native):  original → RanTanPlan internal compilation → C++ backend\n"
            "Path B (UP):      original → UP compilation pipeline → external planner\n\n"
            "Answers: does RTP agree with an external planner?"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    su.add_argument('-d', '--domain',  required=True,  help="PDDL domain file.")
    su.add_argument('-p', '--problem', default=None,   help="PDDL problem file.")
    su.add_argument('--feature', choices=list(FEATURES), default=None,
                    help=f"Remove this feature's step from the UP pipeline to keep it "
                         f"uncompiled on the UP side. Choices: {', '.join(FEATURES)}.")
    su.add_argument('--solver', default='fast-downward',
                    help="UP-compatible solver for Path B (default: fast-downward).")
    _add_common_args(su)

    # ── array subcommand ───────────────────────────────────────────────────
    sa = sub.add_parser(
        'array',
        help="Compare array-axiom configs (encoding backend / frame-axiom shape).",
        description=(
            "Solves the SAME problem once per --configs entry, varying "
            "--array-encoding and --array-frame-mode, and checks that RanTanPlan "
            "agrees with itself across configs.\n\n"
            "A status or plan-length mismatch signals a soundness bug in one of "
            "the array-axiom encodings.\n\n"
            f"Presets: {', '.join(_ARRAY_CONFIG_PRESETS)}.\n"
            f"Default configs: {' '.join(_DEFAULT_ARRAY_CONFIGS)}."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sa.add_argument('--source', metavar='FILE.py', default=None,
                    help="Python UP problem file (.py exposing get_problem()). "
                         "Mutually exclusive with -d/-p.")
    sa.add_argument('-d', '--domain',  default=None, help="PDDL domain file.")
    sa.add_argument('-p', '--problem', default=None, help="PDDL problem file.")
    sa.add_argument('--configs', nargs='+', default=list(_DEFAULT_ARRAY_CONFIGS),
                    metavar='CONFIG',
                    help=(f"Configs to compare. Presets: {', '.join(_ARRAY_CONFIG_PRESETS)}. "
                          f"Or explicit 'encoding[:frame-mode]', e.g. 'theory:ite'. "
                          f"Default: {' '.join(_DEFAULT_ARRAY_CONFIGS)}."))
    sa.add_argument('--strategy', default='seq', help="RanTanPlan strategy (default: seq).")
    sa.add_argument('--timeout', type=float, default=None,
                    help="Per-config time limit in seconds (default: no limit).")
    sa.add_argument('--up-compilers', default='IPAR',
                    help="UP compiler list applied before solving each config (default: IPAR).")
    sa.add_argument('--executable', default=None, help="RanTanPlan binary path.")
    sa.add_argument('--diff', action='store_true',
                    help="Print the full plan produced under each config.")

    return p.parse_args()


# ══════════════════════════════════════════════════════════════════════════════
#  Entry point
# ══════════════════════════════════════════════════════════════════════════════

def main() -> None:
    args = parse_args()

    if args.list_pipelines:
        print("\nAvailable UP pipelines:")
        for name, kinds in PIPELINES.items():
            marker = "  (default)" if name == DEFAULT_PIPELINE else ""
            steps  = ' → '.join(k.name for k in kinds) or '(none)'
            print(f"  {name:<8}  {steps}{marker}")
        print()
        return

    if args.mode == 'pipeline':
        main_pipeline(args)
    elif args.mode == 'up':
        main_up(args)
    elif args.mode == 'array':
        main_array(args)
    else:
        parse_args()  # triggers help/error when no subcommand given
        print("Error: specify a subcommand: pipeline, up, or array")
        sys.exit(1)


if __name__ == "__main__":
    main()