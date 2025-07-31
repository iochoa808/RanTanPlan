from typing import Callable, IO, Optional

import unified_planning as up
from unified_planning.shortcuts import Fraction, PlanValidator
from unified_planning.engines.results import PlanGenerationResult, LogMessage, LogLevel
from unified_planning.engines.mixins import OneshotPlannerMixin
from unified_planning.engines import PlanGenerationResultStatus, Engine, CompilationKind, ValidationResultStatus
from unified_planning.engines.compilers import Grounder, QuantifiersRemover
from unified_planning.grpc.proto_writer import ProtobufWriter
from unified_planning.grpc.proto_reader import ProtobufReader
from unified_planning.exceptions import UPException
from .delete_then_set_compiler import DeleteThenSetRemover
from unified_planning.model import ProblemKind, Problem
from unified_planning.plans import SequentialPlan, ActionInstance
from unified_planning.shortcuts import get_environment
from unified_planning.model.fluent import get_all_fluent_exp
import unified_planning.grpc.generated.unified_planning_pb2 as up_pb2

import subprocess
import tempfile
import os
import sys
import threading

class planMTPlanner(Engine, OneshotPlannerMixin):
    def __init__(self, **options):
        Engine.__init__(self)
        OneshotPlannerMixin.__init__(self)
        
        self._writer = ProtobufWriter()
        self._reader = ProtobufReader()
        self.executable_path = self._find_executable(options.get('executable_path'))
        self._verbosity = options.get('verbosity')  # None if not specified
        self._parallelism = options.get('parallelism', 'sequential')
        self._propagator = options.get('propagator', 'null')
        self._max_steps = options.get('max_steps')  # None if not specified
        self._no_persist_clauses = options.get('no_persist_clauses', False)

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

    def _stream_output(self, pipe, prefix=""):
        """Stream output from subprocess pipe directly to stdout in real-time."""
        if not pipe:
            return
        try:
            for line in iter(pipe.readline, b''):
                if not line:
                    break
                line_str = line.decode('utf-8', errors='replace').rstrip()
                if line_str:
                    print(f"{prefix}{line_str}")
                    sys.stdout.flush()
        except Exception:
            pass  # Handle any decoding errors silently
        finally:
            if pipe:
                pipe.close()

    def _validate_plan(self, problem: Problem, plan: SequentialPlan) -> bool:
        """
        Validate a plan against the problem using Unified Planning's PlanValidator.
        
        Args:
            problem: The original problem (not grounded)
            plan: The plan to validate (should be mapped back to the original problem)
            
        Returns:
            bool: True if the plan is valid, False otherwise
        """
        try:
            with PlanValidator(problem_kind=problem.kind, plan_kind=plan.kind) as validator:
                validation_result = validator.validate(problem, plan)
                
                if validation_result.status == ValidationResultStatus.VALID:
                    print("Plan validation: VALID")
                    print(f"  The plan with {len(plan.actions)} actions is correct and executable.")
                    return True
                else:
                    print(f"Plan validation: {validation_result.status.name}")
                    if validation_result.log_messages:
                        for log_msg in validation_result.log_messages:
                            print(f"  Validation {log_msg.level.name}: {log_msg.message}")
                    else:
                        print("  No detailed validation messages available.")
                    return False
                    
        except Exception as e:
            print(f"Plan validation failed with error: {e}")
            return False

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
              heuristic: Optional[Callable] = None,
              timeout: Optional[float] = None,
              output_stream: Optional[IO[str]] = None) -> PlanGenerationResult:

        # initialise all fluents that are not set in the initial state so we do not have to deal with
        # uninitialized fluents in the C++ planner.
        self._initialize_fluents(problem) 

        # Apply compilation pipeline (quantifier removal + grounding)
        compiled_problem, compilation_result = self._compile_problem(problem)

        pb_problem_msg = self._writer.convert(compiled_problem)

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

            command = [self.executable_path, problem_filepath, solution_filepath, 
                      "--parallelism", self._parallelism, "--propagator", self._propagator]
            
            # Add optional parameters only if specified
            if self._verbosity is not None:
                command.extend(["--verbosity", self._verbosity])
            
            if self._max_steps is not None:
                command.extend(["--max-steps", str(self._max_steps)])
            
            if self._no_persist_clauses:
                command.append("--no-persist-clauses")
            print(f"Running planner: {' '.join(command)}")

            # Run the C++ planner with real-time output streaming
            process = subprocess.Popen(
                command, 
                stdout=subprocess.PIPE, 
                stderr=subprocess.PIPE,
                universal_newlines=False  # Use bytes mode for threading
            )

            # Create threads to stream stdout and stderr in real-time
            stdout_thread = threading.Thread(target=self._stream_output, args=(process.stdout, ""))
            stderr_thread = threading.Thread(target=self._stream_output, args=(process.stderr, "ERROR: "))
            
            stdout_thread.daemon = True
            stderr_thread.daemon = True
            
            stdout_thread.start()
            stderr_thread.start()

            # Wait for process to complete with timeout
            try:
                return_code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                process.kill()
                print("Planner timed out.")
                return PlanGenerationResult(
                    PlanGenerationResultStatus.TIMEOUT, None, self.name,
                    log_messages=[LogMessage(level=LogLevel.INFO, message="Planner timed out.")]
                )

            # Wait for output threads to finish
            stdout_thread.join(timeout=1.0)
            stderr_thread.join(timeout=1.0)

            # Handle process errors
            if return_code != 0:
                error_msg = f"The planner failed with return code {return_code}."
                if return_code == -11:  # Specific check for SIGSEGV
                    error_msg += " - The error might be a segmentation fault (SIGSEGV)."
                
                print(f"ERROR: {error_msg}")
                return PlanGenerationResult(
                    PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                    log_messages=[LogMessage(level=LogLevel.ERROR, message=error_msg)]
                )

            # Read and convert solution
            with open(solution_filepath, "rb") as f:
                pb_plan_generation_result_msg = up_pb2.PlanGenerationResult() # type: ignore
                pb_plan_generation_result_msg.ParseFromString(f.read())

            # Convert the protobuf result. This result is for the compiled_problem.
            result_from_protobuf = self._reader.convert(pb_plan_generation_result_msg, compiled_problem)

            final_plan = None
            if result_from_protobuf.plan:
                try:
                    new_actions: list[ActionInstance] = []
                    
                    # Handle different plan types - directly check if actions attribute exists
                    if hasattr(result_from_protobuf.plan, 'actions'):
                        # Standard SequentialPlan case
                        plan_actions = result_from_protobuf.plan.actions
                    else:
                        # If no actions attribute, create empty actions list
                        plan_actions = []
                    
                    # Map actions back to original problem (empty list if no actions)
                    for ai in plan_actions:
                        mapped_ai = compilation_result.map_back_action_instance(ai)
                        assert mapped_ai is not None
                        new_actions.append(mapped_ai)
                        
                    # Create SequentialPlan (empty if no actions)
                    final_plan = SequentialPlan(new_actions)
                except Exception as e:
                    print(f"Error mapping plan back: {e}")
                    return PlanGenerationResult(
                        PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name,
                        log_messages=[LogMessage(level=LogLevel.ERROR, message=f"Error mapping plan back: {e}")]
                    )
            #print(f"Plan found (after mapping): {final_plan is not None}")
            print(f"Status from planner: {result_from_protobuf.status.name}")

            # Validate the plan if one was found
            if final_plan is not None and result_from_protobuf.status == PlanGenerationResultStatus.SOLVED_SATISFICING:
                plan_is_valid = self._validate_plan(problem, final_plan)
                
                if not plan_is_valid:
                    # If plan validation fails, log it but still return the plan with a warning
                    validation_log = LogMessage(
                        level=LogLevel.WARNING, 
                        message="Plan validation failed - the plan may not be correct"
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

        except Exception as e:
            print(f"An error occurred: {e}")
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

    def _compile_problem(self, problem: Problem):
        """
        Apply a series of compilation steps to prepare the problem for the C++ planner.
        
        Args:
            problem: The original problem
            
        Returns:
            tuple: (compiled_problem, compilation_map) where compilation_map can be used
                   to map results back to the original problem
        """
        current_problem = problem
        compilation_maps = []
        
        print("Starting problem compilation pipeline...")
        
        # Step 1: Remove delete-then-set effects
        delete_then_set_remover = DeleteThenSetRemover()
        
        if delete_then_set_remover.supports(current_problem.kind):
            print("  Applying delete-then-set removal...")
            dts_result = delete_then_set_remover.compile(current_problem, CompilationKind.GROUNDING)  # Using GROUNDING as compilation kind
            current_problem = dts_result.problem
            compilation_maps.append(dts_result)
            print("  Delete-then-set removal completed.")
        else:
            print("  Delete-then-set removal not needed for this problem type.")
        
        # Step 2: Remove quantifiers if present
        quantifier_remover = QuantifiersRemover()
        
        if quantifier_remover.supports(current_problem.kind):
            print("  Applying quantifier removal...")
            quantifier_result = quantifier_remover.compile(current_problem, CompilationKind.QUANTIFIERS_REMOVING)
            current_problem = quantifier_result.problem
            compilation_maps.append(quantifier_result)
            print("  Quantifier removal completed.")
        else:
            print("  Quantifier removal not needed for this problem type.")
        
        # Step 3: Ground the problem
        print("  Applying grounding...")
        grounder = Grounder()
        grounding_result = grounder.compile(current_problem, CompilationKind.GROUNDING)
        current_problem = grounding_result.problem
        compilation_maps.append(grounding_result)
        print("  Grounding completed.")
        
        print("Problem compilation pipeline completed.")
        
        # Create a combined compilation result that can map back through all steps
        class CombinedCompilationResult:
            def __init__(self, problem, compilation_maps):
                self.problem = problem
                self.compilation_maps = compilation_maps
            
            def map_back_action_instance(self, action_instance):
                """Map an action instance back through all compilation steps."""
                current_ai = action_instance
                # Apply mappings in reverse order
                for comp_result in reversed(self.compilation_maps):
                    if hasattr(comp_result, 'map_back_action_instance'):
                        current_ai = comp_result.map_back_action_instance(current_ai)
                        if current_ai is None:
                            return None
                return current_ai
        
        combined_result = CombinedCompilationResult(current_problem, compilation_maps)
        return current_problem, combined_result