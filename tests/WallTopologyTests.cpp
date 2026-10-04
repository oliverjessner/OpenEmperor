#include "maps/WallTopology.h"
#include "maps/RuntimeArchiveLayout.h"

#include <iostream>
#include <stdexcept>

using namespace openemperor::maps;
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
WallTopologyInput wall(unsigned mask) {
    WallTopologyInput input;
    input.center_terrain = 0x4080;
    for (unsigned direction = 0; direction < 8; ++direction)
        if ((mask & (1U << direction)) != 0) input.neighboring_terrain[direction] = 0x4000;
    return input;
}
}
int main() {
    try {
        check(!select_normal_wall({}).wall_bit, "ordinary ground is not wall terrain");
        const auto isolated = select_normal_wall(wall(0));
        check(isolated.verified && isolated.variant == 17 && isolated.semantic_row == 16,
              "isolated wall has its observed end shape");
        check(select_normal_wall(wall(0x44)).variant == 0 &&
              select_normal_wall(wall(0x11)).variant == 2,
              "east-west and north-south wall straights are distinct");
        check(select_normal_wall(wall(0x05)).variant == 7 &&
              select_normal_wall(wall(0x14)).variant == 4 &&
              select_normal_wall(wall(0x50)).variant == 5 &&
              select_normal_wall(wall(0x41)).variant == 6,
              "four corner orientations select different records");
        check(select_normal_wall(wall(0x45)).variant == 8 &&
              select_normal_wall(wall(0x15)).variant == 9 &&
              select_normal_wall(wall(0x54)).variant == 10 &&
              select_normal_wall(wall(0x51)).variant == 11,
              "four T orientations retain their independently observed records");
        auto rotated = wall(0x44); rotated.orientation = 1;
        check(select_normal_wall(rotated).variant == 2, "original view rotates wall selection");
        check(select_normal_wall(wall(0x55)).variant == 12,
              "cross with absent diagonals uses its observed rule");
        check(!select_normal_wall(wall(0x57)).variant,
              "cross with a present diagonal has no invented table rule");
        check(select_normal_wall(wall(0x46)).variant == 0,
              "straight rules ignore diagonal walls");
        auto neighboring_road = wall(0x44);
        neighboring_road.neighboring_terrain[0] = 0x40;
        check(select_normal_wall(neighboring_road).variant == 0 &&
              select_normal_wall(neighboring_road).neighbor_mask == 0x44,
              "a neighboring road never becomes an original wall neighbor");
        auto previous = wall(0x44); previous.generated_cardinal_variants[3] = 0;
        check(select_normal_wall(previous).variant == 1,
              "already generated matching straight triggers alternate record");
        previous.generated_cardinal_variants[0] = 1;
        check(select_normal_wall(previous).variant == 0,
              "an already generated alternate stops the straight alternation");
        auto gate = wall(0x40); gate.neighboring_terrain[2] = 0x8000;
        check(select_normal_wall(gate).gate_context && !select_normal_wall(gate).verified &&
              !select_normal_wall(gate).variant, "gate composition remains explicitly unresolved");
        auto excluded = wall(0); excluded.center_terrain |= 0x100;
        check(!select_normal_wall(excluded).selector_gate,
              "original post-load branch excludes flood wall cells");
        excluded = wall(0); excluded.center_terrain |= 0x40;
        check(!select_normal_wall(excluded).selector_gate, "road crossing is not a guessed wall shape");
        excluded = wall(0); excluded.center_terrain |= 0x8;
        check(!select_normal_wall(excluded).selector_gate, "original model-object cells stay separate");
        for (unsigned mask = 0; mask < 256; ++mask) {
            for (unsigned orientation = 0; orientation < 4; ++orientation) {
                auto input = wall(mask); input.orientation = orientation;
                const auto a = select_normal_wall(input), b = select_normal_wall(input);
                check(a.variant == b.variant && a.semantic_row == b.semantic_row &&
                      a.verified == b.verified, "wall topology is deterministic");
                if (a.variant) check(*a.variant < 18, "static selector stays in the audited wall family");
            }
        }
        openemperor::assets::Sg3Archive synthetic;
        synthetic.header.version = 213;
        synthetic.header.image_capacity = 400;
        synthetic.header.reported_images_in_use = 220;
        synthetic.images.resize(400);
        synthetic.groups.emplace_back();
        auto& system = synthetic.groups.front();
        system.filename = "Zeus_system.bmp";
        system.image_count = 200;
        system.first_image_index = 1;
        system.last_image_index = 200;
        synthetic.index[1] = 201;
        check(!build_runtime_archive_layout(2, synthetic) &&
              !build_runtime_archive_layout(2, synthetic, RuntimeLayoutEvidence::TerrainElevationAndSlot8),
              "historical runtime profiles do not silently acquire General slot2");
        const auto general = build_runtime_archive_layout(
            2, synthetic, RuntimeLayoutEvidence::TerrainElevationAndGeneral2);
        check(general && general->system_record_skip == 200 &&
              general->physical_record_for_local(0) == 201 &&
              general->physical_record_for_local(19) == 220 &&
              !general->physical_record_for_local(20),
              "explicit General2 evidence applies the bounded system skip exactly once");
        check(!build_runtime_archive_layout(8, synthetic, RuntimeLayoutEvidence::TerrainElevationAndGeneral2),
              "General2 evidence does not silently acquire GreatWall slot8");
        synthetic.groups.front().filename = "Other.bmp";
        check(!build_runtime_archive_layout(2, synthetic, RuntimeLayoutEvidence::TerrainElevationAndGeneral2),
              "General2 extension rejects an unobserved system-prefix layout");
        std::cout << "Wall topology checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
