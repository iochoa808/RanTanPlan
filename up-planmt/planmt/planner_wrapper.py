import unified_planning as up
from typing import Callable, IO, Optional
from unified_planning.engines.results import PlanGenerationResult, LogMessage
from unified_planning.engines.mixins import OneshotPlannerMixin
from unified_planning.engines import PlanGenerationResultStatus, Engine
from unified_planning.grpc.proto_writer import ProtobufWriter
from unified_planning.grpc.proto_reader import ProtobufReader
from unified_planning.exceptions import UPException
from unified_planning.model import ProblemKind

import subprocess
import tempfile
import os

class planMTPlanner(Engine, OneshotPlannerMixin):
    def __init__(self, **options):

        # Initialize the parent classes
        Engine.__init__(self)
        OneshotPlannerMixin.__init__(self)
        
        self._writer = ProtobufWriter()
        self._reader = ProtobufReader()

        # Get executable path from options or use default
        executable_path = options.get('executable_path', None)
        
        if executable_path:
            self.executable_path = executable_path
        else:
            # Try to determine a default path (adjust as needed)
            # This assumes the C++ executable 'my_planner_cpp' is in planner/build/
            # relative to the root of your project structure.
            # THIS PATH WILL LIKELY NEED ADJUSTMENT BASED ON YOUR ACTUAL SETUP
            # OR YOU SHOULD PASS IT EXPLICITLY.
            base_path = os.path.join(os.path.dirname(__file__), "..", "..", "planner", "build")
            exe_name = "planmt"  
            self.executable_path = os.path.join(base_path, exe_name)

        if not os.path.exists(self.executable_path):
            raise UPException(
                f"planmt executable not found at: {self.executable_path}. "
                "Please build the planner and/or provide the correct path."
            )

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

    def _solve(self, problem: 'up.model.Problem',
              callback: Optional[Callable[['up.engines.PlanGenerationResult'], None]] = None,
              timeout: Optional[float] = None,
              output_stream: Optional[IO[str]] = None) -> 'up.engines.PlanGenerationResult':

        pb_problem_msg = self._writer.convert(problem)

        with tempfile.NamedTemporaryFile(mode="wb", suffix=".pb", delete=False) as problem_file, \
             tempfile.NamedTemporaryFile(mode="rb", suffix=".pb", delete=False) as solution_file:
            problem_filepath = problem_file.name
            solution_filepath = solution_file.name

        try:
            problem_file_obj = open(problem_filepath, "wb")
            problem_file_obj.write(pb_problem_msg.SerializeToString())
            problem_file_obj.close() # Close before C++ opens it

            command = [self.executable_path, problem_filepath, solution_filepath]
            if output_stream is not None:
                output_stream.write(f"Running planner: {' '.join(command)}\n")

            # Run the C++ planner, but also handle the case where it crashes
            process = subprocess.run(command, timeout=timeout, capture_output=True, text=True)

            # Always show stdout and stderr in the output stream
            if output_stream is not None:
                if process.stdout:
                    output_stream.write(f"planner stdout:\n{process.stdout}\n")
                    output_stream.flush()  # Ensure immediate visibility
                if process.stderr:
                    output_stream.write(f"planner stderr:\n{process.stderr}\n")
                    output_stream.flush()  # Ensure immediate visibility

            # Create log messages that will be visible to the user
            log_messages = []
            
            # Add stdout as info messages
            if process.stdout:
                for line in process.stdout.strip().split('\n'):
                    if line.strip():
                        log_messages.append(LogMessage(level=up.engines.results.LogLevel.INFO, message=f"C++ stdout: {line.strip()}"))
            
            # Add stderr as warning/error messages  
            if process.stderr:
                for line in process.stderr.strip().split('\n'):
                    if line.strip():
                        log_messages.append(LogMessage(level=up.engines.results.LogLevel.ERROR, message=f"C++ stderr: {line.strip()}"))

            if process.returncode != 0:
                error_msg = f"The planner failed with return code {process.returncode}."
                if process.returncode == -11:
                    error_msg += " - The error might be a segmentation fault ..."
                
                log_messages.append(LogMessage(level=up.engines.results.LogLevel.ERROR, message=error_msg))
                
                if output_stream is not None:
                    output_stream.write(f"ERROR: {error_msg}\n")
                    output_stream.flush()
                
                return PlanGenerationResult(
                    PlanGenerationResultStatus.INTERNAL_ERROR,
                    None,
                    self.name,
                    log_messages=log_messages
                )

            # Read the solution from the file written by C++
            solution_file_obj = open(solution_filepath, "rb") # Re-open after C++ writes it
            pb_plan_generation_result_msg = up.grpc.generated.unified_planning_pb2.PlanGenerationResult()
            pb_plan_generation_result_msg.ParseFromString(solution_file_obj.read())
            solution_file_obj.close()

            # Convert protobuf PlanGenerationResult back to UP's PlanGenerationResult
            # The problem object is needed to correctly map back identifiers
            up_plan_result = self._reader.convert(pb_plan_generation_result_msg, problem)
            
            if output_stream is not None:
                output_stream.write(f"Plan found: {up_plan_result.plan is not None}\n")
                output_stream.write(f"Status: {up_plan_result.status}\n")

            return up_plan_result

        except subprocess.TimeoutExpired:
            if output_stream is not None:
                output_stream.write("Planner timed out.\n")
            return PlanGenerationResult(
                PlanGenerationResultStatus.TIMEOUT,
                None,
                self.name,
                log_messages=[LogMessage(level=up.engines.results.LogLevel.INFO, message="Planner timed out.")]
            )
        except Exception as e:
            if output_stream is not None:
                output_stream.write(f"An error occurred: {e}\n")
            return PlanGenerationResult(
                PlanGenerationResultStatus.INTERNAL_ERROR,
                None,
                self.name,
                log_messages=[LogMessage(level=up.engines.results.LogLevel.ERROR, message=str(e))]
            )
        finally:
            # Clean up temporary files
            if os.path.exists(problem_filepath):
                os.remove(problem_filepath)
            if os.path.exists(solution_filepath):
                os.remove(solution_filepath)

    def destroy(self):
        pass