#include "simulation/original/ModelData.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

int main(int argc, char** argv) {
    namespace original = openemperor::original;
    const auto usage = [] {
        std::cerr << "Usage: openemperor-model-inspect GAME_DIRECTORY [--building ID]...\n";
        return 2;
    };
    if (argc < 2 || (argc - 2) % 2 != 0) return usage();
    std::vector<std::uint16_t> requested;
    for (int index = 2; index < argc; index += 2) {
        if (std::string_view(argv[index]) != "--building") return usage();
        const std::string_view text(argv[index + 1]);
        unsigned id = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
        if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            id > original::max_building_id) return usage();
        const auto bounded = static_cast<std::uint16_t>(id);
        if (std::find(requested.begin(), requested.end(), bounded) != requested.end()) return usage();
        requested.push_back(bounded);
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
