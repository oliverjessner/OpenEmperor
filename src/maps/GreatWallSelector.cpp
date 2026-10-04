#include "maps/GreatWallSelector.h"

#include <array>

namespace openemperor::maps {
namespace {
// Independent descriptions of the branches reached by 0x57c450. Columns
// are effective directions 0,2,4,6, after camera and saved quarter rotation.
constexpr std::array<std::array<unsigned, 4>, 28> piece_views{{
    {0,11,1,10}, {1,10,0,11}, {2,12,7,17}, {3,18,8,13}, {4,19,9,14},
    {5,16,6,15}, {6,15,5,16}, {7,17,2,12}, {8,13,3,18}, {9,14,4,19},
    {10,0,11,1}, {11,1,10,0}, {12,7,17,2}, {13,3,18,8}, {14,4,19,9},
    {15,5,16,6}, {16,6,15,5}, {17,2,12,7}, {18,8,13,3}, {19,9,14,4},
    {20,23,21,22}, {21,22,20,23}, {22,20,23,21}, {23,21,22,20},
    {24,25,24,25}, {25,24,25,24}, {25,24,25,24}, {24,25,24,25},
}};
// 0x57ce48..0x57cf44, rows NE,SE,SW,NW; columns effective 0,2,4,6.
constexpr std::array<std::array<unsigned, 4>, 4> north_gate{{
    {32,29,34,31}, {33,30,35,28}, {34,31,32,29}, {35,28,33,30},
}};
constexpr std::array<std::array<unsigned, 4>, 4> east_gate{{
    {35,28,33,30}, {32,29,34,31}, {33,30,35,28}, {34,31,32,29},
}};
constexpr std::array<std::string_view, 10> earthen_archives{
    "China_Mon_Earthen_Greatwall_1", "China_Mon_Earthen_Greatwall_2",
    "China_Mon_Earthen_Greatwall_3", "China_Mon_Earthen_Greatwall_4",
    "China_Mon_Earthen_Greatwall_5", "China_Mon_Earthen_Greatwall_6",
    "China_Mon_Earthen_Greatwall_7", "China_Mon_Earthen_Greatwall_8",
    "China_Mon_Earthen_Greatwall_9", "China_Mon_Earthen_Greatwall_10"};
constexpr std::array<std::string_view, 10> stone_archives{
    "China_Mon_Greatwall_1", "China_Mon_Greatwall_2", "China_Mon_Greatwall_3",
    "China_Mon_Greatwall_4", "China_Mon_Greatwall_5", "China_Mon_Greatwall_6",
    "China_Mon_Greatwall_7", "China_Mon_Greatwall_8", "China_Mon_Greatwall_9",
    "China_Mon_Greatwall_10"};

GreatWallSelection unsupported(const char* reason) {
    GreatWallSelection out;
    out.reason = reason;
    return out;
}
} // namespace

GreatWallSelection select_great_wall(const GreatWallSelectorInput& input) {
    if (!valid_great_wall_model_piece(input.model_piece)) return unsupported("unsupported Great Wall model piece");
    if (!input.restore_context.material) return unsupported("Great Wall restore material context is unresolved");
    const auto material = *input.restore_context.material;
    if (material < 1 || material > 3) return unsupported("unsupported derived Great Wall material");
    const auto camera = input.restore_context.camera_view;
    if (camera > 6 || (camera & 1U)) return unsupported("unsupported original Great Wall camera view");
    if (input.orientation > 3) return unsupported("unsupported saved Great Wall orientation");
    if (input.phase == 0) return unsupported("Great Wall phase zero includes an unimplemented material override branch");
    GreatWallSelection out;
    const auto& row = input.model_piece;
    out.side = row.side;
    out.effective_view = (row.position + 8U - camera + 2U * input.orientation) % 8U;
    const auto view_column = out.effective_view / 2U;
    if (row.kind == GreatWallPieceKind::Gate) {
        if (input.phase != 1) return unsupported("unsupported Great Wall gate phase");
        out.group = {0x1001};
        out.variant = material == 1 ? 31U :
            (row.position == 0 ? north_gate : east_gate)[(row.piece - 1U) / 2U][view_column];
    } else if (row.kind == GreatWallPieceKind::Road) {
        if (input.phase != 1 && input.phase != 2) return unsupported("unsupported Great Wall road phase");
        const bool along_second_axis = out.effective_view == 2 || out.effective_view == 6;
        out.group = {input.phase == 1 ? 0x61eU : 0x1001U};
        out.variant = input.phase == 1 ? unsigned(along_second_axis) : 40U + unsigned(!along_second_axis);
        out.flags = 8;
    } else {
        const bool final_tower = row.kind == GreatWallPieceKind::Tower && input.phase == 11;
        if (input.phase < 1 || (input.phase > 10 && !final_tower)) return unsupported("unsupported Great Wall wall or tower phase");
        const auto archive_phase = static_cast<unsigned>(final_tower ? 10 : input.phase);
        // 0x57be29..0x57c020: phases 1/4/7/10 -> slot 8, 2/5/8 ->
        // slot 9, 3/6/9 -> slot 10. Every selected archive has group position 0.
        out.slot = 8U + (archive_phase - 1U) % 3U;
        out.group = {out.slot * 512U + 1U};
        const auto archive = material == 1 ? std::string_view("China_Mon_Greatwall_Ruined") :
            (material == 2 ? earthen_archives : stone_archives)[archive_phase - 1U];
        out.required_registration = GreatWallRegistration{out.slot, archive};
        if (final_tower) {
            // 0x57d474: final towers retain 26/27; perpendicular views swap.
            out.variant = out.effective_view == 0 || out.effective_view == 4 ? row.piece : 53U - row.piece;
        } else out.variant = piece_views[row.piece][view_column];
    }
    out.slot = out.group.value / 512U;
    out.supported = true;
    out.reason = "EXE-observed Great Wall piece selector with explicit restore context";
    return out;
}

} // namespace openemperor::maps
