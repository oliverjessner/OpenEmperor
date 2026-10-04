#include "maps/PinnacleSelector.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace openemperor::maps;
namespace {
constexpr std::size_t count = stored_grid_width * stored_grid_height;
constexpr GridCell origin{80, 90};

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::size_t index(GridCell cell) {
    return std::size_t(cell.y) * stored_grid_width + cell.x;
}
struct Fixture {
    std::vector<std::uint32_t> terrain = std::vector<std::uint32_t>(count, 0);
    std::vector<std::uint32_t> objects = std::vector<std::uint32_t>(count, 0);
    std::vector<std::uint8_t> variation = std::vector<std::uint8_t>(count, 0);
    std::vector<std::uint8_t> fertility = std::vector<std::uint8_t>(count, 100);
    std::vector<std::uint8_t> properties = std::vector<std::uint8_t>(count, 0);
    std::vector<std::uint8_t> candidates = std::vector<std::uint8_t>(count, 0);

    void place(GridCell rear = origin, std::uint32_t object_bits = 8) {
        for (unsigned y = 0; y < 5; ++y) for (unsigned x = 0; x < 5; ++x) {
            const auto i = index({rear.x + x, rear.y + y});
            terrain[i] = 0x02000080U;
            objects[i] = object_bits;
            properties[i] = 0x24;
            candidates[i] = static_cast<std::uint8_t>(x | (y << 3U) |
                (x == 0 && y == 4 ? 0x40U : 0));
        }
    }
    LandscapeSelectorInput input(unsigned orientation = 0) const {
        return {terrain, objects, variation, fertility, std::nullopt, orientation};
    }
    std::optional<PinnacleSelection> select(GridCell cell) const {
        return select_pinnacle(input(), cell, properties, candidates);
    }
};
} // namespace

int main() {
    try {
        Fixture f;
        f.place();
        for (unsigned y = 0; y < 5; ++y) for (unsigned x = 0; x < 5; ++x) {
            const auto selected = f.select({origin.x + x, origin.y + y});
            check(selected && selected->origin == origin && selected->side == 5 &&
                selected->group.value == 0x60d, "every member recovers one canonical 5x5 owner");
        }
        for (const auto [bits, key] : {
            std::pair{0U, 0x60dU}, {0x08U, 0x60dU}, {0x10U, 0x60aU},
            {0x20U, 0x609U}, {0x40U, 0x619U}, {0x78U, 0x60dU},
            {0x70U, 0x60aU}, {0x60U, 0x609U}, {0x41U, 0x619U}}) {
            f.place(origin, bits);
            check(f.select(origin)->group.value == key,
                "bank priority is 08 then 10 then 20 then 40, with unrelated bits ignored");
        }
        const auto ordinary = f.select(origin);
        std::fill(f.variation.begin(), f.variation.end(), 255);
        std::fill(f.fertility.begin(), f.fertility.end(), 0);
        const auto changed = f.select(origin);
        check(ordinary && changed && ordinary->group.value == changed->group.value &&
            ordinary->origin == changed->origin, "variation and fertility cannot select a pinnacle bank");
        check(!f.select({origin.x - 1, origin.y}), "adjacent elevated or ordinary terrain is not a pinnacle");
        const GridCell outside_owner{origin.x + 5, origin.y};
        const auto outside_index = index(outside_owner);
        f.terrain[outside_index] = 0x02000000;
        f.objects[outside_index] = 0x40;
        f.properties[outside_index] = 4;
        f.candidates[outside_index] = 5;
        check(!f.select(outside_owner), "a malformed sixth column cannot claim the neighbouring owner");

        const auto member = index({origin.x + 3, origin.y + 2});
        f.candidates[member] ^= 0x80;
        check(!f.select(origin), "unknown candidate bit 80 is not repaired");
        f.place(); f.candidates[member] = 0;
        check(!f.select(origin), "incorrect part coordinates invalidate the entire owner");
        f.place(); f.candidates[index({origin.x, origin.y + 4})] &= 0x3f;
        check(!f.select(origin), "missing draw marker fails closed");
        f.place(); f.candidates[member] |= 0x40;
        check(!f.select(origin), "duplicate draw marker fails closed");
        f.place(); f.properties[member] = 3;
        check(!f.select(origin), "all members must declare canonical side five");
        f.place(); f.terrain[member] = 0x80;
        check(!f.select(origin), "mixed raw terrain does not become a mountain");
        for (const auto earlier_bit : {4U, 1U, 2U, 0x4000U, 0x40U, 0x100U, 0x20000U, 0x80000000U}) {
            f.place();
            f.terrain[member] |= earlier_bit;
            check(!f.select(origin),
                "a member with water, vegetation, rock, wall or another terrain branch retains earlier authority");
            f.place();
            f.terrain[index(origin)] |= earlier_bit;
            check(!f.select({origin.x + 1, origin.y + 1}),
                "origin terrain conflicts fail from every otherwise canonical member");
        }
        f.place(); f.objects[member] = 0x20;
        check(!f.select(origin), "conflicting banks cannot depend on traversal order");
        check(!select_pinnacle(f.input(), origin, std::span(f.properties).first(7), f.candidates),
            "incomplete byte grids fail closed");
        check(!select_pinnacle(f.input(1), origin, f.properties, f.candidates),
            "other view shapes are outside the frozen orientation-zero presentation");
        check(!f.select({228, 0}), "outside-storage coordinates fail closed");

        Fixture edge;
        edge.place({223, 223});
        check(edge.select({227, 227})->origin == GridCell{223, 223},
            "complete footprint at final storage row and column is bounded");
        edge.candidates[index({223, 223})] = 1;
        check(!edge.select({223, 223}), "origin recovery cannot accept noncanonical ownership");
        Fixture underflow;
        underflow.terrain[0] = 0x02000000;
        underflow.properties[0] = 4;
        underflow.candidates[0] = 1;
        check(!underflow.select({0, 0}), "origin walking cannot underflow a row");

        std::cout << "pinnacle selector passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
