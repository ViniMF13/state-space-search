#include "state.hpp"

#include <algorithm>
#include <functional>

std::vector<Edge> get_edges(const Problem& p, uint32_t bits) {
    std::vector<Edge> out;
    const uint32_t torch = p.torch_bit();
    const bool torch_far = (bits & torch) != 0;
    const uint32_t same_side = torch_far ? (bits & p.people_mask) : (~bits & p.people_mask);

    for (int i = 0; i < p.n; i++) {
        const uint32_t bi = 1u << i;
        if (!(same_side & bi)) continue;
        out.push_back({bits ^ (bi | torch), p.costs[static_cast<size_t>(i)]});
        for (int j = i + 1; j < p.n; j++) {
            const uint32_t bj = 1u << j;
            if (same_side & bj) {
                const int cost = std::max(p.costs[static_cast<size_t>(i)],
                                          p.costs[static_cast<size_t>(j)]);
                out.push_back({bits ^ (bi | bj | torch), cost});
            }
        }
    }
    return out;
}

bool is_goal(const Problem& p, uint32_t bits) {
    return bits == p.goal_mask;
}

int heuristic(HeuristicId id, const Problem& p, uint32_t bits) {
    if (id == HeuristicId::H0) return 0;

    const uint32_t remaining = ~bits & p.people_mask;
    if (remaining == 0) return 0;

    int mx = 0;
    int sum = 0;
    int cnt = 0;
    std::array<int, 31> buf{};
    for (int i = 0; i < p.n; i++) {
        if (remaining & (1u << i)) {
            const int c = p.costs[static_cast<size_t>(i)];
            mx = std::max(mx, c);
            sum += c;
            buf[static_cast<size_t>(cnt++)] = c;
        }
    }

    switch (id) {
        case HeuristicId::H0: return 0;
        case HeuristicId::H1: return mx;
        case HeuristicId::H3: return sum;
        case HeuristicId::H2: {
            std::sort(buf.begin(), buf.begin() + cnt, std::greater<int>());
            int acc = 0;
            for (int k = 0; k < cnt; k += 2) acc += buf[static_cast<size_t>(k)];
            return acc;
        }
    }
    return 0;
}
