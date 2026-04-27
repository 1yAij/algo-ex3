#include "graph_coloring.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace gc {

namespace {

// --- CSP backtracking: textbook layout with optional FC, MRV, and static degree order. ---

class CspColoringSolver {
public:
    CspColoringSolver(const Graph& graph, int k, const Heuristics& heuristics, const SearchLimits& limits)
        : graph_(graph),
          n_(graph.vertex_count()),
          k_(k),
          h_(heuristics),
          limits_(limits),
          start_(std::chrono::steady_clock::now()),
          color_(n_, -1) {
        var_order_ = build_variable_order();
    }

    bool solve() {
        if (h_.mrv) {
            return mrv_recurse(0);
        }
        return static_order_recurse(0);
    }

    std::vector<int> move_colors() {
        return std::move(color_);
    }
    std::uint64_t backtracks() const {
        return backtracks_;
    }
    std::uint64_t explored() const {
        return explored_;
    }
    bool aborted() const {
        return aborted_;
    }
    std::chrono::steady_clock::time_point start() const {
        return start_;
    }

private:
    const Graph& graph_;
    int n_ = 0;
    int k_ = 0;
    Heuristics h_;
    SearchLimits limits_;
    std::chrono::steady_clock::time_point start_;
    std::vector<int> color_;
    // Used when !MRV: vertex at each depth
    std::vector<int> var_order_;

    std::vector<std::unordered_set<int>> domains_;

    std::uint64_t backtracks_ = 0;
    std::uint64_t explored_ = 0;
    bool aborted_ = false;

    bool time_exhausted() const {
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count();
        return ms >= limits_.max_elapsed_ms;
    }

    // Lexicographic color try order: 0, 1, ..., k-1
    void init_domains_full() {
        domains_.assign(static_cast<std::size_t>(n_), {});
        for (int v = 0; v < n_; ++v) {
            set_domain_all_colors(v);
        }
    }

    // Only used to rebuild full domains at graph_coloring::solve entry (if ever needed);
    // after partial FC, always restore a saved snapshot, never a blind full reset.
    void set_domain_all_colors(int v) {
        auto& d = domains_[static_cast<std::size_t>(v)];
        d.clear();
        for (int c2 = 0; c2 < k_; ++c2) {
            d.insert(c2);
        }
    }

    static bool legal_with_assignment(const Graph& g, const std::vector<int>& col, int v, int c) {
        for (int u : g.neighbors(v)) {
            if (col[static_cast<std::size_t>(u)] == c) {
                return false;
            }
        }
        return true;
    }

    int count_legal_values(int v) const {
        int cnt = 0;
        for (int c = 0; c < k_; ++c) {
            if (legal_with_assignment(graph_, color_, v, c)) {
                ++cnt;
            }
        }
        return cnt;
    }

    // Without FC, legal colors for v under current color[].
    std::vector<int> list_legal_colors(int v) const {
        std::vector<int> out;
        for (int c = 0; c < k_; ++c) {
            if (legal_with_assignment(graph_, color_, v, c)) {
                out.push_back(c);
            }
        }
        return out;
    }

    std::vector<int> build_variable_order() {
        if (h_.mrv) {
            return {};
        }
        if (h_.degree_descending) {
            std::vector<int> ord(n_, 0);
            std::iota(ord.begin(), ord.end(), 0);
            std::sort(ord.begin(), ord.end(), [this](int a, int b) {
                const int da = graph_.degree(a);
                const int db = graph_.degree(b);
                if (da != db) {
                    return da > db;
                }
                return a < b;
            });
            return ord;
        }
        std::vector<int> natural(n_, 0);
        std::iota(natural.begin(), natural.end(), 0);
        return natural;
    }

    // --- Static order: depth t assigns var_order_[t] ---
    bool static_order_recurse(int depth) {
        if (time_exhausted()) {
            aborted_ = true;
            return false;
        }
        if (depth == n_) {
            return true;
        }
        const int v = var_order_[static_cast<std::size_t>(depth)];

        if (h_.forward_checking) {
            if (static_cast<int>(domains_.size()) != n_) {
                init_domains_full();
            }
            return try_all_colors_static_fc(v, depth);
        }
        // No forward checking: plain + degree (same logic; order differs in var_order_)
        return try_all_colors_static_plain(v, depth);
    }

    bool try_all_colors_static_plain(int v, int depth) {
        for (int c = 0; c < k_; ++c) {
            ++explored_;
            if (time_exhausted()) {
                aborted_ = true;
                return false;
            }
            if (!legal_with_assignment(graph_, color_, v, c)) {
                continue;
            }
            color_[static_cast<std::size_t>(v)] = c;
            if (static_order_recurse(depth + 1)) {
                return true;
            }
            color_[static_cast<std::size_t>(v)] = -1;
        }
        ++backtracks_;
        return false;
    }

    // Forward check on assign v = c. Record uncolored neighbors that lost c, restore on failure.
    bool try_all_colors_static_fc(int v, int depth) {
        for (int c = 0; c < k_; ++c) {
            ++explored_;
            if (time_exhausted()) {
                aborted_ = true;
                return false;
            }
            const auto& d = domains_[static_cast<std::size_t>(v)];
            if (d.find(c) == d.end() || !legal_with_assignment(graph_, color_, v, c)) {
                continue;
            }

            const std::unordered_set<int> dom_v_before = d;
            std::vector<int> pruned;
            pruned.reserve(static_cast<std::size_t>(graph_.degree(v)));

            bool fail = false;
            for (int w : graph_.neighbors(v)) {
                if (color_[static_cast<std::size_t>(w)] != -1) {
                    continue;
                }
                std::unordered_set<int>& dom_w = domains_[static_cast<std::size_t>(w)];
                if (dom_w.erase(c) > 0) {
                    pruned.push_back(w);
                    if (dom_w.empty()) {
                        fail = true;
                    }
                }
            }

            if (fail) {
                for (int w : pruned) {
                    domains_[static_cast<std::size_t>(w)].insert(c);
                }
                continue;
            }

            domains_[static_cast<std::size_t>(v)] = {c};
            color_[static_cast<std::size_t>(v)] = c;

            if (static_order_recurse(depth + 1)) {
                return true;
            }

            for (int w : pruned) {
                domains_[static_cast<std::size_t>(w)].insert(c);
            }
            domains_[static_cast<std::size_t>(v)] = dom_v_before;
            color_[static_cast<std::size_t>(v)] = -1;
        }
        ++backtracks_;
        return false;
    }

    // --- MRV: at each call pick uncolored vertex with min domain size; FC optional ---
    int select_mrv() {
        int best = -1;
        int best_sz = -1;
        for (int v = 0; v < n_; ++v) {
            if (color_[static_cast<std::size_t>(v)] != -1) {
                continue;
            }
            int sz = 0;
            if (h_.forward_checking) {
                sz = static_cast<int>(domains_[static_cast<std::size_t>(v)].size());
            } else {
                sz = count_legal_values(v);
            }
            if (best < 0 || sz < best_sz) {
                best = v;
                best_sz = sz;
            } else if (sz == best_sz) {
                if (graph_.degree(v) > graph_.degree(best) ||
                    (graph_.degree(v) == graph_.degree(best) && v < best)) {
                    best = v;
                }
            }
        }
        return best;
    }

    bool mrv_recurse(int n_assigned) {
        if (time_exhausted()) {
            aborted_ = true;
            return false;
        }
        if (n_assigned == n_) {
            return true;
        }
        if (h_.forward_checking && static_cast<int>(domains_.size()) != n_) {
            init_domains_full();
        }
        int v = select_mrv();
        if (v < 0) {
            return n_assigned == n_;
        }
        if (h_.forward_checking) {
            if (h_.lcv) {
                return try_all_mrv_fc_lcv(v, n_assigned);
            }
            return try_all_mrv_fc(v, n_assigned);
        }
        return try_all_mrv_plain(v, n_assigned);
    }

    bool try_all_mrv_plain(int v, int n_assigned) {
        for (int c = 0; c < k_; ++c) {
            ++explored_;
            if (time_exhausted()) {
                aborted_ = true;
                return false;
            }
            if (!legal_with_assignment(graph_, color_, v, c)) {
                continue;
            }
            color_[static_cast<std::size_t>(v)] = c;
            if (mrv_recurse(n_assigned + 1)) {
                return true;
            }
            color_[static_cast<std::size_t>(v)] = -1;
        }
        ++backtracks_;
        return false;
    }

    // Count how many domain values color c would remove from unassigned neighbors of v.
    int count_fc_constraints(int v, int c) const {
        int count = 0;
        for (int w : graph_.neighbors(v)) {
            if (color_[static_cast<std::size_t>(w)] != -1) continue;
            if (domains_[static_cast<std::size_t>(w)].count(c) > 0) ++count;
        }
        return count;
    }

    // Returns the legal colors in v's domain sorted ascending by constraint count (LCV).
    std::vector<int> lcv_order(int v) const {
        const auto& d = domains_[static_cast<std::size_t>(v)];
        std::vector<int> candidates;
        candidates.reserve(d.size());
        for (int c : d) {
            if (legal_with_assignment(graph_, color_, v, c)) {
                candidates.push_back(c);
            }
        }
        std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
            return count_fc_constraints(v, a) < count_fc_constraints(v, b);
        });
        return candidates;
    }

    bool try_all_mrv_fc_lcv(int v, int n_assigned) {
        const std::vector<int> ordered = lcv_order(v);
        for (int c : ordered) {
            ++explored_;
            if (time_exhausted()) {
                aborted_ = true;
                return false;
            }

            const std::unordered_set<int> dom_v_before = domains_[static_cast<std::size_t>(v)];
            std::vector<int> pruned;
            pruned.reserve(static_cast<std::size_t>(graph_.degree(v)));
            bool fail = false;
            for (int w : graph_.neighbors(v)) {
                if (color_[static_cast<std::size_t>(w)] != -1) continue;
                std::unordered_set<int>& dom_w = domains_[static_cast<std::size_t>(w)];
                if (dom_w.erase(c) > 0) {
                    pruned.push_back(w);
                    if (dom_w.empty()) fail = true;
                }
            }

            if (fail) {
                for (int w : pruned) domains_[static_cast<std::size_t>(w)].insert(c);
                continue;
            }

            domains_[static_cast<std::size_t>(v)] = {c};
            color_[static_cast<std::size_t>(v)] = c;
            if (mrv_recurse(n_assigned + 1)) return true;
            for (int w : pruned) domains_[static_cast<std::size_t>(w)].insert(c);
            domains_[static_cast<std::size_t>(v)] = dom_v_before;
            color_[static_cast<std::size_t>(v)] = -1;
        }
        ++backtracks_;
        return false;
    }

    bool try_all_mrv_fc(int v, int n_assigned) {
        for (int c = 0; c < k_; ++c) {
            ++explored_;
            if (time_exhausted()) {
                aborted_ = true;
                return false;
            }
            const auto& d = domains_[static_cast<std::size_t>(v)];
            if (d.find(c) == d.end() || !legal_with_assignment(graph_, color_, v, c)) {
                continue;
            }

            const std::unordered_set<int> dom_v_before = d;
            std::vector<int> pruned;
            pruned.reserve(static_cast<std::size_t>(graph_.degree(v)));
            bool fail = false;
            for (int w : graph_.neighbors(v)) {
                if (color_[static_cast<std::size_t>(w)] != -1) {
                    continue;
                }
                std::unordered_set<int>& dom_w = domains_[static_cast<std::size_t>(w)];
                if (dom_w.erase(c) > 0) {
                    pruned.push_back(w);
                    if (dom_w.empty()) {
                        fail = true;
                    }
                }
            }

            if (fail) {
                for (int w : pruned) {
                    domains_[static_cast<std::size_t>(w)].insert(c);
                }
                continue;
            }

            domains_[static_cast<std::size_t>(v)] = {c};
            color_[static_cast<std::size_t>(v)] = c;
            if (mrv_recurse(n_assigned + 1)) {
                return true;
            }
            for (int w : pruned) {
                domains_[static_cast<std::size_t>(w)].insert(c);
            }
            domains_[static_cast<std::size_t>(v)] = dom_v_before;
            color_[static_cast<std::size_t>(v)] = -1;
        }
        ++backtracks_;
        return false;
    }
};

}  // namespace

Graph::Graph(int vertex_count)
    : vertex_count_(vertex_count),
      adjacency_list_(vertex_count),
      adjacency_matrix_(vertex_count, std::vector<std::uint8_t>(vertex_count, 0)) {}

void Graph::add_edge(int u, int v) {
    if (u < 0 || v < 0 || u >= vertex_count_ || v >= vertex_count_) {
        throw std::out_of_range("vertex index out of range");
    }
    if (u == v || adjacency_matrix_[u][v]) {
        return;
    }
    adjacency_matrix_[u][v] = adjacency_matrix_[v][u] = 1;
    adjacency_list_[u].push_back(v);
    adjacency_list_[v].push_back(u);
    edge_count_ += 1;
}

int Graph::vertex_count() const {
    return vertex_count_;
}

int Graph::edge_count() const {
    return edge_count_;
}

int Graph::degree(int v) const {
    return static_cast<int>(adjacency_list_[v].size());
}

bool Graph::has_edge(int u, int v) const {
    return adjacency_matrix_[u][v] != 0;
}

const std::vector<int>& Graph::neighbors(int v) const {
    return adjacency_list_[v];
}

Graph load_dimacs_graph(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open DIMACS file: " + path);
    }

    Graph graph;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == 'c') {
            continue;
        }

        std::istringstream iss(line);
        char type = '\0';
        iss >> type;

        if (type == 'p') {
            std::string format;
            int vertices = 0;
            int edges = 0;
            iss >> format >> vertices >> edges;
            if (format != "edge") {
                throw std::runtime_error("unsupported DIMACS format: " + format);
            }
            graph = Graph(vertices);
        } else if (type == 'e') {
            int u = 0;
            int v = 0;
            iss >> u >> v;
            graph.add_edge(u - 1, v - 1);
        }
    }

    return graph;
}

Graph generate_random_graph(int vertex_count, double density, std::uint32_t seed) {
    if (vertex_count <= 0) {
        throw std::invalid_argument("vertex_count must be positive");
    }
    if (density < 0.0 || density > 1.0) {
        throw std::invalid_argument("density must be in [0, 1]");
    }

    Graph graph(vertex_count);
    std::mt19937 rng(seed);
    std::bernoulli_distribution dist(density);

    for (int u = 0; u < vertex_count; ++u) {
        for (int v = u + 1; v < vertex_count; ++v) {
            if (dist(rng)) {
                graph.add_edge(u, v);
            }
        }
    }
    return graph;
}

ColoringResult solve_graph_coloring(const Graph& graph,
                                    int color_limit,
                                    const Heuristics& heuristics,
                                    const SearchLimits& limits) {
    if (color_limit <= 0) {
        throw std::invalid_argument("color_limit must be positive");
    }

    CspColoringSolver solver(graph, color_limit, heuristics, limits);
    const bool success = solver.solve();
    const auto end = std::chrono::steady_clock::now();

    ColoringResult result;
    result.success = success;
    result.colors = solver.move_colors();
    result.metrics.heuristics = heuristics;
    result.metrics.color_limit = color_limit;
    result.metrics.success = success;
    result.metrics.aborted = solver.aborted();
    result.metrics.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - solver.start()).count();
    result.metrics.backtracks = solver.backtracks();
    result.metrics.explored_nodes = solver.explored();
    return result;
}

void write_metrics_csv(const std::string& path, const std::vector<Metrics>& rows) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open csv file: " + path);
    }

    out << "experiment,instance,color_limit,use_degree,use_mrv,use_forward,use_lcv,success,aborted,time_ms,backtracks,explored_nodes\n";
    out << std::fixed << std::setprecision(6);
    for (const Metrics& row : rows) {
        out << row.experiment << ','
            << row.instance << ','
            << row.color_limit << ','
            << (row.heuristics.degree_descending ? 1 : 0) << ','
            << (row.heuristics.mrv ? 1 : 0) << ','
            << (row.heuristics.forward_checking ? 1 : 0) << ','
            << (row.heuristics.lcv ? 1 : 0) << ','
            << (row.success ? 1 : 0) << ','
            << (row.aborted ? 1 : 0) << ','
            << row.elapsed_ms << ','
            << row.backtracks << ','
            << row.explored_nodes << '\n';
    }
}

std::string heuristics_to_string(const Heuristics& heuristics) {
    if (!heuristics.degree_descending && !heuristics.mrv && !heuristics.forward_checking && !heuristics.lcv) {
        return "plain_backtracking";
    }

    std::vector<std::string> parts;
    if (heuristics.degree_descending) {
        parts.emplace_back("degree");
    }
    if (heuristics.mrv) {
        parts.emplace_back("mrv");
    }
    if (heuristics.forward_checking) {
        parts.emplace_back("forward");
    }
    if (heuristics.lcv) {
        parts.emplace_back("lcv");
    }

    std::ostringstream oss;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            oss << '+';
        }
        oss << parts[i];
    }
    return oss.str();
}

}  // namespace gc