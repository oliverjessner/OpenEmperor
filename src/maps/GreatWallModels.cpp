#include "maps/GreatWallModels.h"
#include "core/PerformanceDiagnostics.h"

#include <array>
#include <charconv>
#include <fstream>
#include <utility>

namespace openemperor::maps {
namespace {
constexpr std::size_t max_model_bytes = 64U * 1024U;
constexpr std::size_t max_phase_rows = 128U;

struct Tokens {
    std::string_view text;
    std::size_t cursor = 0;
    bool invalid = false;

    std::string_view next() {
        while (cursor < text.size()) {
            const char c = text[cursor];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { ++cursor; continue; }
            if (c == ';') {
                const auto end = text.find('\n', cursor);
                cursor = end == std::string_view::npos ? text.size() : end + 1U;
                continue;
            }
            if (text.substr(cursor, 2) == "/*") {
                const auto end = text.find("*/", cursor + 2U);
                if (end == std::string_view::npos) { invalid = true; return {}; }
                cursor = end + 2U;
                continue;
            }
            break;
        }
        if (cursor == text.size()) return {};
        const auto begin = cursor++;
        if (text[begin] == '{' || text[begin] == '}' || text[begin] == '<' ||
            text[begin] == '>' || text[begin] == ',') return text.substr(begin, 1);
        while (cursor < text.size()) {
            const char c = text[cursor];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' ||
                c == '{' || c == '}' || c == '<' || c == '>' || c == ';') break;
            ++cursor;
        }
        return text.substr(begin, cursor - begin);
    }
};

bool integer(std::string_view token, std::int32_t& value) {
    if (token.empty()) return false;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    return error == std::errc{} && end == token.data() + token.size();
}

template<std::size_t N>
bool columns(Tokens& tokens, std::array<std::string_view, N>& values, std::string_view close) {
    for (std::size_t i = 0; i < N; ++i) {
        values[i] = tokens.next();
        if (values[i].empty() || values[i] == "," || values[i] == close) return false;
        if (i + 1U < N && tokens.next() != ",") return false;
    }
    auto tail = tokens.next();
    if (tail == ",") tail = tokens.next();
    return tail == close && !tokens.invalid;
}

std::optional<unsigned> direction(std::string_view token) {
    constexpr std::array<std::string_view, 9> names{
        "NORTH", "NE", "EAST", "SE", "SOUTH", "SW", "WEST", "NW", "CENTER"};
    for (unsigned i = 0; i < names.size(); ++i) if (token == names[i]) return i;
    return {};
}

GreatWallModelResult failure(const char* reason) { return {{}, reason}; }

std::size_t expected_piece_count(std::int16_t type) { return type == 259 ? 51U : 53U; }

bool inside(const std::filesystem::path& root, const std::filesystem::path& path) {
    const auto relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return false;
    for (const auto& component : relative) if (component == "..") return false;
    return true;
}
} // namespace

std::optional<std::string_view> great_wall_model_filename(std::int16_t type) {
    switch (type) {
    case 256: return "Mon_Great_Wall_04_subs.txt";
    case 257: return "Mon_Great_Wall_05_subs.txt";
    case 259: return "Mon_Great_Wall_07_subs.txt";
    default: return {};
    }
}

unsigned great_wall_piece_side(GreatWallPieceKind kind) {
    switch (kind) {
    case GreatWallPieceKind::Wall: case GreatWallPieceKind::Tower: return 4;
    case GreatWallPieceKind::Gate: return 2;
    case GreatWallPieceKind::Road: return 1;
    }
    return 0;
}

bool valid_great_wall_model_piece(const GreatWallModelPiece& row) {
    if (row.offset_x < -227 || row.offset_x > 227 || row.offset_y < -227 || row.offset_y > 227 ||
        row.elevation < 0 || row.elevation > 5 || row.side == 0 ||
        row.side != great_wall_piece_side(row.kind)) return false;
    switch (row.kind) {
    case GreatWallPieceKind::Wall: return row.position == 0 && row.piece <= 25;
    case GreatWallPieceKind::Tower: return row.position == 0 && (row.piece == 26 || row.piece == 27);
    case GreatWallPieceKind::Gate:
        return (row.position == 0 || row.position == 2) &&
            (row.piece == 1 || row.piece == 3 || row.piece == 5 || row.piece == 7);
    case GreatWallPieceKind::Road: return (row.position == 0 || row.position == 2) && row.piece == 0;
    }
    return false;
}

GreatWallModelResult parse_great_wall_model(std::string_view text, std::int16_t type) {
    if (!great_wall_model_filename(type)) return failure("unsupported stored Great Wall monument type");
    if (text.empty() || text.size() > max_model_bytes) return failure("Great Wall model text size is outside bounds");
    GreatWallModel model;
    model.monument_type = type;
    Tokens tokens{text};
    bool saw_phase = false;
    bool piece_separator_allowed = false;
    for (;;) {
        const auto token = tokens.next();
        if (token.empty()) break;
        if (token == "," && piece_separator_allowed) {
            piece_separator_allowed = false;
            continue;
        }
        piece_separator_allowed = false;
        if (token == "{") {
            if (saw_phase) return failure("Great Wall model piece appears after phase rows");
            if (model.pieces.size() >= expected_piece_count(type)) return failure("too many Great Wall model pieces");
            std::array<std::string_view, 6> fields;
            if (!columns(tokens, fields, "}")) return failure("malformed Great Wall model piece row");
            GreatWallModelPiece row;
            if (!integer(fields[0], row.offset_x) || !integer(fields[1], row.offset_y) ||
                !integer(fields[3], row.elevation)) return failure("invalid Great Wall model piece integer");
            if (fields[2] == "SB_GREAT_WALL") row.kind = GreatWallPieceKind::Wall;
            else if (fields[2] == "SB_GREAT_WALL_GATE") row.kind = GreatWallPieceKind::Gate;
            else if (fields[2] == "SB_GREAT_WALL_TOWER") row.kind = GreatWallPieceKind::Tower;
            else if (fields[2] == "SB_GREAT_WALL_ROAD") row.kind = GreatWallPieceKind::Road;
            else return failure("unknown Great Wall model piece type token");
            const auto position = direction(fields[4]);
            if (!position) return failure("unknown Great Wall model position token");
            row.position = *position;
            if (const auto ent = direction(fields[5])) row.piece = *ent;
            else {
                std::int32_t numeric_piece = -1;
                if (!integer(fields[5], numeric_piece) || numeric_piece < 0) return failure("unknown Great Wall model piece token");
                row.piece = static_cast<unsigned>(numeric_piece);
            }
            row.side = great_wall_piece_side(row.kind);
            if (!valid_great_wall_model_piece(row)) return failure("unsupported Great Wall model piece values");
            model.pieces.push_back(row);
            piece_separator_allowed = true;
        } else if (token == "<") {
            saw_phase = true;
            if (model.phases.size() >= max_phase_rows) return failure("too many Great Wall model phase rows");
            std::array<std::string_view, 7> fields;
            if (!columns(tokens, fields, ">")) return failure("malformed Great Wall model phase row");
            std::array<std::int32_t, 7> values;
            for (unsigned i = 0; i < values.size(); ++i)
                if (!integer(fields[i], values[i]) || values[i] < 0) return failure("invalid Great Wall model phase integer");
            if (values[0] > 8 || values[1] > 1 || values[2] != 0 || values[3] > values[4] ||
                static_cast<std::size_t>(values[4]) >= model.pieces.size() || values[5] >= values[6] || values[6] > 11)
                return failure("unsupported Great Wall model phase values");
            model.phases.push_back({static_cast<unsigned>(values[0]), static_cast<unsigned>(values[1]),
                static_cast<unsigned>(values[2]), static_cast<unsigned>(values[3]), static_cast<unsigned>(values[4]),
                static_cast<unsigned>(values[5]), static_cast<unsigned>(values[6])});
        } else return failure("unknown Great Wall model syntax");
    }
    if (tokens.invalid) return failure("unterminated Great Wall model comment");
    if (model.pieces.size() != expected_piece_count(type) || model.phases.empty())
        return failure("incomplete Great Wall model");
    return {std::move(model), "validated local Great Wall model rows"};
}

GreatWallModelResult load_great_wall_model(const std::filesystem::path& root, std::int16_t type) {
    const auto filename = great_wall_model_filename(type);
    if (!filename) return failure("unsupported stored Great Wall monument type");
    std::error_code error;
    const auto canonical_root = std::filesystem::canonical(root, error);
    if (error || !std::filesystem::is_directory(canonical_root, error) || error)
        return failure("Great Wall model data root cannot be resolved");
    const auto canonical_path = std::filesystem::canonical(canonical_root / "Model" / *filename, error);
    if (error || !inside(canonical_root, canonical_path) ||
        !std::filesystem::is_regular_file(canonical_path, error) || error)
        return failure("required Great Wall model file is missing or escapes data root");
    std::ifstream file(canonical_path, std::ios::binary | std::ios::ate);
    if (!file) return failure("required local Great Wall model file is missing");
    const auto end = file.tellg();
    if (end <= 0 || end > static_cast<std::streamoff>(max_model_bytes))
        return failure("Great Wall model file size is outside bounds");
    std::string text(static_cast<std::size_t>(end), '\0');
    file.seekg(0);
    performance::increment(performance::Counter::FileReads);
    if (!file.read(text.data(), static_cast<std::streamsize>(text.size())))
        return failure("Great Wall model file read failed");
    return parse_great_wall_model(text, type);
}

} // namespace openemperor::maps
