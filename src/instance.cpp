#include "instance.hpp"

#include <algorithm>
#include <numeric>
#include <random>

namespace {

Problem build(std::vector<int> costs) {
    Problem p;
    p.n = static_cast<int>(costs.size());
    p.costs = std::move(costs);
    p.goal_mask = p.n >= 31 ? 0xFFFFFFFFu : ((1u << (p.n + 1)) - 1u);
    p.people_mask = p.goal_mask ^ p.torch_bit();
    return p;
}

}  

Problem make_classic() {
    return build({1, 2, 5, 10});
}

Problem make_random(int n, unsigned seed) {
    std::vector<int> pool(static_cast<size_t>(2 * n) + 1);
    std::iota(pool.begin(), pool.end(), 1);
    std::mt19937 rng(seed);
    std::shuffle(pool.begin(), pool.end(), rng);
    std::vector<int> costs(pool.begin(), pool.begin() + n);
    std::sort(costs.begin(), costs.end());
    return build(std::move(costs));
}

Problem make_geometric(int n) {
    std::vector<int> costs(static_cast<size_t>(n));
    for (int i = 0; i < n; i++) costs[static_cast<size_t>(i)] = 1 << i;
    return build(std::move(costs));
}
