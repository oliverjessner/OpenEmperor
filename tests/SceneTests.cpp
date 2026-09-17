#include "scene/Scene.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using namespace openemperor::scene;
void u16(std::vector<std::uint8_t>& v, std::size_t p, std::uint16_t x) {
    v[p] = static_cast<std::uint8_t>(x); v[p + 1] = static_cast<std::uint8_t>(x >> 8);
}
void u32(std::vector<std::uint8_t>& v, std::size_t p, std::uint32_t x) {
    u16(v, p, static_cast<std::uint16_t>(x)); u16(v, p + 2, static_cast<std::uint16_t>(x >> 16));
}
void write(const fs::path& p, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out{p, std::ios::binary};
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
void write_text(const fs::path& p, const std::string& s) { std::ofstream{p} << s; }
bool close(double a, double b) { return std::abs(a - b) < 1e-8; }
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template <class F> void rejects(F f) { try { f(); } catch (const std::exception&) { return; } throw std::runtime_error("expected rejection"); }
} // namespace

int main() {
    try {
        check(close(project({2, 1}).x, 40) && close(project({2, 1}).y, 60), "projection reference");
        const auto inverse = unproject({40, 60});
        check(close(inverse.x, 2) && close(inverse.y, 1), "inverse reference");
        check(pick_cell(project({2.5, 1.5}), 4, 4) == Cell{2, 1}, "cell center");
        check(pick_cell(project({2, 1}), 4, 4) == Cell{2, 1}, "boundary to positive side");
        check(!pick_cell(project({-0.2, 0.5}), 4, 4), "negative selection");
        Camera2D camera;
        camera.viewport_width = 800; camera.viewport_height = 600;
        camera.center_on({40, 60});
        const auto screen = camera.world_to_screen({67, -24});
        const auto before = camera.screen_to_world(screen);
        camera.zoom_at(screen, 1.5);
        const auto after = camera.screen_to_world(screen);
        check(close(before.x, after.x) && close(before.y, after.y), "zoom point fixed");
        check(close(camera.world_to_screen({67, -24}).x, screen.x), "camera inverse");
        Asset one; one.width = 78; one.height = 40; one.anchor = {39, 0};
        const auto a = image_origin({0, 0}, one);
        const auto b = image_origin({1, 0}, one);
        check(close(a.x, -39) && close(a.y, 0) && close(b.x, 1) && close(b.y, 20), "1x1 alignment");
        Asset two; two.width = 158; two.height = 120; two.footprint = 2; two.anchor = {79, 40};
        const auto c = image_origin({2, 3}, two);
        check(close(c.x, -119) && close(c.y, 60), "2x2 tall anchor");
        Scene sort_scene; sort_scene.assets["small"] = one; sort_scene.assets["large"] = two;
        sort_scene.objects = {{"z", "small", {2, 2}}, {"a", "small", {2, 2}}, {"front", "large", {1, 1}}};
        const auto ordered = sorted_objects(sort_scene);
        check(ordered[0].id == "a" && ordered[1].id == "front" && ordered[2].id == "z", "stable ordering");

        const auto root = fs::temp_directory_path() / "openemperor-scene-tests";
        fs::remove_all(root); fs::create_directories(root / "data");
        std::vector<std::uint8_t> sg3(40680 + 72, 0);
        u32(sg3, 0, static_cast<std::uint32_t>(sg3.size())); u32(sg3, 4, 214);
        u32(sg3, 12, 1); u32(sg3, 16, 1); u32(sg3, 20, 1); u32(sg3, 680 + 124, 1);
        u32(sg3, 40680, 0); u32(sg3, 40684, 3200); u32(sg3, 40688, 3200);
        u16(sg3, 40680 + 20, 78); u16(sg3, 40680 + 22, 40);
        u16(sg3, 40680 + 50, 30);
        write(root / "data" / "tile.sg3", sg3);
        write(root / "data" / "tile.555", std::vector<std::uint8_t>(3200, 0));
        const std::string base = R"({"schema_version":1,"grid":{"width":4,"height":4,"cell_width":80,"cell_height":40},"assets":{"ground_test":{"sg3":"tile.sg3","image_index":0,"anchor":[39,0],"footprint":1}},"terrain":{"default":"ground_test"},"objects":[{"id":"a","asset":"ground_test","x":1,"y":1}]})";
        write_text(root / "scene.json", base);
        check(load_scene(root / "data", root / "scene.json").objects.size() == 1, "valid manifest");
        auto invalid = base; invalid.replace(invalid.find("\"x\":1"), 5, "\"x\":4");
        write_text(root / "scene.json", invalid);
        rejects([&] { load_scene(root / "data", root / "scene.json"); });
        invalid = base; invalid.replace(invalid.find("tile.sg3"), 8, "../x.sg3");
        write_text(root / "scene.json", invalid);
        rejects([&] { load_scene(root / "data", root / "scene.json"); });
        write_text(root / "scene.json", base);
        fs::remove(root / "data" / "tile.555");
        fs::create_symlink(root / "outside.555", root / "data" / "tile.555");
        rejects([&] { load_scene(root / "data", root / "scene.json"); });
        fs::remove_all(root);
        std::cout << "scene math and manifest checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
