from typing import Callable, IO, Optional

import unified_planning as up
from unified_planning.shortcuts import Fraction
from unified_planning.engines.results import PlanGenerationResult, LogMessage, LogLevel
from unified_planning.engines.mixins import OneshotPlannerMixin
from unified_planning.engines import PlanGenerationResultStatus, Engine, CompilationKind
from unified_planning.engines.compilers import Grounder
from unified_planning.grpc.proto_writer import ProtobufWriter
from unified_planning.grpc.proto_reader import ProtobufReader
from unified_planning.exceptions import UPException
from unified_planning.model import ProblemKind, Problem
from unified_planning.plans import SequentialPlan, ActionInstance
from unified_planning.shortcuts import get_environment
from unified_planning.model.fluent import get_all_fluent_exp
import unified_planning.grpc.generated.unified_planning_pb2 as up_pb2

import subprocess
import tempfile
import os

class planMTPlanner(Engine, OneshotPlannerMixin):
    def __init__(self, **options):
        Engine.__init__(self)
        OneshotPlannerMixin.__init__(self)
        
        self._writer = ProtobufWriter()
        self._reader = ProtobufReader()
        self.executable_path = self._find_executable(options.get('executable_path'))
        self._verbose = options.get('verbose', False)

    def _find_executable(self, provided_path):
        """Find the planmt executable, trying various locations."""
        if provided_path:
            return provided_path
            
        package_dir = os.path.dirname(__file__)
        candidates = [
            os.path.join(package_dir, "bin", "planmt"),
            os.path.join(package_dir, "cpp", "build", "planmt"),
            os.path.join(package_dir, "..", "planner", "build", "planmt"),
            "planmt"  # System PATH
        ]
        
        for path in candidates:
            if os.path.exists(path) or (path == "planmt" and self._check_executable_in_path(path)):
                if os.path.exists(path):
                    return path
        
        # Use cpp/build path as default for error message
        default_path = os.path.join(package_dir, "cpp", "build", "planmt")
        if not os.path.exists(default_path):
            raise UPException(
                f"planmt executable not found. Tried: {', '.join(candidates)}. "
                "Please build the planner and/or provide the correct path."
            )
        return default_path

    def _check_executable_in_path(self, executable_name):
        """Check if an executable exists in the system PATH."""
        try:
            subprocess.run([executable_name, "--help"], 
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5)
            return True
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError, FileNotFoundError):
            return False

    @property
    def name(self) -> str:
        return "planMT"

    @staticmethod
    def supported_kind():
        supported_kind = ProblemKind()
        supported_kind.set_problem_class("ACTION_BASED")
        supported_kind.set_problem_type("GENERAL_NUMERIC_PLANNING")
        supported_kind.set_typing('FLAT_TYPING')
        supported_kind.set_typing('HIERARCHICAL_TYPING')
        supported_kind.set_numbers('CONTINUOUS_NUMBERS')
        supported_kind.set_numbers('DISCRETE_NUMBERS')
        supported_kind.set_fluents_type('NUMERIC_FLUENTS')
        supported_kind.set_numbers('BOUNDED_TYPES')
        supported_kind.set_fluents_type('OBJECT_FLUENTS')
        supported_kind.set_conditions_kind('NEGATIVE_CONDITIONS')
        supported_kind.set_conditions_kind('DISJUNCTIVE_CONDITIONS')
        supported_kind.set_conditions_kind('EQUALITIES')
        supported_kind.set_conditions_kind('EXISTENTIAL_CONDITIONS')
        supported_kind.set_conditions_kind('UNIVERSAL_CONDITIONS')
        supported_kind.set_effects_kind('CONDITIONAL_EFFECTS')
        supported_kind.set_effects_kind('INCREASE_EFFECTS')
        supported_kind.set_effects_kind('DECREASE_EFFECTS')
        supported_kind.set_effects_kind('FLUENTS_IN_NUMERIC_ASSIGNMENTS')
        return supported_kind

    @staticmethod
    def supports(problem_kind):
        return problem_kind <= planMTPlanner.supported_kind()

    def _create_log_messages(self, process, output_stream):
        """Create log messages from subprocess output and optionally stream to output."""
        log_messages = []
        
        # Handle stdout - only add to LogMessages if verbose, but always stream if verbose
        if process.stdout:
            stdout_lines = [line.strip() for line in process.stdout.strip().split('\n') if line.strip()]
            if stdout_lines:
                    #self._log_to_stream(output_stream, f"stdout:\n{chr(10).join(stdout_lines)}")
                    for line in stdout_lines:
                        log_messages.append(LogMessage(level=LogLevel.INFO, message=line))
        
        # Handle stderr - always add to LogMessages and always stream (errors should always be visible)
        if process.stderr:
            stderr_lines = [line.strip() for line in process.stderr.strip().split('\n') if line.strip()]
            if stderr_lines:
                #self._log_to_stream(output_stream, f"stderr:\n{chr(10).join(stderr_lines)}")
                for line in stderr_lines:
                    log_messages.append(LogMessage(level=LogLevel.ERROR, message=line))
        
        return log_messages

    def _log_to_stream(self, output_stream, message):
        """Helper to write and flush to output stream."""
        if output_stream:
            output_stream.write(f"{message}\n")
            output_stream.flush()

    def _initialize_fluents(self, task: Problem):
        """
        Initialize the int and real fluents of a given task with a default value of 0.
        Any Boolean fluent is initialized with a default value of False.
        Args:
            task (Problem): The UP task object
        Updates:
            task.initial_defaults: Adds default values for real and integer types.
            task.explicit_initial_values: Sets initial values for uninitialized fluents.
        """
        # update the initial defaults to account for real and integer types.
        _env = get_environment()
        _tm = _env.type_manager
        _em = _env.expression_manager
        task.initial_defaults.update({_tm.RealType():_em.Real(Fraction(0))})
        task.initial_defaults.update({_tm.IntType() :_em.Int(0)})
        task.initial_defaults.update({_tm.BoolType() :_em.Bool(False)})

        # Get all fluent expressions from the problem and flatten into a single list
        all_fluent_expressions = []
        for fluent in task.fluents:
            fluent_expressions = list(get_all_fluent_exp(task, fluent))
            all_fluent_expressions.extend(fluent_expressions)
        fluentslist = all_fluent_expressions

        # now lets separate the initialized and uninitialized fluents.
        initialized_fluents  = list(task.explicit_initial_values.keys())
        unintialized_fluents = list(filter(lambda x: not x in initialized_fluents, fluentslist))
        
        # Check for real or integer fluents being initialized to 0 and issue warning
        real_int_fluents_to_init = []
        for fe in unintialized_fluents:
            if fe.type == _tm.RealType() or fe.type == _tm.IntType():
                real_int_fluents_to_init.append(fe)
        
        if real_int_fluents_to_init:
            fluent_names = [str(f) for f in real_int_fluents_to_init]
            print(f"WARNING: Initializing {len(real_int_fluents_to_init)} real/integer fluents to 0: {', '.join(fluent_names)}")
            print("         This is not semantically correct but is done in practice for planning purposes.")
        
        # update the initial values for the fluents that are not initialized.
        for fe in unintialized_fluents:
            task.set_initial_value(fe, task.initial_defaults[fe.type]) 


    def _solve(self, problem: Problem,
              callback: Optional[Callable[[PlanGenerationResult], None]] = None,
              timeout: Optional[float] = None,
              output_stream: Optional[IO[str]] = None) -> PlanGenerationResult:

        # initialise all fluents that are not set in the initial state so we do not have to deal with
        # uninitialized fluents in the C++ planner.
        self._initialize_fluents(problem) 

        # TODO: implement a sequence of compiler applications depending on the problem kind and encoder capabilities
        self._log_to_stream(output_stream, "Starting grounding process.")
        grounder = Grounder()
        grounding_result = grounder.compile(problem, CompilationKind.GROUNDING)
        grounded_problem = grounding_result.problem
        self._log_to_stream(output_stream, "Grounding process completed.")

        pb_problem_msg = self._writer.convert(grounded_problem)

        # Create temporary files for problem and solution
        problem_file = tempfile.NamedTemporaryFile(suffix=".pb", delete=False)
        solution_file = tempfile.NamedTemporaryFile(suffix=".pb", delete=False)
        problem_filepath = problem_file.name
        solution_filepath = solution_file.name
        problem_file.close()
        solution_file.close()

        try:
            # Write problem to file
            with open(problem_filepath, "wb") as f:
                f.write(pb_problem_msg.SerializeToString())

            command = [self.executable_path, problem_filepath, solution_filepath]
            self._log_to_stream(output_stream, f"Running planner: {' '.join(command)}")

            # Run the C++ planner
            process = subprocess.run(command, timeout=timeout, capture_output=True, text=True)

            # Create log messages from subprocess output and optionally log to stream
            log_messages_from_subprocess = self._create_log_messages(process, output_stream)

            # Handle process errors
            if process.returncode != 0:
                error_msg = f"The planner failed with return code {process.returncode}."
                if process.returncode == -11: # Specific check for SIGSEGV
                    error_msg += " - The error might be a segmentation fault (SIGSEGV)."
                
                log_messages_from_subprocess.append(LogMessage(level=LogLevel.ERROR, message=error_msg))
                self._log_to_stream(output_stream, f"ERROR: {error_msg}")
                
                return PlanGenerationResult(
                    PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name, log_messages=log_messages_from_subprocess
                )

            # Read and convert solution
            with open(solution_filepath, "rb") as f:
                pb_plan_generation_result_msg = up_pb2.PlanGenerationResult() # type: ignore
                pb_plan_generation_result_msg.ParseFromString(f.read())

            # Convert the protobuf result. This result is for the grounded_problem.
            result_from_protobuf = self._reader.convert(pb_plan_generation_result_msg, grounded_problem)

            # Combine log messages from subprocess and protobuf result
            all_log_messages = log_messages_from_subprocess + (result_from_protobuf.log_messages or [])

            final_plan = None
            if result_from_protobuf.plan:
                try:
                    new_actions: list[ActionInstance] = []
                    for ai in result_from_protobuf.plan.actions:
                        mapped_ai = grounding_result.map_back_action_instance(ai)
                        assert mapped_ai is not None
                        new_actions.append(mapped_ai)
                    final_plan = SequentialPlan(new_actions)
                except Exception as e:
                    self._log_to_stream(output_stream, f"Error mapping plan back: {e}")
                    all_log_messages.append(LogMessage(level=LogLevel.ERROR, message=f"Error mapping plan back: {e}"))
                    return PlanGenerationResult(
                        PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                        log_messages=all_log_messages
                    )
            
            self._log_to_stream(output_stream, f"Plan found (after mapping): {final_plan is not None}")
            self._log_to_stream(output_stream, f"Status from planner: {result_from_protobuf.status.name}")

            return PlanGenerationResult(
                result_from_protobuf.status,
                final_plan,
                self.name,
                log_messages=all_log_messages,
                metrics=result_from_protobuf.metrics
            )

        except subprocess.TimeoutExpired:
            self._log_to_stream(output_stream, "Planner timed out.")
            return PlanGenerationResult(
                PlanGenerationResultStatus.TIMEOUT, None, self.name,
                log_messages=[LogMessage(level=LogLevel.INFO, message="Planner timed out.")]
            )
        except Exception as e:
            self._log_to_stream(output_stream, f"An error occurred: {e}")
            return PlanGenerationResult(
                PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                log_messages=[LogMessage(level=LogLevel.ERROR, message=str(e))]
            )
        finally:
            # Clean up temporary files
            for filepath in [problem_filepath, solution_filepath]:
                if os.path.exists(filepath):
                    os.remove(filepath)

    def destroy(self):
        pass