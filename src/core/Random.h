#pragma once

#include <cstdint>

namespace ee {

// Small deterministic RNG (splitmix64). Same seed -> same sequence on every platform.
class Random {
public:
    explicit Random(std::uint64_t seed = 0x9E3779B97F4A7C15ull) : state_(seed) {}

    std::uint64_t nextU64() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // Uniform in [0, 1).
    float nextFloat() { return static_cast<float>(nextU64() >> 40) * (1.0f / 16777216.0f); }

    float range(float lo, float hi) { return lo + (hi - lo) * nextFloat(); }

private:
    std::uint64_t state_;
};

} // namespace ee
