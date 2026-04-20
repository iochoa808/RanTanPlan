#include "h2_mutex_checker.hpp"

namespace rantanplan {

H2MutexChecker::H2MutexChecker(const Problem& problem)
    : problem_(problem) {

    // Build compact index over boolean grounded fluents only.
    const auto& pool = problem_.pool();
    for (ExprID fid : problem_.grounded_fluents()) {
        if (problem_.is_bool_type(fid)) {
            size_t idx = num_facts_;
            fact_index_[fid] = idx;
            index_to_fact_.push_back(fid);
            num_facts_++;
        }
    }

    // Initialize reachability arrays.
    reachable_.resize(num_facts_, false);
    co_reachable_.resize(num_facts_, std::vector<bool>(num_facts_, false));

    // Pre-process action data.
    for (const auto& action : problem_.actions()) {
        ActionData ad;

        // Extract positive boolean preconditions.
        if (action.has_precondition()) {
            collect_positive_preconditions(action.precondition_id(), ad.preconditions);
        }

        // Extract effects.
        for (const auto& effect : action.effects()) {
            const auto& ee = effect.effect_expression();
            ExprID fid = ee.fluent_id();

            auto it = fact_index_.find(fid);
            if (it == fact_index_.end()) continue;  // not boolean or not tracked
            size_t idx = it->second;

            if (ee.is_assign()) {
                ExprID val = ee.value_id();
                if (pool.is_true_constant(val)) {
                    // Add effect (always include — conditional adds are treated
                    // as potentially firing for over-approximation).
                    ad.add_effects.push_back(idx);
                } else if (pool.is_false_constant(val)) {
                    // Delete effect — only count if unconditional.
                    // Conditional deletes are ignored (fact assumed to persist).
                    if (!ee.is_conditional()) {
                        ad.del_effects.push_back(idx);
                    }
                }
            }
        }

        actions_.push_back(std::move(ad));
    }
}

void H2MutexChecker::collect_positive_preconditions(ExprID eid,
                                                     std::vector<size_t>& out) const {
    if (!eid.valid()) return;
    const auto& pool = problem_.pool();
    ExprKind kind = pool.kind(eid);

    // A state variable appearing directly as a precondition = positive atom.
    if (kind == ExprKind::STATE_VARIABLE) {
        auto it = fact_index_.find(eid);
        if (it != fact_index_.end()) {
            out.push_back(it->second);
        }
        return;
    }

    // AND node: recurse into children.
    if (kind == ExprKind::FUNCTION_APPLICATION && pool.is_and(eid)) {
        for (ExprID child : pool.children(eid)) {
            collect_positive_preconditions(child, out);
        }
        return;
    }

    // NOT node: negative precondition — skip (conservative: ignore negatives).
    // OR node: disjunctive precondition — skip (conservative: treat as satisfied).
    // Numeric comparisons (GE, LE, etc.): skip (conservative: treat as satisfied).
    // Everything else: skip.
}

bool H2MutexChecker::mark_co_reachable(size_t i, size_t j) {
    if (co_reachable_[i][j]) return false;  // already marked
    co_reachable_[i][j] = true;
    co_reachable_[j][i] = true;
    return true;
}

void H2MutexChecker::compute() {
    // === Phase 1: Initialize from initial state ===

    // Build initial state lookup.
    std::unordered_set<ExprID> init_true;
    const auto& pool = problem_.pool();
    for (const auto& assignment : problem_.initial_state()) {
        ExprID fid = assignment.fluent_id();
        ExprID vid = assignment.value_id();
        if (problem_.is_bool_type(fid) && pool.is_true_constant(vid)) {
            init_true.insert(fid);
        }
    }

    // Mark initial reachability.
    std::vector<size_t> init_facts;
    for (size_t i = 0; i < num_facts_; i++) {
        if (init_true.count(index_to_fact_[i])) {
            reachable_[i] = true;
            init_facts.push_back(i);
        }
    }

    // Mark initial co-reachability (all pairs of initially-true facts).
    for (size_t a = 0; a < init_facts.size(); a++) {
        for (size_t b = a + 1; b < init_facts.size(); b++) {
            mark_co_reachable(init_facts[a], init_facts[b]);
        }
    }

    // === Phase 2: Fixpoint iteration ===

    // Pre-compute delete sets for fast lookup.
    // del_set[action_idx] = set of fact indices unconditionally deleted.
    std::vector<std::unordered_set<size_t>> del_sets(actions_.size());
    for (size_t ai = 0; ai < actions_.size(); ai++) {
        for (size_t d : actions_[ai].del_effects) {
            del_sets[ai].insert(d);
        }
    }

    bool changed = true;
    int iterations = 0;
    while (changed) {
        changed = false;
        iterations++;

        for (size_t ai = 0; ai < actions_.size(); ai++) {
            const auto& ad = actions_[ai];

            // Step 1: Check h²-applicability.
            // All preconditions must be individually reachable.
            bool applicable = true;
            for (size_t prec : ad.preconditions) {
                if (!reachable_[prec]) {
                    applicable = false;
                    break;
                }
            }
            if (!applicable) continue;

            // All precondition PAIRS must be co-reachable.
            if (ad.preconditions.size() >= 2) {
                for (size_t pi = 0; pi < ad.preconditions.size() && applicable; pi++) {
                    for (size_t pj = pi + 1; pj < ad.preconditions.size() && applicable; pj++) {
                        if (!co_reachable_[ad.preconditions[pi]][ad.preconditions[pj]]) {
                            applicable = false;
                        }
                    }
                }
            }
            if (!applicable) continue;

            // Step 2: Mark add effects as reachable.
            for (size_t f : ad.add_effects) {
                if (!reachable_[f]) {
                    reachable_[f] = true;
                    changed = true;
                }
            }

            // Step 3: Mark co-reachable pairs.
            for (size_t f : ad.add_effects) {
                // (a) f co-reachable with other add effects of same action.
                for (size_t g : ad.add_effects) {
                    if (g <= f) continue;  // avoid duplicates (symmetric)
                    if (mark_co_reachable(f, g)) changed = true;
                }

                // (b) f co-reachable with any fact g that persists through a.
                //     g persists if: reachable, not deleted by a, and co-reachable
                //     with all preconditions of a.
                for (size_t g = 0; g < num_facts_; g++) {
                    if (g == f) continue;
                    if (co_reachable_[f][g]) continue;  // already known
                    if (!reachable_[g]) continue;
                    if (del_sets[ai].count(g)) continue;  // a deletes g

                    // Check: g must be co-reachable with all preconditions of a.
                    // (g was true before a fired; a's preconditions were also true;
                    //  so g and each precondition must have been simultaneously true.)
                    bool g_compatible = true;
                    for (size_t prec : ad.preconditions) {
                        if (prec == g) continue;  // trivially co-reachable with itself
                        if (!co_reachable_[g][prec]) {
                            g_compatible = false;
                            break;
                        }
                    }
                    if (g_compatible) {
                        if (mark_co_reachable(f, g)) changed = true;
                    }
                }
            }

            // Step 4: Precondition facts also persist through a (if not deleted).
            // Two precondition facts that are co-reachable remain co-reachable
            // after a fires (they were both true before, a doesn't delete them).
            // This is already captured: they're in co_reachable from a prior state.
            // But we also need: precondition facts co-reachable with add effects
            // of a (precondition was true when a fired, add effect becomes true).
            for (size_t prec : ad.preconditions) {
                if (del_sets[ai].count(prec)) continue;  // a deletes this precondition
                for (size_t f : ad.add_effects) {
                    if (f == prec) continue;
                    if (mark_co_reachable(prec, f)) changed = true;
                }
            }
        }
    }
}

bool H2MutexChecker::is_mutex(ExprID f, ExprID g) const {
    auto it_f = fact_index_.find(f);
    auto it_g = fact_index_.find(g);
    if (it_f == fact_index_.end() || it_g == fact_index_.end()) return false;
    size_t fi = it_f->second;
    size_t gi = it_g->second;
    // Mutex iff both reachable individually but not co-reachable.
    return reachable_[fi] && reachable_[gi] && !co_reachable_[fi][gi];
}

bool H2MutexChecker::all_pairs_mutex(const std::vector<ExprID>& group) const {
    for (size_t i = 0; i < group.size(); i++) {
        for (size_t j = i + 1; j < group.size(); j++) {
            if (!is_mutex(group[i], group[j])) {
                return false;
            }
        }
    }
    return true;
}

size_t H2MutexChecker::mutex_pair_count() const {
    size_t count = 0;
    for (size_t i = 0; i < num_facts_; i++) {
        for (size_t j = i + 1; j < num_facts_; j++) {
            if (reachable_[i] && reachable_[j] && !co_reachable_[i][j]) {
                count++;
            }
        }
    }
    return count;
}

} // namespace rantanplan
