#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openemperor::maps {

// Controller-array indices observed at 0x85b2d0, not original Building types.
enum class GreatWallPieceKind : std::uint8_t { Wall = 14, Gate = 15, Tower = 16, Road = 17 };

struct GreatWallModelPiece {
    std::int32_t offset_x = 0, offset_y = 0;
    GreatWallPieceKind kind = GreatWallPieceKind::Wall;
    std::int32_t elevation = 0;
    unsigned position = 0; // Original NORTH=0 or EAST=2.
    unsigned piece = 0; // Numeric wall piece, or original gate direction.
    unsigned side = 0;
};

struct GreatWallModelPhase {
    unsigned monument_phase = 0, list_type = 0, list_number = 0;
    unsigned first_piece = 0, last_piece = 0, begin_phase = 0, end_phase = 0;
};

struct GreatWallModel {
    std::int16_t monument_type = 0;
    std::vector<GreatWallModelPiece> pieces;
    std::vector<GreatWallModelPhase> phases;
};

struct GreatWallModelResult {
    std::optional<GreatWallModel> model;
    std::string reason;
};

// Registration 0x564976..0x5649bd, bounded to types in the examined corpus.
// The map's stored type chooses the model; a filename or map title never does.
std::optional<std::string_view> great_wall_model_filename(std::int16_t monument_type);
unsigned great_wall_piece_side(GreatWallPieceKind kind);
bool valid_great_wall_model_piece(const GreatWallModelPiece& piece);

// Independent strict parser for the observed six-column piece rows and
// seven-column phase rows. Original model rows are read locally, never embedded.
GreatWallModelResult parse_great_wall_model(std::string_view text, std::int16_t monument_type);

// Preparation-only bounded read, outside per-frame render/inspect/pick.
// Resolve the root and required regular file before opening; an escaping
// symlink is rejected. The owning load plan calls this once per needed type.
GreatWallModelResult load_great_wall_model(
    const std::filesystem::path& data_root, std::int16_t monument_type);

} // namespace openemperor::maps
