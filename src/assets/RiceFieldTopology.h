#pragma once

#include <cstdint>

namespace openemperor::assets {

// Canonical inactive Rice paddy base, source view0 and variation0. Bits0..7
// represent N, NE, E, SE, S, SW, W, NW. This independent Boolean classification
// admits only offsets0..46 of group0x42d. It does not select the staffed seasonal
// bank or quantity overlays; see the agriculture source contract.
constexpr std::uint8_t rice_inactive_topology_offset(std::uint8_t mask) noexcept {
    const auto joined = [mask](unsigned direction) constexpr {
        return (mask & (1u << (direction % 8u))) != 0;
    };
    unsigned cardinals = 0;
    unsigned only_cardinal = 0;
    for (unsigned r = 0; r < 4; ++r) {
        if (joined(2u * r)) {
            ++cardinals;
            only_cardinal = r;
        }
    }
    if (cardinals == 0) return 0;
    if (cardinals == 1) return static_cast<std::uint8_t>(1u + only_cardinal);
    if (cardinals == 2) {
        if (joined(0) && joined(4)) return 13;
        if (joined(2) && joined(6)) return 14;
        for (unsigned r = 0; r < 4; ++r) {
            if (joined(2u * r) && joined(2u * r + 2u)) {
                return static_cast<std::uint8_t>(5u + r +
                    (joined(2u * r + 1u) ? 4u : 0u));
            }
        }
    }
    if (cardinals == 3) {
        for (unsigned r = 0; r < 4; ++r) {
            if (joined(2u * r) && joined(2u * r + 2u) && joined(2u * r + 4u)) {
                const bool first = joined(2u * r + 1u);
                const bool second = joined(2u * r + 3u);
                const unsigned base = first && second ? 42u :
                    first ? 19u : second ? 23u : 15u;
                return static_cast<std::uint8_t>(base + r);
            }
        }
    }

    unsigned diagonals = 0;
    unsigned only_diagonal = 0;
    unsigned missing_diagonal = 0;
    for (unsigned r = 0; r < 4; ++r) {
        if (joined(2u * r + 1u)) {
            ++diagonals;
            only_diagonal = r;
        } else {
            missing_diagonal = r;
        }
    }
    if (diagonals == 0) return 27;
    if (diagonals == 1) {
        return static_cast<std::uint8_t>(30u + (only_diagonal + 2u) % 4u);
    }
    if (diagonals == 2) {
        if (joined(1) && joined(5)) return 28;
        if (joined(3) && joined(7)) return 29;
        for (unsigned r = 0; r < 4; ++r) {
            if (joined(2u * r + 1u) && joined(2u * r + 3u)) {
                return static_cast<std::uint8_t>(34u + (r + 3u) % 4u);
            }
        }
    }
    if (diagonals == 3) {
        return static_cast<std::uint8_t>(38u + (missing_diagonal + 1u) % 4u);
    }
    return 46;
}

} // namespace openemperor::assets
