#!/usr/bin/env python3
"""
solve.py — Unified solve entry point.

Accepts a problem from PDDL-XTS files or a Python file exposing a UP Problem.
Optionally applies a named UP compilation pipeline or an explicit compiler list.
Dispatches to any UP-registered solver, with full debug/trace support for RanTanPlan.

USAGE
-----
    # PDDL input (default source mode)
    python solve.py -d domain.pddl -p problem.pddl

    # Python UP Problem file
    python solve.py --source python my_problem.py --strategy seq-dt
    python solve.py --source my_problem.py              # shorthand

    # PDDL-XTS → full UP pre-compilation → any solver
    python solve.py -d domain.pddl -p problem.pddl --pipeline iasciu --solver FastDownward

    # PDDL-XTS → IPAR only → RanTanPlan native arrays/sets (default)
    python solve.py -d domain.pddl -p problem.pddl --pipeline ipar

    # Explicit UP compiler list (RanTanPlan native path only)
    python solve.py -d domain.pddl -p problem.pddl --up-compilers IPAR

    # Trace all pipeline layers (single source)
    python solve.py -d domain.pddl -p problem.pddl --trace

    # Solve both Python and PDDL in one run
    python solve.py --source both my_problem.py -d domain.pddl -p problem.pddl

    # Side-by-side layer comparison (both + trace)
    python solve.py --source both my_problem.py -d domain.pddl -p problem.pddl --trace
    python solve.py --source both my_problem.py -d domain.pddl -p problem.pddl --trace L1,L2

    # Dump protobuf to file
    python solve.py -d domain.pddl -p problem.pddl --dump-pb problem.pb

    # Anytime planning
    python solve.py -d domain.pddl -p problem.pddl --mode anytime

    # List available options
    python solve.py --list-pipelines
    python solve.py --list-solvers
    python solve.py --list-strategies

OPTIONS
    --source MODE [FILE.py]  Input source (default: pddl):
                               pddl              Use -d/-p (default)
                               python FILE.py    Python UP problem file
                               both   FILE.py    Load both; solve both, or compare layers with --trace
                               FILE.py           Shorthand for 'python FILE.py'
    -d/--domain             PDDL-XTS domain file
    -p/--problem            PDDL-XTS problem file

    --pipeline NAME         Named UP compilation pipeline (see --list-pipelines).
                            When omitted and solver=RantanPlan: native IPAR,QR,GR.
                            When omitted and solver=other: no pre-compilation.
    --up-compilers LIST     Explicit UP compiler list for the RanTanPlan native path.
                            Ignored when --pipeline is given.
                            Default when solver=RantanPlan: IPAR,QR,GR
                            (QR expands object-typed forall/exists; see docs/TODO.md).
                            Available: IPAR, QR, CNF, GR, BTR, NCR, DCR, CER

    --solver NAME           UP engine name (default: RantanPlan)
    --strategy NAME         RanTanPlan strategy (default: seq). See --list-strategies.
    --mode                  satisficing | optimal | anytime (default: satisficing)
    --timeout SECS          Time limit in seconds (default: 30)
    --verbosity             silent | info | verbose | debug (RanTanPlan only)

    --trace [LAYERS]        Comma-separated pipeline layers to display (RanTanPlan only).
                            Omit value to show all (L1,L2,L3,L4,L5).
                              L1  UP-parsed problem
                              L2  After UP-side compilation (pipeline or planner_wrapper)
                              L3  Protobuf encoding sent to C++ backend
                              L4  C++ problem dump after all passes
                              L5  SMT formula at the given horizon (--formula-timestep)
    --source both + --trace   Side-by-side layer view (Python left, PDDL right).
                            Use --trace [LAYERS] to select layers. Default: all layers.
    --formula-timestep N    Horizon to encode for --trace L5 (default: 1)
    --dump-pb FILE          Write binary protobuf problem to FILE (implies trace path)

    --list-pipelines        Print available pipeline names and exit
    --list-solvers          Print UP-registered solvers and exit
    --list-strategies       Print RanTanPlan strategies and exit

HOW THIS FILE IS ORGANIZED
--------------------------
    1. Imports & environment setup
    2. Small utilities          — CLI value parsing, printing, temp files
    3. Problem loading          — PDDL files / Python UP-problem files
    4. UP compilation pipeline  — apply_pipeline / map_plan_back
    5. Discovery commands       — the --list-* flags
    6. Solving                  — solve_normal (oneshot) and solve_anytime
    7. Traced solving           — solve_traced, prints pipeline layers L1–L5
    8. 'both' mode              — side-by-side layer comparison + parallel solve
    9. Programmatic API         — solve_rtp (used by the compare_/test_ scripts)
   10. CLI                      — parse_args, source-mode parsing, dispatch, main
"""

import argparse
import importlib.util
import io
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import traceback
import types
from concurrent.futures import ThreadPoolExecutor

# ─────────────────────────────────────────────────────────────────────────────
# 1. Imports & environment setup
# ─────────────────────────────────────────────────────────────────────────────

# Make the local unified-planning checkout and this repo importable.
# This file lives in xts/tools/, so the repo root is two levels up. _ROOT is
# what holds the base project (the rantanplan package and its built binary);
# _HERE is where the sibling extension tools live.
_UP_PATH = os.path.expanduser("~/unified-planning")
_HERE    = os.path.dirname(os.path.abspath(__file__))
_ROOT    = os.path.dirname(os.path.dirname(_HERE))
for _p in (_UP_PATH, _ROOT, _HERE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from unified_planning.engines import PlanGenerationResultStatus, CompilationKind
from unified_planning.shortcuts import get_environment, OneshotPlanner

# PDDL reader: prefer the XTS-aware reader, fall back to the plain UP one.
try:
    from unified_planning.io.pddl_reader import PDDLReader as _PDDLReader

    def _make_reader():
        return _PDDLReader(force_up_pddl_reader=False)
except ImportError:
    from unified_planning.io.up_pddl_reader import UPPDDLReader as _UPPDDLReader

    def _make_reader():
        return _UPPDDLReader()

# Named UP compilation pipelines (e.g. 'iasciu', 'ipar').
from docs.extensions.domains.compilation_solving import COMPILATION_PIPELINES

# Solver name aliases: user-facing names → UP engine registry names.
_SOLVER_ALIASES = {
    "fastdownward": "fast-downward",
    "fast_downward": "fast-downward",
}


def _resolve_solver_name(name: str) -> str:
    return _SOLVER_ALIASES.get(name.lower(), name)

# Importing rantanplan registers the RantanPlan engines in UP.
import rantanplan  # noqa: F401
from rantanplan.planner_wrapper import RantanPlanPlanner

get_environment().credits_stream = None

# Width of single-column section banners.
W = 68

_STATUS_LABEL = {
    PlanGenerationResultStatus.SOLVED_SATISFICING:       "SOLVED (satisficing)",
    PlanGenerationResultStatus.SOLVED_OPTIMALLY:         "SOLVED (optimal)",
    PlanGenerationResultStatus.UNSOLVABLE_PROVEN:        "UNSOLVABLE (proven)",
    PlanGenerationResultStatus.UNSOLVABLE_INCOMPLETELY:  "UNSOLVABLE (incomplete)",
    PlanGenerationResultStatus.TIMEOUT:                  "TIMEOUT",
    PlanGenerationResultStatus.MEMOUT:                   "MEMOUT",
    PlanGenerationResultStatus.INTERNAL_ERROR:           "INTERNAL ERROR",
    PlanGenerationResultStatus.INTERMEDIATE:             "INTERMEDIATE",
    PlanGenerationResultStatus.UNSUPPORTED_PROBLEM:      "UNSUPPORTED PROBLEM",
}

# Layer names accepted by --trace, in pipeline order.
_ALL_LAYERS = ['L1', 'L2', 'L3', 'L4', 'L5']


# ─────────────────────────────────────────────────────────────────────────────
# 2. Small utilities
# ─────────────────────────────────────────────────────────────────────────────

def _parse_up_compilers(s: str) -> list:
    """Convert an --up-compilers string to a list of uppercase compiler names."""
    if not s or s.strip().lower() == "none":
        return []
    return [c.strip().upper() for c in s.split(",") if c.strip()]


def _parse_trace_layers(s) -> set:
    """Convert a --trace value to a set of uppercase layer names, e.g. {'L1','L3'}."""
    if not s:
        return set()
    return {layer.strip().upper() for layer in s.split(",") if layer.strip()}


def _default_up_compilers(args) -> list:
    """Return the up_compilers list for the RanTanPlan native path."""
    if args.up_compilers:
        return _parse_up_compilers(args.up_compilers)
    # QR expands object-typed forall/exists Python-side: the C++ backend
    # currently mis-handles them (segfault / false UNSOLVABLE on ADL domains;
    # see docs/TODO.md). GR appended as before.
    return ["IPAR", "QR", "GR"]


def _section(title: str):
    """Print a full-width banner introducing a new output section."""
    print(f"\n{'═' * W}")
    print(f"  {title}")
    print(f"{'═' * W}\n")


def _problem_summary(problem) -> str:
    return (f"{len(list(problem.actions))} actions, "
            f"{len(list(problem.fluents))} fluents, "
            f"{len(list(problem.all_objects))} objects")


def _show_kind_flags(problem):
    print("\nProblemKind flags:")
    for line in str(problem.kind).splitlines():
        print(f"  {line}")


def print_plan(plan, problem):
    actions = list(plan.actions)
    if not actions:
        print("  (empty plan)")
        return
    width = len(str(len(actions)))
    for i, ai in enumerate(actions, 1):
        params = ", ".join(str(p) for p in ai.actual_parameters)
        call = f"{ai.action.name}({params})" if params else ai.action.name
        print(f"  {i:{width}}.  {call}")


def _tmp_path(suffix: str) -> str:
    """Create an empty named temp file and return its path (caller deletes)."""
    f = tempfile.NamedTemporaryFile(suffix=suffix, delete=False)
    f.close()
    return f.name


def _remove_silently(*paths):
    """Delete the given files, ignoring missing ones and OS errors."""
    for p in paths:
        if not p:
            continue
        try:
            os.remove(p)
        except OSError:
            pass


# ─────────────────────────────────────────────────────────────────────────────
# 3. Problem loading
# ─────────────────────────────────────────────────────────────────────────────

def load_pddl(domain: str, problem: str):
    """Parse a PDDL-XTS domain + problem into a UP Problem."""
    reader = _make_reader()
    return reader.parse_problem(domain, problem)


def load_python(filepath: str):
    """Import a Python file and return its UP Problem.

    The file must expose either a module-level ``problem`` variable or a
    ``get_problem()`` function that returns a unified_planning.model.Problem.
    """
    abs_path = os.path.abspath(filepath)
    spec = importlib.util.spec_from_file_location("_user_problem", abs_path)
    if spec is None or spec.loader is None:
        raise ValueError(f"Cannot import Python file: {abs_path}")
    module = importlib.util.module_from_spec(spec)
    file_dir = os.path.dirname(abs_path)
    if file_dir not in sys.path:
        sys.path.insert(0, file_dir)
    spec.loader.exec_module(module)  # type: ignore[union-attr]

    if hasattr(module, "get_problem") and callable(module.get_problem):
        return module.get_problem()
    if hasattr(module, "problem"):
        return module.problem
    raise ValueError(
        f"{filepath}: must expose a 'problem' variable or a 'get_problem()' function."
    )


def _load_or_exit(loader, error_prefix, *loader_args):
    """Run *loader*; on any exception print '<error_prefix>: <exc>' and exit."""
    try:
        return loader(*loader_args)
    except Exception as exc:
        print(f"{error_prefix}: {exc}")
        sys.exit(1)


def _has_xts_features(problem) -> bool:
    """Return True if the problem uses arrays, sets, or bounded-int parameters."""
    kind = problem.kind
    try:
        if kind.has_array_fluents() or kind.has_set_fluents():
            return True
    except AttributeError:
        pass
    try:
        if kind.has_bounded_int_action_parameters() or kind.has_bounded_int_fluent_parameters():
            return True
    except AttributeError:
        pass
    return False


# ─────────────────────────────────────────────────────────────────────────────
# 4. UP compilation pipeline
# ─────────────────────────────────────────────────────────────────────────────

def apply_pipeline(problem, pipeline_name: str):
    """Apply a named UP compilation pipeline. Returns (compiled_problem, results_list).

    Steps that have no suitable engine for the current problem kind are skipped
    with a note rather than failing the whole pipeline.
    """
    pipeline_steps = COMPILATION_PIPELINES.get(pipeline_name)
    if pipeline_steps is None:
        raise ValueError(
            f"Unknown pipeline '{pipeline_name}'. "
            f"Available: {', '.join(COMPILATION_PIPELINES)}"
        )

    from unified_planning.shortcuts import Compiler
    try:
        from unified_planning.exceptions import UPNoSuitableEngineAvailableException
    except ImportError:
        UPNoSuitableEngineAvailableException = Exception

    results = []
    current = problem
    for ck in pipeline_steps:
        params = {'mode': 'permissive'} if ck == CompilationKind.ARRAYS_REMOVING else {}
        try:
            with Compiler(problem_kind=current.kind, compilation_kind=ck, params=params) as c:
                if not c.supports(current.kind):
                    print(f"  [pipeline] {ck.name}: no engine for current problem kind, skipping.")
                    continue
                r = c.compile(current, ck)
                results.append(r)
                current = r.problem
        except UPNoSuitableEngineAvailableException:
            print(f"  [pipeline] {ck.name}: no suitable engine for current features, skipping.")
    return current, results


def map_plan_back(plan, compilation_results):
    """Walk the compilation chain in reverse and map action instances back."""
    for cr in reversed(compilation_results):
        plan = plan.replace_action_instances(cr.map_back_action_instance)
    return plan


# ─────────────────────────────────────────────────────────────────────────────
# 5. Discovery commands (--list-*)
# ─────────────────────────────────────────────────────────────────────────────

def list_pipelines():
    print("\nAvailable UP compilation pipelines (--pipeline NAME):\n")
    for name, steps in COMPILATION_PIPELINES.items():
        steps_str = " → ".join(s.name for s in steps) if steps else "(none)"
        print(f"  {name:<10}  {steps_str}")
    print()
    print("Special values:")
    print("  None / (omit)   No UP pre-compilation; use RanTanPlan native path.")
    print()


def list_solvers(problem=None):
    """List UP-registered solvers, optionally annotated with problem compatibility."""
    env = get_environment()
    all_names = sorted(env.factory.engines)

    compatible = set()
    if problem is not None:
        try:
            compatible = set(env.factory.get_all_applicable_engines(problem.kind))
        except Exception:
            pass

    print("\nAvailable UP-registered solvers (--solver NAME):\n")
    for name in all_names:
        if compatible:
            tag = "  ✓" if name in compatible else "   "
        else:
            tag = ""
        print(f"  {name}{tag}")
    if compatible:
        print("\n  ✓ = compatible with this problem's features")
    print()


def list_strategies():
    """Ask the RanTanPlan executable for its strategy list."""
    candidates = [
        os.path.join(_ROOT, "rantanplan", "bin", "rantanplan"),
        os.path.join(_ROOT, "rantanplan", "cpp", "build", "rantanplan"),
    ]
    exe = next((c for c in candidates if os.path.isfile(c)), "rantanplan")
    try:
        result = subprocess.run(
            [exe, "/dev/null", "/dev/null", "--list-strategies"],
            capture_output=True, text=True,
        )
        print(result.stdout or result.stderr)
    except FileNotFoundError:
        print(f"Could not find the RanTanPlan executable (tried: {candidates}).")
        sys.exit(1)


# ─────────────────────────────────────────────────────────────────────────────
# 6. Solving — normal (satisficing / optimal) and anytime
# ─────────────────────────────────────────────────────────────────────────────

def _make_rtp_planner(args, up_compilers_list):
    """Build a RantanPlanPlanner from CLI args and an explicit compiler list.

    'anytime' mode never reaches here (solve_anytime uses the UP AnytimePlanner
    engine); 'satisficing' is the backend default, so only 'optimal' is passed.
    """
    opts = {"strategy": args.strategy, "up_compilers": up_compilers_list}
    if args.mode == "optimal":
        opts["mode"] = "optimal"  # forwarded to the C++ backend as --mode optimal
    if args.verbosity:
        opts["verbosity"] = args.verbosity
    if args.array_frame_mode:
        opts["array_frame_mode"] = args.array_frame_mode
    if args.array_encoding:
        opts["array_encoding"] = args.array_encoding
    return RantanPlanPlanner(**opts)


def _print_status_and_plan(result, original_problem, compilation_results, elapsed):
    label = _STATUS_LABEL.get(result.status, str(result.status))
    print(f"\nStatus:  {label}  [solving: {elapsed:.2f}s]")
    if result.log_messages:
        for msg in result.log_messages:
            print(f"  [{msg.level.name}] {msg.message}")
    if result.plan is not None:
        plan = map_plan_back(result.plan, compilation_results) if compilation_results else result.plan
        print(f"\nPlan ({len(list(plan.actions))} steps):")
        print_plan(plan, original_problem)


def _run_cpp_backend(planner, problem_filepath, solution_filepath, timeout):
    """Run the C++ planner subprocess, streaming output in real time.

    Returns (return_code, elapsed_s).  return_code is None on timeout (process killed).
    """
    command = planner._build_command(problem_filepath, solution_filepath, timeout)
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        universal_newlines=False,
    )
    stdout_thread = threading.Thread(target=planner._stream_output, args=(process.stdout, ""))
    stderr_thread = threading.Thread(target=planner._stream_output, args=(process.stderr, "[ERR] "))
    stdout_thread.daemon = stderr_thread.daemon = True
    stdout_thread.start()
    stderr_thread.start()

    t0 = time.perf_counter()
    try:
        return_code = process.wait(timeout=(timeout + 10) if timeout else None)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout_thread.join(timeout=1.0)
        stderr_thread.join(timeout=1.0)
        return None, time.perf_counter() - t0

    stdout_thread.join(timeout=1.0)
    stderr_thread.join(timeout=1.0)
    return return_code, time.perf_counter() - t0


def solve_normal(problem, compiled_problem, compilation_results, args):
    """Satisficing or optimal solve (single result).

    If a pipeline already compiled the problem (compiled_problem is not None),
    solve that and tell the RanTanPlan wrapper to skip its own UP-side
    compilation; the plan is mapped back through compilation_results.
    """
    is_rtp = args.solver.lower().startswith("rantanplan")

    if is_rtp:
        if compiled_problem is not None:
            # Pipeline already compiled; planner only does protobuf + C++ solve.
            target, up_compilers, results = compiled_problem, [], compilation_results
            planner = _make_rtp_planner(args, up_compilers)
            t0 = time.perf_counter()
            result = planner._solve(target, timeout=args.timeout)
            _print_status_and_plan(result, problem, results, time.perf_counter() - t0)
        else:
            # Native path: time UP-side compilation and C++ solving separately.
            target, results = problem, []
            planner = _make_rtp_planner(args, _default_up_compilers(args))
            t_compile = time.perf_counter()
            try:
                compiled, cr, prob_path, sol_path = planner._prepare_and_serialize(target)
            except Exception as exc:
                compile_elapsed = time.perf_counter() - t_compile
                print(f"\nCompilation error: {type(exc).__name__}: {exc}")
                print(f"Compilation:  {compile_elapsed:.2f}s")
                sys.exit(1)
            compile_elapsed = time.perf_counter() - t_compile
            print(f"Compilation:  {compile_elapsed:.2f}s")
            try:
                return_code, solve_elapsed = _run_cpp_backend(
                    planner, prob_path, sol_path, args.timeout)
                if return_code is None:
                    print("\n  Planner timed out.")
                    return
                if return_code != 0:
                    msg = f"\n  Planner failed (return code {return_code}"
                    if return_code == -11:
                        msg += ", SIGSEGV"
                    elif return_code == -6:
                        msg += ", SIGABRT"
                    print(msg + ").")
                    print(f"[solving: {solve_elapsed:.2f}s]")
                    return
                result = planner._read_protobuf_result(sol_path, compiled, cr, target)
                _print_status_and_plan(result, problem, results, solve_elapsed)
            finally:
                _remove_silently(prob_path, sol_path)

    else:
        target = compiled_problem if compiled_problem is not None else problem
        engine = "RantanPlan-optimal" if args.mode == "optimal" else _resolve_solver_name(args.solver)
        try:
            with OneshotPlanner(name=engine) as planner:
                t0 = time.perf_counter()
                result = planner.solve(target, timeout=args.timeout)
        except Exception as exc:
            print(f"\nSolver error: {type(exc).__name__}: {exc}")
            sys.exit(1)
        _print_status_and_plan(result, problem, compilation_results, time.perf_counter() - t0)


def solve_anytime(problem, compiled_problem, compilation_results, args):
    """Anytime planning — streams successive improving plans."""
    from unified_planning.shortcuts import AnytimePlanner

    is_rtp = args.solver.lower().startswith("rantanplan")
    solver_name = "RantanPlan-anytime" if is_rtp else _resolve_solver_name(args.solver)
    target = compiled_problem if compiled_problem is not None else problem

    params = {}
    if is_rtp:
        params = {
            "strategy": args.strategy,
            "up_compilers": [] if compiled_problem is not None else _default_up_compilers(args),
        }
        if args.verbosity:
            params["verbosity"] = args.verbosity
        if args.array_frame_mode:
            params["array_frame_mode"] = args.array_frame_mode
        if args.array_encoding:
            params["array_encoding"] = args.array_encoding

    t0 = time.perf_counter()
    best_plan = None
    try:
        with AnytimePlanner(name=solver_name, params=params or None) as planner:
            for result in planner.get_solutions(target, timeout=args.timeout):
                elapsed = time.perf_counter() - t0
                label = _STATUS_LABEL.get(result.status, str(result.status))
                if result.plan is not None:
                    plan = map_plan_back(result.plan, compilation_results) if compilation_results else result.plan
                    print(f"  [{label}]  {len(list(plan.actions))} steps  ({elapsed:.2f}s)")
                    best_plan = (plan, result)
                else:
                    print(f"  [{label}]  ({elapsed:.2f}s)")
    except Exception as exc:
        print(f"\nSolver error: {type(exc).__name__}: {exc}")
        return

    total_elapsed = time.perf_counter() - t0
    if best_plan is not None:
        plan, _ = best_plan
        print(f"\nBest plan ({len(list(plan.actions))} steps)  [total solving: {total_elapsed:.2f}s]:")
        print_plan(plan, problem)
    else:
        print(f"\nNo plan found within the time limit.  [{total_elapsed:.2f}s]")


# ─────────────────────────────────────────────────────────────────────────────
# 7. Traced solving (RanTanPlan only — exposes layers L1–L5)
#
#    solve_traced  — prints the requested layers, then solves.
#    _collect_layers — same layer extraction, but returns text instead of
#                      printing (used by the 'both'-mode side-by-side view).
#    Both share the backend helpers below.
# ─────────────────────────────────────────────────────────────────────────────

def _run_internal_compilation(planner, problem):
    """Run planner_wrapper's validation + UP-side compilation for *problem*.

    Returns (inner_compiled, inner_result) as produced by _compile_problem.
    """
    planner._check_no_nested_fluents(problem)
    planner._initialize_fluents(problem)
    return planner._compile_problem(problem)


def _export_formula(base_command, timestep):
    """Ask the C++ backend for the SMT formula at *timestep* (layer L5).

    Returns (returncode, formula_text, stderr); formula_text is None on failure.
    """
    formula_path = _tmp_path(".smt2")
    try:
        proc = subprocess.run(
            base_command + [
                "--export-formula",
                "--formula-timestep", str(timestep),
                "--formula-output", formula_path,
            ],
            capture_output=True, text=True,
        )
        if proc.returncode == 0 and os.path.exists(formula_path):
            with open(formula_path) as f:
                return proc.returncode, f.read(), proc.stderr
        return proc.returncode, None, proc.stderr
    finally:
        _remove_silently(formula_path)


def solve_traced(original_problem, compiled_problem, compilation_results, args, trace_layers):
    """Solve with RanTanPlan while printing the requested pipeline layers.

    original_problem    — problem as loaded from PDDL/Python (shown at L1)
    compiled_problem    — result of --pipeline (or None for the native path)
    compilation_results — from apply_pipeline (used to map the plan back)

    When compiled_problem is not None:
      L2 shows the pipeline-compiled problem; planner_wrapper gets up_compilers=[].
    When compiled_problem is None:
      L2 shows what planner_wrapper's internal compilation produces.
    """
    work_problem      = compiled_problem if compiled_problem is not None else original_problem
    up_compilers_list = [] if compiled_problem is not None else _default_up_compilers(args)
    planner = _make_rtp_planner(args, up_compilers_list)

    # ── L1: Original UP-parsed problem ───────────────────────────────────────
    if 'L1' in trace_layers:
        _section("LAYER 1: UP PARSED")
        print(f"  → '{original_problem.name}'  ({_problem_summary(original_problem)})")
        print()
        print(original_problem)
        _show_kind_flags(original_problem)

    # ── L2 header (printed before compilation so any skipped-step warnings fall inside) ──
    if 'L2' in trace_layers:
        if compiled_problem is not None:
            _section(f"LAYER 2: PIPELINE  ({args.pipeline})")
        else:
            compilers_label = ",".join(up_compilers_list) if up_compilers_list else "none"
            _section(f"LAYER 2: UP COMPILATION  ({compilers_label})")

    # Run planner_wrapper's internal compilation
    try:
        inner_compiled, inner_result = _run_internal_compilation(planner, work_problem)
    except Exception as exc:
        print(f"  Error during compilation: {exc}")
        traceback.print_exc()
        return None

    # ── L2 body ──────────────────────────────────────────────────────────────
    if 'L2' in trace_layers:
        if compiled_problem is not None:
            # Pipeline was applied externally — show it
            print(f"  Problem size : {_problem_summary(compiled_problem)}")
            print()
            print(compiled_problem)
            _show_kind_flags(compiled_problem)
        else:
            steps = getattr(inner_result, "step_names", [])
            if steps:
                print(f"  Steps applied : {' → '.join(steps)}")
            else:
                print("  (no UP-side compilation steps applied)")
            print(f"  Problem size  : {_problem_summary(inner_compiled)}")
            print()
            print(inner_compiled)
            _show_kind_flags(inner_compiled)

    # ── L3: Protobuf encoding ────────────────────────────────────────────────
    if 'L3' in trace_layers:
        _section("LAYER 3: PROTOBUF ENCODING")

    pb_problem_msg = planner._pb_writer.convert(inner_compiled)

    if 'L3' in trace_layers:
        for field, label in [("actions", "Actions"), ("fluents", "Fluents"), ("objects", "Objects")]:
            try:
                print(f"  {label:<10}: {len(getattr(pb_problem_msg, field))}")
            except AttributeError:
                pass
        try:
            from google.protobuf import text_format as _pb_tf
            print()
            print(_pb_tf.MessageToString(pb_problem_msg))
        except Exception as exc:
            print(f"\n  (protobuf text format unavailable: {exc})")

    if args.dump_pb:
        with open(args.dump_pb, "wb") as f:
            f.write(pb_problem_msg.SerializeToString())
        print(f"\n  Binary proto written to: {args.dump_pb}")

    # ── Write protobuf to temp files, then invoke the C++ backend ────────────
    problem_filepath  = _tmp_path(".pb")
    solution_filepath = _tmp_path(".pb")

    try:
        with open(problem_filepath, "wb") as f:
            f.write(pb_problem_msg.SerializeToString())

        base_command = planner._build_command(problem_filepath, solution_filepath, args.timeout)

        # ── L4: C++ problem dump (after internal passes) ─────────────────────
        if 'L4' in trace_layers:
            _section("LAYER 4: C++ PROBLEM DUMP  (after pipeline passes)")
            proc = subprocess.run(base_command + ["--dump-problem"], capture_output=True, text=True)
            if proc.stdout:
                print(proc.stdout)
            if proc.stderr:
                print(proc.stderr)

        # ── L5: SMT formula ───────────────────────────────────────────────────
        if 'L5' in trace_layers:
            timestep = getattr(args, 'formula_timestep', 1)
            _section(f"LAYER 5: SMT FORMULA  (horizon={timestep})")
            rc, formula, stderr = _export_formula(base_command, timestep)
            if formula is not None:
                print(formula)
            else:
                print(f"  Formula export failed (return code {rc}).")
                if stderr:
                    print(stderr)

        # ── Solve ─────────────────────────────────────────────────────────────
        _section(f"SOLVING  [strategy={args.strategy}  timeout={args.timeout}s]")

        process = subprocess.Popen(
            base_command,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            universal_newlines=False,
        )
        stdout_thread = threading.Thread(target=planner._stream_output, args=(process.stdout, ""))
        stderr_thread = threading.Thread(target=planner._stream_output, args=(process.stderr, "[ERR] "))
        stdout_thread.daemon = stderr_thread.daemon = True
        stdout_thread.start()
        stderr_thread.start()

        try:
            return_code = process.wait(timeout=(args.timeout + 10) if args.timeout else None)
        except subprocess.TimeoutExpired:
            process.kill()
            print("\n  Planner timed out.")
            return None

        stdout_thread.join(timeout=1.0)
        stderr_thread.join(timeout=1.0)

        if return_code != 0:
            msg = f"\n  Planner failed (return code {return_code}"
            if return_code == -11:
                msg += ", SIGSEGV"
            elif return_code == -6:
                msg += ", SIGABRT"
            print(msg + ").")
            return None

        # Map back: planner_wrapper's internal compilation, then external pipeline.
        result = planner._read_protobuf_result(
            solution_filepath, inner_compiled, inner_result, work_problem)

        label = _STATUS_LABEL.get(result.status, str(result.status))
        print(f"\n  Status: {label}")
        if result.log_messages:
            for msg in result.log_messages:
                print(f"  [{msg.level.name}] {msg.message}")

        if result.plan is not None:
            plan = map_plan_back(result.plan, compilation_results) if compilation_results else result.plan
            print(f"\n  Plan ({len(list(plan.actions))} steps):")
            print_plan(plan, original_problem)

        return result

    finally:
        _remove_silently(problem_filepath, solution_filepath)


# ─────────────────────────────────────────────────────────────────────────────
# 8. 'both' mode — side-by-side layer comparison + parallel solve
# ─────────────────────────────────────────────────────────────────────────────

def _problem_block(problem, *header_lines) -> list:
    """Render *header_lines*, the problem, and its kind flags as text lines."""
    lines = list(header_lines) + [""]
    lines += str(problem).splitlines()
    lines += ["", "ProblemKind:"]
    lines += ["  " + l for l in str(problem.kind).splitlines()]
    return lines


def _collect_layers(problem, compiled_problem, compilation_results, args, layers):
    """Capture compilation layer text for *problem* into a dict {layer: str}.

    Does not print anything.  Mirrors solve_traced but returns strings instead,
    and stops as soon as no deeper layer was requested.
    """
    out = {}
    work_problem      = compiled_problem if compiled_problem is not None else problem
    up_compilers_list = [] if compiled_problem is not None else _default_up_compilers(args)
    planner = _make_rtp_planner(args, up_compilers_list)

    # ── L1 ───────────────────────────────────────────────────────────────────
    if 'L1' in layers:
        header = f"{problem.name}  ({_problem_summary(problem)})"
        out['L1'] = '\n'.join(_problem_block(problem, header))

    if not (layers & {'L2', 'L3', 'L4', 'L5'}):
        return out

    # ── Compile ───────────────────────────────────────────────────────────────
    try:
        inner_compiled, inner_result = _run_internal_compilation(planner, work_problem)
    except Exception as exc:
        for layer in ['L2', 'L3', 'L4', 'L5']:
            if layer in layers:
                out[layer] = f"[compilation error]\n{exc}"
        return out

    # ── L2 ───────────────────────────────────────────────────────────────────
    if 'L2' in layers:
        if compiled_problem is not None:
            pl = getattr(args, 'pipeline', '?') or '?'
            lines = _problem_block(compiled_problem,
                                   f"(pipeline: {pl})",
                                   _problem_summary(compiled_problem))
        else:
            steps = getattr(inner_result, "step_names", [])
            hdr = ' → '.join(steps) if steps else "(none)"
            lines = _problem_block(inner_compiled,
                                   f"compilers: {hdr}",
                                   _problem_summary(inner_compiled))
        out['L2'] = '\n'.join(lines)

    if not (layers & {'L3', 'L4', 'L5'}):
        return out

    # ── Protobuf ─────────────────────────────────────────────────────────────
    pb_msg = planner._pb_writer.convert(inner_compiled)

    if 'L3' in layers:
        try:
            from google.protobuf import text_format as _pb_tf
            out['L3'] = _pb_tf.MessageToString(pb_msg)
        except Exception as exc:
            out['L3'] = f"[protobuf text unavailable]\n{exc}"

    if not (layers & {'L4', 'L5'}):
        return out

    # ── Binary invocation (L4 / L5) ──────────────────────────────────────────
    prob_path = _tmp_path(".pb")
    sol_path  = _tmp_path(".pb")

    try:
        with open(prob_path, "wb") as f:
            f.write(pb_msg.SerializeToString())
        base_cmd = planner._build_command(prob_path, sol_path, args.timeout)

        if 'L4' in layers:
            proc = subprocess.run(base_cmd + ["--dump-problem"], capture_output=True, text=True)
            out['L4'] = (proc.stdout + proc.stderr).strip()

        if 'L5' in layers:
            timestep = getattr(args, 'formula_timestep', 1)
            rc, formula, stderr = _export_formula(base_cmd, timestep)
            if formula is not None:
                out['L5'] = formula.strip()
            else:
                out['L5'] = f"[formula export failed (rc={rc})]\n" + stderr.strip()
    finally:
        _remove_silently(prob_path, sol_path)

    return out


def _format_sidebyside(left_text, right_text, left_header, right_header, col_w):
    """Zip two text blocks into a two-column layout.  Returns a list of lines."""
    SEP = " │ "

    def _wrap(text, w):
        wrapped = []
        for line in (text or "").splitlines():
            if len(line) <= w:
                wrapped.append(line)
            else:
                while len(line) > w:
                    wrapped.append(line[:w])
                    line = "  " + line[w:]
                wrapped.append(line)
        return wrapped

    left_lines  = _wrap(left_text,  col_w)
    right_lines = _wrap(right_text, col_w)
    n = max(len(left_lines), len(right_lines))

    result = [
        f" {left_header:<{col_w}}{SEP}{right_header}",
        f" {'─' * col_w}{SEP}{'─' * col_w}",
    ]
    for i in range(n):
        l = left_lines[i]  if i < len(left_lines)  else ""
        r = right_lines[i] if i < len(right_lines) else ""
        result.append(f" {l:<{col_w}}{SEP}{r}")
    return result


def _two_column_width():
    """Return (col_w, full_w) for the side-by-side views, from the terminal size."""
    term_w = shutil.get_terminal_size((200, 50)).columns
    col_w  = max(50, (term_w - 4) // 2)   # 4 = 1 space indent + 3 for " │ "
    return col_w, col_w * 2 + 4


def compare_layers(py_problem, pddl_problem, py_file, pddl_file, args, layers):
    """Print compilation layers for the Python and PDDL problems side by side."""
    col_w, full_w = _two_column_width()

    py_compiled   = None; py_cr   = []
    pddl_compiled = None; pddl_cr = []

    pipeline = getattr(args, 'pipeline', None)
    if pipeline and pipeline.lower() not in ('', 'none'):
        py_compiled,   py_cr   = apply_pipeline(py_problem,   pipeline)
        pddl_compiled, pddl_cr = apply_pipeline(pddl_problem, pipeline)

    print(f"Collecting layers for Python ({os.path.basename(py_file)}) …")
    print(f"Collecting layers for PDDL   ({os.path.basename(pddl_file)}) …\n")
    with ThreadPoolExecutor(max_workers=2) as ex:
        py_fut   = ex.submit(_collect_layers, py_problem,   py_compiled,   py_cr,   args, layers)
        pddl_fut = ex.submit(_collect_layers, pddl_problem, pddl_compiled, pddl_cr, args, layers)
        py_layers   = py_fut.result()
        pddl_layers = pddl_fut.result()

    py_label   = f"PY   {os.path.basename(py_file)}"
    pddl_label = f"PDDL {os.path.basename(pddl_file)}"

    layer_titles = {
        'L1': 'LAYER 1 — UP PARSED',
        'L2': 'LAYER 2 — COMPILATION',
        'L3': 'LAYER 3 — PROTOBUF ENCODING',
        'L4': 'LAYER 4 — C++ PROBLEM DUMP',
        'L5': 'LAYER 5 — SMT FORMULA',
    }

    for layer in _ALL_LAYERS:
        if layer not in layers:
            continue
        print(f"\n{'═' * full_w}")
        print(f"  {layer_titles[layer]}")
        print(f"{'═' * full_w}\n")
        left  = py_layers.get(layer,   "[not available]")
        right = pddl_layers.get(layer, "[not available]")
        for line in _format_sidebyside(left, right, py_label, pddl_label, col_w):
            print(line)


class _CapturingStdout:
    """Routes sys.stdout writes to per-thread StringIO buffers.

    Threads that set ``self._local.buf`` get their output captured; everything
    else passes through to the real stdout.
    """

    def __init__(self, real):
        self._real  = real
        self._local = threading.local()

    def _target(self):
        return getattr(self._local, 'buf', None) or self._real

    def write(self, s):
        self._target().write(s)

    def flush(self):
        self._target().flush()

    def fileno(self):
        return self._real.fileno()

    def isatty(self):
        return self._real.isatty()

    def __getattr__(self, n):
        return getattr(self._real, n)


def _parallel_solve(py_problem, pddl_problem, py_name, pddl_name, args):
    """Solve both problems in parallel; print captured output side by side."""
    cap = _CapturingStdout(sys.stdout)
    sys.stdout = cap
    try:
        def _run(problem):
            cap._local.buf = io.StringIO()
            _solve_one(problem, args)
            return cap._local.buf.getvalue()

        with ThreadPoolExecutor(max_workers=2) as ex:
            py_fut   = ex.submit(_run, py_problem)
            pddl_fut = ex.submit(_run, pddl_problem)
            py_out   = py_fut.result()
            pddl_out = pddl_fut.result()
    finally:
        sys.stdout = cap._real

    col_w, full_w = _two_column_width()
    print(f"\n{'═' * full_w}")
    print(f"  SOLVE")
    print(f"{'═' * full_w}\n")
    for line in _format_sidebyside(py_out, pddl_out,
                                   f"PY   {py_name}", f"PDDL {pddl_name}", col_w):
        print(line)


# ─────────────────────────────────────────────────────────────────────────────
# 9. Programmatic API
# ─────────────────────────────────────────────────────────────────────────────

def solve_rtp(problem, *, strategy='seq', verbosity=None, timeout=None,
              up_compilers='IPAR', executable=None,
              array_frame_mode=None, array_encoding=None):
    """Solve *problem* with RanTanPlan and return a result dict.

    Returns:
        dict with keys:
            status   (str)           — PlanGenerationResultStatus name
            plan_len (int | None)    — number of actions, or None if no plan
            plan     (list | None)   — action strings, or None if no plan
            time_s   (float)         — wall-clock seconds
            logs     (list[str])     — planner log messages
    """
    params = {
        'strategy':     strategy,
        'up_compilers': _parse_up_compilers(up_compilers),
    }
    if verbosity is not None:
        params['verbosity'] = verbosity
    if executable is not None:
        params['executable_path'] = executable
    if array_frame_mode is not None:
        params['array_frame_mode'] = array_frame_mode
    if array_encoding is not None:
        params['array_encoding'] = array_encoding

    t0 = time.perf_counter()
    with OneshotPlanner(name='RantanPlan', params=params) as planner:
        result = planner.solve(problem, timeout=timeout)
    return {
        'status':   result.status.name,
        'plan_len': len(result.plan.actions) if result.plan else None,
        'plan':     [str(a) for a in result.plan.actions] if result.plan else None,
        'time_s':   time.perf_counter() - t0,
        'logs':     [m.message for m in (result.log_messages or [])],
    }


# ─────────────────────────────────────────────────────────────────────────────
# 10. CLI — argument parsing, source-mode parsing, dispatch, main
# ─────────────────────────────────────────────────────────────────────────────

def parse_args():
    p = argparse.ArgumentParser(
        description="Unified solve entry point: PDDL-XTS or Python UP Problem → any UP solver.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )

    # Input source
    p.add_argument("--source", nargs="+", default=None, metavar="MODE_OR_FILE",
                   help=("Input source. "
                         "'pddl' (default): use -d/-p. "
                         "'python FILE.py': Python UP problem file. "
                         "'both FILE.py': solve both Python and PDDL (use with --trace "
                         "for side-by-side layer comparison). "
                         "Shorthand: --source FILE.py implies python mode."))
    p.add_argument("-d", "--domain",  default=None, help="PDDL-XTS domain file.")
    p.add_argument("-p", "--problem", default=None, help="PDDL-XTS problem file.")

    # Pipeline
    p.add_argument("--pipeline", default=None, metavar="NAME",
                   help=("Named UP compilation pipeline (see --list-pipelines). "
                         "When omitted and solver=RantanPlan: native IPAR only. "
                         "When omitted and solver=other: no pre-compilation."))
    p.add_argument("--up-compilers", default=None, metavar="LIST",
                   help=("Explicit UP compiler list for the RanTanPlan native path. "
                         "Ignored when --pipeline is given. "
                         "Default: IPAR. "
                         "Available: IPAR, QR, CNF, GR, BTR, NCR, DCR, CER"))

    # Solver
    p.add_argument("--solver", default="RantanPlan",
                   help="UP engine name (default: RantanPlan).")
    p.add_argument("--strategy", default="seq",
                   help="RanTanPlan strategy (default: seq). See --list-strategies.")
    p.add_argument("--mode", default="satisficing",
                   choices=["satisficing", "optimal", "anytime"],
                   help="Planner mode (default: satisficing).")
    p.add_argument("--timeout", type=float, default=30.0,
                   help="Time limit in seconds (default: 30).")
    p.add_argument("--verbosity", default=None,
                   choices=["silent", "info", "verbose", "debug"],
                   help="C++ backend verbosity (RanTanPlan only).")
    p.add_argument("--array-frame-mode", default=None,
                   choices=["disequality", "ite"],
                   help="Frame axiom shape for Z3 array/set fluents (RanTanPlan only). "
                        "'disequality' (default): (arr^t != arr^{t+1}) -> disjunction(actions). "
                        "'ite': total-update ITE chain. Applies under both --array-encoding "
                        "backends (pointwise per cell under uf).")
    p.add_argument("--array-encoding", default=None,
                   choices=["theory", "uf"],
                   help="Z3 backend for array/set fluents (RanTanPlan only). "
                        "'uf' (default): uninterpreted functions with pointwise domain "
                        "enumeration. 'theory': Z3 Array sort (select/store).")

    # Debug / trace
    p.add_argument("--trace", nargs="?", const="L1,L2,L3,L4,L5", default=None,
                   metavar="LAYERS",
                   help=("Comma-separated layers to display (RanTanPlan only): "
                         "L1=UP-parsed  L2=after UP compilation  L3=protobuf  "
                         "L4=C++ problem dump  L5=SMT formula. "
                         "Omit value to show all layers."))
    p.add_argument("--formula-timestep", type=int, default=1, metavar="N",
                   help="Horizon to encode when --trace includes L5 (default: 1).")
    p.add_argument("--dump-pb", metavar="FILE",
                   help="Write binary protobuf problem to FILE.")

    # Discovery
    p.add_argument("--list-pipelines",  action="store_true",
                   help="Print available pipeline names and exit.")
    p.add_argument("--list-solvers",    action="store_true",
                   help="Print UP-registered solvers and exit.")
    p.add_argument("--list-strategies", action="store_true",
                   help="Print RanTanPlan strategies and exit.")

    return p.parse_args()


def _parse_source_arg(args):
    """Parse --source into (mode, py_file).

    --source pddl              → ('pddl', None)          use -d/-p
    --source python FILE.py    → ('python', 'FILE.py')   Python UP problem
    --source both   FILE.py    → ('both',   'FILE.py')   Python + PDDL
    --source FILE.py           → ('python', 'FILE.py')   shorthand
    (omitted)                  → ('pddl', None)          default
    """
    if not args.source:
        return 'pddl', None

    parts = args.source
    first = parts[0]

    if first.lower() == 'pddl':
        return 'pddl', None

    if first.lower() in ('python', 'both'):
        if len(parts) < 2:
            print(f"Error: --source {first} requires a Python file path argument.")
            sys.exit(1)
        return first.lower(), parts[1]

    # Bare file path shorthand: --source FILE.py
    return 'python', first


def _solve_one(problem, args):
    """Apply pipeline (if any) and dispatch solver for *problem*.

    Shared by single-source and 'both' modes.  Does not handle 'both'+--trace
    (caller must route that to compare_layers directly).
    """
    is_rtp = args.solver.lower().startswith("rantanplan")

    if args.pipeline and args.up_compilers:
        print("Note: --up-compilers is ignored when --pipeline is given.\n")

    xts_note = "(has XTS features: arrays/sets/bounded-int)" if _has_xts_features(problem) else ""
    compiled_problem    = None
    compilation_results = []

    # Optional UP pre-compilation pipeline
    if args.pipeline and args.pipeline.lower() not in ("none", ""):
        pipeline_steps = COMPILATION_PIPELINES.get(args.pipeline, [])
        print(f"Pipeline: {args.pipeline}  "
              f"({' → '.join(s.name for s in pipeline_steps)})")
        t_compile = time.perf_counter()
        try:
            compiled_problem, compilation_results = apply_pipeline(problem, args.pipeline)
        except Exception as exc:
            compile_elapsed = time.perf_counter() - t_compile
            print(f"Pipeline error: {exc}  [compilation: {compile_elapsed:.2f}s]")
            sys.exit(1)
        compile_elapsed = time.perf_counter() - t_compile
        print(f"  → compiled: {len(list(compiled_problem.actions))} actions, "
              f"{len(list(compiled_problem.fluents))} fluents  "
              f"[compilation: {compile_elapsed:.2f}s]\n")
    elif not args.pipeline and not is_rtp and xts_note:
        print(f"  Warning: {xts_note} — solver '{args.solver}' may not support these. "
              f"Consider --pipeline iasciu.\n")

    # Run header
    print(f"Solver:   {args.solver}  strategy={args.strategy}  "
          f"timeout={args.timeout}s  mode={args.mode}")
    if args.pipeline and args.pipeline.lower() not in ("none", ""):
        print(f"          (pre-compiled via pipeline '{args.pipeline}')")
    elif args.up_compilers and not args.pipeline:
        print(f"          (up-compilers: {args.up_compilers})")
    if xts_note:
        print(f"          {xts_note}")
    if args.array_encoding or args.array_frame_mode:
        print(f"          array-encoding={args.array_encoding or 'theory'}  "
              f"array-frame-mode={args.array_frame_mode or 'disequality'}")
    print()

    # Dispatch: traced / anytime / normal
    trace_layers   = _parse_trace_layers(args.trace)
    use_trace_path = bool(trace_layers) or bool(args.dump_pb)

    if (trace_layers or args.dump_pb) and not is_rtp:
        print(f"Warning: --trace/--dump-pb only supported with RanTanPlan; ignored.\n")
        trace_layers   = set()
        use_trace_path = False

    if use_trace_path:
        solve_traced(problem, compiled_problem, compilation_results, args, trace_layers)
    elif args.mode == "anytime":
        solve_anytime(problem, compiled_problem, compilation_results, args)
    else:
        solve_normal(problem, compiled_problem, compilation_results, args)


def _main_both(args, py_file):
    """'both' mode: load Python + PDDL, optionally compare layers, solve both."""
    if not args.domain or not args.problem:
        print("Error: --source both requires -d/--domain and -p/--problem.")
        sys.exit(1)

    print(f"Loading Python:  {py_file}")
    py_problem = _load_or_exit(load_python, "Error loading Python problem", py_file)
    print(f"  → '{py_problem.name}'  ({_problem_summary(py_problem)})")

    print(f"Parsing PDDL:    {args.domain}")
    print(f"                 {args.problem}")
    pddl_problem = _load_or_exit(load_pddl, "Parse error", args.domain, args.problem)
    print(f"  → '{pddl_problem.name}'  ({_problem_summary(pddl_problem)})\n")

    # Side-by-side layer comparison (when --trace is given)
    trace_layers = _parse_trace_layers(args.trace)
    if trace_layers:
        try:
            compare_layers(py_problem, pddl_problem, py_file, args.problem,
                           args, trace_layers)
        except Exception as exc:
            print(f"\nComparison error: {type(exc).__name__}: {exc}")
            traceback.print_exc()
            sys.exit(1)

    # Solve both in parallel (trace already shown in the comparison above)
    args_solve = types.SimpleNamespace(**vars(args))
    args_solve.trace = None
    try:
        _parallel_solve(py_problem, pddl_problem,
                        py_problem.name, pddl_problem.name, args_solve)
    except Exception as exc:
        print(f"\nPlanner error: {type(exc).__name__}: {exc}")
        traceback.print_exc()
        sys.exit(1)


def main():
    args = parse_args()

    # Info-only flags that need no problem
    if args.list_pipelines:
        list_pipelines()
        return
    if args.list_strategies:
        list_strategies()
        return

    source_mode, py_file = _parse_source_arg(args)

    if source_mode == 'both':
        _main_both(args, py_file)
        return

    # ── Single-source mode: load the problem ──────────────────────────────────
    problem = None

    if source_mode == 'python':
        print(f"Loading:  {py_file}  (Python UP Problem)")
        t0_load = time.perf_counter()
        problem = _load_or_exit(load_python, "Error loading Python problem", py_file)
        load_elapsed = time.perf_counter() - t0_load
        print(f"  → '{problem.name}'  ({_problem_summary(problem)})  [loading: {load_elapsed:.2f}s]\n")

    elif source_mode == 'pddl':
        if not args.domain:
            if args.list_solvers:
                list_solvers()
                return
            print("Error: provide --source python FILE.py, "
                  "--source both FILE.py -d/-p, or -d/--domain + -p/--problem, "
                  "or one of --list-* flags.")
            sys.exit(1)
        if not args.problem:
            print("Error: -p/--problem is required with -d/--domain.")
            sys.exit(1)
        print(f"Parsing:  {args.domain}")
        print(f"          {args.problem}")
        t0_load = time.perf_counter()
        problem = _load_or_exit(load_pddl, "Parse error", args.domain, args.problem)
        load_elapsed = time.perf_counter() - t0_load
        print(f"  → '{problem.name}'  ({_problem_summary(problem)})  [loading: {load_elapsed:.2f}s]\n")

    # --list-solvers with a problem loaded: annotate compatibility
    if args.list_solvers:
        list_solvers(problem)
        return

    # ── Dispatch ──────────────────────────────────────────────────────────────
    try:
        _solve_one(problem, args)
    except Exception as exc:
        print(f"\nPlanner error: {type(exc).__name__}: {exc}")
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()