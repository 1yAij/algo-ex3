#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gc {

struct Heuristics {
    bool degree_descending = false;
    bool mrv = false;
    bool forward_checking = false;
    bool lcv = false;  // Least Constraining Value ordering; requires mrv+forward_checking
};

// Only max_elapsed_ms is used to stop the search. max_explored_nodes is
// recorded in metrics/CSV and does not force search abort.
struct SearchLimits {
    std::uint64_t max_explored_nodes = 5'000'000;
    double max_elapsed_ms = 3000.0;
};

struct Metrics {
    std::string experiment;
    std::string instance;
    int color_limit = 0;
    Heuristics heuristics;
    bool success = false;
    bool aborted = false;
    double elapsed_ms = 0.0;
    std::uint64_t backtracks = 0;
    std::uint64_t explored_nodes = 0;
};

struct ColoringResult {
    bool success = false;
    std::vector<int> colors;
    Metrics metrics;
};

class Graph {
public:
    explicit Graph(int vertex_count = 0);

    void add_edge(int u, int v);
    int vertex_count() const;
    int edge_count() const;
    int degree(int v) const;
    bool has_edge(int u, int v) const;
    const std::vector<int>& neighbors(int v) const;

private:
    int vertex_count_ = 0;
    int edge_count_ = 0;
    std::vector<std::vector<int>> adjacency_list_;
    std::vector<std::vector<std::uint8_t>> adjacency_matrix_;
};

Graph load_dimacs_graph(const std::string& path);
Graph generate_random_graph(int vertex_count, double density, std::uint32_t seed);

ColoringResult solve_graph_coloring(const Graph& graph,
                                    int color_limit,
                                    const Heuristics& heuristics,
                                    const SearchLimits& limits = {});
void write_metrics_csv(const std::string& path, const std::vector<Metrics>& rows);
std::string heuristics_to_string(const Heuristics& heuristics);

}  // namespace gc
