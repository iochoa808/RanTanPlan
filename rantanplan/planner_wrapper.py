from typing import Callable, IO, Iterator, Optional

import unified_planning as up
from unified_planning.shortcuts import Fraction, PlanValidator
from unified_planning.engines.results import PlanGenerationResult, LogMessage, LogLevel
from unified_planning.engines.mixins import OneshotPlannerMixin
from unified_planning.engines.mixins.oneshot_planner import OptimalityGuarantee
from unified_planning.engines.mixins.anytime_planner import AnytimePlannerMixin, AnytimeGuarantee
from unified_planning.engines import PlanGenerationResultStatus, Engine, CompilationKind, ValidationResultStatus
from unified_planning.engines.compilers import (
    Grounder, QuantifiersRemover,
    BoundedTypesRemover, NegativeConditionsRemover,
    DisjunctiveConditionsRemover, ConditionalEffectsRemover,
)
from unified_planning.engines.compilers.usertype_fluents_remover import UsertypeFluentsRemover
from unified_planning.grpc.proto_writer import ProtobufWriter
from unified_planning.grpc.proto_reader import ProtobufReader
from unified_planning.exceptions import UPException
from .cnf_condition_compiler import CNFConditionCompiler
from unified_planning.model import ProblemKind, Problem
from unified_planning.plans import SequentialPlan, ActionInstance
from unified_planning.shortcuts import get_environment
from unified_planning.model.fluent import get_all_fluent_exp
import unified_planning.grpc.generated.unified_planning_pb2 as up_pb2

import subprocess
import tempfile
import time
import os
import sys
import threading
from queue import Queue


# ═══════════════════════════════════════════════════════════════════════
# Shared base class
# ═══════════════════════════════════════════════════════════════════════

class _RantanPlanBase(Engine):
    """Shared logic for all RantanPlan UP engines."""

    def __init__(self, **options):
        Engine.__init__(self)

        self._pb_writer = ProtobufWriter()
        self._pb_reader = ProtobufReader()
        self.executable_path = self._find_executable(options.get('executable_path'))
        self._verbosity = options.get('verbosity')
        self._strategy = options.get('strategy', 'seq')
        self._max_steps = options.get('max_steps')
        self._persist_clauses = options.get('persist_clauses', False)
        self._symmetries = options.get('symmetries', False)
        self._stats_file = options.get('stats_file')
        self._no_cnf_normalization = options.get('no_cnf_normalization', False)
        self._compile_usertype_fluents = options.get('compile_usertype_fluents', False)
        self._smt_rpg_checker = options.get('smt_rpg_checker', False)
        self._no_goal_relevance = options.get('no_goal_relevance', False)
        self._horizon_schedule = options.get('horizon_schedule')
        self._log_file = options.get('log_file')
        self._up_grounding = options.get('up_grounding', False)
        self._numeric_grounding = options.get('numeric_grounding', True)
        self._mode = options.get('mode')
        self._output_plan = options.get('output_plan')
        self._no_local_reasoning = options.get('no_local_reasoning', False)
        self._array_frame_mode = options.get('array_frame_mode')
        self._array_encoding = options.get('array_encoding')
        # Default IPAR,QR,CNF — the pre-XTS pipeline (QR + CNF, grounding left to
        # the C++ backend unless GR is listed or up_grounding=True). IPAR is
        # additive: its supports() check skips standard problems. QR expands
        # object-typed forall/exists Python-side
        raw = options.get('up_compilers', ['IPAR', 'QR', 'CNF'])
        if isinstance(raw, str):
            raw = [] if raw.lower() == 'none' else [c.strip() for c in raw.split(',') if c.strip()]
        self._up_compilers = [c.upper() for c in raw]

    # ── Verbosity helpers ────────────────────────────────────────────────

    @property
    def _is_silent(self) -> bool:
        return self._verbosity == "silent"

    @property
    def _is_verbose(self) -> bool:
        return self._verbosity in ("verbose", "debug")

    def _log(self, msg: str) -> None:
        if not self._is_silent:
            print(msg)

    def _log_verbose(self, msg: str) -> None:
        if self._is_verbose:
            print(msg)

    def _log_warning(self, msg: str) -> None:
        if not self._is_silent:
            print(msg)

    @staticmethod
    def _log_error(msg: str) -> None:
        print(msg, file=sys.stderr)

    # ── Executable discovery ─────────────────────────────────────────────

    def _find_executable(self, provided_path):
        if provided_path:
            return provided_path

        package_dir = os.path.dirname(__file__)
        candidates = [
            os.path.join(package_dir, "bin", "rantanplan"),
            os.path.join(package_dir, "cpp", "build", "rantanplan"),
            os.path.join(package_dir, "..", "planner", "build", "rantanplan"),
            "rantanplan"
        ]

        for path in candidates:
            if os.path.exists(path):
                return path
            if path == "rantanplan" and self._check_executable_in_path(path):
                return path

        default_path = os.path.join(package_dir, "cpp", "build", "rantanplan")
        if not os.path.exists(default_path):
            raise UPException(
                f"rantanplan executable not found. Tried: {', '.join(candidates)}. "
                "Please build the planner and/or provide the correct path."
            )
        return default_path

    def _check_executable_in_path(self, executable_name):
        try:
            subprocess.run([executable_name, "--help"],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5)
            return True
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError, FileNotFoundError):
            return False

    # ── Engine metadata ──────────────────────────────────────────────────

    @property
    def name(self) -> str:
        return "RantanPlan"

    @staticmethod
    def supported_kind():
        supported_kind = ProblemKind()
        supported_kind.set_problem_class("ACTION_BASED")
        supported_kind.set_problem_type("GENERAL_NUMERIC_PLANNING")
        supported_kind.set_problem_type("SIMPLE_NUMERIC_PLANNING")
        supported_kind.set_typing('FLAT_TYPING')
        supported_kind.set_typing('HIERARCHICAL_TYPING')
        supported_kind.set_numbers('CONTINUOUS_NUMBERS')
        supported_kind.set_numbers('DISCRETE_NUMBERS')
        supported_kind.set_numbers('BOUNDED_TYPES')
        supported_kind.set_fluents_type('NUMERIC_FLUENTS')
        supported_kind.set_fluents_type('REAL_FLUENTS')
        supported_kind.set_fluents_type('INT_FLUENTS')
        supported_kind.set_fluents_type('OBJECT_FLUENTS')
        try:
            supported_kind.set_fluents_type('ARRAY_FLUENTS')  # [XTS]
            supported_kind.set_fluents_type('SET_FLUENTS')     # [XTS]
        except AssertionError:
            pass  # older UP versions don't have these features
        supported_kind.set_parameters('BOUNDED_INT_ACTION_PARAMETERS')  # [XTS]
        supported_kind.set_parameters('BOUNDED_INT_FLUENT_PARAMETERS')  # [XTS]
        for _ck in ('COUNTING', 'RANGE_VARIABLES', 'MEMBERING'):  # [XTS]
            try:
                supported_kind.set_conditions_kind(_ck)
            except AssertionError:
                pass
        supported_kind.set_initial_state('UNDEFINED_INITIAL_NUMERIC')
        supported_kind.set_conditions_kind('NEGATIVE_CONDITIONS')
        supported_kind.set_conditions_kind('DISJUNCTIVE_CONDITIONS')
        supported_kind.set_conditions_kind('EQUALITIES')
        supported_kind.set_conditions_kind('EXISTENTIAL_CONDITIONS')
        supported_kind.set_conditions_kind('UNIVERSAL_CONDITIONS')
        supported_kind.set_effects_kind('CONDITIONAL_EFFECTS')
        supported_kind.set_effects_kind('INCREASE_EFFECTS')
        supported_kind.set_effects_kind('DECREASE_EFFECTS')
        supported_kind.set_effects_kind('FLUENTS_IN_NUMERIC_ASSIGNMENTS')
        supported_kind.set_effects_kind('STATIC_FLUENTS_IN_OBJECT_ASSIGNMENTS')
        supported_kind.set_effects_kind('FLUENTS_IN_OBJECT_ASSIGNMENTS')
        supported_kind.set_effects_kind('FORALL_EFFECTS')
        supported_kind.set_quality_metrics('FINAL_VALUE')
        supported_kind.set_quality_metrics('ACTIONS_COST')
        supported_kind.set_actions_cost_kind('INT_NUMBERS_IN_ACTIONS_COST')
        supported_kind.set_actions_cost_kind('REAL_NUMBERS_IN_ACTIONS_COST')
        supported_kind.set_actions_cost_kind('FLUENTS_IN_ACTIONS_COST')
        supported_kind.set_actions_cost_kind('STATIC_FLUENTS_IN_ACTIONS_COST')
        return supported_kind

    @staticmethod
    def supports(problem_kind):
        return problem_kind <= _RantanPlanBase.supported_kind()

    def destroy(self):
        pass

    # ── Subprocess helpers ───────────────────────────────────────────────

    def _stream_output(self, pipe, prefix=""):
        """Stream output from subprocess pipe directly to stdout in real-time."""
        if not pipe:
            return
        try:
            for line in iter(pipe.readline, b''):
                if not line:
                    break
                if self._is_silent:
                    continue
                line_str = line.decode('utf-8', errors='replace').rstrip()
                if line_str:
                    print(f"{prefix}{line_str}")
                    sys.stdout.flush()
        except Exception:
            pass
        finally:
            if pipe:
                pipe.close()

    def _build_command(self, problem_filepath: str, solution_filepath: str,
                       timeout: Optional[float] = None) -> list:
        command = [self.executable_path, problem_filepath, solution_filepath,
                  "--strategy", self._strategy]

        if self._verbosity is not None:
            command.extend(["--verbosity", self._verbosity])
        if self._max_steps is not None:
            command.extend(["--max-steps", str(self._max_steps)])
        if self._persist_clauses:
            command.append("--persist-clauses")
        if self._symmetries:
            command.append("--symmetries")
        if self._stats_file is not None:
            command.extend(["--stats-file", self._stats_file])
        if self._smt_rpg_checker:
            command.append("--smt-rpg-checker")
        if self._no_goal_relevance:
            command.append("--no-goal-relevance")
        if self._horizon_schedule is not None:
            command.extend(["--horizon-schedule", self._horizon_schedule])
        if self._log_file is not None:
            command.extend(["--log-file", self._log_file])
        if self._up_grounding:
            command.append("--up-grounding")
        if not self._numeric_grounding:
            command.append("--no-numeric-grounding")
        if self._no_local_reasoning:
            command.append("--no-local-reasoning")
        if self._mode is not None:
            command.extend(["--mode", self._mode])
        if self._output_plan is not None:
            command.extend(["--output-plan", self._output_plan])
        if self._array_frame_mode is not None:
            command.extend(["--array-frame-mode", self._array_frame_mode])
        if self._array_encoding is not None:
            command.extend(["--array-encoding", self._array_encoding])
        if timeout:
            command.extend(["--timeout", str(int(timeout))])

        return command

    # ── Problem preparation ──────────────────────────────────────────────

    def _validate_plan(self, problem: Problem, plan: SequentialPlan) -> Optional[bool]:
        try:
            with PlanValidator(problem_kind=problem.kind, plan_kind=plan.kind) as validator:
                validation_result = validator.validate(problem, plan)  # type: ignore[attr-defined]

                if validation_result.status == ValidationResultStatus.VALID:
                    self._log("Plan validation: VALID")
                    self._log(f"  The plan with {len(plan.actions)} actions is correct and executable.")
                    return True
                else:
                    self._log_warning(f"Plan validation: {validation_result.status.name}")
                    if validation_result.log_messages:
                        for log_msg in validation_result.log_messages:
                            self._log_warning(f"  Validation {log_msg.level.name}: {log_msg.message}")
                    else:
                        self._log_warning("  No detailed validation messages available.")
                    return False

        except Exception as e:
            msg = str(e)
            if "No available engine" in msg:
                self._log_verbose("  Plan validation skipped: no UP validator supports this problem's features (e.g. arrays).")
                return None
            self._log_error(f"Plan validation failed with error: {e}")
            return False

    def _initialize_fluents(self, task: Problem):
        _env = get_environment()
        _tm = _env.type_manager
        _em = _env.expression_manager
        task.initial_defaults.update({_tm.RealType():_em.Real(Fraction(0))})
        task.initial_defaults.update({_tm.IntType() :_em.Int(0)})
        task.initial_defaults.update({_tm.BoolType() :_em.Bool(False)})

        all_fluent_expressions = []
        for fluent in task.fluents:
            fluent_expressions = list(get_all_fluent_exp(task, fluent))
            all_fluent_expressions.extend(fluent_expressions)
        fluentslist = all_fluent_expressions

        initialized_fluents  = list(task.explicit_initial_values.keys())
        unintialized_fluents = list(filter(lambda x: not x in initialized_fluents, fluentslist))

        # Object fluents have no sensible default — they must be explicitly initialized.
        obj_fluents_missing_init = [fe for fe in unintialized_fluents
                                    if fe.type not in task.initial_defaults]
        if obj_fluents_missing_init:
            names = [str(f) for f in obj_fluents_missing_init]
            raise UPException(
                f"Object fluent(s) missing initial value (no default exists): {', '.join(names)}. "
                "All object fluents must be explicitly initialized in the problem's :init block.")

        real_int_fluents_to_init = []
        for fe in unintialized_fluents:
            if fe.type.is_real_type() or fe.type.is_int_type():
                real_int_fluents_to_init.append(fe)

        if real_int_fluents_to_init:
            fluent_names = [str(f) for f in real_int_fluents_to_init]
            self._log_warning(f"WARNING: Initializing {len(real_int_fluents_to_init)} real/integer fluents to 0")
            self._log_warning("         This is not semantically correct but is done in practice for planning purposes.")

        for fe in unintialized_fluents:
            task.set_initial_value(fe, task.initial_defaults[fe.type])

    def _compile_problem(self, problem: Problem):
        current_problem = problem
        compilation_maps = []
        step_names = []
        enabled = set(self._up_compilers)

        self._log_verbose(
            f"UP compiler pipeline: {', '.join(enabled) if enabled else 'none'}")

        def _run(label, make_compiler, compilation_kind, wrap_exc=False):
            nonlocal current_problem
            compiler = make_compiler()
            if not compiler.supports(current_problem.kind):
                self._log_verbose(f"  {label}: not needed for this problem kind.")
                return
            self._log_verbose(f"  Applying {label}...")
            try:
                result = compiler.compile(current_problem, compilation_kind)
            except Exception as exc:
                if wrap_exc:
                    self._log_warning(f"  {label} skipped due to error: {exc}")
                    return
                raise
            current_problem = result.problem
            compilation_maps.append(result)
            step_names.append(label)
            self._log_verbose(f"  {label} completed.")

        # [XTS] IPAR: expands bounded-int action/fluent params and handles array fluents.
        # This compiler is the XTS extension point; the others below are stock UP compilers.
        if 'IPAR' in enabled:
            from unified_planning.engines.compilers.int_parameter_actions_remover import IntParameterActionsRemover
            _run("IntParameterActionsRemover",
                 IntParameterActionsRemover,
                 CompilationKind.INT_PARAMETER_ACTIONS_REMOVING)

        # QR: removes existential/universal quantifiers
        if 'QR' in enabled:
            _run("QuantifiersRemover",
                 QuantifiersRemover,
                 CompilationKind.QUANTIFIERS_REMOVING)

        # CNF: normalizes disjunctive goals/preconditions into CNF
        if 'CNF' in enabled and not self._no_cnf_normalization:
            _run("CNFConditionCompiler",
                 CNFConditionCompiler,
                 CompilationKind.GROUNDING,
                 wrap_exc=True)

        # BTR: removes bounded types
        if 'BTR' in enabled:
            _run("BoundedTypesRemover",
                 BoundedTypesRemover,
                 CompilationKind.BOUNDED_TYPES_REMOVING)

        # NCR: removes negative conditions
        if 'NCR' in enabled:
            _run("NegativeConditionsRemover",
                 NegativeConditionsRemover,
                 CompilationKind.NEGATIVE_CONDITIONS_REMOVING)

        # DCR: removes all disjunctive conditions
        if 'DCR' in enabled:
            _run("DisjunctiveConditionsRemover",
                 DisjunctiveConditionsRemover,
                 CompilationKind.DISJUNCTIVE_CONDITIONS_REMOVING)

        # CER: removes conditional effects
        if 'CER' in enabled:
            _run("ConditionalEffectsRemover",
                 ConditionalEffectsRemover,
                 CompilationKind.CONDITIONAL_EFFECTS_REMOVING)

        # GR: grounds actions (also triggered by legacy _up_grounding flag)
        if 'GR' in enabled or self._up_grounding:
            _run("Grounder",
                 Grounder,
                 CompilationKind.GROUNDING)

        self._log_verbose("Problem compilation pipeline completed.")

        class CombinedCompilationResult:
            def __init__(self, problem, compilation_maps, step_names):
                self.problem = problem
                self.compilation_maps = compilation_maps
                self.step_names = step_names

            def map_back_action_instance(self, action_instance):
                current_ai = action_instance
                for comp_result in reversed(self.compilation_maps):
                    if hasattr(comp_result, 'map_back_action_instance'):
                        current_ai = comp_result.map_back_action_instance(current_ai)
                        if current_ai is None:
                            return None
                return current_ai

        combined_result = CombinedCompilationResult(current_problem, compilation_maps, step_names)
        return current_problem, combined_result

    # ── Protobuf result reading ──────────────────────────────────────────

    @staticmethod
    def _check_no_nested_fluents(problem: Problem):
        """Reject function composition (nested fluent terms) which is not supported.

        Detects patterns like (city-of (vehicle-location ?t)) where one fluent
        application appears as an argument to another fluent application.
        """
        def has_nested_fluent(node, inside_fluent=False):
            if node.is_fluent_exp():
                if inside_fluent:
                    return node
                for arg in node.args:
                    result = has_nested_fluent(arg, inside_fluent=True)
                    if result is not None:
                        return result
                return None
            for arg in node.args:
                result = has_nested_fluent(arg, inside_fluent)
                if result is not None:
                    return result
            return None

        for action in problem.actions:
            for prec in action.preconditions:
                nested = has_nested_fluent(prec)
                if nested is not None:
                    raise UPException(
                        f"Function composition (nested fluent terms) is not supported. "
                        f"Action '{action.name}' precondition contains nested fluent: {nested}. "
                        f"Rewrite using auxiliary parameters or boolean predicates.")
            for effect in action.effects:
                nested = has_nested_fluent(effect.value)
                if nested is not None:
                    raise UPException(
                        f"Function composition (nested fluent terms) is not supported. "
                        f"Action '{action.name}' effect value contains nested fluent: {nested}. "
                        f"Rewrite using auxiliary parameters or boolean predicates.")
                nested = has_nested_fluent(effect.fluent)
                if nested is not None:
                    raise UPException(
                        f"Function composition (nested fluent terms) is not supported. "
                        f"Action '{action.name}' effect fluent contains nested fluent: {nested}. "
                        f"Rewrite using auxiliary parameters or boolean predicates.")

        for goal in problem.goals:
            nested = has_nested_fluent(goal)
            if nested is not None:
                raise UPException(
                    f"Function composition (nested fluent terms) is not supported. "
                    f"Goal contains nested fluent: {nested}. "
                    f"Rewrite using auxiliary parameters or boolean predicates.")

    def _prepare_and_serialize(self, problem: Problem):
        """Initialize fluents, compile, serialize to protobuf, create temp files.

        Returns (compiled_problem, compilation_result,
                 problem_filepath, solution_filepath).
        """
        self._check_no_nested_fluents(problem)

        pre_compilation_maps = []

        # Optionally compile away usertype (object) fluents
        if self._compile_usertype_fluents and problem.kind.has_object_fluents():
            self._log_verbose("  Removing usertype fluents (UP compilation)...")
            try:
                utf_result = UsertypeFluentsRemover().compile(
                    problem, CompilationKind.USERTYPE_FLUENTS_REMOVING
                )
            except Exception as exc:
                raise UPException(
                    f"UsertypeFluentsRemover failed — this domain likely uses "
                    f"hierarchical typing, which is a known UP limitation. "
                    f"Use compile_usertype_fluents=False (the default) instead. "
                    f"Original error: {exc}"
                ) from exc
            problem = utf_result.problem
            pre_compilation_maps.append(utf_result)
            self._log_verbose("  Usertype fluents removed.")

        self._initialize_fluents(problem)
        compiled_problem, compilation_result = self._compile_problem(problem)

        # Prepend any pre-compilation maps so map_back_action_instance unwinds them last
        compilation_result.compilation_maps = pre_compilation_maps + compilation_result.compilation_maps

        pb_problem_msg = self._pb_writer.convert(compiled_problem)

        problem_file = tempfile.NamedTemporaryFile(suffix=".pb", delete=False)
        solution_file = tempfile.NamedTemporaryFile(suffix=".pb", delete=False)
        problem_filepath = problem_file.name
        solution_filepath = solution_file.name
        problem_file.close()
        solution_file.close()

        with open(problem_filepath, "wb") as f:
            f.write(pb_problem_msg.SerializeToString())

        return compiled_problem, compilation_result, problem_filepath, solution_filepath

    def _read_protobuf_result(self, solution_filepath: str,
                              compiled_problem: Problem,
                              compilation_result,
                              original_problem: Problem) -> PlanGenerationResult:
        """Read the protobuf solution, map actions back, validate."""
        with open(solution_filepath, "rb") as f:
            pb_msg = up_pb2.PlanGenerationResult()  # type: ignore
            pb_msg.ParseFromString(f.read())

        result_from_protobuf = self._pb_reader.convert(pb_msg, compiled_problem)

        final_plan = None
        if result_from_protobuf.plan:
            try:
                new_actions: list[ActionInstance] = []

                if hasattr(result_from_protobuf.plan, 'actions'):
                    plan_actions = result_from_protobuf.plan.actions
                else:
                    plan_actions = []

                for ai in plan_actions:
                    mapped_ai = compilation_result.map_back_action_instance(ai)
                    assert mapped_ai is not None
                    new_actions.append(mapped_ai)

                final_plan = SequentialPlan(new_actions)
            except Exception as e:
                self._log_error(f"Error mapping plan back: {e}")
                return PlanGenerationResult(
                    PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                    log_messages=[LogMessage(level=LogLevel.ERROR, message=f"Error mapping plan back: {e}")]
                )
        self._log(f"Status from planner: {result_from_protobuf.status.name}")

        # Validate the plan if one was found
        if final_plan is not None and result_from_protobuf.status in (
                PlanGenerationResultStatus.SOLVED_SATISFICING,
                PlanGenerationResultStatus.SOLVED_OPTIMALLY):
            plan_is_valid = self._validate_plan(original_problem, final_plan)

            if plan_is_valid is None:
                validation_log = LogMessage(
                    level=LogLevel.WARNING,
                    message="Plan validation skipped"
                )
                result_log_messages = list(result_from_protobuf.log_messages) + [validation_log]
            elif not plan_is_valid:
                validation_log = LogMessage(
                    level=LogLevel.WARNING,
                    message="Plan validation failed"
                )
                result_log_messages = list(result_from_protobuf.log_messages) + [validation_log]
            else:
                result_log_messages = result_from_protobuf.log_messages
        else:
            result_log_messages = result_from_protobuf.log_messages

        return PlanGenerationResult(
            result_from_protobuf.status,
            final_plan,
            self.name,
            log_messages=result_log_messages,
            metrics=result_from_protobuf.metrics
        )

    # ── IPC plan file parsing ────────────────────────────────────────────

    def _parse_ipc_plan(self, plan_filepath: str,
                        compiled_problem: Problem,
                        compilation_result) -> Optional[SequentialPlan]:
        """Parse an IPC-format plan file back into a UP SequentialPlan.

        IPC format: lines of ``(action_name param1 param2 ...)``
        with ``;`` comment lines.  Action/parameter names match those used
        by the C++ protobuf serialiser.
        """
        try:
            with open(plan_filepath, "r") as f:
                lines = f.readlines()
        except FileNotFoundError:
            return None

        # Build lookups from the compiled problem
        action_map = {a.name: a for a in compiled_problem.actions}
        object_map = {}
        for obj in compiled_problem.all_objects:
            object_map[obj.name] = obj

        new_actions: list[ActionInstance] = []
        for raw_line in lines:
            line = raw_line.strip()
            if not line or line.startswith(";"):
                continue
            # Strip enclosing parentheses: "(fly p1 c1 c2)" → "fly p1 c1 c2"
            if line.startswith("(") and line.endswith(")"):
                line = line[1:-1].strip()
            parts = line.split()
            if not parts:
                continue

            action_name = parts[0]
            param_names = parts[1:]

            action = action_map.get(action_name)
            if action is None:
                self._log_warning(f"  IPC parse: unknown action '{action_name}', skipping")
                continue

            params = []
            for pn in param_names:
                obj = object_map.get(pn)
                if obj is None:
                    self._log_warning(f"  IPC parse: unknown object '{pn}' in action '{action_name}', skipping action")
                    break
                params.append(obj)
            else:
                # All params resolved — create ActionInstance
                ai = ActionInstance(action, tuple(params))
                mapped_ai = compilation_result.map_back_action_instance(ai)
                if mapped_ai is not None:
                    new_actions.append(mapped_ai)

        return SequentialPlan(new_actions) if new_actions else None


# ═══════════════════════════════════════════════════════════════════════
# Satisficing planner (oneshot, default)
# ═══════════════════════════════════════════════════════════════════════

class RantanPlanPlanner(_RantanPlanBase, OneshotPlannerMixin):

    def __init__(self, **options):
        _RantanPlanBase.__init__(self, **options)
        OneshotPlannerMixin.__init__(self)

    def _solve(self, problem: Problem,
              heuristic: Optional[Callable] = None,
              timeout: Optional[float] = None,
              output_stream: Optional[IO[str]] = None) -> PlanGenerationResult:

        compiled_problem, compilation_result, problem_filepath, solution_filepath = \
            self._prepare_and_serialize(problem)

        try:
            command = self._build_command(problem_filepath, solution_filepath, timeout)
            self._log_verbose(f"Running planner: {' '.join(command)}")

            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=False
            )

            stdout_thread = threading.Thread(target=self._stream_output, args=(process.stdout, ""))
            stderr_thread = threading.Thread(target=self._stream_output, args=(process.stderr, "ERROR: "))
            stdout_thread.daemon = True
            stderr_thread.daemon = True
            stdout_thread.start()
            stderr_thread.start()

            process_timeout = timeout + 10 if timeout else None
            try:
                return_code = process.wait(timeout=process_timeout)
            except subprocess.TimeoutExpired:
                process.kill()
                self._log("Planner timed out.")
                return PlanGenerationResult(
                    PlanGenerationResultStatus.TIMEOUT, None, self.name,
                    log_messages=[LogMessage(level=LogLevel.INFO, message="Planner timed out.")]
                )

            stdout_thread.join(timeout=1.0)
            stderr_thread.join(timeout=1.0)

            if return_code != 0:
                error_msg = f"The planner failed with return code {return_code}."
                if return_code == -11:
                    error_msg += " - The error might be a segmentation fault (SIGSEGV)."
                self._log_error(error_msg)
                return PlanGenerationResult(
                    PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                    log_messages=[LogMessage(level=LogLevel.ERROR, message=error_msg)]
                )

            return self._read_protobuf_result(
                solution_filepath, compiled_problem, compilation_result, problem)

        except Exception as e:
            self._log_error(f"An error occurred: {e}")
            return PlanGenerationResult(
                PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                log_messages=[LogMessage(level=LogLevel.ERROR, message=str(e))]
            )
        finally:
            for filepath in [problem_filepath, solution_filepath]:
                if os.path.exists(filepath):
                    os.remove(filepath)


# ═══════════════════════════════════════════════════════════════════════
# Optimal planner (oneshot, guarantees optimality)
# ═══════════════════════════════════════════════════════════════════════

class RantanPlanOptimalPlanner(_RantanPlanBase, OneshotPlannerMixin):

    def __init__(self, **options):
        _RantanPlanBase.__init__(self, **options)
        OneshotPlannerMixin.__init__(self)
        self._mode = "optimal"  # Force optimal mode

    @staticmethod
    def satisfies(optimality_guarantee: OptimalityGuarantee) -> bool:
        return True

    def _solve(self, problem: Problem,
              heuristic: Optional[Callable] = None,
              timeout: Optional[float] = None,
              output_stream: Optional[IO[str]] = None) -> PlanGenerationResult:

        compiled_problem, compilation_result, problem_filepath, solution_filepath = \
            self._prepare_and_serialize(problem)

        try:
            command = self._build_command(problem_filepath, solution_filepath, timeout)
            self._log_verbose(f"Running planner: {' '.join(command)}")

            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=False
            )

            stdout_thread = threading.Thread(target=self._stream_output, args=(process.stdout, ""))
            stderr_thread = threading.Thread(target=self._stream_output, args=(process.stderr, "ERROR: "))
            stdout_thread.daemon = True
            stderr_thread.daemon = True
            stdout_thread.start()
            stderr_thread.start()

            process_timeout = timeout + 10 if timeout else None
            try:
                return_code = process.wait(timeout=process_timeout)
            except subprocess.TimeoutExpired:
                process.kill()
                self._log("Planner timed out.")
                return PlanGenerationResult(
                    PlanGenerationResultStatus.TIMEOUT, None, self.name,
                    log_messages=[LogMessage(level=LogLevel.INFO, message="Planner timed out.")]
                )

            stdout_thread.join(timeout=1.0)
            stderr_thread.join(timeout=1.0)

            if return_code != 0:
                error_msg = f"The planner failed with return code {return_code}."
                if return_code == -11:
                    error_msg += " - The error might be a segmentation fault (SIGSEGV)."
                self._log_error(error_msg)
                return PlanGenerationResult(
                    PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                    log_messages=[LogMessage(level=LogLevel.ERROR, message=error_msg)]
                )

            return self._read_protobuf_result(
                solution_filepath, compiled_problem, compilation_result, problem)

        except Exception as e:
            self._log_error(f"An error occurred: {e}")
            return PlanGenerationResult(
                PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                log_messages=[LogMessage(level=LogLevel.ERROR, message=str(e))]
            )
        finally:
            for filepath in [problem_filepath, solution_filepath]:
                if os.path.exists(filepath):
                    os.remove(filepath)


# ═══════════════════════════════════════════════════════════════════════
# Anytime planner (yields improving plans via file-system polling)
# ═══════════════════════════════════════════════════════════════════════

class RantanPlanAnytimePlanner(_RantanPlanBase, AnytimePlannerMixin):

    def __init__(self, **options):
        _RantanPlanBase.__init__(self, **options)
        AnytimePlannerMixin.__init__(self)
        self._mode = "anytime"  # Force anytime mode

    @staticmethod
    def ensures(anytime_guarantee: AnytimeGuarantee) -> bool:
        return anytime_guarantee == AnytimeGuarantee.INCREASING_QUALITY

    def _get_solutions(self, problem: Problem,
                       timeout: Optional[float] = None,
                       output_stream: Optional[IO[str]] = None,
                       ) -> Iterator[PlanGenerationResult]:

        compiled_problem, compilation_result, problem_filepath, solution_filepath = \
            self._prepare_and_serialize(problem)

        plan_base = self._output_plan or "plan.txt"

        # Remove stale plan files from previous runs so the poller
        # doesn't pick them up as new solutions.
        i = 1
        while os.path.exists(f"{plan_base}.{i}"):
            os.remove(f"{plan_base}.{i}")
            i += 1

        try:
            command = self._build_command(problem_filepath, solution_filepath, timeout)
            self._log_verbose(f"Running planner: {' '.join(command)}")

            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=False
            )

            q: Queue[PlanGenerationResult] = Queue()

            def run():
                # Stream stdout/stderr in sub-threads
                stdout_thread = threading.Thread(
                    target=self._stream_output, args=(process.stdout, ""))
                stderr_thread = threading.Thread(
                    target=self._stream_output, args=(process.stderr, "ERROR: "))
                stdout_thread.daemon = stderr_thread.daemon = True
                stdout_thread.start()
                stderr_thread.start()

                # Poll for new plan files while process is alive
                solution_count = 0
                while process.poll() is None:
                    next_file = f"{plan_base}.{solution_count + 1}"
                    if os.path.exists(next_file):
                        solution_count += 1
                        plan = self._parse_ipc_plan(
                            next_file, compiled_problem, compilation_result)
                        if plan:
                            q.put(PlanGenerationResult(
                                PlanGenerationResultStatus.INTERMEDIATE,
                                plan, self.name))
                    else:
                        time.sleep(0.1)

                # Process finished — pick up any remaining plan files
                while os.path.exists(f"{plan_base}.{solution_count + 1}"):
                    solution_count += 1
                    plan = self._parse_ipc_plan(
                        f"{plan_base}.{solution_count}",
                        compiled_problem, compilation_result)
                    if plan:
                        q.put(PlanGenerationResult(
                            PlanGenerationResultStatus.INTERMEDIATE,
                            plan, self.name))

                stdout_thread.join(timeout=1.0)
                stderr_thread.join(timeout=1.0)

                # Final result from protobuf (carries correct status)
                return_code = process.returncode
                if return_code != 0:
                    error_msg = f"The planner failed with return code {return_code}."
                    if return_code == -11:
                        error_msg += " - The error might be a segmentation fault (SIGSEGV)."
                    self._log_error(error_msg)
                    q.put(PlanGenerationResult(
                        PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                        log_messages=[LogMessage(level=LogLevel.ERROR, message=error_msg)]))
                else:
                    try:
                        final = self._read_protobuf_result(
                            solution_filepath, compiled_problem,
                            compilation_result, problem)
                        q.put(final)
                    except Exception as e:
                        self._log_error(f"Error reading final result: {e}")
                        q.put(PlanGenerationResult(
                            PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                            log_messages=[LogMessage(level=LogLevel.ERROR, message=str(e))]))

            t = threading.Thread(target=run, daemon=True)
            t.start()

            # Main thread: yield results as they arrive
            status = PlanGenerationResultStatus.INTERMEDIATE
            while status == PlanGenerationResultStatus.INTERMEDIATE:
                res = q.get()
                status = res.status
                yield res

            t.join()

        except Exception as e:
            self._log_error(f"An error occurred: {e}")
            yield PlanGenerationResult(
                PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                log_messages=[LogMessage(level=LogLevel.ERROR, message=str(e))]
            )
        finally:
            for filepath in [problem_filepath, solution_filepath]:
                if os.path.exists(filepath):
                    os.remove(filepath)
