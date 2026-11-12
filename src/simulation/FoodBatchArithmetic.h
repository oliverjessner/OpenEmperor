#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace openemperor::simulation::food_batch {

// Pure, allocation-free arithmetic over a caller-owned, canonically ordered
// catalog. Indices are not Original resource IDs. These helpers own no stock,
// actor, recipe, clock, capacity or saved state and do not admit new food kinds.
template<std::size_t N>
[[nodiscard]] std::int32_t volume(const std::array<std::int32_t,N>& amounts) {
    static_assert(N>0);
    std::int32_t result=0;
    for (const auto amount:amounts) {
        if (amount<0) throw std::invalid_argument("negative Food portion");
        if (amount>std::numeric_limits<std::int32_t>::max()-result)
            throw std::invalid_argument("Food volume outside bounded integer domain");
        result+=amount;
    }
    return result;
}

template<std::size_t N>
[[nodiscard]] std::uint64_t history_total(const std::array<std::uint64_t,N>& counters) {
    static_assert(N>0);
    std::uint64_t result=0;
    for (const auto amount:counters) {
        if (amount>std::numeric_limits<std::uint64_t>::max()-result)
            throw std::invalid_argument("Food history sum overflows");
        result+=amount;
    }
    return result;
}

// Select physical portions in catalog order, without mutating the source.
// Callers alone decide whether this authored ordering is their consumption rule.
template<std::size_t N>
[[nodiscard]] std::array<std::int32_t,N> prefix_portions(
        const std::array<std::int32_t,N>& source,std::uint64_t count) {
    if (count>static_cast<std::uint64_t>(volume(source)))
        throw std::invalid_argument("Food load exceeds its actual source portions");
    std::array<std::int32_t,N> result{};
    for (std::size_t i=0;i<N;++i) {
        const auto take=std::min(count,static_cast<std::uint64_t>(source[i]));
        result[i]=static_cast<std::int32_t>(take);
        count-=take;
    }
    return result;
}

// Authored mixed-cart selection, not reconstructed Original diversity logic:
// a load of at least two first takes one of each available kind in catalog
// order, as room permits; remaining room uses the same canonical prefix.
// For the frozen Wheat/Millet catalog this is the existing exact cart choice.
template<std::size_t N>
[[nodiscard]] std::array<std::int32_t,N> mixed_portions(
        const std::array<std::int32_t,N>& source,std::uint64_t count) {
    if (count>static_cast<std::uint64_t>(volume(source)))
        throw std::invalid_argument("Food load exceeds its actual source portions");
    std::array<std::int32_t,N> result{};
    if (count>=2) {
        for (std::size_t i=0;i<N && count;++i) if (source[i]) {
            result[i]=1;
            --count;
        }
    }
    auto available=source;
    for (std::size_t i=0;i<N;++i) available[i]-=result[i];
    const auto extra=prefix_portions(available,count);
    for (std::size_t i=0;i<N;++i) result[i]+=extra[i];
    return result;
}

// Caller-supplied scores have no implicit crop/resource binding. This authored
// batch score is distinct from the evidenced Original House-quality blend.
template<std::size_t N>
[[nodiscard]] std::uint8_t weighted_quality(
        const std::array<std::int32_t,N>& amounts,
        const std::array<std::uint8_t,N>& scores) {
    const auto count=volume(amounts);
    if (!count) return 0;
    std::uint64_t weighted=0;
    for (std::size_t i=0;i<N;++i)
        weighted+=static_cast<std::uint64_t>(amounts[i])*scores[i];
    // volume<=INT32_MAX and scores<=255 bound the complete numerator to uint64.
    return static_cast<std::uint8_t>(weighted/static_cast<std::uint64_t>(count));
}

} // namespace openemperor::simulation::food_batch
