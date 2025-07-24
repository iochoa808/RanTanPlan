#!/usr/bin/env python3
"""
Result formatting and output handling for planMT CLI.
"""

from unified_planning.engines.results import LogLevel


def print_result(result, args):
    """Print the planning result."""
    if not result:
        return False
    
    print(f"\n--- planMT Result ---")
    print(f"Status: {result.status}")
    
    plan_found = result.plan is not None
    
    if plan_found:
        if hasattr(result.plan, 'actions'):
            # Sequential plan
            print(f"Plan found with {len(result.plan.actions)} actions:")
            for i, action_instance in enumerate(result.plan.actions):
                params_str = ", ".join(map(str, action_instance.actual_parameters))
                print(f"  {i+1}. {action_instance.action.name}({params_str})")
        else:
            print("Plan found but format not recognized")
            print(f"Plan: {result.plan}")
        
        # Save plan to file if requested
        if args.output_plan:
            save_plan(result, args.output_plan)
    else:
        print("No plan found.")
    
    # Print log messages if verbose
    if args.verbose and result.log_messages:
        print(f"\nLog Messages:")
        for msg in result.log_messages:
            if msg.level == LogLevel.ERROR:
                print(f"[ERROR] {msg.message}")
            else:
                print(msg.message)
    return plan_found


def save_plan(result, output_file):
    """Save the plan to a file."""
    try:
        with open(output_file, 'w') as f:
            f.write(f"Plan for problem (status: {result.status}):\n")
            if hasattr(result.plan, 'actions'):
                for i, action_instance in enumerate(result.plan.actions):
                    params_str = ", ".join(map(str, action_instance.actual_parameters))
                    f.write(f"{i+1}. {action_instance.action.name}({params_str})\n")
        print(f"Plan saved to: {output_file}")
    except Exception as e:
        print(f"Warning: Could not save plan to file: {e}")