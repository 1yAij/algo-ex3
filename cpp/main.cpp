#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "src/graph_coloring.hpp"

namespace {

using gc::ColoringResult;
using gc::Graph;
using gc::Heuristics;
using gc::Metrics;

constexpr const char* kDatasetDir = "地图数据";
constexpr const char* kOutputDir = "cpp/output";
const gc::SearchLimits kDefaultLimits{2'000'000, 2500.0};
// 实验 4：较大 n 需单实例长时限；用固定平均度 p=d/(n-1) 比固定 p 更不易出现「前面全 0、最后打满 30s」
const gc::SearchLimits kExperiment4Limits{2'000'000, 60'000.0};

// Independent proof-of-correctness for a claimed proper coloring (Experiment 3, etc.)
bool verify_solution(int num_vertices,
                       const std::vector<std::pair<int, int>>& edges,
                       int color_limit,
                       const std::vector<int>& colors_solution) {
    if (static_cast<int>(colors_solution.size()) != num_vertices) {
        std::cerr << "VERIFY: wrong vector size, expected " << num_vertices << ", got "
                  << colors_solution.size() << '\n';
        return false;
    }
    for (int c : colors_solution) {
        if (c < 0 || c >= color_limit) {
            return false;  // unassigned or out of [0, color_limit)
        }
    }
    for (const auto& edge : edges) {
        int u = edge.first;
        int v = edge.second;
        if (u < 0 || u >= num_vertices || v < 0 || v >= num_vertices) {
            std::cerr << "VERIFY: bad edge index (" << u << ", " << v << ")\n";
            return false;
        }
        if (colors_solution[static_cast<std::size_t>(u)] == colors_solution[static_cast<std::size_t>(v)]) {
            std::cerr << "CONFLICT between node " << u << " and " << v << " (color: " << colors_solution[static_cast<std::size_t>(u)]
                      << ")\n";
            return false;
        }
    }
    return true;
}

std::vector<std::pair<int, int>> collect_undirected_edges_0(const Graph& graph) {
    std::vector<std::pair<int, int>> edges;
    for (int u = 0; u < graph.vertex_count(); ++u) {
        for (int v : graph.neighbors(u)) {
            if (u < v) {
                edges.emplace_back(u, v);
            }
        }
    }
    return edges;
}

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

void append_result(std::vector<Metrics>& rows,
                   const std::string& experiment,
                   const std::string& instance,
                   ColoringResult result) {
    result.metrics.experiment = experiment;
    result.metrics.instance = instance;
    rows.push_back(result.metrics);
}

void run_experiment_1(std::vector<Metrics>& rows) {
    // 与 地图数据/small map.txt 同图：含 K4(1,2,5,6)，色数为 4；CSV 中 k=1,2,3 为失败，k=4 为首次成功
    Graph graph = gc::load_dimacs_graph((std::filesystem::path(kDatasetDir) / "small_map.col").string());
    const Heuristics heuristics{true, true, true};
    int minimum_colors = -1;
    ColoringResult best_result;
    const std::vector<std::pair<int, int>> edges = collect_undirected_edges_0(graph);

    for (int colors = 1; colors <= graph.vertex_count(); ++colors) {
        ColoringResult result = gc::solve_graph_coloring(graph, colors, heuristics, kDefaultLimits);
        append_result(rows, "experiment_1", "small_map", result);
        if (result.success) {
            if (!verify_solution(graph.vertex_count(), edges, colors, result.colors)) {
                std::cerr << "[BUG] experiment_1: invalid coloring at k=" << colors << '\n';
                throw std::runtime_error("experiment 1 verification failed");
            }
            minimum_colors = colors;
            best_result = result;
            break;
        }
    }

    if (minimum_colors == -1 || !is_valid_coloring(graph, best_result.colors)) {
        throw std::runtime_error("experiment 1 failed to find a valid coloring");
    }

    std::cout << "[Experiment 1] minimum colors = " << minimum_colors << '\n';
}

Graph load_dataset(const std::string& filename) {
    return gc::load_dimacs_graph((std::filesystem::path(kDatasetDir) / filename).string());
}

void run_experiment_2(std::vector<Metrics>& rows) {
    const std::vector<std::string> datasets = {"le450_5a.col", "le450_15b.col", "le450_25a.col"};
    const std::vector<int> colors = {5, 15, 25};
    const Heuristics heuristics{true, true, true};

    for (const std::string& dataset : datasets) {
        Graph graph = load_dataset(dataset);
        for (int color_limit : colors) {
            ColoringResult result = gc::solve_graph_coloring(graph, color_limit, heuristics, kDefaultLimits);
            append_result(rows, "experiment_2", dataset, result);
            std::cout << "[Experiment 2] " << dataset << " colors=" << color_limit
                      << " success=" << result.success << '\n';
        }
    }
}

void run_experiment_3(std::vector<Metrics>& rows) {
    // 策略对比用 le450_5a（450 点，5-染色）
    Graph graph = load_dataset("le450_5a.col");
    const int k_experiment3_colors = 5;
    const std::vector<std::pair<int, int>> exp3_edges = collect_undirected_edges_0(graph);
    const std::vector<std::pair<std::string, Heuristics>> configs = {
        {"plain_backtracking",  {false, false, false, false}},
        {"degree_descending",   {true,  false, false, false}},
        {"mrv",                 {false, true,  false, false}},
        {"forward_checking",    {false, false, true,  false}},
        {"fc_mrv",              {false, true,  true,  false}},
        {"fc_mrv_lcv",          {false, true,  true,  true }},
    };

    for (const auto& [name, heuristics] : configs) {
        ColoringResult result = gc::solve_graph_coloring(graph, k_experiment3_colors, heuristics, kDefaultLimits);
        if (result.success) {
            if (!verify_solution(graph.vertex_count(), exp3_edges, k_experiment3_colors, result.colors)) {
                std::cerr << "[BUG CONFIRMED] Algorithm claimed success but returned an INVALID coloring.\n";
                std::cerr << "Strategy: " << name << " (le450_5a, " << k_experiment3_colors << " colors)\n";
                throw std::runtime_error("run_experiment_3: verification failed for " + name);
            }
            std::cout << "[Experiment 3] " << name << " success=1  VERIFY_OK (valid proper coloring)\n";
        } else {
            std::cout << "[Experiment 3] " << name << " success=0\n";
        }
        append_result(rows, "experiment_3", name, result);
    }
}

void run_experiment_4(std::vector<Metrics>& rows) {
    // 规模敏感：Erdős–Rényi 上若 p 为常数，n 大时边数 ~ n² 会要么太易要么到某点卡死。
    // 用「目标平均度」d 固定、令 p = d/(n-1)（n 大时 p 自动变小）更易得到随 n 平滑变难的曲线。
    const std::vector<int> sizes = {100, 200, 300, 400, 500};
    const std::vector<int> target_mean_deg = {8, 14};
    constexpr int k_experiment4_colors = 28;
    const Heuristics heuristics{true, true, true, false};

    for (int size : sizes) {
        for (int d : target_mean_deg) {
            double p = static_cast<double>(d) / static_cast<double>(size - 1);
            if (p > 0.5) {
                p = 0.5;  // 小 n 时 d/(n-1) 可很大，G(n,0.5) 上界
            }
            const std::uint32_t seed =
                static_cast<std::uint32_t>(static_cast<std::uint32_t>(size) * 10007u
                                             + static_cast<std::uint32_t>(d) * 1009u);
            Graph graph = gc::generate_random_graph(size, p, seed);
            ColoringResult result =
                gc::solve_graph_coloring(graph, k_experiment4_colors, heuristics, kExperiment4Limits);
            // 实例名：V{n}_d{d} 表示本行使用的目标平均度（不是边密度 p）
            const std::string instance = "V" + std::to_string(size) + "_d" + std::to_string(d);
            append_result(rows, "experiment_4", instance, result);
            std::cout << "[Experiment 4] " << instance << " p=" << p << " k=" << k_experiment4_colors
                      << " success=" << result.success << '\n';
        }
    }
}

void run_experiment_5(std::vector<Metrics>& rows) {
    Graph graph = load_dataset("le450_15b.col");
    const std::vector<int> colors = {5, 10, 15, 20, 25};
    const Heuristics heuristics{true, true, true};

    for (int color_limit : colors) {
        ColoringResult result = gc::solve_graph_coloring(graph, color_limit, heuristics, kDefaultLimits);
        append_result(rows, "experiment_5", "le450_15b.col", result);
        std::cout << "[Experiment 5] le450_15b.col colors=" << color_limit
                  << " success=" << result.success << '\n';
    }
}

}  // namespace

int main() {
    try {
        std::filesystem::create_directories(kOutputDir);
        std::vector<Metrics> rows;
        rows.reserve(40);

        run_experiment_1(rows);
        run_experiment_2(rows);
        run_experiment_3(rows);
        run_experiment_4(rows);
        run_experiment_5(rows);

        const std::string output_path = (std::filesystem::path(kOutputDir) / "graph_coloring_results.csv").string();
        gc::write_metrics_csv(output_path, rows);
        std::cout << "Results written to " << output_path << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
