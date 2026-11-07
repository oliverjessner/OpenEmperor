#include "simulation/original/ModelSchema.h"
#include "simulation/original/OriginalModelMapping.h"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace original = openemperor::original;
namespace fs = std::filesystem;
using original::BuildingField;
using original::HouseField;
using original::ModelBuilding;
using original::ModelHouse;
using original::Difficulty;

std::size_t checks = 0;
void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

template<typename Error, typename Operation>
void expect_exception(Operation operation, std::string_view message) {
    try { operation(); }
    catch (const Error&) { ++checks; return; }
    throw std::runtime_error(std::string(message));
}

constexpr std::array building_fields{
    BuildingField::Cost, BuildingField::Desirability, BuildingField::DesirabilityStep,
    BuildingField::DesirabilityStepSize, BuildingField::DesirabilityRange,
    BuildingField::Employees, BuildingField::FireRisk, BuildingField::DamageRisk,
    BuildingField::Resource, BuildingField::RiskReducer, BuildingField::EvolveDesirability,
    BuildingField::Integrity, BuildingField::FengShui
};
constexpr std::array house_fields{
    HouseField::DesirabilityLow, HouseField::DesirabilityHigh, HouseField::Herbalist,
    HouseField::Water, HouseField::Acupuncture, HouseField::Music, HouseField::Acrobat,
    HouseField::Drama, HouseField::FoodQuality, HouseField::Hemp, HouseField::Ceramics,
    HouseField::Tea, HouseField::Silk, HouseField::BronzewareOrLacquerware,
    HouseField::CrimeIncrement, HouseField::CrimeBase, HouseField::Unused16,
    HouseField::Capacity, HouseField::TaxRate, HouseField::Unused19,
    HouseField::DiseaseIncrement, HouseField::Ancestor, HouseField::Confucian,
    HouseField::DaoistOrBuddhist
};
constexpr std::array difficulties{
    Difficulty::VeryEasy, Difficulty::Easy, Difficulty::Normal, Difficulty::Hard,
    Difficulty::VeryHard
};
constexpr std::array<std::string_view, 5> difficulty_labels{
    "Very Easy Modifiers", "Easy Modifiers", "Normal Modifier", "Hard Modifiers",
    "Very Hard Modifiers"
};

struct BuildingIdentity { ModelBuilding model; std::uint16_t id; std::string_view label; };
constexpr std::array<BuildingIdentity, 7> buildings{{
    {ModelBuilding::ClayPit, 35, "Clay Pit"},
    {ModelBuilding::Kiln, 43, "Kiln"},
    {ModelBuilding::Warehouse, 54, "Warehouse"},
    {ModelBuilding::CommonMarketSquare, 59, "Common Market Square"},
    {ModelBuilding::Well, 72, "Well"},
    {ModelBuilding::Farmhouse, 193, "Farmhouse"},
    {ModelBuilding::HerbalistsStall, 207, "Herbalist's Stall"}
}};
struct HouseIdentity { ModelHouse model; std::uint8_t row; std::string_view label; };
constexpr std::array<HouseIdentity, 15> houses{{
    {ModelHouse::Shelter, 0, "1: Shelter"}, {ModelHouse::Hut, 1, "2: Hut"},
    {ModelHouse::PlainCottage, 2, "3: Plain Cottage"},
    {ModelHouse::AttractiveCottage, 3, "4: Attractive Cottage"},
    {ModelHouse::SpaciousDwelling, 4, "5: Spacious Dwelling"},
    {ModelHouse::ElegantDwelling, 5, "6: Elegant Dwelling"},
    {ModelHouse::OrnateApartment, 6, "7: Ornate Apartment"},
    {ModelHouse::LuxuriousApartment, 7, "8: Lux. Apartment"},
    {ModelHouse::EliteUnoccupied, 8, "Elite: Unoccupied"},
    {ModelHouse::EliteAbandoned, 9, "Elite 0: Abandoned"},
    {ModelHouse::ModestSiheyuan, 10, "Elite 1: Modest Siheyuan"},
    {ModelHouse::LavishSiheyuan, 11, "Elite 2: Lavish Siheyuan"},
    {ModelHouse::HumbleCompound, 12, "Elite 3: Humble Compound"},
    {ModelHouse::ImpressiveCompound, 13, "Elite 4: Impressive Compound"},
    {ModelHouse::HeavenlyCompound, 14, "Elite 5: Heavenly Compound"}
}};
static_assert(building_fields.size() == 13 && house_fields.size() == 24 && houses.size() == 15);

template<std::size_t N>
std::string row(std::string_view label, const std::array<std::int32_t, N>& values) {
    std::string result(label);
    result += ",{,";
    for (auto value : values) result += std::to_string(value) + ',';
    return result + "},,,\n";
}

// Authored numbers exercise field order and arithmetic; no original numeric table.
struct Fixture {
    std::array<original::BuildingRow, 7> building_values{};
    std::array<original::HouseRow, 15> house_values{};
    std::array<original::BuildingRow, 5> building_modifiers{};
    std::array<original::HouseRow, 5> house_modifiers{};
    std::array<std::string, 7> building_labels{};
    std::array<std::string, 15> house_labels{};

    Fixture() {
        for (std::size_t b = 0; b < buildings.size(); ++b) {
            building_labels[b] = buildings[b].label;
            for (std::size_t f = 0; f < building_fields.size(); ++f) {
                auto value = static_cast<std::int32_t>(1031 + b * 79 + f * 11);
                building_values[b][f] = f % 2 == 0 ? value : -value;
            }
        }
        for (std::size_t h = 0; h < houses.size(); ++h) {
            house_labels[h] = houses[h].label;
            for (std::size_t f = 0; f < house_fields.size(); ++f)
                house_values[h][f] = static_cast<std::int32_t>(h * 3 + f) - 18;
        }
        constexpr std::array<std::int32_t, 5> percentages{23, 61, 103, 147, 209};
        for (std::size_t d = 0; d < difficulties.size(); ++d) {
            for (std::size_t f = 0; f < building_fields.size(); ++f)
                building_modifiers[d][f] = percentages[d] + static_cast<std::int32_t>(f * 5);
            for (std::size_t f = 0; f < house_fields.size(); ++f)
                house_modifiers[d][f] = static_cast<std::int32_t>(d * 9 + f % 5) - 17;
        }
    }

    std::string text() const {
        std::string result = "Authored schema fixture\nBUILDING MODS\n";
        for (std::size_t d = 0; d < difficulties.size(); ++d)
            result += row(std::to_string(d) + ',' + std::string(difficulty_labels[d]), building_modifiers[d]);
        result += "ALL BUILDINGS\n";
        for (std::size_t b = 0; b < buildings.size(); ++b)
            result += row(std::to_string(buildings[b].id) + ',' + building_labels[b], building_values[b]);
        original::BuildingRow boundary{};
        boundary[0] = -991;
        result += row("4096,Authored boundary", boundary);
        result += "HOUSE MODS\n";
        for (std::size_t d = 0; d < difficulties.size(); ++d)
            result += row(difficulty_labels[d], house_modifiers[d]);
        result += "ALL HOUSES\n";
        for (std::size_t h = 0; h < houses.size(); ++h)
            result += row(house_labels[h], house_values[h]);
        return result + "End of model data.\n-------------------\n";
    }
};

original::ModelData parse(const Fixture& fixture) {
    auto parsed = original::parse_model_data(fixture.text());
    check(static_cast<bool>(parsed), parsed.error ? parsed.error->reason : "synthetic parse");
    return std::move(*parsed.model);
}

void field_and_identity_checks() {
    check(original::model_schema_version == 1, "versioned model schema");
    for (std::size_t f = 0; f < building_fields.size(); ++f)
        check(static_cast<std::size_t>(building_fields[f]) == f, "documented building field order");
    for (std::size_t f = 0; f < house_fields.size(); ++f)
        check(static_cast<std::size_t>(house_fields[f]) == f, "documented house field order");
    check(static_cast<std::size_t>(HouseField::BronzewareOrLacquerware) == 13,
          "one either-luxury column, not two goods columns");
    check(static_cast<std::size_t>(HouseField::Unused16) == 16 &&
          static_cast<std::size_t>(HouseField::Unused19) == 19, "unused columns retain positions");
    for (const auto& b : buildings) {
        check(original::model_id(b.model).value == b.id, "literal source building ID");
        check(original::model_label(b.model) == b.label, "literal source building label");
    }
    for (const auto& h : houses) {
        check(original::model_row(h.model).value == h.row, "literal source house row");
        check(original::model_label(h.model) == h.label, "literal source house label");
        const auto id = static_cast<std::uint16_t>(h.row + 3);
        check(original::house_building_type(h.model).value == id, "house building type is row plus three");
        check(original::house_model_for_building({id}) == h.model, "house type to actual row");
    }
    for (const auto id : std::array<std::uint16_t, 6>{0, 1, 2, 18, 35, 4096})
        check(!original::house_model_for_building({id}), "unmapped type has no invented house row");
    for (const auto id : std::array<std::uint16_t, 2>{4097, 65535})
        expect_exception<std::out_of_range>([&] { (void)original::house_model_for_building({id}); }, "invalid raw building ID");
    for (const auto bad : {static_cast<ModelBuilding>(0), static_cast<ModelBuilding>(36), static_cast<ModelBuilding>(65535)}) {
        expect_exception<std::out_of_range>([&] { (void)original::model_id(bad); }, "invalid named building ID");
        expect_exception<std::out_of_range>([&] { (void)original::model_label(bad); }, "invalid named building label");
    }
    for (const auto bad : {static_cast<ModelHouse>(15), static_cast<ModelHouse>(255)}) {
        expect_exception<std::out_of_range>([&] { (void)original::model_row(bad); }, "invalid named house row");
        expect_exception<std::out_of_range>([&] { (void)original::model_label(bad); }, "invalid named house label");
        expect_exception<std::out_of_range>([&] { (void)original::house_building_type(bad); }, "invalid house building type");
    }
}

void values_and_failures() {
    const Fixture fixture;
    const auto model = parse(fixture);
    for (std::size_t b = 0; b < buildings.size(); ++b) {
        for (std::size_t f = 0; f < building_fields.size(); ++f) {
            check(original::raw_building_value(model, buildings[b].model, building_fields[f]) == fixture.building_values[b][f], "named raw building value");
            for (std::size_t d = 0; d < difficulties.size(); ++d) {
                const auto expected = static_cast<std::int64_t>(fixture.building_values[b][f]) * fixture.building_modifiers[d][f] / 100;
                check(original::building_value(model, buildings[b].model, building_fields[f], difficulties[d]) == expected,
                      "signed building transform across every field and difficulty");
            }
        }
    }
    for (std::size_t h = 0; h < houses.size(); ++h) {
        for (std::size_t f = 0; f < house_fields.size(); ++f) {
            check(original::raw_house_value(model, houses[h].model, house_fields[f]) == fixture.house_values[h][f], "named raw house value");
            for (std::size_t d = 0; d < difficulties.size(); ++d) {
                const auto expected = std::clamp(static_cast<std::int64_t>(fixture.house_values[h][f]) + fixture.house_modifiers[d][f], std::int64_t{-99}, std::int64_t{100});
                check(original::house_value(model, houses[h].model, house_fields[f], difficulties[d]) == expected,
                      "house transform across every field and difficulty");
            }
        }
    }
    check(model.building({4096}, BuildingField::Cost) == -991, "generic parser retains nonsemantic source ID");
    for (const auto bad : {static_cast<ModelBuilding>(36), static_cast<ModelBuilding>(65535)}) {
        expect_exception<std::out_of_range>([&] { (void)original::raw_building_value(model, bad, BuildingField::Cost); }, "invalid named building raw access");
        expect_exception<std::out_of_range>([&] { (void)original::building_value(model, bad, BuildingField::Cost, Difficulty::Normal); }, "invalid named building adjusted access");
    }
    for (const auto bad : {static_cast<ModelHouse>(15), static_cast<ModelHouse>(255)}) {
        expect_exception<std::out_of_range>([&] { (void)original::raw_house_value(model, bad, HouseField::Capacity); }, "invalid named house raw access");
        expect_exception<std::out_of_range>([&] { (void)original::house_value(model, bad, HouseField::Capacity, Difficulty::Normal); }, "invalid named house adjusted access");
    }
    for (const auto bad : {static_cast<Difficulty>(5), static_cast<Difficulty>(-1)}) {
        expect_exception<std::out_of_range>([&] { (void)original::building_value(model, ModelBuilding::ClayPit, BuildingField::Cost, bad); }, "invalid building difficulty");
        expect_exception<std::out_of_range>([&] { (void)original::house_value(model, ModelHouse::Shelter, HouseField::Capacity, bad); }, "invalid house difficulty");
    }
    for (const auto bad : {static_cast<BuildingField>(13), static_cast<BuildingField>(-1)}) {
        expect_exception<std::out_of_range>([&] { (void)original::raw_building_value(model, ModelBuilding::ClayPit, bad); }, "invalid building field");
        expect_exception<std::out_of_range>([&] { (void)original::building_value(model, ModelBuilding::ClayPit, bad, Difficulty::Normal); }, "invalid transformed building field");
    }
    for (const auto bad : {static_cast<HouseField>(24), static_cast<HouseField>(-1)}) {
        expect_exception<std::out_of_range>([&] { (void)original::raw_house_value(model, ModelHouse::Shelter, bad); }, "invalid house field");
        expect_exception<std::out_of_range>([&] { (void)original::house_value(model, ModelHouse::Shelter, bad, Difficulty::Normal); }, "invalid transformed house field");
    }
    auto changed = fixture;
    changed.building_labels[0] = "Different source identity";
    auto wrong_building = parse(changed);
    check(wrong_building.building({35}, BuildingField::Cost) == fixture.building_values[0][0], "raw parser preserves modified building data");
    expect_exception<std::invalid_argument>([&] { (void)original::raw_building_value(wrong_building, ModelBuilding::ClayPit, BuildingField::Cost); }, "named API rejects building label mismatch");
    expect_exception<std::invalid_argument>([&] { (void)original::building_value(wrong_building, ModelBuilding::ClayPit, BuildingField::Cost, Difficulty::Normal); }, "transformed API rejects building label mismatch");
    changed = fixture;
    std::swap(changed.house_labels[0], changed.house_labels[1]);
    const auto wrong_house = parse(changed);
    check(wrong_house.house({0}, HouseField::Capacity) == fixture.house_values[0][17], "raw parser preserves reordered house labels");
    expect_exception<std::invalid_argument>([&] { (void)original::raw_house_value(wrong_house, ModelHouse::Shelter, HouseField::Capacity); }, "named API rejects reordered house identity");
    expect_exception<std::invalid_argument>([&] { (void)original::house_value(wrong_house, ModelHouse::Hut, HouseField::Capacity, Difficulty::Normal); }, "transformed API rejects reordered house identity");
}

void arithmetic_boundaries() {
    auto fixture = Fixture{};
    fixture.building_values[0][0] = -7;
    fixture.building_modifiers[0][0] = 13;
    fixture.house_values[0][0] = 97;
    fixture.house_modifiers[0][0] = 17;
    fixture.house_values[0][1] = -97;
    fixture.house_modifiers[0][1] = -17;
    auto model = parse(fixture);
    check(original::building_value(model, ModelBuilding::ClayPit, BuildingField::Cost, Difficulty::VeryEasy) == 0, "negative fraction truncates toward zero");
    check(original::house_value(model, ModelHouse::Shelter, HouseField::DesirabilityLow, Difficulty::VeryEasy) == 100, "in-domain positive sum clamps");
    check(original::house_value(model, ModelHouse::Shelter, HouseField::DesirabilityHigh, Difficulty::VeryEasy) == -99, "in-domain negative sum clamps");
    fixture.building_values[0][0] = std::numeric_limits<std::int32_t>::max();
    fixture.building_modifiers[0][0] = 1;
    model = parse(fixture);
    check(original::building_value(model, ModelBuilding::ClayPit, BuildingField::Cost, Difficulty::VeryEasy) == 21474836, "int32 maximum product accepted before division");
    fixture.building_values[0][0] = std::numeric_limits<std::int32_t>::min();
    model = parse(fixture);
    check(original::building_value(model, ModelBuilding::ClayPit, BuildingField::Cost, Difficulty::VeryEasy) == -21474836, "int32 minimum product accepted before division");
    for (const std::int32_t value : {std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        fixture.building_values[0][0] = value;
        fixture.building_modifiers[0][0] = 100;
        model = parse(fixture);
        expect_exception<std::overflow_error>([&] { (void)original::building_value(model, ModelBuilding::ClayPit, BuildingField::Cost, Difficulty::VeryEasy); }, "original product overflows although divided result fits int32");
    }
    fixture.house_values[0][0] = std::numeric_limits<std::int32_t>::max();
    fixture.house_modifiers[0][0] = 1;
    fixture.house_values[0][1] = std::numeric_limits<std::int32_t>::min();
    fixture.house_modifiers[0][1] = -1;
    model = parse(fixture);
    expect_exception<std::overflow_error>([&] { (void)original::house_value(model, ModelHouse::Shelter, HouseField::DesirabilityLow, Difficulty::VeryEasy); }, "positive original house addition overflow is not hidden by clamp");
    expect_exception<std::overflow_error>([&] { (void)original::house_value(model, ModelHouse::Shelter, HouseField::DesirabilityHigh, Difficulty::VeryEasy); }, "negative original house addition overflow is not hidden by clamp");
    fixture.building_values[0][0] = 7;
    fixture.building_modifiers[0][0] = -13;
    model = parse(fixture);
    check(original::building_value(model, ModelBuilding::ClayPit, BuildingField::Cost, Difficulty::VeryEasy) == 0, "signed negative modifier supported without floor rounding");
}

void functional_mapping_checks() {
    using openemperor::simulation::Object;
    using Status = original::EvidenceStatus;
    struct Expected { Object object; std::optional<ModelBuilding> candidate; bool image; };
    const std::array<Expected, 13> expected{{
        {Object::Empty, {}, false}, {Object::Road, {}, false}, {Object::Workshop, {}, false},
        {Object::Warehouse, ModelBuilding::Warehouse, false}, {Object::ClaySource, ModelBuilding::ClayPit, false},
        {Object::Pottery, ModelBuilding::Kiln, false}, {Object::Household, {}, false},
        {Object::Farm, ModelBuilding::Farmhouse, false}, {Object::ServicePost, {}, false},
        {Object::Market, ModelBuilding::CommonMarketSquare, false}, {Object::FireWatch, {}, false},
        {Object::Well, ModelBuilding::Well, true}, {Object::HealthPost, ModelBuilding::HerbalistsStall, true}
    }};
    std::size_t mapped = 0;
    for (const auto& item : expected) {
        const auto result = original::functional_model_candidate(item.object);
        check(result.candidate == item.candidate, "explicit functional candidate for every legacy Object");
        check(result.functional == (item.candidate ? Status::Inferred : Status::Unknown), "candidate is a functional inference");
        check(result.mechanics_identity == Status::Unknown, "candidate does not prove original mechanics identity");
        check(result.presentation_identity == (item.image ? Status::ReverseEngineered : Status::Unknown), "image registration evidence is separate");
        if (result.candidate) ++mapped;
    }
    check(mapped == 7 && expected.size() - mapped == 6, "seven legacy candidates and six explicitly unmapped legacy Objects");
    for (const auto kind:{Object::OriginalInfluenceSource,Object::OriginalElitePlot,Object::OriginalEliteHouse}) {
        const auto source=original::functional_model_candidate(kind);
        check(!source.candidate && source.functional==Status::Unknown &&
              source.mechanics_identity==Status::Unknown && source.presentation_identity==Status::Unknown,
              "authored source/Elite geometry does not invent a Building-model or sprite identity");
    }
    const auto theatre=original::functional_model_candidate(Object::OriginalTheatre75);
    check(theatre.candidate==ModelBuilding::TheatrePavilion &&
          theatre.functional==Status::AuthoredFallback && theatre.mechanics_identity==Status::AuthoredFallback &&
          theatre.presentation_identity==Status::Unknown,
          "explicit Theatre model mapping does not claim original runtime or visual parity");
    constexpr auto first_invalid = static_cast<Object>(static_cast<unsigned>(Object::OriginalTheatre75) + 1U);
    for (const auto bad : {first_invalid, static_cast<Object>(255)})
        expect_exception<std::out_of_range>([&] { (void)original::functional_model_candidate(bad); }, "invalid Object is not merely unmapped");
}

std::string read_bytes(const fs::path& path) {
    const auto length = fs::file_size(path);
    check(length <= original::max_model_bytes, "bounded test source before allocation");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open golden source " + path.string());
    std::string result(static_cast<std::size_t>(length), '\0');
    input.read(result.data(), static_cast<std::streamsize>(length));
    check(input.gcount() == static_cast<std::streamsize>(length) && !input.bad(),
          "golden source read must match bounded size");
    char extra = 0;
    check(!input.get(extra) && input.eof() && !input.bad(), "golden source cannot grow beyond bounded read");
    check(fs::file_size(path) == length, "golden source size remains stable during read");
    return result;
}

std::string sha256(std::string_view bytes) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int size = 0;
    check(EVP_Digest(bytes.data(), bytes.size(), digest.data(), &size, EVP_sha256(), nullptr) == 1 && size == 32, "independent golden SHA-256");
    constexpr std::string_view hex = "0123456789abcdef";
    std::string result;
    for (unsigned int i = 0; i < size; ++i) {
        result += hex[digest[i] >> 4];
        result += hex[digest[i] & 15U];
    }
    return result;
}

std::string_view trim(std::string_view text) {
    const auto begin = text.find_first_not_of(" \t\r");
    if (begin == std::string_view::npos) return {};
    return text.substr(begin, text.find_last_not_of(" \t\r") - begin + 1);
}

// A point scanner independent of ModelData: exact section/row/line and token.
// It reads only the five curated points, not a second complete model table.
std::int32_t source_point(std::string_view bytes, std::string_view section,
                          std::string_view label, std::size_t field, std::size_t expected_line) {
    std::istringstream input{std::string(bytes)};
    std::string raw;
    std::string current_section;
    std::size_t line = 0;
    std::optional<std::int32_t> found;
    while (std::getline(input, raw)) {
        ++line;
        const auto text = trim(raw);
        if (text == "BUILDING MODS" || text == "ALL BUILDINGS" || text == "HOUSE MODS" || text == "ALL HOUSES") current_section = text;
        if (current_section != section || !text.starts_with(std::string(label) + ",{,")) continue;
        check(!found.has_value() && line == expected_line, "independent golden identity and source line");
        auto body = text.substr(text.find("{,") + 2);
        for (std::size_t index = 0; index < field; ++index) {
            const auto comma = body.find(',');
            check(comma != std::string_view::npos, "golden column exists");
            body.remove_prefix(comma + 1);
        }
        const auto token = trim(body.substr(0, body.find(',')));
        std::int32_t value = 0;
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
        check(parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size(), "independent golden numeric token");
        found = value;
    }
    check(found.has_value(), "independent golden source point found");
    return *found;
}

int real_golden(const fs::path& root) {
    if (root.empty()) { std::cout << "SKIP: original model root not configured\n"; return 77; }
    std::error_code error;
    const auto root_status = fs::symlink_status(root, error);
    if (root_status.type() == fs::file_type::not_found) { std::cout << "SKIP: original root missing\n"; return 77; }
    check(!error, "original root metadata must be readable");
    const auto followed_root = fs::status(root, error);
    check(!error && fs::is_directory(followed_root), "existing original root must resolve to a readable directory");
    const auto directory = root / "Model";
    const auto directory_status = fs::symlink_status(directory, error);
    if (directory_status.type() == fs::file_type::not_found) { std::cout << "SKIP: original model directory missing\n"; return 77; }
    check(!error, "original model directory metadata must be readable");
    const auto followed_directory = fs::status(directory, error);
    check(!error && fs::is_directory(followed_directory), "existing original model directory must resolve to a readable directory");
    const auto source = directory / "EmperorBuildingModels.txt";
    const auto source_status = fs::symlink_status(source, error);
    if (source_status.type() == fs::file_type::not_found) { std::cout << "SKIP: original model missing\n"; return 77; }
    check(!error && fs::is_regular_file(fs::status(source)), "existing golden model must be a regular file");
    const auto initial_size = fs::file_size(source);
    const auto initial_modified = fs::last_write_time(source);
    // The production loader enforces canonical containment before the independent
    // oracle opens any payload, including when the model path is a symlink.
    auto loaded = original::load_model_data(root);
    check(static_cast<bool>(loaded), loaded.error ? loaded.error->reason : "load original golden source");
    const auto& model = *loaded.model;
    const auto before = read_bytes(source);
    check(before.size() <= original::max_model_bytes, "bounded golden source");
    check(sha256(before) == "7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6", "golden source must match documented installation SHA-256");
    // Exactly five small, documented original values; no original table fixture.
    struct Point { std::string_view section, label; std::size_t field, line; std::int32_t expected; };
    const std::array<Point, 5> golden{{
        {"ALL BUILDINGS", "35,Clay Pit", 0, 119, 75},
        {"ALL BUILDINGS", "43,Kiln", 1, 127, -30},
        {"ALL HOUSES", "1: Shelter", 17, 364, 7},
        {"ALL HOUSES", "3: Plain Cottage", 8, 366, 20},
        {"ALL HOUSES", "Elite 5: Heavenly Compound", 13, 378, 1}
    }};
    const std::array<std::int32_t, 5> named{
        original::raw_building_value(model, ModelBuilding::ClayPit, BuildingField::Cost),
        original::raw_building_value(model, ModelBuilding::Kiln, BuildingField::Desirability),
        original::raw_house_value(model, ModelHouse::Shelter, HouseField::Capacity),
        original::raw_house_value(model, ModelHouse::PlainCottage, HouseField::FoodQuality),
        original::raw_house_value(model, ModelHouse::HeavenlyCompound, HouseField::BronzewareOrLacquerware)
    };
    const std::array<std::int32_t, 5> normal{
        original::building_value(model, ModelBuilding::ClayPit, BuildingField::Cost, Difficulty::Normal),
        original::building_value(model, ModelBuilding::Kiln, BuildingField::Desirability, Difficulty::Normal),
        original::house_value(model, ModelHouse::Shelter, HouseField::Capacity, Difficulty::Normal),
        original::house_value(model, ModelHouse::PlainCottage, HouseField::FoodQuality, Difficulty::Normal),
        original::house_value(model, ModelHouse::HeavenlyCompound, HouseField::BronzewareOrLacquerware, Difficulty::Normal)
    };
    for (std::size_t i = 0; i < golden.size(); ++i) {
        const auto& point = golden[i];
        check(source_point(before, point.section, point.label, point.field, point.line) == point.expected, "curated original golden point");
        check(named[i] == point.expected, "named API equals independent golden value");
        check(normal[i] == point.expected, "normal difficulty matches documented golden value");
    }
    std::size_t transform_queries = 0;
    for (const auto& identity : buildings) {
        for (const auto field : building_fields) {
            const auto raw = original::raw_building_value(model, identity.model, field);
            for (const auto difficulty : difficulties) {
                const auto product = static_cast<std::int64_t>(raw) * model.building_modifier(difficulty, field);
                check(product >= std::numeric_limits<std::int32_t>::min() &&
                      product <= std::numeric_limits<std::int32_t>::max(), "real building intermediate fits original domain");
                check(original::building_value(model, identity.model, field, difficulty) == product / 100,
                      "real checked building transform agrees with independent arithmetic");
                ++transform_queries;
            }
        }
    }
    for (const auto& identity : houses) {
        for (const auto field : house_fields) {
            const auto raw = original::raw_house_value(model, identity.model, field);
            for (const auto difficulty : difficulties) {
                const auto sum = static_cast<std::int64_t>(raw) + model.house_modifier(difficulty, field);
                check(sum >= std::numeric_limits<std::int32_t>::min() &&
                      sum <= std::numeric_limits<std::int32_t>::max(), "real house intermediate fits original domain");
                check(original::house_value(model, identity.model, field, difficulty) ==
                      std::clamp(sum, std::int64_t{-99}, std::int64_t{100}),
                      "real checked house transform agrees with independent arithmetic");
                ++transform_queries;
            }
        }
    }
    check(model.provenance().sha256 == sha256(before), "parser provenance binds independently hashed file");
    check(read_bytes(source) == before, "original source remains byte exact after inspection");
    check(fs::file_size(source) == initial_size && fs::last_write_time(source) == initial_modified,
          "original model metadata remains unchanged across load and inspection");
    std::cout << "golden points=5; checked transform queries=" << transform_queries
              << "; original source SHA-256=" << model.provenance().sha256 << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && (std::string_view(argv[1]) == "--real" || std::string_view(argv[1]) == "--golden")) {
            const auto result = real_golden(argv[2]);
            if (result == 77) return result;
        } else if (argc == 1) {
            field_and_identity_checks();
            values_and_failures();
            arithmetic_boundaries();
            functional_mapping_checks();
        } else {
            std::cerr << "usage: original-model-schema-tests [--real|--golden ROOT]\n";
            return 2;
        }
        std::cout << "original model schema checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "original model schema test failed: " << error.what() << '\n';
        return 1;
    }
}
