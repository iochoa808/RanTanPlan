"""
Shared generator for the Storytellers benchmark (paper: "11 instances with
5-20 stories, 2 audiences, 2 storytellers" -- though the actual reference
script uses 5 storytellers, not 2; kept as-is since the script is the
verified ground truth, same reconciliation approach used elsewhere in this
batch for paper-vs-repo mismatches). No instances.txt or fixed instance
files ship for this domain -- only Storytellers.py's single hardcoded
n_stories=20 example -- so the 11 instances here are parametrically
generated at n_stories in {5,6,8,10,11,12,14,16,17,18,20}
(numpy.linspace(5,20,11) rounded to integers), evenly spanning the paper's
stated 5-20 range.

Replicates the exact UP model in
~/unified-planning/docs/extensions/domains/storytellers/Storytellers.py
(generalized from its hardcoded n_stories=20): 5 storytellers, 2
audiences, n_stories stories split ~evenly across storytellers (a
leftover remainder from int(n_stories/5) truncation is simply never known
by any storyteller when n_stories isn't a multiple of 5 -- harmless, since
the goal only requires each audience to have heard >= ceil(n_stories/2)
stories, not all of them). SetType fluents (no arrays), 1 action
('entertain'), goal: every audience has heard at least half the stories,
AND all audiences have heard exactly the same set of stories.

The paper explicitly warns count-expression compilation blows up for this
domain at ~20 objects (600+ seconds) -- expect the n_stories=20 instance
to be very slow or infeasible to compile through the 'up'/iasciu pipeline;
that matches the paper's own finding, not a bug here.
"""
import math
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def build_storytellers(name: str, n_stories: int):
    p = Problem(name)

    Storyteller = UserType('Storyteller')
    storytellers = [Object(f'st{i + 1}', Storyteller) for i in range(5)]

    Audience = UserType('Audiences')
    a1 = Object('a1', Audience)
    a2 = Object('a2', Audience)

    p.add_objects(storytellers)
    p.add_objects([a1, a2])

    Stories = UserType('Stories')
    stories = [Object(f's{i + 1}', Stories) for i in range(n_stories)]
    p.add_objects(stories)

    known = Fluent('known', SetType(Stories), st=Storyteller)
    heard = Fluent('heard', SetType(Stories), a=Audience)
    story_set = Fluent('story_set', SetType(Stories))

    p.add_fluent(known, default_initial_value=set())
    p.add_fluent(heard, default_initial_value=set())
    p.add_fluent(story_set, default_initial_value=set())

    p.set_initial_value(story_set, {*stories})
    # RTP's native ingestion requires an explicit :init entry for every
    # object/set fluent -- default_initial_value alone isn't enough, same
    # issue as elsewhere in this batch. heard(a1)/heard(a2) are left at
    # their default (empty set) by the original script, so make that
    # explicit here rather than relying on the default.
    p.set_initial_value(heard(a1), set())
    p.set_initial_value(heard(a2), set())

    n_per_st = int(n_stories / 5)
    split = 0
    for st_n in range(5):
        st = storytellers[st_n]
        st_objects = [stories[split + n] for n in range(n_per_st)]
        split += n_per_st
        p.set_initial_value(known(st), {*st_objects})

    entertain = InstantaneousAction('entertain', st=Storyteller, a=Audience)
    st, a = entertain.parameter('st'), entertain.parameter('a')
    entertain.add_effect(heard(a), SetUnion(heard(a), known(st)))
    p.add_action(entertain)

    a_var = Variable('a_var', Audience)
    p.add_goal(Forall(GE(SetCardinality(heard(a_var)), math.ceil(n_stories / 2)), a_var))

    a_var2 = Variable('a_var2', Audience)
    p.add_goal(Forall(Equals(heard(a_var), heard(a_var2)), a_var, a_var2))

    return p


SIZES = {
    'st_n5': 5,
    'st_n6': 6,
    'st_n8': 8,
    'st_n10': 10,
    'st_n11': 11,
    'st_n12': 12,
    'st_n14': 14,
    'st_n16': 16,
    'st_n17': 17,
    'st_n18': 18,
    'st_n20': 20
}
