#!/usr/bin/env python3
"""
test_PDDL-XTS.py — PDDL-XTS test suite.

For each eligible test, one instance is generated per valid
(source × pipeline × solver) combination.

  -src/--source  {python,pddl,both}   Input source (default: python).
      python — load the Python UP Problem (--source problem.py)
      pddl   — parse PDDL files (-d domain.pddl -p instance.pddl)
      both   — run both source variants

  --pipeline     {native,up,both}     UP compilation (default: both).
      native — RanTanPlan's internal compilers (IPAR,QR,GR); no --pipeline flag
      up     — apply full UP compiler chain: iasciu (IPAR→ARRAYS→SETS→COUNT→INTEGER→USERTYPES)
      both   — run both pipeline variants

  --solver       {rtp,fd,both}        Solver (default: rtp).
      rtp  — RanTanPlan
      fd   — FastDownward  (requires --pipeline up or both)
      both — run both solvers (requires --pipeline up or both)

  --array-encoding {theory,uf,both}    Z3 array/set backend, RanTanPlan only (default: uf).
      theory — Z3 Array sort (select/store)
      uf     — uninterpreted functions with pointwise domain enumeration
      both   — run both encodings
      Ignored for --solver fd (FastDownward doesn't take this flag).

  --array-frame-mode {disequality,ite,both}  Frame-axiom shape, RanTanPlan only (default: disequality).
      ArraysRemover — (arr^t != arr^{t+1}) -> disjunction(actions)
      ite         — total-update ITE chain
      both        — run both frame modes
      Ignored for --solver fd.

  --path         DIR [DIR ...]         One or more suite directories to scan (default: xts/benchmarks/unit).
                                       When multiple paths are given, test names are prefixed
                                       with each directory's basename for disambiguation.

Auto-discovers every folder in the given path(s):
  - Regular folders: one test per valid combo.
  - X_* folders: one test per valid combo (expected: ERROR or UNSOLVABLE).

Always included regardless of flags:
  - Info flags (no variation)

  --filter       PATTERN               Only run tests matching PATTERN (glob or substring).
  --repeat       N                     Run each test N times; report mean ± stddev (default: 1).
  --csv          FILE                  Write results to a CSV file (one row per test).

Run: .venv/bin/python test_PDDL-XTS.py [--source ...] [--pipeline ...] [--solver ...] [--array-encoding ...] [--array-frame-mode ...] [--path DIR ...] [--filter PATTERN] [--repeat N] [--csv FILE]
"""
import argparse
import csv
import fnmatch
import os
import re
import statistics
import subprocess
import sys
import tempfile
import time
from collections import Counter
from concurrent.futures import ProcessPoolExecutor, as_completed

# This file lives in xts/tools/. ROOT is the repo root (base project); XTS_ROOT
# is the extension subtree holding the tools and corpus this suite drives.
XTS_ROOT  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT      = os.path.dirname(XTS_ROOT)
VENV_PY   = sys.executable
RUN       = os.path.join(XTS_ROOT, "tools", "solve.py")
TESTS_DIR = os.path.join(XTS_ROOT, "benchmarks", "unit")

GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
BOLD   = "\033[1m"
DIM    = "\033[2m"
END    = "\033[0m"

TIMEOUT_S   = 240  # per-test wall-clock timeout (subprocess)
RTP_TIMEOUT = 120   # --timeout passed to solve.py
# Limiting parallelism is critical: each test spawns a fresh solve.py process
# (full Python import + compilation + C++ planner). At os.cpu_count() workers the
# 20 concurrent Python startups saturate the machine and eat into the planner's
# time budget, causing spurious timeouts on tests that finish fine individually.
MAX_WORKERS = max(1, os.cpu_count() // 2)

# Extra args injected for specific PDDL-XTS folders.
# Note: array/set folders cannot override --strategy to a forall/exists-step
# strategy — StrategyFactory::create_encoder rejects that combination.
FOLDER_EXTRA = {}

# Solvers to skip for specific folders.  Use this for combinations that are
# fundamentally incompatible rather than bugs (e.g. a domain with continuous
# numeric fluents that this FD build cannot handle).
FOLDER_SKIP_SOLVERS = {
    # Zenotravel keeps fuel/distance as RealType() fluents (bounded-int would
    # require IntType(0, 99999), making INTEGERS_REMOVING infeasible).  The
    # installed FastDownward build rejects :numeric-fluents at the translate step.
    "zenotravel": {"fd"},
}

# Array/set encodings to skip for specific folders.  --array-encoding uf is only
# implemented by the plain grounded encoder's effect paths (see config.cpp), so a
# folder whose FOLDER_EXTRA selects a Chained/R2E-encoder strategy must skip it, or
# every "uf" combo would fail validation with a configuration error, not a real bug.
FOLDER_SKIP_ENCODINGS = {}

# ─────────────────────────────────────────────────────────────────────────────
# Input-source helpers
# ─────────────────────────────────────────────────────────────────────────────

V = ["--verbosity", "silent"]

def _py(path):
    return ["--source", path]

def _pddl(folder, xts_dir):
    folder_path = os.path.join(xts_dir, folder)
    for instance in ("instance.pddl", "problem.pddl"):
        if os.path.exists(os.path.join(folder_path, instance)):
            return ["-d", os.path.join(folder_path, "domain.pddl"),
                    "-p", os.path.join(folder_path, instance)]
    return ["-d", os.path.join(folder_path, "domain.pddl"),
            "-p", os.path.join(folder_path, "instance.pddl")]

# ─────────────────────────────────────────────────────────────────────────────
# Combinatorial framework
# ─────────────────────────────────────────────────────────────────────────────

def _combos(sources, pipelines, solvers, aencs, afms, has_py=True, has_pddl=True, skip_encodings=None):
    """Yield valid (src, pl, slv, aenc, afm) tuples from the requested dimensions.

    aenc/afm only affect the RanTanPlan solver (solve.py forwards them to
    RantanPlanPlanner only when is_rtp); FastDownward ignores both flags, so a
    'both'-selected encoding/frame-mode dimension must not multiply fd runs —
    each fd combo is yielded once, with aenc=afm=None (meaning: not applicable).
    """
    skip_encodings = skip_encodings or set()
    for src in sources:
        if src == "python" and not has_py:   continue
        if src == "pddl"   and not has_pddl: continue
        for pl in pipelines:
            for slv in solvers:
                if slv == "fd" and pl == "native": continue   # FD requires UP pipeline
                if slv == "fd":
                    yield src, pl, slv, None, None
                    continue
                for aenc in aencs:
                    if aenc in skip_encodings:
                        continue
                    for afm in afms:
                        yield src, pl, slv, aenc, afm


def _tag(src, pl, slv, aenc=None, afm=None):
    s = "py" if src == "python" else "pddl"
    base = f"{s}->{pl}->{slv}"
    if aenc is not None and afm is not None:
        return f"[{base}|{aenc}|{afm}]"
    return f"[{base}]"


def _make_args(src, pl, slv, py_args, pddl_args, extra=None, aenc=None, afm=None):
    base = list(py_args if src == "python" else pddl_args)
    if pl == "up":
        base += ["--pipeline", "iasciu"]
    if slv == "fd":
        base += ["--solver", "FastDownward"]
    if aenc is not None:
        base += ["--array-encoding", aenc]
    if afm is not None:
        base += ["--array-frame-mode", afm]
    return base + (extra or []) + V


# ─────────────────────────────────────────────────────────────────────────────
# Test discovery
# ─────────────────────────────────────────────────────────────────────────────

def build_tests(sources, pipelines, solvers, aencs, afms, xts_dirs):
    """Return the full test list for the given dimension selections.

    xts_dirs: list of (dir_path, prefix) — prefix is prepended to test names when
    running multiple suites so results are unambiguous (e.g. "tests/" or "trans/").
    """
    tests = []

    # ── Info flags (always, no variation, always from TESTS_DIR) ──────────────
    tests += [
        ("info/list-pipelines",  ["--list-pipelines"],  None),
        ("info/list-solvers",    ["--list-solvers"],    None),
        ("info/list-strategies", ["--list-strategies"], None),
        ("info/list-solvers · with problem", _pddl("2d", TESTS_DIR) + ["--list-solvers"], None),
    ]

    # ── Classical PDDL mprime (PDDL source only; always included) ─────────────
    #mprime = ["-d", os.path.join(ROOT, "pddl/small-test/mprime/domain.pddl"),
    #          "-p", os.path.join(ROOT, "pddl/small-test/mprime/instances/pfile05.pddl")]
    #for _, pl, slv, aenc, afm in _combos(["pddl"], pipelines, solvers, aencs, afms, has_py=False, has_pddl=True):
    #    tests.append((f"classical·mprime {_tag('pddl', pl, slv, aenc, afm)}",
    #                  _make_args("pddl", pl, slv, [], mprime, aenc=aenc, afm=afm), "SOLVED"))

    # ── Auto-discovery over each requested suite dir ───────────────────────────
    for xts_dir, prefix in xts_dirs:
        for folder in sorted(os.listdir(xts_dir)):
            folder_path = os.path.join(xts_dir, folder)
            if not os.path.isdir(folder_path):
                continue

            extra     = FOLDER_EXTRA.get(folder, [])
            has_py    = os.path.exists(os.path.join(folder_path, "problem.py"))
            has_pddl  = (os.path.exists(os.path.join(folder_path, "domain.pddl")) and
                         any(os.path.exists(os.path.join(folder_path, f))
                             for f in ("instance.pddl", "problem.pddl")))
            py_args   = _py(os.path.join(folder_path, "problem.py"))
            pddl_args = _pddl(folder, xts_dir)

            skip_slvs = FOLDER_SKIP_SOLVERS.get(folder, set())
            skip_enc  = FOLDER_SKIP_ENCODINGS.get(folder, set())
            expected  = "ERROR" if folder.startswith("X_") else "SOLVED"
            name_pfx  = f"{prefix}X/{folder}" if folder.startswith("X_") else f"{prefix}{folder}"
            for src, pl, slv, aenc, afm in _combos(sources, pipelines, solvers, aencs, afms,
                                                    has_py=has_py, has_pddl=has_pddl,
                                                    skip_encodings=skip_enc):
                if slv in skip_slvs:
                    continue
                tests.append((f"{name_pfx} {_tag(src, pl, slv, aenc, afm)}",
                               _make_args(src, pl, slv, py_args, pddl_args, extra,
                                          aenc=aenc, afm=afm), expected))

    return tests


# ─────────────────────────────────────────────────────────────────────────────
# Runner
# ─────────────────────────────────────────────────────────────────────────────

def _parse_timings(text):
    """Extract (load_t, compile_t, solve_t) in seconds from solve.py output, or None if absent.

    Matches:
      loading:   "[loading: X.XXs]"
      native:    "Compilation:  X.XXs"   and  "[solving: X.XXs]"
      pipeline:  "[compilation: X.XXs]"  and  "[solving: X.XXs]"
    """
    m_l = re.search(r'\[loading:\s+([\d.]+)s\]', text)
    m_c = re.search(r'ompilation[:\s]+([\d.]+)s', text)
    m_s = re.search(r'\[solving:\s+([\d.]+)s\]', text)
    return (float(m_l.group(1)) if m_l else None,
            float(m_c.group(1)) if m_c else None,
            float(m_s.group(1)) if m_s else None)


def _run_test_core(name, args, expected):
    """Run one test; return (status, elapsed, combined, detail)."""
    cmd = [VENV_PY, RUN] + args
    if expected in ("SOLVED", "ERROR"):
        cmd += ["--timeout", str(RTP_TIMEOUT)]

    t0 = time.perf_counter()
    # Each test gets its own working directory so FD's output.sas is isolated
    # from concurrent tests (FD's translator writes output.sas to CWD).
    with tempfile.TemporaryDirectory() as test_cwd:
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=TIMEOUT_S,
                cwd=test_cwd,
            )
            elapsed = time.perf_counter() - t0
            combined = result.stdout + result.stderr
            rc = result.returncode
        except subprocess.TimeoutExpired as e:
            # CPython does not decode stdout/stderr on TimeoutExpired even when
            # subprocess.run was given text=True, so these can come back as bytes
            # and "bytes + str" raises TypeError, killing the whole run.
            _out, _err = e.stdout or "", e.stderr or ""
            if isinstance(_out, bytes): _out = _out.decode("utf-8", "replace")
            if isinstance(_err, bytes): _err = _err.decode("utf-8", "replace")
            combined = _out + _err
            return "TIMEOUT", time.perf_counter() - t0, combined, f"subprocess timeout ({TIMEOUT_S}s)"

    if expected is None:
        ok = rc == 0
        detail = "" if ok else f"exit={rc}\n{combined[:300]}"
        return ("OK" if ok else "FAIL"), elapsed, combined, detail

    if expected == "ERROR":
        # X_ tests: accept either a non-zero exit (validator rejects) or UNSOLVABLE.
        if rc != 0:
            return "REJECTED", elapsed, combined, ""
        if "UNSOLVABLE" in combined:
            return "UNSOLVABLE", elapsed, combined, ""
        detail = f"expected error or unsolvable but got exit=0 and SOLVED\n{combined[:300]}"
        return "FAIL", elapsed, combined, detail

    # expected == "SOLVED"
    if rc != 0:
        return "FAIL", elapsed, combined, f"exit={rc}"

    if "SOLVED" in combined:
        m = re.search(r'Plan \((\d+) steps\)', combined)
        steps = f"  [{m.group(1)} steps]" if m else ""
        return "SOLVED" + steps, elapsed, combined, ""

    # Two distinct timeout shapes reach here:
    #   - the backend noticed its own --timeout and printed "Status: TIMEOUT";
    #   - solve.py had to KILL it at args.timeout + 10s (solve.py:772) and printed
    #     only "Planner timed out.", so no uppercase marker exists to match.
    # Both are timeouts; matching only the first reported the second as FAIL.
    if "TIMEOUT" in combined or "Planner timed out" in combined:
        return "TIMEOUT", elapsed, combined, "planner timed out"

    if "INTERNAL ERROR" in combined or "INTERNAL_ERROR" in combined:
        tail = combined[-400:].replace('\n', ' ')
        return "FAIL", elapsed, combined, f"INTERNAL_ERROR: {tail}"

    return "FAIL", elapsed, combined, f"unexpected output: {combined[-200:]}"


def run_test(name, args, expected):
    status, elapsed, combined, detail = _run_test_core(name, args, expected)
    load_t, compile_t, solve_t = _parse_timings(combined)
    return status, elapsed, combined, detail, load_t, compile_t, solve_t


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

def _worker(item):
    name, args, expected = item
    status, elapsed, combined, detail, load_t, compile_t, solve_t = run_test(name, args, expected)
    return name, expected, status, elapsed, combined, detail, load_t, compile_t, solve_t


TAG_RE = re.compile(r'\[(py|pddl)->(native|up)->(rtp|fd)(?:\|(theory|uf)\|(disequality|ite))?\]')


def _split_name(name):
    """Return (base, tag_str) splitting off the trailing [src->pl->slv(|aenc|afm)] tag."""
    m = TAG_RE.search(name)
    if m:
        return name[:m.start()].strip(), m.group(0)
    return name, ""


def _matches_filter(base, pattern):
    """Glob match if pattern contains wildcards, else case-insensitive substring."""
    if any(c in pattern for c in ('*', '?', '[')):
        return fnmatch.fnmatch(base, pattern)
    return pattern.lower() in base.lower()


def _fmt_t(vals, suffix, time_w):
    """Format a list of timing values into a fixed-width display column."""
    if not vals:
        return " " * time_w
    if time_w <= 7:
        return f"{vals[0]:6.2f}{suffix}"
    mean = statistics.mean(vals)
    std  = statistics.stdev(vals) if len(vals) > 1 else 0.0
    return f"{mean:5.2f}±{std:4.2f}{suffix}"


def _aggregate_reps(reps):
    """Collapse N repetition result tuples into one display/CSV record."""
    name, expected = reps[0][0], reps[0][1]
    statuses = [r[2] for r in reps]
    counter  = Counter(statuses)
    top_status, top_count = counter.most_common(1)[0]
    status = top_status if top_count == len(reps) else "FLAKY"

    elapsed_vals = [r[3] for r in reps]
    load_vals    = [r[6] for r in reps if r[6] is not None]
    compile_vals = [r[7] for r in reps if r[7] is not None]
    solve_vals   = [r[8] for r in reps if r[8] is not None]

    # For error display prefer a non-passing rep; fall back to last rep.
    _pass = {"OK", "REJECTED", "UNSOLVABLE"}
    rep_err = next((r for r in reps
                    if r[2] not in _pass and "SOLVED" not in r[2]), reps[-1])
    full_out = rep_err[4]
    if status == "FLAKY":
        detail = "FLAKY: " + ", ".join(f"{s}×{c}" for s, c in counter.most_common())
    else:
        detail = rep_err[5]

    return (name, expected, status, top_status,
            elapsed_vals, full_out, detail,
            load_vals, compile_vals, solve_vals)


def _write_csv(path, results_agg, repeat):
    with open(path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['name', 'base', 'tag', 'status',
                         'load_mean', 'load_std',
                         'compile_mean', 'compile_std',
                         'solve_mean', 'solve_std',
                         'total_mean', 'total_std',
                         'n_reps'])
        for agg in results_agg:
            if agg is None:
                continue
            name, expected, status, _, elapsed_vals, _, _, \
                load_vals, compile_vals, solve_vals = agg
            base, tag_str = _split_name(name)

            def _ms(vals):
                if not vals:
                    return '', ''
                m = statistics.mean(vals)
                s = statistics.stdev(vals) if len(vals) > 1 else 0.0
                return f"{m:.4f}", f"{s:.4f}"

            lm, ls = _ms(load_vals)
            cm, cs = _ms(compile_vals)
            sm, ss = _ms(solve_vals)
            tm, ts = _ms(elapsed_vals)
            writer.writerow([name, base, tag_str, status,
                             lm, ls, cm, cs, sm, ss, tm, ts, repeat])


def _parse_args():
    p = argparse.ArgumentParser(
        description="PDDL-XTS test suite.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "-src", "--source", choices=["python", "pddl", "both"], default="python",
        help="Input source: 'python' (UP problem files), 'pddl' (PDDL files), or 'both'. Default: python.",
    )
    p.add_argument(
        "--pipeline", choices=["native", "up", "both"], default="both",
        help="UP compilation: 'native' (RTP internal), 'up' (INT_PARAM→ARRAYS), or 'both'. Default: both.",
    )
    p.add_argument(
        "--solver", choices=["rtp", "fd", "both"], default="rtp",
        help="Solver: 'rtp' (RanTanPlan), 'fd' (FastDownward, needs pipeline=up/both), or 'both'. Default: rtp.",
    )
    p.add_argument(
        "--array-encoding", choices=["theory", "uf", "both"], default="uf",
        help="Z3 array/set backend, RanTanPlan only: 'theory' (Array sort), "
             "'uf' (uninterpreted functions), or 'both'. Ignored for --solver fd. Default: uf.",
    )
    p.add_argument(
        "--array-frame-mode", choices=["disequality", "ite", "both"], default="disequality",
        help="Frame-axiom shape, RanTanPlan only: 'disequality', 'ite', or 'both'. "
             "Ignored for --solver fd. Default: disequality.",
    )
    p.add_argument(
        "--path", nargs='+', default=[TESTS_DIR], metavar="DIR",
        help="One or more suite directories to scan (default: xts/benchmarks/unit). "
             "When multiple directories are given, test names are prefixed with each directory's basename.",
    )
    p.add_argument(
        "--filter", default=None, metavar="PATTERN",
        help="Only run tests whose base name matches PATTERN. "
             "Supports * and ? wildcards; plain text is matched as a case-insensitive substring.",
    )
    p.add_argument(
        "--repeat", type=int, default=1, metavar="N",
        help="Run each test N times and report mean ± stddev of timings (default: 1).",
    )
    p.add_argument(
        "--csv", default=None, metavar="FILE",
        help="Write results to a CSV file (one row per test).",
    )
    p.add_argument(
        "--no-errors", dest="show_errors", action="store_false", default=True,
        help="Suppress the failure details printed at the end of the run.",
    )
    p.add_argument(
        "--workers", type=int, default=MAX_WORKERS, metavar="N",
        help=f"Number of parallel test workers (default: {MAX_WORKERS}).",
    )
    p.add_argument(
        "--timeout", type=float, default=None, metavar="SECONDS",
        help=f"Override the per-test RTP solve timeout (default: {RTP_TIMEOUT}s). "
             f"The subprocess wall-clock timeout scales with it (timeout + "
             f"{TIMEOUT_S - RTP_TIMEOUT}s buffer).",
    )
    return p.parse_args()


def main():
    args = _parse_args()

    if args.timeout is not None:
        global RTP_TIMEOUT, TIMEOUT_S
        buffer = TIMEOUT_S - RTP_TIMEOUT
        RTP_TIMEOUT = args.timeout
        TIMEOUT_S = args.timeout + buffer

    sources   = ["python", "pddl"] if args.source   == "both" else [args.source]
    pipelines = ["native", "up"]   if args.pipeline == "both" else [args.pipeline]
    solvers   = ["rtp", "fd"]      if args.solver   == "both" else [args.solver]
    aencs     = ["theory", "uf"]         if args.array_encoding   == "both" else [args.array_encoding]
    afms      = ["disequality", "ite"]   if args.array_frame_mode == "both" else [args.array_frame_mode]

    if "fd" in solvers and "up" not in pipelines:
        print("Error: --solver fd (or both) requires --pipeline up or both.")
        sys.exit(1)

    paths = [os.path.abspath(p) for p in args.path]
    if len(paths) == 1:
        xts_dirs = [(paths[0], "")]
    else:
        xts_dirs = [(p, os.path.basename(p) + "/") for p in paths]

    tests = build_tests(sources, pipelines, solvers, aencs, afms, xts_dirs)

    if args.filter:
        tests = [(n, a, e) for n, a, e in tests
                 if _matches_filter(_split_name(n)[0], args.filter)]
        if not tests:
            print(f"No tests match filter: {args.filter!r}")
            return 0

    NAME_W = max((len(_split_name(n)[0]) for n, _, _ in tests), default=32)
    TAG_W  = max((len(_split_name(n)[1]) for n, _, _ in tests), default=20)
    TIME_W = 11 if args.repeat > 1 else 7
    th     = lambda s: f"{s:>{TIME_W}}"
    sep_t  = "─" * TIME_W

    label    = (f"source={args.source}  pipeline={args.pipeline}  solver={args.solver}  "
                f"array-encoding={args.array_encoding}  array-frame-mode={args.array_frame_mode}")
    rpt_note = f"  ·  repeat={args.repeat}" if args.repeat > 1 else ""
    print(f"\n{BOLD}PDDL-XTS test suite{END}  [{label}]")
    print(f"{len(tests)} tests  ·  {args.workers} workers  ·  "
          f"rtp timeout={RTP_TIMEOUT}s  ·  wall timeout={TIMEOUT_S}s{rpt_note}\n")
    w = len(str(len(tests)))
    print(f"  {'':>{w*2+3}}  {'Test':<{NAME_W}}  {'Pipeline':<{TAG_W}}  {'Result':<22}  "
          f"{th('load')}  {th('compile')}  {th('solve')}  {th('total')}")
    print(f"  {'─'*(w*2+3)}  {'─'*NAME_W}  {'─'*TAG_W}  {'─'*22}  "
          f"{sep_t}  {sep_t}  {sep_t}  {sep_t}")

    passed = failed = timeout = 0
    failures     = []
    timeouts     = []
    tag_counts   = {}
    done_tests   = 0
    pending      = {i: [] for i in range(len(tests))}
    results_agg  = [None] * len(tests)
    t_start      = time.perf_counter()

    with ProcessPoolExecutor(max_workers=args.workers) as ex:
        futures = {}
        for i, item in enumerate(tests):
            for _ in range(args.repeat):
                futures[ex.submit(_worker, item)] = i

        for fut in as_completed(futures):
            i = futures[fut]
            pending[i].append(fut.result())
            if len(pending[i]) < args.repeat:
                continue

            done_tests += 1
            reps = pending[i]
            name, expected, status, top_status, elapsed_vals, full_out, detail, \
                load_vals, compile_vals, solve_vals = _aggregate_reps(reps)
            results_agg[i] = (name, expected, status, top_status,
                              elapsed_vals, full_out, detail,
                              load_vals, compile_vals, solve_vals)

            base, tag_str = _split_name(name)

            if status != "FLAKY" and ("SOLVED" in status or status in ("OK", "REJECTED", "UNSOLVABLE")):
                colour = GREEN
                passed += 1
                bucket = 0
                last_err = ""
            elif status != "FLAKY" and status == "TIMEOUT":
                colour = YELLOW
                timeout += 1
                bucket = 2
                last_err = ""
                timeouts.append((name, status, detail, full_out))
            else:
                colour = RED
                failed += 1
                bucket = 1
                failures.append((name, status, detail, full_out))
                out_lines = [l.strip() for l in (full_out or "").splitlines() if l.strip()]
                last_err = out_lines[-1] if out_lines else ""

            lt = _fmt_t(load_vals,    "l", TIME_W)
            ct = _fmt_t(compile_vals, "c", TIME_W)
            st = _fmt_t(solve_vals,   "s", TIME_W)
            tt = _fmt_t(elapsed_vals, "s", TIME_W)
            inline = f"  {DIM}{last_err}{END}" if last_err else ""
            print(f"  [{done_tests:>{w}}/{len(tests):>{w}}]  {base:<{NAME_W}}  {tag_str:<{TAG_W}}  "
                  f"{colour}{status:<22}{END}  {lt}  {ct}  {st}  {tt}{inline}")

            m = TAG_RE.search(name)
            if m:
                tag = m.group(0)
                if tag not in tag_counts:
                    tag_counts[tag] = [0, 0, 0]
                tag_counts[tag][bucket] += 1

    total_elapsed = time.perf_counter() - t_start
    total = passed + failed + timeout
    print(f"\n  {'─'*100}")
    print(f"  {BOLD}Results:{END}  {GREEN}{passed} passed{END}  "
          f"{RED}{failed} failed{END}  "
          f"{YELLOW}{timeout} timeout{END}  "
          f"/ {total} total  ·  {BOLD}total time: {total_elapsed:.1f}s{END}\n")

    if (args.source == "both" or args.pipeline == "both" or args.solver == "both"
            or args.array_encoding == "both" or args.array_frame_mode == "both"):
        print(f"  {BOLD}Breakdown:{END}\n")
        for src, pl, slv, aenc, afm in _combos(sources, pipelines, solvers, aencs, afms):
            tag = _tag(src, pl, slv, aenc, afm)
            if tag not in tag_counts:
                continue
            p, f_, t = tag_counts[tag]
            print(f"    {tag:<{TAG_W}}  {GREEN}{p:>3} passed{END}  "
                  f"{RED}{f_:>3} failed{END}  "
                  f"{YELLOW}{t:>3} timeout{END}  / {p + f_ + t}")
        print()

    def _print_recap(entries, colour, label):
        print(f"{BOLD}{label}:{END}\n")
        for name, status, detail, full_out in entries:
            base, tag_str = _split_name(name)
            print(f"  {colour}{status:<8}{END}  {base:<{NAME_W}}  {tag_str:<{TAG_W}}")
            if args.show_errors:
                if detail:
                    print(f"           {detail[:200]}")
                lines = (full_out or "").strip().splitlines()
                for line in lines[-20:]:
                    print(f"           {line}")
                print()

    if failures:
        _print_recap(failures, RED, "Failures")
    if timeouts:
        _print_recap(timeouts, YELLOW, "Timeouts")

    if args.csv:
        _write_csv(args.csv, results_agg, args.repeat)
        print(f"\n  CSV written to: {args.csv}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())