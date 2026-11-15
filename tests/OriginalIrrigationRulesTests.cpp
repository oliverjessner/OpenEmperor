#include "simulation/original/IrrigationRules.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

namespace original = openemperor::original;
using openemperor::simulation::Cell;

std::size_t checks = 0;

void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

template<class Error, class Operation>
void rejected(Operation operation) {
    try { operation(); }
    catch (const Error&) { ++checks; return; }
    throw std::runtime_error("invalid irrigation input was accepted");
}

template<std::size_t Count>
bool contains(const std::array<Cell, Count>& cells, Cell candidate) {
    return std::find(cells.begin(), cells.end(), candidate) != cells.end();
}

void pump_geometry() {
    constexpr Cell origin{20, 30};
    constexpr std::array<Cell, 4> expected_footprint{{{20, 30}, {21, 30}, {20, 31}, {21, 31}}};
    constexpr std::array<std::array<Cell, 2>, 4> expected_land{{
        {{{20, 31}, {21, 31}}},
        {{{20, 30}, {20, 31}}},
        {{{20, 30}, {21, 30}}},
        {{{21, 30}, {21, 31}}}
    }};
    constexpr std::array<std::array<Cell, 4>, 4> expected_seeds{{
        {{{20, 32}, {21, 32}, {19, 31}, {22, 31}}},
        {{{19, 30}, {19, 31}, {20, 29}, {20, 32}}},
        {{{20, 29}, {21, 29}, {22, 30}, {19, 30}}},
        {{{22, 30}, {22, 31}, {21, 29}, {21, 32}}}
    }};
    constexpr std::array<std::array<Cell, 8>, 4> expected_water{{
        {{{19, 29}, {20, 29}, {21, 29}, {22, 29}, {19, 30}, {20, 30}, {21, 30}, {22, 30}}},
        {{{21, 29}, {22, 29}, {21, 30}, {22, 30}, {21, 31}, {22, 31}, {21, 32}, {22, 32}}},
        {{{19, 31}, {20, 31}, {21, 31}, {22, 31}, {19, 32}, {20, 32}, {21, 32}, {22, 32}}},
        {{{19, 29}, {20, 29}, {19, 30}, {20, 30}, {19, 31}, {20, 31}, {19, 32}, {20, 32}}}
    }};
    for (std::uint8_t orientation = 0; orientation < 4; ++orientation) {
        const auto geometry = original::pump_geometry(origin, orientation);
        check(geometry.footprint == expected_footprint, "Pump full occupied body changed");
        check(geometry.land == expected_land[orientation], "Pump physical land half changed");
        check(geometry.seeds == expected_seeds[orientation], "Pump four exterior outflow seeds changed");
        check(geometry.water == expected_water[orientation], "Pump eight intake-water cells changed");
        std::size_t body_water = 0;
        for (const auto cell : geometry.footprint) {
            body_water += contains(geometry.water, cell) ? 1U : 0U;
            check(contains(geometry.water, cell) != contains(geometry.land, cell),
                  "Pump body has overlapping or missing land/water context");
        }
        check(body_water == 2, "Pump body lost its two occupied water cells");
        for (const auto seed : geometry.seeds)
            check(!contains(geometry.footprint, seed), "Pump seed conducts through its own body");
        const auto negative = original::pump_geometry({-20, -30}, orientation);
        check(negative.footprint[0] == Cell{-20, -30}, "Pure geometry silently clamps source coordinates");
        const auto high = std::numeric_limits<int>::max() - 2;
        const auto low = std::numeric_limits<int>::min() + 1;
        (void)original::pump_geometry({high, high}, orientation);
        (void)original::pump_geometry({low, low}, orientation);
        rejected<std::overflow_error>([&] { (void)original::pump_geometry({high + 1, 30}, orientation); });
        rejected<std::overflow_error>([&] { (void)original::pump_geometry({20, high + 1}, orientation); });
        rejected<std::overflow_error>([&] { (void)original::pump_geometry({low - 1, 30}, orientation); });
        rejected<std::overflow_error>([&] { (void)original::pump_geometry({20, low - 1}, orientation); });
    }
    rejected<std::out_of_range>([] { (void)original::pump_geometry({0, 0}, 4); });
    rejected<std::out_of_range>([] { (void)original::pump_geometry({0, 0}, 255); });
}

void terrain_admission() {
    check(original::ditch_terrain_allowed(0, 0), "Empty resolved terrain failed the source predicate");
    constexpr std::array<unsigned, 17> blocked_bits{
        0, 1, 2, 5, 9, 12, 14, 16, 17, 18, 20, 21, 24, 25, 26, 28, 31
    };
    for (unsigned bit = 0; bit < 32; ++bit) {
        const bool blocked = std::find(blocked_bits.begin(), blocked_bits.end(), bit) != blocked_bits.end();
        const bool expected = !blocked && bit != 3;
        check(original::ditch_terrain_allowed(std::uint32_t{1} << bit, 0) == expected,
              "Ditch terrain bit acquired an unsupported source exemption or veto");
    }
    constexpr std::array<bool, 16> manager_allowed{
        false, false, true, false, true, false, false, false,
        false, true, true, true, true, true, true, true
    };
    for (unsigned bit = 0; bit < manager_allowed.size(); ++bit)
        check(original::ditch_terrain_allowed(0, static_cast<std::uint16_t>(1U << bit)) == manager_allowed[bit],
              "Ditch retained-manager blocker changed");
    for (const auto model : {std::uint16_t{126}, std::uint16_t{203}})
        check(original::ditch_terrain_allowed(0x8U, 0, model), "Exact source occupied-cell exception failed");
    for (const auto model : {std::uint16_t{0}, std::uint16_t{125}, std::uint16_t{127},
                             std::uint16_t{193}, std::uint16_t{202}, std::uint16_t{204},
                             std::numeric_limits<std::uint16_t>::max()})
        check(!original::ditch_terrain_allowed(0x8U, 0, model), "Unqualified occupied cell passed the source predicate");
    check(!original::ditch_terrain_allowed(0xcU, 0, 203), "Pump identity bypassed blocked water terrain");
    check(!original::ditch_terrain_allowed(0x8U, 1, 126), "RoadBlock identity bypassed the manager mask");
    check(original::ditch_terrain_allowed(0x800U, 0xffffU, 65535), "Existing ditch lost its observed early-success branch");
    check(original::ditch_terrain_allowed(0xffffffffU, 0xffffU, 65535), "Existing800 was reinterpreted by later source tests");
    check(!original::ditch_terrain_allowed(0xfffff7ffU, 0xffffU, 65535), "Unknown terrain passed without the existing800 branch");
}

void crossings_and_variants() {
    // Each literal row is a16-bit set of accepted ditch-neighbor masks for
    // the corresponding road-neighbor mask. Both roads and ditches use NESW.
    constexpr std::array<std::uint16_t, 16> allowed_ditches{
        0, 0x505, 0x033, 0, 0x505, 0x505, 0, 0,
        0x033, 0, 0x033, 0, 0, 0, 0, 0
    };
    for (std::uint8_t road = 0; road < 16; ++road) {
        for (std::uint8_t ditch = 0; ditch < 16; ++ditch)
            check(original::crossing_allowed(road, ditch) == ((allowed_ditches[road] & (1U << ditch)) != 0),
                  "Road crossing accepted a corner, junction or parallel ditch");
        check(!original::crossing_allowed(road, 16), "Crossing accepted an unknown ditch direction bit");
        check(!original::crossing_allowed(16, road), "Crossing accepted an unknown road direction bit");
    }
    constexpr std::array<std::uint8_t, 16> expected{15, 6, 7, 2, 8, 1, 3, 10, 9, 5, 0, 13, 4, 12, 11, 14};
    constexpr std::array<std::uint8_t, 16> crossing_x{17, 17, 16, 17, 17, 17, 17, 17, 16, 17, 16, 17, 17, 17, 17, 17};
    constexpr std::array<std::uint8_t, 16> crossing_y{16, 17, 16, 16, 17, 17, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
    for (std::uint8_t mask = 0; mask < 16; ++mask) {
        check(original::ditch_variant(mask) == expected[mask], "Physical view0 ditch topology variant changed");
        check(original::ditch_variant(mask, original::IrrigationAxis::X) == crossing_x[mask],
              "Original road-X crossing selector changed");
        check(original::ditch_variant(mask, original::IrrigationAxis::Y) == crossing_y[mask],
              "Original road-Y crossing selector changed");
    }
    rejected<std::out_of_range>([] { (void)original::ditch_variant(16); });
    rejected<std::out_of_range>([] { (void)original::ditch_variant(255); });
    rejected<std::out_of_range>([] { (void)original::ditch_variant(0, static_cast<original::IrrigationAxis>(255)); });
}

void field_range() {
    constexpr Cell field{50, 60};
    std::size_t in_range = 0;
    for (int y = -6; y <= 6; ++y) {
        for (int x = -6; x <= 6; ++x) {
            const Cell ditch{field.x + x, field.y + y};
            const bool expected = x >= -5 && x <= 5 && y >= -5 && y <= 5 && (x != 0 || y != 0);
            const bool accepted = original::field_in_irrigation_range(field, ditch);
            check(accepted == expected, "Field irrigation lost its square perimeter boundary");
            check(!original::field_in_irrigation_range(field, ditch, true), "Active flood failed to suppress field irrigation");
            in_range += accepted ? 1U : 0U;
        }
    }
    check(in_range == 120, "Field irrigation range has the wrong candidate count");
    check(original::field_in_irrigation_range({0, 0}, {5, 5}), "Diagonal radius5 was treated as a Manhattan distance");
    check(!original::field_in_irrigation_range({0, 0}, {0, 0}), "Field irrigation included the center cell");
    check(!original::field_in_irrigation_range({0, 0}, {6, 0}), "Field irrigation reached radius6");
    constexpr int low = std::numeric_limits<int>::min();
    constexpr int high = std::numeric_limits<int>::max();
    check(original::field_in_irrigation_range({low, low}, {low + 5, low + 5}), "Low coordinates broke a valid field query");
    check(original::field_in_irrigation_range({high, high}, {high - 5, high - 5}), "High coordinates broke a valid field query");
    check(!original::field_in_irrigation_range({low, low}, {high, high}), "Extreme coordinates overflowed the range query");
    check(!original::field_in_irrigation_range({high, high}, {low, low}), "Reverse extreme coordinates overflowed the range query");
}

} // namespace

int main() {
    try {
        pump_geometry();
        terrain_admission();
        crossings_and_variants();
        field_range();
        std::cout << "Original irrigation geometry, terrain, crossings and field range "
                  << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original irrigation rules failed " << error.what() << '\n';
        return 1;
    }
}
