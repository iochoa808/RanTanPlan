import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from generators.fo_counters_small import generate
def get_problem(): return generate(n=3)
