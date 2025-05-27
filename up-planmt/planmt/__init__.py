import unified_planning as up
from .planner_wrapper import planMTPlanner

# Register planner to UP framework
env = up.environment.get_environment()
env.factory.add_engine('planMT', 'planmt.planner_wrapper', 'planMTPlanner')