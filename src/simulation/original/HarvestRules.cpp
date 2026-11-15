#include "simulation/original/HarvestRules.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace openemperor::original {
namespace {

constexpr std::array rules{
    HarvestRule{ModelBuilding::FishingQuay, HarvestResource::Fish, 179, 49, 19,
                2, 100, 100, std::nullopt, HarvestReceipt::Assign},
    HarvestRule{ModelBuilding::HuntersTent, HarvestResource::Meat, 183, 40, 19,
                2, 50, 100, 400, HarvestReceipt::Add}
};

struct StaffingTier {
    std::uint8_t minimum_percent;
    std::uint8_t hunter_attempt_threshold;
    std::uint8_t hunter_limit;
    std::uint8_t boat_work_multiplier;
};

constexpr std::array staffing_tiers{
    StaffingTier{100, 1, 3, 1},
    StaffingTier{75, 4, 2, 2},
    StaffingTier{50, 16, 1, 3},
    StaffingTier{25, 32, 1, 4},
    StaffingTier{1, 64, 0, 5}
};
constexpr StaffingTier unstaffed{};

const StaffingTier& staffing_tier(std::uint8_t percent) {
    if (percent > 100) throw std::out_of_range("harvest staffing percentage is outside0..100");
    for (const auto& tier : staffing_tiers)
        if (percent >= tier.minimum_percent) return tier;
    return unstaffed;
}

void validate_stock(const HarvestRule& rule, std::uint16_t stock) {
    if (stock > static_cast<std::uint16_t>(std::numeric_limits<std::int16_t>::max()))
        throw std::out_of_range("harvest stock is outside the nonnegative signed-WORD domain");
    if (rule.raw_source_capacity && stock > *rule.raw_source_capacity)
        throw std::out_of_range("harvest stock exceeds its proved source capacity");
}

std::int16_t increment_counter(std::int16_t counter) noexcept {
    if (counter == std::numeric_limits<std::int16_t>::max())
        return std::numeric_limits<std::int16_t>::min();
    return static_cast<std::int16_t>(counter + 1);
}

} // namespace

const HarvestRule& harvest_rule(HarvestProducer producer) {
    for (const auto& rule : rules)
        if (static_cast<std::uint16_t>(rule.model) == static_cast<std::uint16_t>(producer))
            return rule;
    throw std::out_of_range("unsupported Original harvest producer");
}

std::string_view harvest_resource_name(HarvestResource resource) {
    switch (resource) {
    case HarvestResource::Fish: return "Fish";
    case HarvestResource::Meat: return "Meat";
    }
    throw std::out_of_range("unsupported Original harvest resource");
}

bool huntable_actor(std::uint8_t actor) noexcept {
    return actor >= 75 && actor <= 77;
}

PreyActor initial_prey_actor(std::uint16_t source_selector) noexcept {
    switch (source_selector) {
    case 0: return PreyActor::Antelope;
    case 1: return PreyActor::Pheasant;
    default: return PreyActor::WildPig;
    }
}

std::string_view prey_actor_name(PreyActor actor) {
    switch (actor) {
    case PreyActor::WildPig: return "FIG_WILD_PIG";
    case PreyActor::Pheasant: return "FIG_PHEASANT";
    case PreyActor::Antelope: return "FIG_ANTELOPE";
    }
    throw std::out_of_range("unsupported Original prey actor");
}

HunterDispatchRule hunter_dispatch_rule(
    std::uint8_t staff_percent, std::uint16_t raw_stock) {
    const auto& tier = staffing_tier(staff_percent);
    const auto& rule = harvest_rule(HarvestProducer::HuntersTent);
    validate_stock(rule, raw_stock);
    const auto headroom = static_cast<std::uint16_t>((*rule.raw_source_capacity - raw_stock) / 100);
    const auto limit = std::min(static_cast<std::uint16_t>(tier.hunter_limit), headroom);
    return {staff_percent > 0, tier.hunter_attempt_threshold, static_cast<std::uint8_t>(limit)};
}

HunterAttemptStep advance_hunter_attempt(
    std::uint8_t counter, std::uint8_t staff_percent, std::uint16_t raw_stock) {
    const auto rule = hunter_dispatch_rule(staff_percent, raw_stock);
    if (!rule.staffed) return {counter, false};
    const auto next = static_cast<std::uint8_t>(static_cast<unsigned>(counter) + 1U);
    const bool due = next > rule.attempt_threshold;
    return {due ? std::uint8_t{0} : next, due};
}

std::optional<std::uint16_t> boat_work_threshold(
    BoatWorkStage stage, std::uint8_t staff_percent) {
    if (stage != BoatWorkStage::Fishing && stage != BoatWorkStage::Dock)
        throw std::out_of_range("unsupported Original boat work stage");
    const auto& tier = staffing_tier(staff_percent);
    if (!tier.boat_work_multiplier) return std::nullopt;
    const auto base = stage == BoatWorkStage::Fishing ? 408U : 204U;
    return static_cast<std::uint16_t>(base * tier.boat_work_multiplier);
}

BoatCounterStep advance_boat_work_counter(
    BoatWorkStage stage, std::int16_t counter, std::uint8_t staff_percent) {
    const auto threshold = boat_work_threshold(stage, staff_percent);
    if (!threshold) return {counter, false};
    const auto next = increment_counter(counter);
    const auto compared = stage == BoatWorkStage::Fishing ? next : counter;
    const bool due = compared >= static_cast<std::int16_t>(*threshold);
    return {due ? std::int16_t{0} : next, due};
}

BoatCounterStep advance_boat_search_counter(std::int16_t counter) noexcept {
    const auto next = increment_counter(counter);
    const bool due = next >= 51;
    return {due ? std::int16_t{0} : next, due};
}

HarvestStockStep harvest_source_return(
    HarvestProducer producer, std::uint16_t raw_stock) {
    const auto& rule = harvest_rule(producer);
    validate_stock(rule, raw_stock);
    if (rule.receipt == HarvestReceipt::Assign)
        return {rule.raw_return_amount, raw_stock};
    if (raw_stock > *rule.raw_source_capacity - rule.raw_return_amount)
        throw std::overflow_error("Hunter return would exceed the proved source capacity");
    return {static_cast<std::uint16_t>(raw_stock + rule.raw_return_amount), 0};
}

HarvestCartStep ordinary_harvest_cart_debit(
    HarvestProducer producer, std::uint16_t raw_stock, std::uint8_t external_effect_byte) {
    const auto& rule = harvest_rule(producer);
    validate_stock(rule, raw_stock);
    if (external_effect_byte)
        throw std::invalid_argument("nonzero harvest effect has an unproved cargo/accounting contract");
    if (raw_stock < rule.ordinary_raw_cart_amount)
        throw std::out_of_range("ordinary harvest cart requires100 actual raw source units");
    return {static_cast<std::uint16_t>(raw_stock - rule.ordinary_raw_cart_amount),
            rule.ordinary_raw_cart_amount};
}

} // namespace openemperor::original
