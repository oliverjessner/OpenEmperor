#include "maps/GreatWallSelector.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace openemperor::maps;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
GreatWallSelectorInput input(GreatWallPieceKind kind, unsigned piece, std::int32_t phase,
                            unsigned position = 0, std::int32_t material = 3) {
    return {{0,0,kind,4,position,piece,great_wall_piece_side(kind)}, phase, 0,
        {{material},0,GreatWallContextSource::VerifiedOriginal}};
}
} // namespace

int main() {
    try {
        for (const auto mode:{GreatWallPresentationMode::Automatic,GreatWallPresentationMode::HistoricalFallback,
            GreatWallPresentationMode::PreviewRuined,GreatWallPresentationMode::PreviewEarthen,GreatWallPresentationMode::PreviewStone})
            check(parse_great_wall_presentation_mode(great_wall_presentation_mode_name(mode))==mode,
                "canonical presentation mode names round trip without aliases");
        check(!parse_great_wall_presentation_mode("stone") && !parse_great_wall_presentation_mode("preview-Stone"),
            "implicit material and unknown mode names are rejected");
        GreatWallOriginalRestoreInputs original;
        check(!resolve_original_great_wall_context(original).material,
            "missing original mode cannot default to ruined or editor mode");
        original.mode=0;
        check(!resolve_original_great_wall_context(original).material,
            "known non-editor mode with missing current-player goals stays unavailable");
        original.current_player_goals=std::vector<GreatWallOriginalGoal>{};
        check(!resolve_original_great_wall_context(original).material,
            "a present but unvalidated goal list does not establish original context");
        original.goals_validated=true;
        const auto empty_goals=resolve_original_great_wall_context(original);
        check(empty_goals.material==1 && great_wall_original_context_verified(empty_goals),
            "validated empty original goals prove the original no-match ruined branch");
        original.current_player_goals=std::vector<GreatWallOriginalGoal>{{1,86},{2,85},{2,86}};
        check(resolve_original_great_wall_context(original).material==2,
            "first matching type-two goal wins instead of preferring stone across the list");
        original.current_player_goals=std::vector<GreatWallOriginalGoal>{{2,86},{2,85}};
        check(resolve_original_great_wall_context(original).material==3,
            "original goal order also admits a first stone goal");
        original.current_player_goals=std::vector<GreatWallOriginalGoal>{{1,85},{2,84}};
        check(resolve_original_great_wall_context(original).material==1,
            "validated irrelevant goals follow the original no-match branch");
        original.mode=1;original.current_player_goals.reset();original.goals_validated=false;
        const auto editor=resolve_original_great_wall_context(original);
        check(editor.material==3 && great_wall_original_context_verified(editor),
            "known original mode one selects stone without consulting unavailable goals");
        original.mode.reset();original.goals_validated=true;
        original.current_player_goals=std::vector<GreatWallOriginalGoal>{{2,86}};
        check(!resolve_original_great_wall_context(original).material,
            "goal content never fills missing original mode");
        check(great_wall_context_from_mode(GreatWallPresentationMode::Automatic,editor).source==GreatWallContextSource::VerifiedOriginal &&
            !great_wall_context_from_mode(GreatWallPresentationMode::HistoricalFallback,editor).material,
            "automatic preserves independently validated input while explicit historical mode disables selection");
        for (const auto mode:{GreatWallPresentationMode::PreviewRuined,GreatWallPresentationMode::PreviewEarthen,GreatWallPresentationMode::PreviewStone}) {
            const auto context=great_wall_context_from_mode(mode);
            auto preview=input(GreatWallPieceKind::Wall,24,10);preview.restore_context=context;
            const auto choice=select_great_wall(preview);
            check(choice.supported && choice.evidence==SelectorEvidence::Preview &&
                choice.restore_context.source==GreatWallContextSource::ExplicitPreview &&
                choice.restore_context.mode==mode && !great_wall_original_context_verified(context),
                "each explicit material mode selects a preview with its original source preserved");
            check(!great_wall_context_from_mode(GreatWallPresentationMode::Automatic,context).material,
                "preview material cannot become verified original authority through automatic mode");
        }
        const auto invalid=great_wall_context_from_mode(static_cast<GreatWallPresentationMode>(255));
        check(!invalid.material && invalid.source==GreatWallContextSource::Unavailable &&
            std::string_view(great_wall_presentation_mode_name(invalid.mode))=="invalid",
            "invalid presentation enum fails closed without a guessed material");
        auto source_missing=input(GreatWallPieceKind::Wall,24,10);
        source_missing.restore_context={3,0};
        check(!select_great_wall(source_missing).supported,
            "a bare material aggregate cannot bypass mandatory provenance source");
        auto wall = input(GreatWallPieceKind::Wall, 24, 10);
        auto selected = select_great_wall(wall);
        check(selected.supported && selected.group.value == 0x1001 && selected.variant == 24 &&
              selected.side == 4 && selected.required_registration &&
              selected.required_registration->archive_basename == "China_Mon_Greatwall_10",
              "saved wall phase ten requires its actual archive, independently of historical slot eight IDs");
        wall.phase = 1; wall.restore_context.material = 2;
        selected = select_great_wall(wall);
        check(selected.required_registration->archive_basename == "China_Mon_Earthen_Greatwall_1",
              "earthen phase one registration is explicit");
        wall.phase = 2;
        selected = select_great_wall(wall);
        check(selected.slot == 9 && selected.group.value == 0x1201 &&
              selected.required_registration->archive_basename == "China_Mon_Earthen_Greatwall_2",
              "phase two uses the independent slot nine group");
        wall.phase = 3;
        selected = select_great_wall(wall);
        check(selected.slot == 10 && selected.group.value == 0x1401,
              "phase three does not resolve through slot eight");
        wall.phase = 10; wall.restore_context.material = 1;
        check(select_great_wall(wall).required_registration->archive_basename == "China_Mon_Greatwall_Ruined",
              "derived material one selects ruined independently of stored phase");

        for (const auto piece : {26U,27U}) {
            auto tower = input(GreatWallPieceKind::Tower,piece,10);
            check(select_great_wall(tower).variant == (piece == 26 ? 25U : 24U),
                  "tower phase ten uses the remapped wall piece");
            tower.phase = 11;
            check(select_great_wall(tower).variant == piece,
                  "tower phase eleven retains the separate completed tower variant");
            tower.restore_context.camera_view = 2;
            check(select_great_wall(tower).variant == 53U-piece,
                  "tower perpendicular view swaps only twenty six and twenty seven");
        }
        auto slope = input(GreatWallPieceKind::Wall,2,10);
        constexpr std::array<unsigned,4> slope_camera_variants{2,17,7,12};
        for (unsigned camera = 0; camera <= 6; camera += 2) {
            slope.restore_context.camera_view = camera;
            check(select_great_wall(slope).variant == slope_camera_variants[camera/2],
                  "camera subtraction selects the observed slope orientation");
        }
        slope.restore_context.camera_view = 2; slope.orientation = 1;
        check(select_great_wall(slope).effective_view == 0 && select_great_wall(slope).variant == 2,
              "saved quarter orientation and camera rotation combine before piece selection");

        constexpr std::array<unsigned,4> gate_ent{7,1,5,3};
        constexpr std::array<unsigned,4> gate_camera_zero{31,28,30,29};
        for (unsigned i = 0; i < gate_ent.size(); ++i) {
            const auto gate = select_great_wall(input(GreatWallPieceKind::Gate,gate_ent[i],1,2));
            check(gate.supported && gate.effective_view == 2 && gate.group.value == 0x1001 &&
                  gate.variant == gate_camera_zero[i] && gate.side == 2 && !gate.required_registration,
                  "east gates at camera zero inherit slot eight and select their exact directional part");
        }
        const auto ruined_gate = select_great_wall(input(GreatWallPieceKind::Gate,7,1,2,1));
        check(ruined_gate.supported && ruined_gate.variant == 31 && !ruined_gate.required_registration,
              "derived ruined gate material follows the dedicated variant thirty one branch");
        const auto temporary_road = select_great_wall(input(GreatWallPieceKind::Road,0,1,2));
        check(temporary_road.supported && temporary_road.slot == 3 && temporary_road.group.value == 0x61e &&
              temporary_road.variant == 1 && temporary_road.flags == 8 && !temporary_road.required_registration,
              "MP wall phase one roads use the terrain road group without registering a monument archive");
        const auto final_road = select_great_wall(input(GreatWallPieceKind::Road,0,2,2));
        check(final_road.supported && final_road.variant == 40 && final_road.group.value == 0x1001 &&
              !final_road.required_registration,
              "Badaling and Handan phase two roads inherit the current monument archive");

        auto unresolved = input(GreatWallPieceKind::Wall,0,10);
        unresolved.restore_context.material.reset();
        check(!select_great_wall(unresolved).supported &&
              !select_great_wall(unresolved).restore_context.material,
              "serialized material cannot fill a missing restore context");
        unresolved.restore_context.material = 4;
        check(!select_great_wall(unresolved).supported, "unknown material is not defaulted");
        unresolved.restore_context.material = 3; unresolved.phase = 0;
        check(!select_great_wall(unresolved).supported, "unimplemented phase zero overrides remain unsupported");
        unresolved.phase = 11;
        check(!select_great_wall(unresolved).supported, "wall cannot borrow the tower phase eleven branch");
        unresolved.phase = 10; unresolved.orientation = 255;
        check(!select_great_wall(unresolved).supported, "unknown orientation is not normalized");
        unresolved.orientation = 0; unresolved.restore_context.camera_view = 1;
        check(!select_great_wall(unresolved).supported, "odd camera view fails closed");
        unresolved.restore_context.camera_view = 0; unresolved.model_piece.piece = 41;
        check(!select_great_wall(unresolved).supported, "the forty two record group is not a blanket wall piece domain");
        unresolved.model_piece.piece = 0; unresolved.model_piece.side = 2;
        check(!select_great_wall(unresolved).supported, "model claim size must match the observed controller");
        check(!select_great_wall(input(GreatWallPieceKind::Gate,7,2,2)).supported,
              "gate phases are validated independently of wall phases");
        std::cout << "Great Wall selector passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
