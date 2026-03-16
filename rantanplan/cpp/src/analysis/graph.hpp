#pragma once

#include <vector>

namespace rantanplan {

/**
 * @brief Minimal directed graph class for parallelism analysis
 * 
 * Simple adjacency list based graph for representing action dependencies
 * and mutex relationships in planning problems.
 */
class Graph {
public:
    Graph() = default; // Construct an empty graph
    explicit Graph(size_t num_nodes); // Construct a graph with a specified number of nodes
    int add_node(); // Add a new node to the graph and returns the ID of the newly created node

    void add_edge(int source, int target); // Add a directed edge from source to target
    bool has_edge(int source, int target) const; // Check if an edge exists between two nodes
    const std::vector<int>& get_neighbours(int node) const; // Get all neighbours (outgoing edges) of a node
    size_t num_nodes() const { return adjacency_list_.size(); } // Get the number of nodes in the graph
    size_t num_edges() const; // Get the total number of edges in the graph

    // Strongly Connected Components (SCC) analysis
    std::vector<std::vector<int>> compute_strongly_connected_components() const;
    std::vector<int> get_scc_mapping() const; // Returns node_id -> scc_id mapping

    // Topological sorting
    std::vector<int> topological_sort(const std::vector<int>& nodes) const;

private:
    // Adjacency list representation: node_id -> list of neighbours
    std::vector<std::vector<int>> adjacency_list_;
    
    // SCC caching
    mutable std::vector<int> scc_mapping_; // Cached SCC results: node_id -> scc_id
    mutable bool scc_computed_ = false;

};

} // namespace rantanplan
