#!/usr/bin/env python3
"""
Script to analyze RPG layer requirements for test instances.
For each PDDL problem, records the minimal number of layers needed to achieve goals.
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

def run_rpg_analysis(domain_path, problem_path):
    """Run planMT to get RPG analysis without solving."""
    try:
        # Use seq strategy as it's simplest and fastest for RPG analysis
        cmd = [
            "planmt",
            "-d", domain_path,
            "-p", problem_path,
            "--strategy", "seq",
            "--timeout", "30"  # 30 second timeout
        ]

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=35  # Extra 5 seconds for safety
        )

        # Parse output for RPG information
        output = result.stdout + result.stderr

        # Extract RPG build time and layer count
        rpg_time = None
        total_layers = None
        goals_reachable = None

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

        return {
            'success': True,
            'rpg_time_ms': rpg_time,
            'total_layers': total_layers,
            'goals_reachable': goals_reachable,
            'output_size': len(output)
        }

    except subprocess.TimeoutExpired:
        return {
            'success': False,
            'error': 'timeout',
            'rpg_time_ms': None,
            'total_layers': None,
            'goals_reachable': None
        }
    except Exception as e:
        return {
            'success': False,
            'error': str(e),
            'rpg_time_ms': None,
            'total_layers': None,
            'goals_reachable': None
        }

def main():
    print("RPG Layer Analysis for Test Instances")
    print("=" * 50)

    # Find all test instances
    instances = find_test_instances()
    print(f"Found {len(instances)} test instances")

    results = []

    for i, instance in enumerate(instances, 1):
        print(f"\n[{i:2d}/{len(instances)}] Analyzing {instance['name']}...", end=" ", flush=True)

        start_time = time.time()
        result = run_rpg_analysis(instance['domain'], instance['problem'])
        analysis_time = time.time() - start_time

        result['instance'] = instance['name']
        result['analysis_time_s'] = round(analysis_time, 2)
        results.append(result)

        if result['success']:
            layers = result['total_layers']
            rpg_time = result['rpg_time_ms']
            reachable = result['goals_reachable']
            print(f"✓ {layers} layers, {rpg_time}ms, goals: {'✓' if reachable else '✗'}")
        else:
            print(f"✗ {result['error']}")

    # Summary statistics
    print("\n" + "=" * 50)
    print("SUMMARY")
    print("=" * 50)

    successful = [r for r in results if r['success']]
    failed = [r for r in results if not r['success']]

    print(f"Successful analyses: {len(successful)}/{len(results)}")

    if successful:
        layers = [r['total_layers'] for r in successful if r['total_layers'] is not None]
        times = [r['rpg_time_ms'] for r in successful if r['rpg_time_ms'] is not None]
        reachable_count = sum(1 for r in successful if r['goals_reachable'])

        if layers:
            print(f"Layer count - Min: {min(layers)}, Max: {max(layers)}, Avg: {sum(layers)/len(layers):.1f}")
        if times:
            print(f"RPG time - Min: {min(times):.1f}ms, Max: {max(times):.1f}ms, Avg: {sum(times)/len(times):.1f}ms")
        print(f"Goals reachable: {reachable_count}/{len(successful)}")

        # Show instances with most layers
        if layers:
            sorted_by_layers = sorted(successful, key=lambda x: x['total_layers'] or 0, reverse=True)
            print(f"\nTop 5 instances by layer count:")
            for r in sorted_by_layers[:5]:
                if r['total_layers']:
                    print(f"  {r['instance']:30} {r['total_layers']:3d} layers")

    if failed:
        print(f"\nFailed analyses: {len(failed)}")
        for r in failed:
            print(f"  {r['instance']:30} {r['error']}")

    # Save detailed results
    output_file = "rpg_analysis_results.json"
    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)
    print(f"\nDetailed results saved to: {output_file}")

if __name__ == "__main__":
    main()