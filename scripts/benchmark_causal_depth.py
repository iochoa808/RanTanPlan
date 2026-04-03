#!/usr/bin/env python3
"""Benchmark causal depth vs RPG lower bound across small-test instances."""

import subprocess
import re
import os
import sys

SMALL_TEST = "pddl/small-test"
TIMEOUT = 120
STRATEGY = "causal-exists"


def run_instance(domain, problem, name):
    cmd = [
        sys.executable, "-m", "rantanplan",
        "-d", domain, "-p", problem,
        "--strategy", STRATEGY,
        "--timeout", str(TIMEOUT),
    ]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT + 30)
        output = result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        return {"name": name, "status": "timeout"}

    info = {"name": name, "status": "timeout"}

    m = re.search(r"RPG lower bound: (\d+)", output)
    if m:
        info["rpg_lb"] = int(m.group(1))

    m = re.search(r"causal depth: (\d+)", output)
    if m:
        info["causal_depth"] = int(m.group(1))

    m = re.search(r"max_goal_depth=(\d+)", output)
    if m:
        info["max_goal_depth"] = int(m.group(1))

    m = re.search(
        r"PLAN FOUND: (\d+) actions, (\d+) rounds, horizon=(\d+) "
        r"\(total time: ([\d.]+)s\)",
        output,
    )
    if m:
        info["status"] = "solved"
        info["actions"] = int(m.group(1))
        info["rounds"] = int(m.group(2))
        info["horizon"] = int(m.group(3))
        info["time"] = float(m.group(4))

    return info


def main():
    results = []

    for domain_dir in sorted(os.listdir(SMALL_TEST)):
        domain_path = os.path.join(SMALL_TEST, domain_dir, "domain.pddl")
        instances_dir = os.path.join(SMALL_TEST, domain_dir, "instances")
        if not os.path.isdir(instances_dir):
            continue

        problems = sorted(f for f in os.listdir(instances_dir) if f.endswith(".pddl"))
        if not problems:
            continue

        problem_path = os.path.join(instances_dir, problems[0])
        name = f"{domain_dir}/{problems[0].replace('.pddl', '')}"

        print(f"Running {name}...", end=" ", flush=True)
        info = run_instance(domain_path, problem_path, name)
        results.append(info)
        print(f"solved in {info['time']:.2f}s" if info["status"] == "solved" else "timeout")

    hdr = (
        f"{'Instance':<35} {'RPG':>4} {'Causal':>7} {'Rounds':>6} "
        f"{'Horizon':>7} {'Actions':>7} {'Time':>8} {'Status':<8}"
    )
    print(f"\n{hdr}\n{'-' * len(hdr)}")

    for r in results:
        rpg = str(r.get("rpg_lb", "?"))
        causal = str(r.get("causal_depth", "?"))
        rounds = str(r.get("rounds", "-"))
        horizon = str(r.get("horizon", "-"))
        actions = str(r.get("actions", "-"))
        time_s = f"{r['time']:.2f}" if "time" in r else "-"
        status = r["status"]

        marker = ""
        if r.get("rpg_lb") is not None and r.get("causal_depth") is not None:
            if r["causal_depth"] > r["rpg_lb"]:
                marker = " <<"
            elif r["causal_depth"] < r["rpg_lb"]:
                marker = " >>"

        print(
            f"{r['name']:<35} {rpg:>4} {causal:>7} {rounds:>6} "
            f"{horizon:>7} {actions:>7} {time_s:>8} {status:<8}{marker}"
        )


if __name__ == "__main__":
    main()
