#ifndef STATE_HPP
#define STATE_HPP

#include "instance.hpp"

#include <vector>

struct Edge {
    uint32_t bits;
    int cost;
};

enum class HeuristicId : int { H0 = 0, H1 = 1, H2 = 2, H3 = 3 };

std::vector<Edge> get_edges(const Problem& p, uint32_t bits);
bool is_goal(const Problem& p, uint32_t bits);
int heuristic(HeuristicId id, const Problem& p, uint32_t bits);

#endif
