from unified_planning.shortcuts import OneshotPlanner
from unified_planning.io import PDDLReader
import os
import sys
import planmt # type: ignore # This will trigger the registration in __init__.py

def main():
    reader = PDDLReader()
    pddl_domain_file = "domain.pddl"
    pddl_problem_file = "problem.pddl"

    if not (os.path.exists(pddl_domain_file) and os.path.exists(pddl_problem_file)):
        print(f"Error: PDDL files not found. Expected at {pddl_domain_file} and {pddl_problem_file}")
        print(f"Current working directory: {os.getcwd()}")
        return

    try:
        problem = reader.parse_problem(pddl_domain_file, pddl_problem_file)
    except Exception as e:
        print(f"Error parsing PDDL files: {e}")
        return

    # Use the planner
    with OneshotPlanner(name='planMT',
                        # executable_path=cpp_exe_path, # Uncomment and set if needed
                        ) as planner:
        if planner:
            print(f"Successfully created planner '{planner.name}'.")
            try:
                result = planner.solve(problem, output_stream=sys.stdout, timeout=10)

                print(f"\n--- Planner Result ---")
                print(f"Status: {result.status}")

                if result.plan:
                    print("Plan found:")
                    for action_instance in result.plan.actions:
                        params_str = ", ".join(map(str, action_instance.actual_parameters))
                        print(f"  {action_instance.action.name}({params_str})")
                else:
                    print("No plan found.")

                if result.log_messages:
                    print("\nLog Messages:")
                    for msg in result.log_messages:
                        print(f"  [{msg.level}] {msg.message}")

            except Exception as e:
                print(f"An error occurred during planning: {e}")
        else:
            print(f"Could not create planner. Is it registered correctly?")

if __name__ == "__main__":
    main()