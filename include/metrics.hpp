#ifndef METRICS_HPP
#define METRICS_HPP

#include <cstdint>

struct SearchMetrics {
    uint64_t expanded = 0;
    uint64_t generated = 0;
    uint64_t duplicates_skipped = 0;
    uint64_t max_frontier = 0;
    uint64_t max_explored = 0;
    uint64_t max_nodes = 0;
    bool limit_hit = false;
};

#endif
