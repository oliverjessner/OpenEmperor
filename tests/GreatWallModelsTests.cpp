#include "maps/GreatWallModels.h"
#include "core/PerformanceDiagnostics.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <tuple>

using namespace openemperor::maps;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string fixture(std::string_view first = "{ 0, 0, SB_GREAT_WALL, 4, NORTH, 24,},", unsigned count = 53) {
    std::string text = "; Synthetic model, no original rows\r\n/* 0 */ ";
    text += first;
    for (unsigned i = 1; i < count; ++i) text += "\n{ 4, -4, SB_GREAT_WALL, 0, NORTH, 0,},";
    text += "\n<0,0,0,0," + std::to_string(count-1) + ",0,1,>\n";
    return text;
}
struct TemporaryRoot {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("openemperor-great-wall-models-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryRoot() { std::filesystem::create_directories(path / "Model"); }
    ~TemporaryRoot() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};
} // namespace

int main() {
    try {
        check(great_wall_model_filename(256) == "Mon_Great_Wall_04_subs.txt" &&
              great_wall_model_filename(257) == "Mon_Great_Wall_05_subs.txt" &&
              great_wall_model_filename(259) == "Mon_Great_Wall_07_subs.txt" &&
              !great_wall_model_filename(258), "stored monument type chooses the bounded registered model");
        const auto parsed = parse_great_wall_model(fixture(),257);
        check(parsed.model && parsed.model->pieces.size() == 53 && parsed.model->phases.size() == 1 &&
              parsed.model->pieces[1].offset_y == -4 && parsed.model->pieces[0].side == 4,
              "ordered local rows retain signed relative coordinates and derived controller size");
        for (const auto [row,kind,side,piece] : {
            std::tuple{"{0,0,SB_GREAT_WALL_GATE,2,EAST,NW,}",GreatWallPieceKind::Gate,2U,7U},
            std::tuple{"{0,0,SB_GREAT_WALL_TOWER,2,NORTH,27,}",GreatWallPieceKind::Tower,4U,27U},
            std::tuple{"{0,0,SB_GREAT_WALL_ROAD,2,EAST,0,}",GreatWallPieceKind::Road,1U,0U}}) {
            const auto result = parse_great_wall_model(fixture(row),256);
            check(result.model && result.model->pieces[0].kind == kind && result.model->pieces[0].side == side &&
                  result.model->pieces[0].piece == piece, "explicit type and direction tokens derive their numerical values");
        }
        check(parse_great_wall_model(fixture("{0,0,SB_GREAT_WALL,0,NORTH,24,}",51),259).model.has_value(),
              "model seven has its distinct complete piece count");
        for (const auto row : {
            "{0,0,SB_UNKNOWN,0,NORTH,0,}", "{0,0,SB_GREAT_WALL,0,NORTH,41,}",
            "{0,0,SB_GREAT_WALL_GATE,0,EAST,garbage,}", "{0,0,SB_GREAT_WALL_GATE,0,EAST,0,}",
            "{0,0,SB_GREAT_WALL,0,north,0,}", "{0,0,SB_GREAT_WALL,0,NORTH,7extra,}",
            "{0,0,SB_GREAT_WALL,6,NORTH,0,}", "{228,0,SB_GREAT_WALL,0,NORTH,0,}",
            "{2147483648,0,SB_GREAT_WALL,0,NORTH,0,}", "{0,0,SB_GREAT_WALL,0,NORTH,0,extra,}"})
            check(!parse_great_wall_model(fixture(row),257).model, "invalid tokens and out-of-domain fields fail atomically");
        check(!parse_great_wall_model(fixture("{0,0,SB_GREAT_WALL,4,NORTH,24,},",52),257).model, "missing model rows are not invented");
        check(!parse_great_wall_model(fixture("{0,0,SB_GREAT_WALL,4,NORTH,24,},",54),257).model, "additional model rows fail the bounded model contract");
        check(!parse_great_wall_model(fixture()+"/*",257).model, "unterminated comments fail closed");
        check(!parse_great_wall_model(fixture()+"unknown",257).model, "unknown trailing syntax is not ignored");
        check(!parse_great_wall_model(std::string(65537,' '),257).model, "model size is bounded before tokenization");
        auto malformed_phase = fixture();
        malformed_phase.replace(malformed_phase.find("<0,0"),2,"<9");
        check(!parse_great_wall_model(malformed_phase,257).model, "unknown phase plan values are rejected");

        TemporaryRoot temp;
        openemperor::performance::set_enabled(true);
        openemperor::performance::reset();
        check(!load_great_wall_model(temp.path,257).model, "a missing dependency is named, without fallback rows");
        check(openemperor::performance::counter(openemperor::performance::Counter::FileReads) == 0,
              "missing model files cause no payload read");
        { std::ofstream file(temp.path / "Model" / "Mon_Great_Wall_05_subs.txt",std::ios::binary); file << fixture(); }
        check(load_great_wall_model(temp.path,257).model.has_value(), "bounded preparation-only read accepts a complete synthetic model");
        check(openemperor::performance::counter(openemperor::performance::Counter::FileReads) == 1,
              "one complete model payload is read once during preparation");
        check(!load_great_wall_model(temp.path,256).model, "stored type cannot borrow another model filename");
        TemporaryRoot outside;
        { std::ofstream file(outside.path / "Model" / "Mon_Great_Wall_05_subs.txt",std::ios::binary); file << fixture(); }
        std::filesystem::create_symlink(outside.path / "Model" / "Mon_Great_Wall_05_subs.txt",
                                       temp.path / "Model" / "Mon_Great_Wall_04_subs.txt");
        openemperor::performance::reset();
        check(!load_great_wall_model(temp.path,256).model,
              "a model file symlink cannot expose bytes outside the selected data root");
        check(openemperor::performance::counter(openemperor::performance::Counter::FileReads) == 0,
              "an escaping model dependency is rejected before its payload is read");
        std::filesystem::remove_all(temp.path / "Model");
        std::filesystem::create_directory_symlink(outside.path / "Model", temp.path / "Model");
        check(!load_great_wall_model(temp.path,257).model &&
              openemperor::performance::counter(openemperor::performance::Counter::FileReads) == 0,
              "an escaping Model directory symlink is also rejected before opening");
        openemperor::performance::set_enabled(false);
        std::cout << "Great Wall models passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
