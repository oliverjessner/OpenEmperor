#include "simulation/original/FishingRules.h"
#include "simulation/original/IrrigationRules.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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
    throw std::runtime_error("invalid fishing-rule input was accepted");
}

template<std::size_t Count>
bool contains(const std::array<Cell, Count>& cells, Cell cell) {
    return std::find(cells.begin(), cells.end(), cell) != cells.end();
}

void complete_oriented_geometry() {
    constexpr Cell origin{20, 30};
    constexpr std::array<Cell, 4> expected_body{{{20, 30}, {21, 30}, {20, 31}, {21, 31}}};
    constexpr std::array<Cell, 4> expected_docks{{{20, 29}, {22, 30}, {21, 32}, {19, 31}}};
    constexpr std::array<Cell, 8> expected_perimeter{{
        {20, 29}, {21, 29}, {22, 30}, {22, 31},
        {21, 32}, {20, 32}, {19, 31}, {19, 30}
    }};
    for (std::uint8_t orientation = 0; orientation < 4; ++orientation) {
        const auto geometry = original::fishing_quay_geometry(origin, orientation);
        const auto shore = original::pump_geometry(origin, orientation);
        check(geometry.body == expected_body && geometry.body == shore.footprint,
              "Quay main body lost the existing source shore geometry");
        check(geometry.water == shore.water && geometry.land == shore.land,
              "Quay intake context diverged from its shared source helper");
        check(geometry.dock == expected_docks[orientation], "Quay exterior dock orientation changed");
        check(geometry.perimeter == expected_perimeter, "Quay source perimeter order changed");
        check(!contains(geometry.body, geometry.dock) && contains(geometry.water, geometry.dock),
              "Quay dock overlaps the main body or lies outside its intake water context");
        check(geometry.occupied.back() == geometry.dock,
              "Quay complete occupied shape omitted its extra dock claim");
        for (const auto body_cell : expected_body)
            check(contains(geometry.occupied, body_cell), "Quay complete shape lost a main-body cell");
        for (std::size_t i = 0; i < geometry.occupied.size(); ++i)
            for (std::size_t j = i + 1; j < geometry.occupied.size(); ++j)
                check(geometry.occupied[i] != geometry.occupied[j], "Quay occupied shape repeats a cell");
        std::size_t claimed_water = 0;
        for (const auto cell : geometry.occupied) claimed_water += contains(geometry.water, cell) ? 1U : 0U;
        check(claimed_water == 3, "Quay lost its two body-water cells and exterior dock-water claim");
        const auto negative = original::fishing_quay_geometry({-20, -30}, orientation);
        check(negative.body.front() == Cell{-20, -30}, "Pure Quay geometry clamped source coordinates");
    }
}

void checked_geometry_bounds() {
    constexpr auto high = std::numeric_limits<int>::max() - 2;
    constexpr auto low = std::numeric_limits<int>::min() + 1;
    for (std::uint8_t orientation = 0; orientation < 4; ++orientation) {
        (void)original::fishing_quay_geometry({high, high}, orientation);
        (void)original::fishing_quay_geometry({low, low}, orientation);
        rejected<std::overflow_error>([&] {
            (void)original::fishing_quay_geometry({high + 1, 30}, orientation);
        });
        rejected<std::overflow_error>([&] {
            (void)original::fishing_quay_geometry({20, high + 1}, orientation);
        });
        rejected<std::overflow_error>([&] {
            (void)original::fishing_quay_geometry({low - 1, 30}, orientation);
        });
        rejected<std::overflow_error>([&] {
            (void)original::fishing_quay_geometry({20, low - 1}, orientation);
        });
    }
    for (const std::uint8_t orientation : {std::uint8_t{4}, std::uint8_t{255}})
        rejected<std::out_of_range>([&] { (void)original::fishing_quay_geometry({0, 0}, orientation); });
}

void source_byte_domains() {
    for (unsigned value = 0; value <= 255; ++value)
        check(original::fishing_origin_flood_byte_allowed(static_cast<std::uint8_t>(value)) ==
                  (value < 254),
              "Source navigation-byte predicate changed its exact FE/FF exclusions");
    check(!original::fishing_origin_connection_present(0), "Empty connection plane became supplied");
    check(!original::fishing_origin_connection_present(1),
          "The separate source bit1 granted fishing-origin connectivity");
    check(original::fishing_origin_connection_present(2) &&
              original::fishing_origin_connection_present(3),
          "Source fishing bit2 was lost when combined with another connection bit");
    for (unsigned bit = 0; bit < 8; ++bit)
        check(original::fishing_origin_connection_present(static_cast<std::uint8_t>(1U << bit)) ==
                  (bit == 1),
              "An unrelated source connection bit granted fishing-origin reachability");
}

void distance_preparation_extra_is_a_separate_byte_domain() {
    for (unsigned value = 0; value < 254; ++value) {
        const auto extra = original::boat_distance_extra_penalty(static_cast<std::uint8_t>(value));
        check(extra.has_value() && *extra == 0,
              "Ordinary raw distance bytes acquired an extra penalty or became unavailable");
    }
    const auto deferred = original::boat_distance_extra_penalty(254);
    check(deferred.has_value() && *deferred == 4,
          "Raw FE distance extra was confused with origin-flood exclusion");
    check(!original::boat_distance_extra_penalty(255).has_value(),
          "Raw FF distance preparation exclusion was lost");
}

void resolved_perimeter_query() {
    constexpr int width = 9, height = 8;
    constexpr Cell origin{3, 3};
    constexpr std::array<Cell, 8> perimeter{{
        {3, 2}, {4, 2}, {5, 3}, {5, 4}, {4, 5}, {3, 5}, {2, 4}, {2, 3}
    }};
    std::vector<std::uint8_t> flags(static_cast<std::size_t>(width * height), 0);
    const auto index = [](Cell cell) {
        return static_cast<std::size_t>(cell.y * width + cell.x);
    };
    check(!original::quay_perimeter_has_fishing_connection(origin, width, height, flags),
          "An empty resolved plane granted a fishing connection");
    for (const auto cell : perimeter) {
        flags[index(cell)] = 2;
        const auto before = flags;
        check(original::quay_perimeter_has_fishing_connection(origin, width, height, flags),
              "A resolved source connection on the body perimeter was ignored");
        check(flags == before, "Fishing perimeter query mutated its resolved source input");
        flags[index(cell)] = 0;
    }
    for (const auto cell : {Cell{3, 3}, Cell{4, 4}, Cell{2, 2}, Cell{5, 5}, Cell{0, 0}}) {
        flags[index(cell)] = 2;
        check(!original::quay_perimeter_has_fishing_connection(origin, width, height, flags),
              "Body interior, diagonal corner or remote source flags granted perimeter connectivity");
        flags[index(cell)] = 0;
    }
    for (const auto cell : perimeter) flags[index(cell)] = 1;
    check(!original::quay_perimeter_has_fishing_connection(origin, width, height, flags),
          "Another source connection bit satisfied the fishing gate");
    std::fill(flags.begin(), flags.end(), 0);
    flags[index({3, 2})] = 3;
    check(original::quay_perimeter_has_fishing_connection(origin, width, height, flags),
          "Combined source connection attributes lost the fishing bit");
}

void rejected_perimeter_inputs() {
    const std::vector<std::uint8_t> full(228U * 228U, 2);
    check(original::quay_perimeter_has_fishing_connection({1, 1}, 228, 228, full) &&
              original::quay_perimeter_has_fishing_connection({225, 225}, 228, 228, full),
          "Exact source-grid interior query bounds changed");
    const auto stable = full;
    for (const auto origin : {Cell{0, 1}, Cell{1, 0}, Cell{226, 225}, Cell{225, 226},
                              Cell{-1, 1}, Cell{std::numeric_limits<int>::max(), 1},
                              Cell{1, std::numeric_limits<int>::min()}})
        rejected<std::out_of_range>([&] {
            (void)original::quay_perimeter_has_fishing_connection(origin, 228, 228, full);
        });
    for (const auto dimensions : {Cell{0, 228}, Cell{228, 0}, Cell{-1, 228}, Cell{229, 228},
                                  Cell{228, 229}, Cell{std::numeric_limits<int>::max(), 228}})
        rejected<std::invalid_argument>([&] {
            (void)original::quay_perimeter_has_fishing_connection(
                {1, 1}, dimensions.x, dimensions.y, full);
        });
    rejected<std::invalid_argument>([&] {
        (void)original::quay_perimeter_has_fishing_connection({1, 1}, 228, 228, {});
    });
    rejected<std::invalid_argument>([&] {
        (void)original::quay_perimeter_has_fishing_connection(
            {1, 1}, 228, 228, std::span(full).first(full.size() - 1));
    });
    const std::array<std::uint8_t, 4> small{};
    rejected<std::out_of_range>([&] {
        (void)original::quay_perimeter_has_fishing_connection({0, 0}, 2, 2, small);
    });
    check(full == stable, "Rejected source query modified its caller-owned plane");
}

} // namespace

int main() {
    try {
        complete_oriented_geometry();
        checked_geometry_bounds();
        source_byte_domains();
        distance_preparation_extra_is_a_separate_byte_domain();
        resolved_perimeter_query();
        rejected_perimeter_inputs();
        std::cout << "Original fishing shore/dock geometry and source connection predicates "
                  << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original fishing rules failed: " << error.what() << '\n';
        return 1;
    }
}
