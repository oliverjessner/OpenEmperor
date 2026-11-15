#include "simulation/original/ModelData.h"
#include "simulation/original/FishingRules.h"
#include "simulation/original/HarvestRules.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace original = openemperor::original;
using Json = nlohmann::json;
using Cell = openemperor::simulation::Cell;

Json cells_json(std::span<const Cell> cells) {
    auto result = Json::array();
    for (const auto cell : cells) result.push_back({cell.x, cell.y});
    return result;
}

Json harvest_source_json(const original::ModelData& model, std::uint16_t id) {
    const auto producer = static_cast<original::HarvestProducer>(id);
    const auto& rule = original::harvest_rule(producer);
    const original::ModelBuildingId model_id{id};
    const std::array<Cell, 4> body{{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
    auto result = Json{
        {"model_id", id}, {"label", model.building_label(model_id)},
        {"raw_cost", model.building(model_id, original::BuildingField::Cost)},
        {"raw_employees", model.building(model_id, original::BuildingField::Employees)},
        {"raw_resource", model.building(model_id, original::BuildingField::Resource)},
        {"output_resource_id", static_cast<unsigned>(rule.resource)},
        {"output_resource_name", original::harvest_resource_name(rule.resource)},
        {"work_actor_id", rule.worker_actor}, {"delivery_actor_id", rule.cart_actor},
        {"source_point_model_id", rule.source_point_model_id},
        {"geometry", {{"body_cells", cells_json(body)}}},
        {"raw_return_amount", rule.raw_return_amount},
        {"raw_receipt_operation", rule.receipt == original::HarvestReceipt::Assign ? "assign" : "add"},
        {"ordinary_raw_cart_amount", rule.ordinary_raw_cart_amount},
        {"raw_source_capacity", rule.raw_source_capacity ? Json(*rule.raw_source_capacity) : Json(nullptr)},
        {"external_effect_supported", false},
        {"gameplay_enabled", false}, {"food_conversion_verified", false},
        {"clock_mapping_verified", false},
        {"rule_evidence", "EXE_OBSERVED"},
        {"observed_exe_sha256", "6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e"}
    };
    constexpr std::array<std::uint8_t, 5> staffing{100, 75, 50, 25, 1};
    if (producer == original::HarvestProducer::FishingQuay) {
        auto orientations = Json::array();
        for (std::uint8_t direction = 0; direction < 4; ++direction) {
            const auto shape = original::fishing_quay_geometry({0, 0}, direction);
            orientations.push_back({{"orientation", direction},
                {"occupied_cells", cells_json(shape.occupied)},
                {"intake_water_cells", cells_json(shape.water)},
                {"land_cells", cells_json(shape.land)},
                {"dock_cell", {shape.dock.x, shape.dock.y}}});
        }
        result["geometry"]["orientations"] = std::move(orientations);
        result["geometry"]["requires_resolved_fishing_origin_connection"] = true;
        auto navigation = Json::array();
        for (const std::uint8_t value : {std::uint8_t{0}, std::uint8_t{0xfe}, std::uint8_t{0xff}}) {
            const auto penalty = original::boat_distance_extra_penalty(value);
            navigation.push_back({{"navigation_byte", value},
                {"origin_flood_allowed", original::fishing_origin_flood_byte_allowed(value)},
                {"distance_preparation_extra", penalty ? Json(*penalty) : Json(nullptr)}});
        }
        result["navigation_byte_examples"] = std::move(navigation);
        auto tiers = Json::array();
        for (const auto percent : staffing) {
            tiers.push_back({{"minimum_percent", percent},
                {"fishing_comparison", *original::boat_work_threshold(original::BoatWorkStage::Fishing, percent)},
                {"dock_comparison", *original::boat_work_threshold(original::BoatWorkStage::Dock, percent)}});
        }
        result["work_tiers"] = std::move(tiers);
        result["blockers"] = {"resolved navigation-plane classification and rebuild ordering",
            "actor callback frequency and physical movement timing", "complete interruption and stock-assignment accounting",
            "fishing availability and regeneration", "raw-to-Food conversion and FoodQuality",
            "physical cart target acceptance and return", "nonzero external-effect cargo accounting"};
    } else {
        auto tiers = Json::array();
        for (const auto percent : staffing) {
            const auto tier = original::hunter_dispatch_rule(percent, 0);
            tiers.push_back({{"minimum_percent", percent}, {"attempt_threshold", tier.attempt_threshold},
                {"active_hunter_cap", tier.hunter_limit}});
        }
        result["staffing_tiers"] = std::move(tiers);
        result["huntable_actor_ids"] = {75, 76, 77};
        auto selectors = Json::array();
        for (const std::uint16_t selector : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{2}}) {
            const auto actor = original::initial_prey_actor(selector);
            selectors.push_back({{"raw_selector", selector}, {"actor_id", static_cast<unsigned>(actor)},
                {"actor_identifier", original::prey_actor_name(actor)}});
        }
        result["prey_initialization"] = {
            {"attempts_per_eligible_point", original::initial_prey_attempts_per_eligible_point},
            {"raw_selector_examples", std::move(selectors)},
            {"prerequisites", {"positive source-point coordinates", "zero source initialization bypass flag",
                "successful source-group allocation", "admitted spawn position and actual successful actor creation"}}
        };
        result["blockers"] = {"complete paid construction admission",
            "prey initialization, species inputs and regeneration", "actor callback frequency and physical pursuit timing",
            "concurrent assignment and interruption", "raw-to-Food conversion and FoodQuality",
            "physical cart target acceptance and return", "nonzero external-effect cargo accounting"};
    }
    return result;
}
} // namespace

int main(int argc, char** argv) {
    namespace original = openemperor::original;
    const auto usage = [] {
        std::cerr << "Usage: openemperor-model-inspect GAME_DIRECTORY [--building ID]... [--harvest-source 31|33]...\n";
        return 2;
    };
    if (argc < 2 || (argc - 2) % 2 != 0) return usage();
    std::vector<std::uint16_t> requested;
    std::vector<std::uint16_t> harvest_requested;
    for (int index = 2; index < argc; index += 2) {
        const std::string_view option(argv[index]);
        if (option != "--building" && option != "--harvest-source") return usage();
        const std::string_view text(argv[index + 1]);
        unsigned id = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
        if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            id > original::max_building_id) return usage();
        const auto bounded = static_cast<std::uint16_t>(id);
        if (option == "--harvest-source" && id != 31 && id != 33) return usage();
        auto& queries = option == "--building" ? requested : harvest_requested;
        if (std::find(queries.begin(), queries.end(), bounded) != queries.end()) return usage();
        queries.push_back(bounded);
    }
    const auto result = original::load_model_data(argv[1]);
    if (!result) {
        const auto& error = *result.error;
        std::cerr << nlohmann::json{{"status", "error"}, {"line", error.line},
            {"section", error.section}, {"reason", error.reason}}.dump(2) << '\n';
        return 1;
    }
    const auto& model = *result.model;
    const auto& p = model.provenance();
    auto report = nlohmann::json{
        {"status", "validated"}, {"evidence", "RAW MODEL FACT"},
        {"reader_format_version", p.format_version}, {"source_kind", "file"},
        {"source_path", p.source_path}, {"sha256", p.sha256},
        {"byte_count", p.byte_count}, {"building_rows", p.building_count},
        {"house_rows", p.house_count},
        {"building_modifier_rows", p.building_modifier_count},
        {"house_modifier_rows", p.house_modifier_count},
        {"building_fields", original::building_field_count},
        {"house_fields", original::house_field_count}
    };
    if (!requested.empty()) {
        auto rows = nlohmann::json::array();
        for (const auto id : requested) {
            if (!model.has_building({id})) {
                std::cerr << nlohmann::json{{"status", "error"}, {"line", 0},
                    {"section", "query"}, {"reason", "requested Building ID is absent"},
                    {"model_id", id}}.dump(2) << '\n';
                return 1;
            }
            rows.push_back({{"model_id", id}, {"label", model.building_label({id})},
                {"raw_cost", model.building({id}, original::BuildingField::Cost)},
                {"raw_employees", model.building({id}, original::BuildingField::Employees)},
                {"raw_resource", model.building({id}, original::BuildingField::Resource)}});
        }
        report["building_observation_contract"] = "raw-building-columns-v1";
        report["raw_buildings"] = std::move(rows);
        report["unresolved_runtime_semantics"] = {"World admission", "footprint", "field/plant ownership",
            "final purchase price", "recipe and commodity identity", "quantity and capacity",
            "production/calendar timing", "routes and demand"};
    }
    if (!harvest_requested.empty()) {
        auto sources = nlohmann::json::array();
        for (const auto id : harvest_requested) {
            if (!model.has_building({id})) {
                std::cerr << nlohmann::json{{"status", "error"}, {"line", 0},
                    {"section", "query"}, {"reason", "requested Building ID is absent"},
                    {"model_id", id}}.dump(2) << '\n';
                return 1;
            }
            sources.push_back(harvest_source_json(model, id));
        }
        report["harvest_source_observation_contract"] = "harvest-source-rules-v1";
        report["harvest_sources"] = std::move(sources);
    }
    // The raw reader permits non-UTF-8 bytes in labels. Refuse an unrepresentable
    // query before publishing any JSON rather than replacing source bytes.
    try {
        const auto serialized = report.dump(2);
        std::cout << serialized << '\n';
    } catch (const nlohmann::json::exception&) {
        std::cerr << "{\"status\":\"error\",\"line\":0,\"section\":\"query\","
                     "\"reason\":\"model observations cannot be represented as UTF-8 JSON\"}\n";
        return 1;
    }
    return 0;
}
