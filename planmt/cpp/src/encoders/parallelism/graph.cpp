#include "graph.h"
#include <algorithm>
#include <stack>
#include <functional>

namespace planmt {

Graph::Graph(size_t num_nodes) {
    adjacency_list_.resize(num_nodes);
}

int Graph::add_node() {
    int new_node = static_cast<int>(adjacency_list_.size());
    adjacency_list_.emplace_back();
    return new_node;
}

void Graph::add_edge(int source, int target) {
    if (source < num_nodes() && target < num_nodes()) {
        // Check if edge already exists to avoid duplicates
        auto& neighbours = adjacency_list_[source];
        if (std::find(neighbours.begin(), neighbours.end(), target) == neighbours.end()) {
            neighbours.push_back(target);
        }
    }
}

bool Graph::has_edge(int source, int target) const {
    if (source >= num_nodes() || target >= num_nodes()) {
        return false;
    }
    
    const auto& neighbours = adjacency_list_[source];
    return std::find(neighbours.begin(), neighbours.end(), target) != neighbours.end();
}

const std::vector<int>& Graph::get_neighbours(int node) const {
    static const std::vector<int> empty_list;
    if (node >= num_nodes()) {
        return empty_list;
    }
    return adjacency_list_[node];
}

size_t Graph::num_edges() const {
    size_t total = 0;
    for (const auto& neighbours : adjacency_list_) {
        total += neighbours.size();
    }
    return total;
}

// Strongly Connected Components (SCC) analysis
std::vector<std::vector<int>> Graph::compute_strongly_connected_components() const {
    int n = num_nodes();
    std::vector<int> ids(n, -1), low(n, -1);
    std::vector<bool> on_stack(n, false);
    std::stack<int> s;
    std::vector<std::vector<int>> sccs;
    int at = 0;

    std::function<void(int)> dfs = 
        [&](int u) {
        s.push(u);
        on_stack[u] = true;
        ids[u] = low[u] = at++;

        for (int v : get_neighbours(u)) {
            if (ids[v] == -1) {
                dfs(v);
            }
            if (on_stack[v]) {
                low[u] = std::min(low[u], low[v]);
            }
        }

        if (low[u] == ids[u]) {
            std::vector<int> scc;
            while (true) {
                int node = s.top();
                s.pop();
                on_stack[node] = false;
                low[node] = ids[u];
                scc.push_back(node);
                if (node == u) break;
            }
            sccs.push_back(scc);
        }
    };

    for (int i = 0; i < n; ++i) {
        if (ids[i] == -1) {
            dfs(i);
        }
    }
    return sccs;
}

std::vector<int> Graph::get_scc_mapping() const {
    if (!scc_computed_) {
        auto sccs = compute_strongly_connected_components();
        scc_mapping_.assign(num_nodes(), -1);
        for (int scc_id = 0; scc_id < sccs.size(); ++scc_id) {
            for (int node : sccs[scc_id]) {
                scc_mapping_[node] = scc_id;
            }
        }
        scc_computed_ = true;
    }
    return scc_mapping_;
}

// Topological sorting
std::vector<int> Graph::topological_sort(const std::vector<int>& nodes) const {
    std::vector<int> sorted_nodes;
    std::vector<bool> visited(num_nodes(), false);
    std::function<void(int)> dfs = [&](int u) {
        visited[u] = true;
        for (int v : get_neighbours(u)) {
            if (!visited[v]) {
                dfs(v);
            }
        }
        sorted_nodes.push_back(u);
    };

    for (int node : nodes) {
        if (!visited[node]) {
            dfs(node);
        }
    }
    
    std::reverse(sorted_nodes.begin(), sorted_nodes.end());
    
    // The user wants B to appear before A for an edge A -> B.
    // A standard topological sort produces A before B.
    // The DFS-based approach with appending to a list and then reversing gives a standard sort.
    // To get the reverse order, we can just not reverse it.
    // However, the post-order traversal from DFS naturally gives the reverse topological order.
    // Let's re-implement to be clearer.

    sorted_nodes.clear();
    visited.assign(num_nodes(), false);
    std::function<void(int)> post_order_dfs = [&](int u) {
        visited[u] = true;
        for (int v : get_neighbours(u)) {
            // We only consider nodes within the provided subset for traversal
            bool is_in_nodes_subset = false;
            for(int n : nodes) {
                if (n == v) {
                    is_in_nodes_subset = true;
                    break;
                }
            }
            if (is_in_nodes_subset && !visited[v]) {
                post_order_dfs(v);
            }
        }
        sorted_nodes.push_back(u);
    };

    for (int node : nodes) {
        if (!visited[node]) {
            post_order_dfs(node);
        }
    }

    // The post-order traversal gives the reverse of a topological sort.
    // If A -> B, B is visited and added to sorted_nodes before A.
    // So the list will be [..., B, ..., A, ...]. This is what is required.
    return sorted_nodes;
}

} // namespace planmt
