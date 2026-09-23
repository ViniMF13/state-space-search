#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <cstdint>
#include <vector>

struct Problem {
    int n = 0;
    std::vector<int> costs;
    uint32_t people_mask = 0;
    uint32_t goal_mask = 0;

    uint32_t torch_bit() const { return 1u << n; } 
    uint32_t start_bits() const { return 0; }
};

Problem make_classic();
Problem make_random(int n, unsigned seed);
Problem make_geometric(int n);

#endif
