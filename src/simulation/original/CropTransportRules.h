#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace openemperor::original {

// Pinned Farm constructor4c43b0 initializes each bin capacity to400 raw units,
// with resource0 and stock0 across five bins. Native bin fields are DWORDs.
// Later capacity writers are not closed. The actual resolved capacity remains
// an explicit input; this default supplies no World inventory authority.
inline constexpr std::uint16_t farm_initial_raw_bin_capacity=400;
inline constexpr std::uint16_t farm_raw_cart_amount=400;
inline constexpr std::size_t farm_raw_bin_count=5;

struct RawFarmBin {
    std::uint8_t resource=0;
    std::uint16_t quantity=0;
    std::uint16_t capacity=farm_initial_raw_bin_capacity;
    bool operator==(const RawFarmBin&) const = default;
};

// Supported Food resources come from CropRules195..199. All quantities and
// capacities use the API's supported subset0..32767, quantity<=capacity.
// This word-safe domain does not relabel the native DWORD bin fields as WORDs.
// Resource0 requires quantity0; a retained nonzero identity may have quantity0.
// Original helper4b04f0 considers a bin empty only when BOTH fields are zero.
// The selector4c5090 returns the first matching resource before any empty bin.
[[nodiscard]] std::optional<std::size_t> select_farm_raw_bin(
    const std::array<RawFarmBin,farm_raw_bin_count>& bins,std::uint8_t resource);

struct RawFarmBinCredit {
    RawFarmBin bin;
    std::uint16_t accepted=0;
    std::uint16_t overflow=0;
    bool operator==(const RawFarmBinCredit&) const = default;
};

// One caller-resolved compatible credit. Original5d2790 clamps and returns
// overflow; Farm4c4f46 does not inspect that return before checking the bin.
// This pure operation exposes overflow and does not decide its disposition.
// A mismatched retained identity is refused rather than silently relabeled.
[[nodiscard]] RawFarmBinCredit credit_farm_raw_bin(
    RawFarmBin bin,std::uint8_t resource,std::uint16_t offered);

struct RawFarmBinDebit {
    RawFarmBin bin;
    std::uint16_t removed=0;
    bool operator==(const RawFarmBinDebit&) const = default;
};

// Original5d2760 floors an insufficient debit at0 and optionally clears the
// identity of the resulting empty bin. Callers supply their actual completed
// handoff; this arithmetic grants neither cart creation nor cargo delivery.
[[nodiscard]] RawFarmBinDebit debit_farm_raw_bin(
    RawFarmBin bin,std::uint16_t requested,bool clear_empty_identity);

enum class RawCartReceiptBranch : std::uint8_t { ReturnToSource, ReselectDestination };

struct RawCartReceipt {
    std::uint16_t accepted=0;
    std::uint16_t residual=0;
    RawCartReceiptBranch branch=RawCartReceiptBranch::ReturnToSource;
    bool operator==(const RawCartReceipt&) const = default;
};

// Actor19 helper4e2a20 receives an ACTUAL target accepted count, not overflow.
// For0<=accepted<=cargo<=32767, full acceptance clears cargo and chooses its
// source-return branch; otherwise only accepted cargo is subtracted before
// destination reselection. No target acceptance, route, movement, retirement,
// reservation or raw-to-World-Food conversion is established by this result.
[[nodiscard]] RawCartReceipt accept_farm_raw_cart_cargo(
    std::uint16_t cargo,std::uint16_t accepted);

} // namespace openemperor::original
