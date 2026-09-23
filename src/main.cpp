#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "instance.hpp"
#include "metrics.hpp"
#include "search.hpp"
#include "state.hpp"

namespace {

struct Row {
    std::string algo;
    std::string heuristic;
    std::string family;
    int n = 0;
    int instance_idx = 0;
    unsigned seed = 0;
    int trial = 0;
    uint64_t cap = 0;
    bool found = false;
    int solution_cost = -1;
    int optimal_cost = -1;
    int path_len = -1;
    bool path_recheck_ok = false;
    uint64_t expanded = 0;
    uint64_t generated = 0;
    uint64_t duplicates_skipped = 0;
    uint64_t max_frontier = 0;
    uint64_t max_explored = 0;
    uint64_t max_nodes = 0;
    bool limit_hit = false;
    int64_t time_ns = 0;
};

struct Outcome {
    SearchResult result;
    SearchMetrics metrics;
    int64_t time_ns = 0;
};

Outcome run_once(const Problem& p, const SearchConfig& cfg) {
    Outcome o;
    SearchMetrics m;
    const auto t0 = std::chrono::steady_clock::now();
    o.result = graph_search(p, cfg, m);
    const auto t1 = std::chrono::steady_clock::now();
    o.metrics = m;
    o.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return o;
}

int edge_cost(const Problem& p, uint32_t from, uint32_t to) {
    const uint32_t diff = from ^ to;
    const uint32_t movers = diff & ~p.torch_bit() & p.people_mask;
    int c = 0;
    for (int i = 0; i < p.n; i++)
        if (movers & (1u << i)) c = std::max(c, p.costs[static_cast<size_t>(i)]);
    return c;
}

bool recheck_path(const Problem& p, const std::vector<uint32_t>& path, int reported_cost) {
    if (path.empty() || path.front() != p.start_bits() || path.back() != p.goal_mask)
        return false;
    const uint32_t torch = p.torch_bit();
    int total = 0;
    for (size_t k = 0; k + 1 < path.size(); k++) {
        const uint32_t diff = path[k] ^ path[k + 1];
        if (!(diff & torch)) return false;
        const uint32_t movers = diff & ~torch & p.people_mask;
        const int cnt = __builtin_popcount(movers);
        if (cnt < 1 || cnt > 2) return false;
        if (path[k] & torch) {
            if ((movers & path[k]) != movers) return false;
        } else {
            if ((movers & path[k]) != 0) return false;
        }
        total += edge_cost(p, path[k], path[k + 1]);
    }
    return total == reported_cost;
}

SearchResult search_from(const Problem& p, uint32_t start, Algo algo, HeuristicId h) {
    SearchConfig cfg;
    cfg.algo = algo;
    cfg.heuristic = h;
    cfg.expansion_cap = 100000000;
    cfg.start = start;
    SearchMetrics m;
    return graph_search(p, cfg, m);
}

int optimal_cost_of(const Problem& p) {
    const SearchResult r = search_from(p, p.start_bits(), Algo::UCS, HeuristicId::H0);
    if (!r.found || !recheck_path(p, r.path, r.cost)) {
        std::fprintf(stderr, "FATAL: UCS failed to certify an optimal solution\n");
        std::exit(1);
    }
    return r.cost;
}

struct AlgoSpec {
    Algo algo;
    HeuristicId h;
};

const std::vector<AlgoSpec>& battery() {
    static const std::vector<AlgoSpec> b = {
        {Algo::BFS,   HeuristicId::H0},
        {Algo::DFS,   HeuristicId::H0},
        {Algo::UCS,   HeuristicId::H0},
        {Algo::ASTAR, HeuristicId::H0},
        {Algo::ASTAR, HeuristicId::H1},
        {Algo::ASTAR, HeuristicId::H2},
        {Algo::ASTAR, HeuristicId::H3},
    };
    return b;
}

int64_t median(std::vector<int64_t> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const size_t k = v.size() / 2;
    return v.size() % 2 ? v[k] : (v[k - 1] + v[k]) / 2;
}

void run_battery(const Problem& p, const std::string& family, int instance_idx, unsigned seed,
                 int trials, int warmups, uint64_t cap, int optimal, std::vector<Row>& rows) {
    std::printf("  instance n=%d %s #%d seed=%u optimal=%d\n", p.n, family.c_str(),
                instance_idx, seed, optimal);
    for (const AlgoSpec& spec : battery()) {
        SearchConfig cfg;
        cfg.algo = spec.algo;
        cfg.heuristic = spec.h;
        cfg.expansion_cap = cap;
        cfg.start = p.start_bits();

        for (int w = 0; w < warmups; w++) run_once(p, cfg);

        uint64_t expanded_ref = 0;
        int cost_ref = -2;
        bool nondeterministic = false;
        std::vector<int64_t> times;

        for (int t = 0; t < trials; t++) {
            Outcome o = run_once(p, cfg);
            if (t == 0) {
                expanded_ref = o.metrics.expanded;
                cost_ref = o.result.found ? o.result.cost : -1;
            }
            if (o.metrics.expanded != expanded_ref ||
                (o.result.found ? o.result.cost : -1) != cost_ref)
                nondeterministic = true;
            times.push_back(o.time_ns);

            Row row;
            row.algo = algo_name(spec.algo);
            row.heuristic = heuristic_name(spec.h);
            row.family = family;
            row.n = p.n;
            row.instance_idx = instance_idx;
            row.seed = seed;
            row.trial = t;
            row.cap = cap;
            row.found = o.result.found;
            row.solution_cost = o.result.found ? o.result.cost : -1;
            row.optimal_cost = optimal;
            row.path_len = o.result.found ? static_cast<int>(o.result.path.size()) - 1 : -1;
            row.path_recheck_ok = o.result.found && recheck_path(p, o.result.path, o.result.cost);
            row.expanded = o.metrics.expanded;
            row.generated = o.metrics.generated;
            row.duplicates_skipped = o.metrics.duplicates_skipped;
            row.max_frontier = o.metrics.max_frontier;
            row.max_explored = o.metrics.max_explored;
            row.max_nodes = o.metrics.max_nodes;
            row.limit_hit = o.metrics.limit_hit;
            row.time_ns = o.time_ns;
            rows.push_back(row);
        }

        if (nondeterministic)
            std::fprintf(stderr, "WARN: nondeterministic results for %s/%s\n",
                         algo_name(spec.algo), heuristic_name(spec.h));

        const Row& first = rows[rows.size() - static_cast<size_t>(trials)];
        std::printf("    %-6s h=%-2s cost=%-5d exp=%-6llu gen=%-6llu frontier=%-6llu "
                    "t_med=%8lld ns%s\n",
                    algo_name(spec.algo), heuristic_name(spec.h), first.solution_cost,
                    (unsigned long long)expanded_ref, (unsigned long long)rows.back().generated,
                    (unsigned long long)rows.back().max_frontier, median(std::move(times)),
                    rows.back().limit_hit ? "  [LIMIT]" : "");
    }
}

void write_csv(const std::string& path, const std::vector<Row>& rows) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "FATAL: cannot open %s for writing\n", path.c_str());
        std::exit(1);
    }
    std::fprintf(f, "algo,heuristic,family,n,instance_idx,seed,trial,cap,found,"
                    "solution_cost,optimal_cost,path_len,path_recheck_ok,expanded,generated,"
                    "duplicates_skipped,max_frontier,max_explored,max_nodes,limit_hit,time_ns\n");
    for (const Row& r : rows) {
        std::fprintf(f,
                     "%s,%s,%s,%d,%d,%u,%d,%llu,%d,%d,%d,%d,%d,%llu,%llu,%llu,%llu,%llu,%llu,"
                     "%d,%lld\n",
                     r.algo.c_str(), r.heuristic.c_str(), r.family.c_str(), r.n, r.instance_idx,
                     r.seed, r.trial, (unsigned long long)r.cap, r.found ? 1 : 0, r.solution_cost,
                     r.optimal_cost, r.path_len, r.path_recheck_ok ? 1 : 0,
                     (unsigned long long)r.expanded, (unsigned long long)r.generated,
                     (unsigned long long)r.duplicates_skipped,
                     (unsigned long long)r.max_frontier, (unsigned long long)r.max_explored,
                     (unsigned long long)r.max_nodes, r.limit_hit ? 1 : 0,
                     (long long)r.time_ns);
    }
    std::fclose(f);
}

bool arg_flag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; i++)
        if (std::strcmp(argv[i], flag) == 0) return true;
    return false;
}

const char* arg_value(int argc, char** argv, const char* flag, const char* def) {
    for (int i = 1; i < argc - 1; i++)
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    return def;
}

int mode_selftest() {
    int failures = 0;
    auto check = [&](bool ok, const std::string& label) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", label.c_str());
        if (!ok) failures++;
    };

    const Problem classic = make_classic();
    const int opt = optimal_cost_of(classic);

    struct Full {
        SearchResult result;
        SearchMetrics metrics;
    };
    auto classic_search = [&](Algo a, HeuristicId h) {
        SearchConfig cfg;
        cfg.algo = a;
        cfg.heuristic = h;
        cfg.expansion_cap = 1000000;
        cfg.start = classic.start_bits();
        SearchMetrics m;
        SearchResult r = graph_search(classic, cfg, m);
        return Full{r, m};
    };

    const Full ucs = classic_search(Algo::UCS, HeuristicId::H0);
    const Full ah0 = classic_search(Algo::ASTAR, HeuristicId::H0);
    const Full ah1 = classic_search(Algo::ASTAR, HeuristicId::H1);
    const Full ah2 = classic_search(Algo::ASTAR, HeuristicId::H2);
    const Full ah3 = classic_search(Algo::ASTAR, HeuristicId::H3);
    const Full bfs = classic_search(Algo::BFS, HeuristicId::H0);
    const Full dfs = classic_search(Algo::DFS, HeuristicId::H0);

    check(opt == 17, "classic optimum is 17");
    check(ucs.result.found && ucs.result.cost == 17, "UCS finds optimal cost 17");
    check(ah1.result.found && ah1.result.cost == 17, "A*(h1) finds optimal cost 17");
    check(ah2.result.found && ah2.result.cost == 17, "A*(h2) finds optimal cost 17");
    check(ah3.result.found && ah3.result.cost >= 17, "A*(h3) finds a valid solution");
    check(bfs.result.found && bfs.result.cost >= 17, "BFS finds a solution (cost >= 17)");
    check(dfs.result.found && dfs.result.cost >= 17, "DFS finds a solution (cost >= 17)");
    check(bfs.result.cost > 17, "BFS is NOT cost-optimal on classic instance (cost > 17)");
    check(recheck_path(classic, ucs.result.path, ucs.result.cost), "path recheck: UCS path valid");
    check(recheck_path(classic, ah1.result.path, ah1.result.cost), "path recheck: A*(h1) path valid");
    check(recheck_path(classic, bfs.result.path, bfs.result.cost), "path recheck: BFS path valid");
    check(recheck_path(classic, dfs.result.path, dfs.result.cost), "path recheck: DFS path valid");
    check(ah0.result.found && ah0.result.cost == ucs.result.cost, "A*(h0) cost equals UCS cost");
    check(ah0.metrics.expanded == ucs.metrics.expanded && ah0.metrics.generated == ucs.metrics.generated,
          "A*(h0) mimics UCS exactly (same expansions/generations)");

    const Problem r1 = make_random(6, 42);
    const Problem r2 = make_random(6, 42);
    check(r1.costs == r2.costs, "instance generator is deterministic");

    bool opt_match = true;
    for (unsigned seed = 0; seed < 5; seed++) {
        const Problem p = make_random(7, seed);
        const int o = optimal_cost_of(p);
        const SearchResult a = search_from(p, p.start_bits(), Algo::ASTAR, HeuristicId::H1);
        const SearchResult b = search_from(p, p.start_bits(), Algo::ASTAR, HeuristicId::H2);
        if (!a.found || !b.found || a.cost != o || b.cost != o) opt_match = false;
    }
    check(opt_match, "A*(h1) and A*(h2) optimal on 5 random instances (n=7)");

    int h3_subopt = 0;
    for (unsigned seed = 0; seed < 5; seed++) {
        const Problem p = make_random(7, seed);
        const int o = optimal_cost_of(p);
        const SearchResult a = search_from(p, p.start_bits(), Algo::ASTAR, HeuristicId::H3);
        if (a.found && a.cost > o) h3_subopt++;
    }
    std::printf("[INFO] A*(h3) suboptimal on %d/5 random instances (inadmissible h as designed)\n",
                h3_subopt);

    int admissible_ok = 1;
    int consistent_ok = 1;
    int h3_violations = 0;
    for (size_t k = 0; k + 1 < ucs.result.path.size(); k++) {
        const uint32_t s = ucs.result.path[k];
        const uint32_t s2 = ucs.result.path[k + 1];
        const int rem = search_from(classic, s2, Algo::UCS, HeuristicId::H0).cost;
        const int c = edge_cost(classic, s, s2);
        for (HeuristicId h : {HeuristicId::H1, HeuristicId::H2}) {
            if (heuristic(h, classic, s2) > rem) admissible_ok = 0;
            if (heuristic(h, classic, s) - heuristic(h, classic, s2) > c) consistent_ok = 0;
        }
        const int h3v = heuristic(HeuristicId::H3, classic, s2);
        if (h3v > rem) h3_violations++;
    }
    check(admissible_ok, "h1 and h2 admissible along classic optimal path");
    check(consistent_ok, "h1 and h2 consistent along classic optimal path");
    std::printf("[INFO] h3 inadmissible at %d path states (expected)\n", h3_violations);

    std::printf(failures == 0 ? "\nSELFTEST: ALL PASS\n" : "\nSELFTEST: %d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}

int mode_classic(const std::string& out, int trials, uint64_t cap) {
    const Problem p = make_classic();
    const int opt = optimal_cost_of(p);
    std::vector<Row> rows;
    run_battery(p, "classic", 0, 0, trials, 2, cap, opt, rows);
    write_csv(out, rows);
    std::printf("Wrote %zu rows to %s\n", rows.size(), out.c_str());
    return 0;
}

int mode_benchmark(const std::string& out, int trials, const std::vector<int>& ns, int instances,
                   uint64_t cap) {
    std::vector<Row> rows;
    for (int n : ns) {
        for (int fi = 0; fi < instances; fi++) {
            const unsigned seed = static_cast<unsigned>(1000 * n + fi);
            const Problem p = make_random(n, seed);
            const int opt = optimal_cost_of(p);
            run_battery(p, "random", fi, seed, trials, 2, cap, opt, rows);
        }
        const Problem g = make_geometric(n);
        const int opt = optimal_cost_of(g);
        run_battery(g, "geometric", 0, 0, trials, 2, cap, opt, rows);
    }
    write_csv(out, rows);
    std::printf("Wrote %zu rows to %s\n", rows.size(), out.c_str());
    return 0;
}

void usage() {
    std::printf(
        "tp1_app - Bridge and Torch search benchmark\n\n"
        "  tp1_app --selftest\n"
        "  tp1_app --classic   [--out results_classic.csv] [--trials 30] [--cap 15000]\n"
        "  tp1_app --benchmark [--out results_benchmark.csv] [--trials 30]\n"
        "                      [--ns 4,6,8,10,12] [--instances 5] [--cap 1000000]\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || arg_flag(argc, argv, "--help")) {
        usage();
        return 0;
    }

    if (arg_flag(argc, argv, "--selftest")) return mode_selftest();

    const int trials = std::atoi(arg_value(argc, argv, "--trials", "30"));

    if (arg_flag(argc, argv, "--classic")) {
        const uint64_t cap =
            std::strtoull(arg_value(argc, argv, "--cap", "15000"), nullptr, 10);
        const std::string out = arg_value(argc, argv, "--out", "results_classic.csv");
        return mode_classic(out, trials, cap);
    }

    if (arg_flag(argc, argv, "--benchmark")) {
        const uint64_t cap =
            std::strtoull(arg_value(argc, argv, "--cap", "1000000"), nullptr, 10);
        const std::string out = arg_value(argc, argv, "--out", "results_benchmark.csv");
        const std::string ns_str = arg_value(argc, argv, "--ns", "4,6,8,10,12");
        std::vector<int> ns;
        size_t pos = 0;
        while (pos < ns_str.size()) {
            const size_t comma = ns_str.find(',', pos);
            const std::string tok = ns_str.substr(
                pos, comma == std::string::npos ? std::string::npos : comma - pos);
            if (!tok.empty()) ns.push_back(std::atoi(tok.c_str()));
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        const int instances = std::atoi(arg_value(argc, argv, "--instances", "5"));
        return mode_benchmark(out, trials, ns, instances, cap);
    }

    usage();
    return 1;
}
