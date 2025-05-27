import unified_planning as up
from .planner_wrapper import planMTPlanner

# Register the planner to the UP framework
# This is done once the package is imported so its transparent to the user.
env = up.environment.get_environment()
env.factory.add_engine('planMT', 'planmt.planner_wrapper', 'planMTPlanner')