#!/usr/bin/env python3
"""
Script to analyze RPG reachability vs total fluents/actions for test instances.
For each PDDL problem, compares what fluents and actions are reachable in the RPG
versus the total number of grounded fluents/actions in the problem.
"""

import os
import subprocess
import json
import time
from pathlib import Path

def find_test_instances():
    """Find all domain/problem pairs in the test directory."""
    test_dir = Path("pddl/test")
    instances = []

    for domain_dir in test_dir.iterdir():
        if domain_dir.is_dir() and not domain_dir.name.startswith('__'):
            domain_file = domain_dir / "domain.pddl"
            problem_file = domain_dir / "problem.pddl"

            if domain_file.exists() and problem_file.exists():
                instances.append({
                    'name': domain_dir.name,
                    'domain': str(domain_file),
                    'problem': str(problem_file)
                })

    return sorted(instances, key=lambda x: x['name'])

def run_rpg_reachability_analysis(domain_path, problem_path):
    """Run planMT with modified RPG to get reachability analysis."""
    try:
        # Use seq strategy for RPG analysis with debug output
        cmd = [
            "planmt",
            "-d", domain_path,
            "-p", problem_path,
            "--strategy", "seq",
            "-vv",  # Enable debug to get detailed RPG info
            "--timeout", "30"  # 30 second timeout
        ]

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=35  # Extra 5 seconds for safety
        )

        # Parse output for RPG and problem information
        output = result.stdout + result.stderr

        # Extract information from output
        rpg_time = None
        total_layers = None
        goals_reachable = None
        total_grounded_fluents = None
        reachable_fluents = None
        total_actions = None
        reachable_actions = None
        fluent_coverage = None
        action_coverage = None
        unreachable_fluents = None
        unreachable_actions = None

        for line in output.split('\n'):
            if "RPG built in" in line:
                # Extract time: "RPG built in 26ms"
                parts = line.split()
                for i, part in enumerate(parts):
                    if part == "in" and i + 1 < len(parts):
                        time_str = parts[i + 1].replace('ms', '').replace('s', '')
                        try:
                            rpg_time = float(time_str)
                            if 'ms' in parts[i + 1]:
                                rpg_time = rpg_time  # Already in ms
                            else:
                                rpg_time = rpg_time * 1000  # Convert s to ms
                        except ValueError:
                            pass
                        break

            elif "Total layers:" in line:
                # Extract layers: "Total layers: 7"
                parts = line.split()
                for i, part in enumerate(parts):
                    if part == "layers:" and i + 1 < len(parts):
                        try:
                            total_layers = int(parts[i + 1])
                        except ValueError:
                            pass
                        break

            elif "Goals reachable:" in line:
                goals_reachable = "YES" in line

            # Look for reachability analysis output
            elif "Total grounded fluents:" in line:
                import re
                match = re.search(r'Total grounded fluents: (\d+)', line)
                if match:
                    total_grounded_fluents = int(match.group(1))

            elif "Reachable fluents:" in line:
                import re
                match = re.search(r'Reachable fluents: (\d+)', line)
                if match:
                    reachable_fluents = int(match.group(1))

            elif "Unreachable fluents:" in line:
                import re
                match = re.search(r'Unreachable fluents: (\d+)', line)
                if match:
                    unreachable_fluents = int(match.group(1))

            elif "Fluent coverage:" in line:
                import re
                match = re.search(r'Fluent coverage: ([\d.]+)%', line)
                if match:
                    fluent_coverage = float(match.group(1))

            elif "Total actions:" in line:
                import re
                match = re.search(r'Total actions: (\d+)', line)
                if match:
                    total_actions = int(match.group(1))

            elif "Reachable actions:" in line:
                import re
                match = re.search(r'Reachable actions: (\d+)', line)
                if match:
                    reachable_actions = int(match.group(1))

            elif "Unreachable actions:" in line:
                import re
                match = re.search(r'Unreachable actions: (\d+)', line)
                if match:
                    unreachable_actions = int(match.group(1))

            elif "Action coverage:" in line:
                import re
                match = re.search(r'Action coverage: ([\d.]+)%', line)
                if match:
                    action_coverage = float(match.group(1))

        # Extract reachability info from debug output if available
        # This would require modifications to the C++ RPG to output this info
        # For now, we'll work with what we can get from existing output

        return {
            'success': True,
            'rpg_time_ms': rpg_time,
            'total_layers': total_layers,
            'goals_reachable': goals_reachable,
            'total_grounded_fluents': total_grounded_fluents,
            'reachable_fluents': reachable_fluents,
            'unreachable_fluents': unreachable_fluents,
            'fluent_coverage': fluent_coverage,
            'total_actions': total_actions,
            'reachable_actions': reachable_actions,
            'unreachable_actions': unreachable_actions,
            'action_coverage': action_coverage,
            'output_size': len(output),
            'full_output': output  # Store full output for debugging
        }

    except subprocess.TimeoutExpired:
        return {
            'success': False,
            'error': 'timeout',
            'rpg_time_ms': None,
            'total_layers': None,
            'goals_reachable': None,
            'total_grounded_fluents': None,
            'reachable_fluents': None,
            'unreachable_fluents': None,
            'fluent_coverage': None,
            'total_actions': None,
            'reachable_actions': None,
            'unreachable_actions': None,
            'action_coverage': None
        }
    except Exception as e:
        return {
            'success': False,
            'error': str(e),
            'rpg_time_ms': None,
            'total_layers': None,
            'goals_reachable': None,
            'total_grounded_fluents': None,
            'reachable_fluents': None,
            'unreachable_fluents': None,
            'fluent_coverage': None,
            'total_actions': None,
            'reachable_actions': None,
            'unreachable_actions': None,
            'action_coverage': None
        }

def main():
    print("RPG Reachability Analysis for Test Instances")
    print("=" * 60)
    print("Analyzing which fluents and actions are reachable vs total counts")
    print("=" * 60)

    # Find all test instances
    instances = find_test_instances()
    print(f"Found {len(instances)} test instances")

    results = []

    for i, instance in enumerate(instances, 1):
        print(f"\n[{i:2d}/{len(instances)}] Analyzing {instance['name']}...", end=" ", flush=True)

        start_time = time.time()
        result = run_rpg_reachability_analysis(instance['domain'], instance['problem'])
        analysis_time = time.time() - start_time

        result['instance'] = instance['name']
        result['analysis_time_s'] = round(analysis_time, 2)
        results.append(result)

        if result['success']:
            layers = result['total_layers']
            rpg_time = result['rpg_time_ms']
            reachable = result['goals_reachable']
            total_fluents = result['total_grounded_fluents']
            reachable_fluents = result['reachable_fluents']
            fluent_coverage = result['fluent_coverage']
            total_actions = result['total_actions']
            reachable_actions = result['reachable_actions']
            action_coverage = result['action_coverage']

            status_parts = []
            status_parts.append(f"{layers} layers")
            status_parts.append(f"{rpg_time}ms" if rpg_time else "?ms")
            status_parts.append(f"goals: {'✓' if reachable else '✗'}")

            if total_fluents and reachable_fluents is not None:
                status_parts.append(f"fluents: {reachable_fluents}/{total_fluents}")
                if fluent_coverage is not None:
                    status_parts.append(f"({fluent_coverage:.1f}%)")
            elif total_fluents:
                status_parts.append(f"fluents: {total_fluents}")

            if total_actions and reachable_actions is not None:
                status_parts.append(f"actions: {reachable_actions}/{total_actions}")
                if action_coverage is not None:
                    status_parts.append(f"({action_coverage:.1f}%)")
            elif total_actions:
                status_parts.append(f"actions: {total_actions}")

            print(f"✓ {', '.join(status_parts)}")
        else:
            print(f"✗ {result['error']}")

    # Summary statistics
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)

    successful = [r for r in results if r['success']]
    failed = [r for r in results if not r['success']]

    print(f"Successful analyses: {len(successful)}/{len(results)}")

    if successful:
        layers = [r['total_layers'] for r in successful if r['total_layers'] is not None]
        times = [r['rpg_time_ms'] for r in successful if r['rpg_time_ms'] is not None]
        reachable_count = sum(1 for r in successful if r['goals_reachable'])

        fluent_counts = [r['total_grounded_fluents'] for r in successful if r['total_grounded_fluents'] is not None]
        reachable_fluent_counts = [r['reachable_fluents'] for r in successful if r['reachable_fluents'] is not None]
        fluent_coverages = [r['fluent_coverage'] for r in successful if r['fluent_coverage'] is not None]

        action_counts = [r['total_actions'] for r in successful if r['total_actions'] is not None]
        reachable_action_counts = [r['reachable_actions'] for r in successful if r['reachable_actions'] is not None]
        action_coverages = [r['action_coverage'] for r in successful if r['action_coverage'] is not None]

        if layers:
            print(f"Layer count - Min: {min(layers)}, Max: {max(layers)}, Avg: {sum(layers)/len(layers):.1f}")
        if times:
            print(f"RPG time - Min: {min(times):.1f}ms, Max: {max(times):.1f}ms, Avg: {sum(times)/len(times):.1f}ms")
        print(f"Goals reachable: {reachable_count}/{len(successful)}")

        print(f"\nFluent Analysis:")
        if fluent_counts:
            print(f"  Total fluents - Min: {min(fluent_counts)}, Max: {max(fluent_counts)}, Avg: {sum(fluent_counts)/len(fluent_counts):.1f}")
        if reachable_fluent_counts:
            print(f"  Reachable fluents - Min: {min(reachable_fluent_counts)}, Max: {max(reachable_fluent_counts)}, Avg: {sum(reachable_fluent_counts)/len(reachable_fluent_counts):.1f}")
        if fluent_coverages:
            print(f"  Fluent coverage - Min: {min(fluent_coverages):.1f}%, Max: {max(fluent_coverages):.1f}%, Avg: {sum(fluent_coverages)/len(fluent_coverages):.1f}%")

        print(f"\nAction Analysis:")
        if action_counts:
            print(f"  Total actions - Min: {min(action_counts)}, Max: {max(action_counts)}, Avg: {sum(action_counts)/len(action_counts):.1f}")
        if reachable_action_counts:
            print(f"  Reachable actions - Min: {min(reachable_action_counts)}, Max: {max(reachable_action_counts)}, Avg: {sum(reachable_action_counts)/len(reachable_action_counts):.1f}")
        if action_coverages:
            print(f"  Action coverage - Min: {min(action_coverages):.1f}%, Max: {max(action_coverages):.1f}%, Avg: {sum(action_coverages)/len(action_coverages):.1f}%")

        # Show instances with lowest coverage (most unreachable items)
        if fluent_coverages:
            sorted_by_fluent_coverage = sorted(successful, key=lambda x: x['fluent_coverage'] or 100.0)
            print(f"\nInstances with lowest fluent coverage:")
            for r in sorted_by_fluent_coverage[:5]:
                if r['fluent_coverage'] is not None:
                    print(f"  {r['instance']:30} {r['fluent_coverage']:5.1f}% ({r['reachable_fluents']}/{r['total_grounded_fluents']})")

        if action_coverages:
            sorted_by_action_coverage = sorted(successful, key=lambda x: x['action_coverage'] or 100.0)
            print(f"\nInstances with lowest action coverage:")
            for r in sorted_by_action_coverage[:5]:
                if r['action_coverage'] is not None:
                    print(f"  {r['instance']:30} {r['action_coverage']:5.1f}% ({r['reachable_actions']}/{r['total_actions']})")

    if failed:
        print(f"\nFailed analyses: {len(failed)}")
        for r in failed:
            print(f"  {r['instance']:30} {r['error']}")

    # Save detailed results
    output_file = "rpg_reachability_results.json"
    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)
    print(f"\nDetailed results saved to: {output_file}")

    # Note about implementation
    print("\n" + "=" * 60)
    print("This analysis uses extended RPG implementation that tracks:")
    print("- Which fluents are reachable vs unreachable in the relaxed planning graph")
    print("- Which actions are applicable vs inapplicable in the relaxed planning graph")
    print("- Coverage percentages showing the fraction of reachable items")
    print("Use --verbosity debug to see detailed unreachable fluents/actions listed.")
    print("=" * 60)

if __name__ == "__main__":
    main()