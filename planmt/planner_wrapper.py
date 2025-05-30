import unified_planning as up
from typing import Callable, IO, Optional
from unified_planning.engines.results import PlanGenerationResult, LogMessage, LogLevel
from unified_planning.engines.mixins import OneshotPlannerMixin
from unified_planning.engines import PlanGenerationResultStatus, Engine, CompilationKind # Added CompilationKind
from unified_planning.engines.compilers import Grounder # Added Grounder
from unified_planning.grpc.proto_writer import ProtobufWriter
from unified_planning.grpc.proto_reader import ProtobufReader
from unified_planning.exceptions import UPException
from unified_planning.model import ProblemKind
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

    def _create_log_messages(self, process):
        """Create log messages from subprocess output."""
        log_messages = []
        
        if process.stdout:
            for line in process.stdout.strip().split('\n'):
                if line.strip():
                    log_messages.append(LogMessage(level=LogLevel.INFO, message=f"C++ stdout: {line.strip()}"))
        
        if process.stderr:
            for line in process.stderr.strip().split('\n'):
                if line.strip():
                    log_messages.append(LogMessage(level=LogLevel.ERROR, message=f"C++ stderr: {line.strip()}"))
        
        return log_messages

    def _log_to_stream(self, output_stream, message):
        """Helper to write and flush to output stream."""
        if output_stream:
            output_stream.write(f"{message}\n")
            output_stream.flush()

    def _solve(self, problem: 'up.model.Problem',
              callback: Optional[Callable[['up.engines.PlanGenerationResult'], None]] = None,
              timeout: Optional[float] = None,
              output_stream: Optional[IO[str]] = None) -> 'up.engines.PlanGenerationResult':

        # TODO: implement a sequence of compiler applications depending on the problem kind and encoder capabilities
        self._log_to_stream(output_stream, "Starting grounding process.")
        grounder = Grounder()
        grounding_result = grounder.compile(problem, CompilationKind.GROUNDING)
        grounded_problem = grounding_result.problem
        self._log_to_stream(output_stream, "Grounding process completed.")

        pb_problem_msg = self._writer.convert(grounded_problem)

        with tempfile.NamedTemporaryFile(mode="wb", suffix=".pb", delete=False) as problem_file, \
             tempfile.NamedTemporaryFile(mode="rb", suffix=".pb", delete=False) as solution_file:
            problem_filepath = problem_file.name
            solution_filepath = solution_file.name

        try:
            # Write problem to file
            with open(problem_filepath, "wb") as f:
                f.write(pb_problem_msg.SerializeToString())

            command = [self.executable_path, problem_filepath, solution_filepath]
            self._log_to_stream(output_stream, f"Running planner: {' '.join(command)}")

            # Run the C++ planner
            process = subprocess.run(command, timeout=timeout, capture_output=True, text=True)

            # Log subprocess output
            if process.stdout:
                self._log_to_stream(output_stream, f"planner stdout:\n{process.stdout}")
            if process.stderr:
                self._log_to_stream(output_stream, f"planner stderr:\n{process.stderr}")

            log_messages_from_subprocess = self._create_log_messages(process)

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
                self._log_to_stream(output_stream, "Mapping plan back to original problem.")
                try:
                    final_plan = grounding_result.map_back_plan(result_from_protobuf.plan, problem)
                    self._log_to_stream(output_stream, "Plan mapping complete.")
                except Exception as e:
                    self._log_to_stream(output_stream, f"Error mapping plan back: {e}")
                    all_log_messages.append(LogMessage(level=LogLevel.ERROR, message=f"Error mapping plan back: {e}"))
                    # Depending on severity, might change status or return error
                    return PlanGenerationResult(
                        PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                        log_messages=all_log_messages
                    )
            
            self._log_to_stream(output_stream, f"Plan found (after mapping): {final_plan is not None}")
            self._log_to_stream(output_stream, f"Status from planner: {result_from_protobuf.status}")

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