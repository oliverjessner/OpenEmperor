#pragma once

#include <cstdint>

namespace openemperor {

inline constexpr std::uint64_t target_frame_nanoseconds=1'000'000'000ULL/60ULL;

constexpr std::uint64_t frame_wait_nanoseconds(std::uint64_t elapsed,
                                                std::uint64_t target=target_frame_nanoseconds) {
    return elapsed<target ? target-elapsed:0;
}

} // namespace openemperor
