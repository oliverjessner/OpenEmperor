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

GreatWallSelection unsupported(const char* reason,GreatWallRestoreContext context) {
    GreatWallSelection out;
    out.reason = reason;
    out.restore_context=context;
    return out;
}
} // namespace

const char* great_wall_presentation_mode_name(GreatWallPresentationMode mode) {
    switch (mode) {
    case GreatWallPresentationMode::Automatic:return "auto";
    case GreatWallPresentationMode::HistoricalFallback:return "historical";
    case GreatWallPresentationMode::PreviewRuined:return "preview-ruined";
    case GreatWallPresentationMode::PreviewEarthen:return "preview-earthen";
    case GreatWallPresentationMode::PreviewStone:return "preview-stone";
    }
    return "invalid";
}
std::optional<GreatWallPresentationMode> parse_great_wall_presentation_mode(std::string_view name) {
    for (const auto mode:{GreatWallPresentationMode::Automatic,GreatWallPresentationMode::HistoricalFallback,
        GreatWallPresentationMode::PreviewRuined,GreatWallPresentationMode::PreviewEarthen,GreatWallPresentationMode::PreviewStone})
        if (name==great_wall_presentation_mode_name(mode)) return mode;
    return {};
}
const char* great_wall_context_source_name(GreatWallContextSource source) {
    switch (source) {
    case GreatWallContextSource::Unavailable:return "unavailable";
    case GreatWallContextSource::VerifiedOriginal:return "verified_original";
    case GreatWallContextSource::ExplicitPreview:return "explicit_preview";
    }
    return "invalid";
}
bool great_wall_original_context_verified(const GreatWallRestoreContext& context) {
    return context.source==GreatWallContextSource::VerifiedOriginal &&
        context.mode==GreatWallPresentationMode::Automatic && context.material &&
        *context.material>=1 && *context.material<=3;
}
GreatWallRestoreContext great_wall_context_from_mode(GreatWallPresentationMode mode,GreatWallRestoreContext original) {
    GreatWallRestoreContext out;out.mode=mode;out.camera_view=original.camera_view;
    switch (mode) {
    case GreatWallPresentationMode::Automatic:
        if (great_wall_original_context_verified(original)) return original;
        out.reason="automatic Great Wall restore context is unavailable";return out;
    case GreatWallPresentationMode::HistoricalFallback:
        out.reason="explicit historical Great Wall fallback";return out;
    case GreatWallPresentationMode::PreviewRuined:out.material=1;break;
    case GreatWallPresentationMode::PreviewEarthen:out.material=2;break;
    case GreatWallPresentationMode::PreviewStone:out.material=3;break;
    default:out.reason="invalid Great Wall presentation mode";return out;
    }
    out.source=GreatWallContextSource::ExplicitPreview;
    out.reason="explicit Great Wall material preview; original restore context unverified";
    return out;
}
GreatWallRestoreContext resolve_original_great_wall_context(const GreatWallOriginalRestoreInputs& inputs) {
    GreatWallRestoreContext out;out.camera_view=inputs.camera_view;
    if (!inputs.mode) {out.reason="original Great Wall mode unavailable";return out;}
    if (*inputs.mode==1) {
        out.material=3;out.source=GreatWallContextSource::VerifiedOriginal;
        out.reason="EXE-observed original mode one selects material three";return out;
    }
    if (!inputs.current_player_goals || !inputs.goals_validated) {
        out.reason="validated original current-player goal list unavailable";return out;
    }
    out.material=1;out.source=GreatWallContextSource::VerifiedOriginal;
    out.reason="EXE-observed validated original goal list has no matching material goal";
    for (const auto& goal:*inputs.current_player_goals) if (goal.type==2 && (goal.value==85 || goal.value==86)) {
        out.material=goal.value==85 ? 2:3;
        out.reason="EXE-observed first matching original current-player goal selects material";
        break;
    }
    return out;
}

GreatWallSelection select_great_wall(const GreatWallSelectorInput& input) {
    const auto fail=[&](const char* reason){return unsupported(reason,input.restore_context);};
    if (!valid_great_wall_model_piece(input.model_piece)) return fail("unsupported Great Wall model piece");
    const auto& context=input.restore_context;
    const bool preview=context.source==GreatWallContextSource::ExplicitPreview &&
        ((context.mode==GreatWallPresentationMode::PreviewRuined && context.material==1) ||
         (context.mode==GreatWallPresentationMode::PreviewEarthen && context.material==2) ||
         (context.mode==GreatWallPresentationMode::PreviewStone && context.material==3));
    if (!preview && !great_wall_original_context_verified(context))
        return fail(context.material ? "Great Wall material has no validated context source":context.reason);
    const auto material = *input.restore_context.material;
    if (material < 1 || material > 3) return fail("unsupported derived Great Wall material");
    const auto camera = input.restore_context.camera_view;
    if (camera > 6 || (camera & 1U)) return fail("unsupported original Great Wall camera view");
    if (input.orientation > 3) return fail("unsupported saved Great Wall orientation");
    if (input.phase == 0) return fail("Great Wall phase zero includes an unimplemented material override branch");
    GreatWallSelection out;
    out.restore_context=context;
    out.evidence=preview ? SelectorEvidence::Preview:SelectorEvidence::Verified;
    const auto& row = input.model_piece;
    out.side = row.side;
    out.effective_view = (row.position + 8U - camera + 2U * input.orientation) % 8U;
    const auto view_column = out.effective_view / 2U;
    if (row.kind == GreatWallPieceKind::Gate) {
        if (input.phase != 1) return fail("unsupported Great Wall gate phase");
        out.group = {0x1001};
        out.variant = material == 1 ? 31U :
            (row.position == 0 ? north_gate : east_gate)[(row.piece - 1U) / 2U][view_column];
    } else if (row.kind == GreatWallPieceKind::Road) {
        // 57d91c/57d98a: the controller reads the raw phase, not material.
        // No Ruined phase-2 normalization or Road registration is evidenced.
        // Compatibility with the inherited group is checked during load.
        if (input.phase != 1 && input.phase != 2) return fail("unsupported Great Wall road phase");
        const bool along_second_axis = out.effective_view == 2 || out.effective_view == 6;
        out.group = {input.phase == 1 ? 0x61eU : 0x1001U};
        out.variant = input.phase == 1 ? unsigned(along_second_axis) : 40U + unsigned(!along_second_axis);
        out.flags = 8;
    } else {
        const bool final_tower = row.kind == GreatWallPieceKind::Tower && input.phase == 11;
        if (input.phase < 1 || (input.phase > 10 && !final_tower)) return fail("unsupported Great Wall wall or tower phase");
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
    out.reason = preview ? "EXE-observed Great Wall piece selector with explicit preview material":
        "EXE-observed Great Wall piece selector with verified original restore context";
    return out;
}

} // namespace openemperor::maps
