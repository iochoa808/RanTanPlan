#pragma once

#include <vector>

namespace planmt {

/**
 * @brief Minimal directed graph class for parallelism analysis
 * 
 * Simple adjacency list based graph for representing action dependencies
 * and mutex relationships in planning problems.
 */
class Graph {
public:
    using NodeId = int;
    
    Graph() = default; // Construct an empty graph
    explicit Graph(size_t num_nodes); // Construct a graph with a specified number of nodes
    NodeId add_node(); // Add a new node to the graph and returns the ID of the newly created node
    
    void add_edge(NodeId source, NodeId target); // Add a directed edge from source to target
    bool has_edge(NodeId source, NodeId target) const; // Check if an edge exists between two nodes
    const std::vector<NodeId>& get_neighbours(NodeId node) const; // Get all neighbours (outgoing edges) of a node
    size_t num_nodes() const { return adjacency_list_.size(); } // Get the number of nodes in the graph
    size_t num_edges() const; // Get the total number of edges in the graph

private:
    // Adjacency list representation: node_id -> list of neighbours
    std::vector<std::vector<NodeId>> adjacency_list_;
};

} // namespace planmt
