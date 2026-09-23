#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "instance.hpp"
#include "metrics.hpp"
#include "state.hpp"

#include <vector>

enum class Algo : int { BFS = 0, DFS = 1, UCS = 2, ASTAR = 3 };

struct SearchConfig {
    Algo algo = Algo::UCS;
    HeuristicId heuristic = HeuristicId::H1;
    uint64_t expansion_cap = 15000;
    uint32_t start = 0;
};

struct SearchResult {
    bool found = false;
    int cost = 0;
    std::vector<uint32_t> path;
};

SearchResult graph_search(const Problem& p, const SearchConfig& cfg, SearchMetrics& m);

const char* algo_name(Algo a);
const char* heuristic_name(HeuristicId h);

#endif
