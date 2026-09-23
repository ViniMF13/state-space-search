#include "search.hpp"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace {

struct Node {
    uint32_t bits;
    int g;
    int32_t parent;
};

struct QEntry {
    int64_t priority;
    uint64_t seq;
    uint32_t node;
};

struct QGreater {
    bool operator()(const QEntry& a, const QEntry& b) const {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.seq > b.seq;
    }
};

}  // namespace

const char* algo_name(Algo a) {
    switch (a) {
        case Algo::BFS: return "BFS";
        case Algo::DFS: return "DFS";
        case Algo::UCS: return "UCS";
        case Algo::ASTAR: return "ASTAR";
    }
    return "?";
}

const char* heuristic_name(HeuristicId h) {
    switch (h) {
        case HeuristicId::H0: return "h0";
        case HeuristicId::H1: return "h1";
        case HeuristicId::H2: return "h2";
        case HeuristicId::H3: return "h3";
    }
    return "?";
}

SearchResult graph_search(const Problem& p, const SearchConfig& cfg, SearchMetrics& m) {
    m = SearchMetrics{};
    SearchResult result;

    std::vector<Node> nodes;
    nodes.reserve(1024);
    nodes.push_back({cfg.start, 0, -1});

    auto priority_of = [&](uint64_t seq, int g, uint32_t bits) -> int64_t {
        switch (cfg.algo) {
            case Algo::BFS: return static_cast<int64_t>(seq);
            case Algo::DFS: return -static_cast<int64_t>(seq);
            case Algo::UCS: return g;
            case Algo::ASTAR: return g + heuristic(cfg.heuristic, p, bits);
        }
        return 0;
    };

    std::priority_queue<QEntry, std::vector<QEntry>, QGreater> frontier;
    frontier.push({priority_of(0, 0, cfg.start), 0, 0});

    std::unordered_set<uint32_t> explored;
    explored.reserve(4096);

    uint64_t seq = 1;

    while (!frontier.empty()) {
        const QEntry e = frontier.top();
        frontier.pop();
        const Node& nd = nodes[e.node];

        if (explored.count(nd.bits)) continue;

        explored.insert(nd.bits);
        m.expanded++;
        m.max_explored = std::max(m.max_explored, static_cast<uint64_t>(explored.size()));

        if (is_goal(p, nd.bits)) {
            result.found = true;
            result.cost = nd.g;
            for (int32_t i = static_cast<int32_t>(e.node); i != -1; i = nodes[static_cast<size_t>(i)].parent)
                result.path.push_back(nodes[static_cast<size_t>(i)].bits);
            std::reverse(result.path.begin(), result.path.end());
            return result;
        }

        for (const Edge& ed : get_edges(p, nd.bits)) {
            m.generated++;
            if (explored.count(ed.bits)) {
                m.duplicates_skipped++;
                continue;
            }
            nodes.push_back({ed.bits, nd.g + ed.cost, static_cast<int32_t>(e.node)});
            m.max_nodes = std::max(m.max_nodes, static_cast<uint64_t>(nodes.size()));
            const uint64_t s = seq++;
            frontier.push({priority_of(s, nodes.back().g, ed.bits), s,
                           static_cast<uint32_t>(nodes.size() - 1)});
            m.max_frontier = std::max(m.max_frontier, static_cast<uint64_t>(frontier.size()));
        }

        if (m.expanded >= cfg.expansion_cap) {
            m.limit_hit = true;
            break;
        }
    }

    return result;
}
