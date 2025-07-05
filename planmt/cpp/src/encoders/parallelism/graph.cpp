#include "graph.h"
#include <algorithm>

namespace planmt {

Graph::Graph(size_t num_nodes) {
    adjacency_list_.resize(num_nodes);
}

Graph::NodeId Graph::add_node() {
    NodeId new_node = static_cast<NodeId>(adjacency_list_.size());
    adjacency_list_.emplace_back();
    return new_node;
}

void Graph::add_edge(NodeId source, NodeId target) {
    if (source < num_nodes() && target < num_nodes()) {
        // Check if edge already exists to avoid duplicates
        auto& neighbours = adjacency_list_[source];
        if (std::find(neighbours.begin(), neighbours.end(), target) == neighbours.end()) {
            neighbours.push_back(target);
        }
    }
}

bool Graph::has_edge(NodeId source, NodeId target) const {
    if (source >= num_nodes() || target >= num_nodes()) {
        return false;
    }
    
    const auto& neighbours = adjacency_list_[source];
    return std::find(neighbours.begin(), neighbours.end(), target) != neighbours.end();
}

const std::vector<Graph::NodeId>& Graph::get_neighbours(NodeId node) const {
    static const std::vector<NodeId> empty_list;
    if (node >= num_nodes()) {
        return empty_list;
    }
    return adjacency_list_[node];
}

} // namespace planmt
