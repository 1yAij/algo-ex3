#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "../src/graph_coloring.hpp"

namespace {

using gc::ColoringResult;
using gc::Graph;
using gc::Heuristics;
using gc::Metrics;

bool is_valid_coloring(const Graph& graph, const std::vector<int>& coloring) {
    if (static_cast<int>(coloring.size()) != graph.vertex_count()) {
        return false;
    }
    for (int u = 0; u < graph.vertex_count(); ++u) {
        for (int v : graph.neighbors(u)) {
            if (u < v && coloring[u] == coloring[v]) {
                return false;
            }
        }
    }
    return true;
}

void test_small_graph_solution() {
    Graph graph(5);
    graph.add_edge(0, 1);
    graph.add_edge(0, 2);
    graph.add_edge(0, 3);
    graph.add_edge(1, 2);
    graph.add_edge(1, 4);
    graph.add_edge(2, 3);
    graph.add_edge(2, 4);
    graph.add_edge(3, 4);

    ColoringResult result = gc::solve_graph_coloring(graph, 4, Heuristics{true, true, true});
    assert(result.success);
    assert(is_valid_coloring(graph, result.colors));
}

void test_triangle_infeasible_with_two_colors() {
    Graph graph(3);
    graph.add_edge(0, 1);
    graph.add_edge(1, 2);
    graph.add_edge(0, 2);

    ColoringResult result = gc::solve_graph_coloring(graph, 2, Heuristics{false, false, false});
    assert(!result.success);
    assert(result.metrics.backtracks > 0);
}

void test_search_limits_abort() {
    Graph graph(7);
    for (int u = 0; u < 7; ++u) {
        for (int v = u + 1; v < 7; ++v) {
            graph.add_edge(u, v);
        }
    }

    // Stopping condition is time only (not explored-node count).
    ColoringResult result = gc::solve_graph_coloring(
        graph, 3, Heuristics{false, false, false}, gc::SearchLimits{5'000'000, 0.0});
    assert(!result.success);
    assert(result.metrics.aborted);
}

void test_dimacs_loader() {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "gc_test_dimacs.col";
    {
        std::ofstream out(path);
        out << "c test graph\n";
        out << "p edge 4 3\n";
        out << "e 1 2\n";
        out << "e 2 3\n";
        out << "e 3 4\n";
    }

    Graph graph = gc::load_dimacs_graph(path.string());
    assert(graph.vertex_count() == 4);
    assert(graph.edge_count() == 3);
    assert(graph.has_edge(0, 1));
    assert(graph.has_edge(1, 2));
    assert(graph.has_edge(2, 3));
    std::filesystem::remove(path);
}

void test_random_graph_density_bounds() {
    Graph graph = gc::generate_random_graph(20, 0.4, 42);
    assert(graph.vertex_count() == 20);
    assert(graph.edge_count() >= 0);
    assert(graph.edge_count() <= (20 * 19) / 2);
}

void test_csv_writer() {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "gc_test_metrics.csv";
    std::vector<Metrics> metrics = {
        {"exp", "case_a", 5, {false, true, false}, true, false, 123.0, 10, 20},
        {"exp", "case_b", 6, {true, true, true}, false, true, 456.0, 30, 40},
    };

    gc::write_metrics_csv(path.string(), metrics);
    std::ifstream in(path);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert(content.find("experiment,instance,color_limit") != std::string::npos);
    assert(content.find("case_a") != std::string::npos);
    assert(content.find("case_b") != std::string::npos);
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    test_small_graph_solution();
    test_triangle_infeasible_with_two_colors();
    test_search_limits_abort();
    test_dimacs_loader();
    test_random_graph_density_bounds();
    test_csv_writer();
    std::cout << "All C++ graph coloring tests passed.\n";
    return 0;
}
