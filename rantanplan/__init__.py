import unified_planning as up
from .planner_wrapper import RantanPlanPlanner, RantanPlanOptimalPlanner, RantanPlanAnytimePlanner

# Register the planners to the UP framework
# This is done once the package is imported so its transparent to the user.
env = up.environment.get_environment()
env.factory.add_engine('RantanPlan', 'rantanplan.planner_wrapper', 'RantanPlanPlanner')
env.factory.add_engine('RantanPlan-optimal', 'rantanplan.planner_wrapper', 'RantanPlanOptimalPlanner')
env.factory.add_engine('RantanPlan-anytime', 'rantanplan.planner_wrapper', 'RantanPlanAnytimePlanner')
