#include "maps/MapGeometry.h"
#include "maps/MapVisualization.h"
#include "maps/TerrainInterpretation.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
namespace maps = openemperor::maps;
void check(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
maps::ParsedEmperorMap fixture(std::uint32_t size = 84) {
    maps::ParsedEmperorMap map;
    map.declared_map_size = size;
    map.terrain_raw.logical_offset = maps::terrain_logical_offset;
    map.objects_raw.logical_offset = maps::objects_logical_offset;
    map.terrain_raw.values.resize(228U * 228U);
    map.objects_raw.values.resize(228U * 228U);
    return map;
}
std::array<std::uint8_t,4> rgba_at(const maps::DebugPixels& image, std::uint32_t x, std::uint32_t y) {
    const auto i = (static_cast<std::size_t>(y) * image.width + x) * 4U;
    return {image.rgba.at(i),image.rgba.at(i+1),image.rgba.at(i+2),image.rgba.at(i+3)};
}
}
int main() {
    try {
        using C = maps::TerrainCategory;
        auto a = maps::interpret_terrain(0x80005U, 2U);
        check(a.terrain_raw == 0x80005U && a.objects_raw == 2U && a.category == C::OffMap &&
              a.rule == "offmap_bit" && a.recognized_terrain_flags == 0x80005U,
              "offmap overrides other flags without changing raw values");
        check(maps::interpret_terrain(0x100U,0).category == C::Flood &&
              maps::interpret_terrain(0x104U,0).category == C::Water,
              "flood without water differs from flood and water");
        check(maps::interpret_terrain(0x101U,0).category == C::Flood &&
              maps::interpret_terrain(0x105U,0).category == C::Vegetation,
              "flood precedence before vegetation; water suppresses flood branch");
        check(maps::interpret_terrain(1U,0).category == C::Vegetation &&
              maps::interpret_terrain(1U,2U).category == C::Bamboo &&
              maps::interpret_terrain(0U,2U).recognized_object_flags == 0U,
              "bamboo object bit only inside vegetation branch");
        check(maps::interpret_terrain(0x2U | 0x40U,0).category == C::Rock &&
              maps::interpret_terrain(0x4U | 0x40U,0).category == C::Road &&
              maps::interpret_terrain(0x104U | 0x40U,0).category == C::Water,
              "rock and water-road precedence");
        check(maps::interpret_terrain(0x8U | 0x4U,0).category == C::Structure &&
              maps::interpret_terrain(0x80U,0).category == C::Fertile &&
              maps::interpret_terrain(0x80U,0).partial,
              "building before water, fertility remains a hint");
        a = maps::interpret_terrain(0x40000000U,0x80000000U);
        check(a.category == C::Unknown && a.unknown_terrain_bits == 0x40000000U &&
              a.unknown_object_bits == 0x80000000U && a.partial,
              "unknown bits and objects are preserved, not called grass");
        check(maps::interpret_terrain(0,0).category == C::Empty, "zero terrain is the reference empty branch");

        constexpr std::array<std::uint32_t,5> sizes{84,112,140,170,226};
        constexpr std::array<std::uint32_t,5> candidate_counts{3612,6384,9940,14620,25764};
        for (std::size_t i=0;i<sizes.size();++i) {
            maps::MapGeometry g(sizes[i]);
            check(g.supported && g.candidate.size()==228U*228U &&
                  g.projected_pixels.size()==static_cast<std::size_t>(sizes[i])*sizes[i],
                  "supported geometry dimensions");
            std::uint32_t count=0, projected=0;
            for (auto v:g.candidate) count+=v;
            for (auto v:g.projected_pixels) projected+=v.has_value();
            check(count==candidate_counts[i] && projected==sizes[i]*sizes[i],
                  "fixed reference-derived diamond counts and projected pixel coverage");
            check(!g.contains({0,0}) && g.contains({113,113}) &&
                  !g.at_projected({sizes[i],0}), "boundary and center reference cells");
        }
        maps::MapGeometry g{84};
        const auto center_pixel = g.projected_origin({114,114});
        check(g.contains({113,72}) && g.at_projected({82,0})==maps::GridCell{113,72} &&
              g.at_projected({0,0})==maps::GridCell{72,113} &&
              center_pixel==maps::BitmapPixel{41,43} &&
              g.at_projected({42,43})==maps::GridCell{114,114} &&
              ((center_pixel->x+center_pixel->y)&1U)==0,
              "fixed origin, axis and boundary coordinates");
        check(!g.projected_origin({155,114}), "clipped reference edge cell has no projected pixel");
        for (std::uint32_t y=0;y<84;++y) for (std::uint32_t x=0;x<84;++x) {
            auto cell=g.at_projected({x,y});
            check(cell && g.contains(*cell), "every projected pixel identifies one storage cell");
        }
        check(!maps::MapGeometry{85}.supported && !maps::MapGeometry{0}.supported &&
              !maps::MapGeometry{228}.supported, "unsupported sizes remain storage-only");

        auto map=fixture();
        for (std::uint32_t y=0;y<228;++y) for (std::uint32_t x=0;x<228;++x)
            if (!g.contains({x,y})) map.terrain_raw.values[y*228U+x]=0x80000U;
        auto mask=maps::compare_masks(map,g);
        check(mask.candidate_and_onmap==3612 && mask.outside_and_offmap==48372 &&
              mask.mismatches()==0, "exact candidate/offmap agreement fixture");
        map.terrain_raw.values[113U*228U+113U] |= 0x80000U;
        map.terrain_raw.values[0]=0;
        mask=maps::compare_masks(map,g);
        check(mask.candidate_and_offmap==1 && mask.outside_and_onmap==1 &&
              mask.mismatches()==2 && mask.mismatch_examples.size()==2 &&
              mask.mismatch_examples[0]==maps::GridCell{0,0} &&
              mask.mismatch_examples[1]==maps::GridCell{113,113},
              "both mismatch directions and deterministic storage positions");
        map.terrain_raw.values[113U*228U+72U]=0x4U;
        map.terrain_raw.values[113U*228U+73U]=0x40000000U;
        map.objects_raw.values[113U*228U+72U]=0x12345678U;
        const auto interpreted=maps::interpret_map(map);
        const auto raw=maps::make_map_debug_pixels(map,interpreted,g,maps::MapViewMode::Storage,
                                                   maps::RawLayer::Terrain,maps::MaskMode::Full);
        check(raw.width==228 && rgba_at(raw,72,113)==maps::raw_value_color(4U),
              "unchanged raw storage representation");
        const auto semantic=maps::make_map_debug_pixels(map,interpreted,g,maps::MapViewMode::Semantic,
                                                        maps::RawLayer::Objects,maps::MaskMode::Full);
        check(rgba_at(semantic,72,113)==std::array<std::uint8_t,4>{36,92,176,255} &&
              rgba_at(semantic,73,113)==std::array<std::uint8_t,4>{255,0,200,255} &&
              rgba_at(semantic,0,0)==maps::semantic_color(C::Empty),
              "semantic palette uses raw cell and does not hide unknown/candidate mismatch");
        const auto projection=maps::make_map_debug_pixels(map,interpreted,g,maps::MapViewMode::Projected,
                                                          maps::RawLayer::Terrain,maps::MaskMode::Full);
        check(projection.width==84 && rgba_at(projection,0,0)==std::array<std::uint8_t,4>{36,92,176,255},
              "projected pixel retains marked asymmetric storage cell");
        const auto compare=maps::make_map_debug_pixels(map,interpreted,g,maps::MapViewMode::Semantic,
                                                       maps::RawLayer::Terrain,maps::MaskMode::Compare);
        check(rgba_at(compare,113,113)==std::array<std::uint8_t,4>{255,58,170,255} &&
              rgba_at(compare,0,0)==std::array<std::uint8_t,4>{255,181,43,255},
              "mask disagreement colors are distinct");
        maps::StorageGridCamera camera;
        camera.grid_width=84;camera.grid_height=84;camera.viewport_width=160;camera.viewport_height=120;
        camera.center_on({42,42});
        auto point=camera.grid_to_screen({41.5,43.5});
        check(camera.pick(point)==maps::DisplayCell{41,43}, "projected camera selection");
        camera.zoom_at(point,2.0);
        check(camera.pick(point)==maps::DisplayCell{41,43} &&
              g.at_projected({41,43})==maps::GridCell{114,114},
              "zoom keeps projected pixel tied to storage cell");
        std::cout << "semantic geometry checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
