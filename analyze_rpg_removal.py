#!/usr/bin/env python3
"""
Analyze action removal by Boolean and Numeric RPG across all test domains.
"""

import subprocess
import re
from pathlib import Path
from collections import defaultdict

def find_test_problems():
    """Find all domain.pddl files and their corresponding problem files."""
    test_dir = Path("pddl/test")
    domains = []
    
    for domain_path in sorted(test_dir.glob("**/domain.pddl")):
        domain_dir = domain_path.parent
        # Find first problem file in the same directory
        problem_files = list(domain_dir.glob("problem*.pddl")) + list(domain_dir.glob("p*.pddl"))
        if problem_files:
            domains.append((str(domain_path), str(problem_files[0]), domain_dir.name))
    
    return domains

def run_planner(domain_path, problem_path, timeout=10):
    """Run planner and extract RPG statistics."""
    try:
        cmd = [
            "planmt",
            "-d", domain_path,
            "-p", problem_path,
            "--strategy", "seq"
        ]
        
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout
        )
        
        output = result.stdout + result.stderr
        
        # Extract statistics
        stats = {
            'boolean_removed': 0,
            'boolean_total': 0,
            'numeric_unreachable': 0,
            'numeric_removed': 0,
            'numeric_total': 0,
            'goals_reachable': False,
            'success': False
        }
        
        # Parse Boolean RPG line: [RPG] Removed 0/37 unreachable actions (0.0%)
        boolean_match = re.search(r'\[RPG\] Removed (\d+)/(\d+) unreachable actions', output)
        if boolean_match:
            stats['boolean_removed'] = int(boolean_match.group(1))
            stats['boolean_total'] = int(boolean_match.group(2))
        
        # Parse Numeric RPG line: [Numeric RPG] 8 unreachable, removed 8/37 additional actions (21.6%)
        numeric_match = re.search(r'\[Numeric RPG\] (\d+) unreachable, removed (\d+)/(\d+) additional actions', output)
        if numeric_match:
            stats['numeric_unreachable'] = int(numeric_match.group(1))
            stats['numeric_removed'] = int(numeric_match.group(2))
            stats['numeric_total'] = int(numeric_match.group(3))
        
        # Check if goals reachable
        if "Goals reachable: YES" in output:
            stats['goals_reachable'] = True
        
        # Check if successful
        if "Plan found" in output or "PLAN FOUND" in output:
            stats['success'] = True
        
        return stats, None
        
    except subprocess.TimeoutExpired:
        return None, "timeout"
    except Exception as e:
        return None, str(e)

def main():
    print("Analyzing RPG action removal across test domains...")
    print("=" * 80)
    
    problems = find_test_problems()
    print(f"Found {len(problems)} domains with problems\n")
    
    results = []
    
    for i, (domain_path, problem_path, domain_name) in enumerate(problems, 1):
        print(f"[{i}/{len(problems)}] Testing {domain_name}...", end=" ", flush=True)
        
        stats, error = run_planner(domain_path, problem_path)
        
        if error:
            print(f"ERROR: {error}")
            continue
        
        if stats:
            print(f"✓ Bool: {stats['boolean_removed']}/{stats['boolean_total']}, "
                  f"Num: {stats['numeric_unreachable']} unreachable, {stats['numeric_removed']}/{stats['numeric_total']} removed")
            results.append((domain_name, stats))
        else:
            print("FAILED")
    
    print("\n" + "=" * 80)
    print("SUMMARY")
    print("=" * 80)
    
    # Group by numeric removal effectiveness
    no_numeric_removal = []
    some_numeric_removal = []
    
    # Check relationship between Boolean and Numeric removal
    numeric_removes_less = []
    numeric_removes_same = []
    numeric_removes_more = []
    
    for domain_name, stats in results:
        bool_removed = stats['boolean_removed']
        num_unreachable = stats['numeric_unreachable']
        
        if stats['numeric_unreachable'] == 0:
            no_numeric_removal.append((domain_name, stats))
        else:
            some_numeric_removal.append((domain_name, stats))
        
        # Compare unreachable counts
        if num_unreachable < bool_removed:
            numeric_removes_less.append((domain_name, stats))
        elif num_unreachable == bool_removed:
            numeric_removes_same.append((domain_name, stats))
        else:  # num_unreachable > bool_removed
            numeric_removes_more.append((domain_name, stats))
    
    print(f"\nDomains where Numeric RPG removes NO actions: {len(no_numeric_removal)}")
    for domain_name, stats in no_numeric_removal[:10]:
        print(f"  - {domain_name}: {stats['boolean_total']} total actions")
    if len(no_numeric_removal) > 10:
        print(f"  ... and {len(no_numeric_removal) - 10} more")
    
    print(f"\nDomains where Numeric RPG removes actions: {len(some_numeric_removal)}")
    # Sort by percentage removed
    some_numeric_removal.sort(key=lambda x: x[1]['numeric_removed'] / max(x[1]['numeric_total'], 1), reverse=True)
    
    for domain_name, stats in some_numeric_removal[:15]:
        total = stats['numeric_total']
        removed = stats['numeric_removed']
        unreachable = stats['numeric_unreachable']
        pct = (removed / total * 100) if total > 0 else 0
        print(f"  - {domain_name}: {unreachable} unreachable, {removed}/{total} removed ({pct:.1f}%)")
    if len(some_numeric_removal) > 15:
        print(f"  ... and {len(some_numeric_removal) - 15} more")
    
    print(f"\nTotal domains analyzed: {len(results)}")
    print(f"Domains with numeric removal: {len(some_numeric_removal)} ({len(some_numeric_removal)/len(results)*100:.1f}%)")
    print(f"Domains with NO numeric removal: {len(no_numeric_removal)} ({len(no_numeric_removal)/len(results)*100:.1f}%)")
    
    # Analyze relationship between Boolean and Numeric RPG
    print("\n" + "=" * 80)
    print("BOOLEAN vs NUMERIC RPG COMPARISON")
    print("=" * 80)
    
    print(f"\nNumeric identifies FEWER unreachable than Boolean: {len(numeric_removes_less)}")
    if numeric_removes_less:
        print("  ⚠️  WARNING: Numeric RPG should identify at least as many as Boolean!")
        for domain_name, stats in numeric_removes_less[:10]:
            print(f"    - {domain_name}: Bool={stats['boolean_removed']}, Num={stats['numeric_unreachable']}")
    
    print(f"\nNumeric identifies SAME unreachable as Boolean: {len(numeric_removes_same)}")
    for domain_name, stats in numeric_removes_same[:5]:
        print(f"    - {domain_name}: Both={stats['boolean_removed']}")
    if len(numeric_removes_same) > 5:
        print(f"    ... and {len(numeric_removes_same) - 5} more")
    
    print(f"\nNumeric identifies MORE unreachable than Boolean: {len(numeric_removes_more)}")
    for domain_name, stats in numeric_removes_more[:10]:
        diff = stats['numeric_unreachable'] - stats['boolean_removed']
        print(f"    - {domain_name}: Bool={stats['boolean_removed']}, Num={stats['numeric_unreachable']} (+{diff})")
    if len(numeric_removes_more) > 10:
        print(f"    ... and {len(numeric_removes_more) - 10} more")
    
    # Check if numeric ALWAYS finds at least as many as boolean
    if len(numeric_removes_less) == 0:
        print("\n✓ VERIFIED: Numeric RPG always identifies ≥ unreachable actions than Boolean RPG")
    else:
        print(f"\n✗ ANOMALY: {len(numeric_removes_less)} domain(s) where Numeric < Boolean!")

if __name__ == "__main__":
    main()
