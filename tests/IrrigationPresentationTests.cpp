// Independent authored geography and ordinary SDL actions. Scripted acceptance
// is separate from native input and human playtesting.
#include "EconomyStartViewFixture.h"
#include "IrrigationFixture.h"
#include "assets/IrrigationVisualProfile.h"
#include "core/PerformanceDiagnostics.h"
#include <cmath>

namespace {
namespace oe = openemperor;
namespace sim = oe::simulation;
namespace f = oe::testing::economy;
namespace q = oe_irrigation_fixture;
namespace fs = std::filesystem;
using f::check;
using Point = oe::scene::Point;
void event(oe::SandboxView& v, SDL_Event e) {
    bool running = true;
    v.handle_event(e, running);
    check(running, "Input closed view");
}
void key(oe::SandboxView& v, SDL_Keycode code, SDL_Keymod mod = SDL_KMOD_NONE) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = code;
    e.key.mod = mod;
    event(v, e);
}
void center(oe::SandboxView& v, Point world) {
    const auto map = v.layout().map;
    const auto z = v.camera().zoom;
    const Point target{map.x + map.w * .5 - world.x * z, map.y + map.h * .5 - world.y * z};
    for (unsigned n = 0; n < 1000; ++n) {
        const auto now = v.camera().offset;
        const auto dx = target.x - now.x, dy = target.y - now.y;
        if (std::abs(dx) < 1e-6 && std::abs(dy) < 1e-6)
            return;
        oe::SandboxView::PanKeyState held{};
        if (std::abs(dx) > 1e-6) {
            held.left = dx > 0;
            held.right = dx < 0;
            v.update(std::min(.05, std::abs(dx) / 400), held);
        } else {
            held.up = dy > 0;
            held.down = dy < 0;
            v.update(std::min(.05, std::abs(dy) / 400), held);
        }
    }
    throw std::runtime_error("Paused camera did not converge");
}
Point ground(sim::Cell c) {
    return oe::maps::terrain_ground(
        {static_cast<std::uint32_t>(c.x), static_cast<std::uint32_t>(c.y)}, 72);
}
void zoom(oe::SandboxView& view, SDL_Renderer* renderer, double wanted) {
    const auto map = view.layout().map;
    float x = 0, y = 0;
    check(SDL_RenderCoordinatesToWindow(renderer, float(map.x + map.w * .5),
                                        float(map.y + map.h * .5), &x, &y),
          "Zoom input transform");
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.mouse_x = x;
    e.wheel.mouse_y = y;
    e.wheel.y = static_cast<float>(std::log(wanted / view.camera().zoom) / std::log(1.15));
    event(view, e);
    view.update(0);
    check(std::abs(view.camera().zoom - wanted) < 1e-6, "Requested native2x zoom not reached");
}
void pointer(oe::SandboxView& v, SDL_Renderer* r, Point screen, unsigned type) {
    float x = 0, y = 0;
    check(SDL_RenderCoordinatesToWindow(r, float(screen.x), float(screen.y), &x, &y),
          "Input transform");
    SDL_Event e{};
    e.type = type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        e.motion.x = x;
        e.motion.y = y;
    } else {
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = x;
        e.button.y = y;
    }
    event(v, e);
}
void click(oe::SandboxView& v, SDL_Renderer* r, Point p) {
    pointer(v, r, p, SDL_EVENT_MOUSE_MOTION);
    v.update(0);
    pointer(v, r, p, SDL_EVENT_MOUSE_BUTTON_DOWN);
    pointer(v, r, p, SDL_EVENT_MOUSE_BUTTON_UP);
    v.update(0);
}
void action(oe::SandboxView& v, SDL_Renderer* r, oe::sandbox_ui::Action wanted) {
    const auto& buttons = v.layout().buttons;
    const auto found = std::find_if(buttons.begin(), buttons.end(),
                                    [&](const auto& b) { return b.action == wanted; });
    check(found != buttons.end(), "Missing actual irrigation toolbar action");
    click(v, r, {found->rect.x + found->rect.w * .5, found->rect.y + found->rect.h * .5});
}
bool contains(const std::vector<std::string>& lines, std::string_view s) {
    return std::any_of(lines.begin(), lines.end(),
                       [&](const auto& line) { return line.find(s) != std::string::npos; });
}
void capture(oe::SandboxView& v, SDL_Renderer* r, const fs::path& p) {
    int w = 0, h = 0;
    check(SDL_GetCurrentRenderOutputSize(r, &w, &h), "Output dimensions");
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h),
        SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(r, target.get()), "Capture target");
    check(v.render(), "Irrigation frame");
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
        SDL_RenderReadPixels(r, nullptr), SDL_DestroySurface);
    check(surface && SDL_SavePNG(surface.get(), p.string().c_str()), "Private capture");
    check(SDL_SetRenderTarget(r, nullptr), "Restore output");
}
void menu_selection(const fs::path& data, const fs::path& app, SDL_Window* window,
                    SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data, app);
    menu.initialize(window, renderer);
    check(menu.state() == oe::menu::MenuSession::State::MainMenu && menu.render(),
          "Ordinary main menu unavailable");
    check(menu.diagnostic_button_rect("G Agriculture Irrigation (rule 26)").has_value(),
          "Explicit irrigation choice missing from regular main menu");
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = SDLK_G;
    menu.handle_event(e);
    menu.advance();
    check(menu.state() == oe::menu::MenuSession::State::NewSandbox &&
              menu.original_agriculture_setup(),
          "G did not select Agriculture setup");
    check(menu.map_rules_result().request.rule_version == 26,
          "G selected a historical Agriculture rule");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (menu.map_rules_result().status == oe::maps::MapRulesStatus::Checking &&
           std::chrono::steady_clock::now() < deadline) {
        menu.advance();
        std::this_thread::yield();
    }
    check(menu.map_rules_can_start(), "Actual Rule26 preflight did not admit Start");
    e.key.key = SDLK_RETURN;
    menu.handle_event(e);
    menu.advance();
    check(menu.state() == oe::menu::MenuSession::State::Loading,
          "Enter did not reach the ordinary Loading state");
    check(menu.render(), "Actual Rule26 Loading screen failed");
    menu.advance();
    if (menu.state() != oe::menu::MenuSession::State::Playing || !menu.sandbox())
        throw std::runtime_error("Actual Rule26 menu Start failed: " + menu.message());
    check(menu.sandbox()->world().rule_version() == 26 &&
              menu.sandbox()->world().irrigation_enabled(),
          "Regular menu started a historical Agriculture World");
    menu.shutdown();
}
void layouts() {
    using A = oe::sandbox_ui::Action;
    for (const auto size : {std::pair{800, 600}, std::pair{1280, 800}, std::pair{1920, 1080}}) {
        const auto make = [&](bool irrigation) {
            return oe::sandbox_ui::make_layout(size.first, size.second, size.first, size.second,
                                               true, true, true, true, true, true, true, true, true,
                                               true, false, true, false, true, irrigation);
        };
        const auto old = make(false), current = make(true);
        for (const auto wanted : {A::IrrigationDitch, A::IrrigationPump, A::RemoveIrrigationDitch,
                                  A::RotateIrrigationPump, A::IrrigationOverlay}) {
            check(std::none_of(old.buttons.begin(), old.buttons.end(),
                               [&](const auto& b) { return b.action == wanted; }),
                  "Historical toolbar acquired irrigation");
            const auto found = std::find_if(current.buttons.begin(), current.buttons.end(),
                                            [&](const auto& b) { return b.action == wanted; });
            check(found != current.buttons.end() && found->rect.w > 0 && found->rect.h > 0,
                  "Irrigation control hidden");
            check(current.button_at(found->rect.x + found->rect.w * .5,
                                    found->rect.y + found->rect.h * .5) == wanted,
                  "Rendered irrigation control has a different input action");
            check(found->rect.y >= current.toolbar.y &&
                      found->rect.y + found->rect.h <= current.toolbar.y + current.toolbar.h,
                  "Irrigation toolbar overflow");
        }
    }
}
} // namespace
int main(int argc, char** argv) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    try {
        check(argc == 1 || (argc == 3 && std::string_view(argv[1]) == "--real"),
              "Expected optional --real ORIGINAL_DATA_ROOT");
        layouts();
        const auto repo = fs::canonical(fs::path(__FILE__).parent_path().parent_path());
        const auto output =
            repo / ".local/agri-04/presentation" / (argc == 3 ? "real" : "synthetic");
        fs::create_directories(output);
        f::Temp temp;
        const auto local_fixture = output / temp.root.filename();
        fs::rename(temp.root, local_fixture);
        temp.root = fs::canonical(local_fixture);
        const sim::Cell origin{114, 98};
        const auto geometry = oe::original::pump_geometry(origin, 0);
        f::write(temp.root / "data/Cities/A.map", f::map_file(true, true, true, geometry.water));
        fs::create_directories(temp.root / "data/Model");
        q::f::write_text(temp.root / "data/Model/EmperorBuildingModels.txt", q::authored_model());
        if (argc == 3)
            for (const auto name : {"China_General.sg3", "China_General.555"})
                fs::copy_file(fs::canonical(argv[2]) / "DATA" / name,
                              temp.root / "data/DATA" / name);
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        check(SDL_Init(SDL_INIT_VIDEO), "Initialize isolated SDL");
        check(SDL_CreateWindowAndRenderer("Irrigation scripted presentation", 1280, 800,
                                          SDL_WINDOW_RESIZABLE, &window, &renderer),
              "Create renderer");
        menu_selection(temp.root / "data", temp.root / "menu-settings", window, renderer);
        auto session = oe::maps::load_stored_map_session(
            temp.root / "data", "Cities/A.map", oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8, oe::maps::GreatWallPresentationMode::Automatic,
            {}, true);
        auto owner =
            oe::SandboxView::create_original_irrigation(std::move(session), std::nullopt, false);
        auto& view = *owner;
        view.configure_save(temp.root / "data", "Cities/A.map", temp.root / "irrigation.save.json");
        if (argc == 3) {
            const auto pack = repo / "resources/compatibility/gog-derived-2.0.0.2-en-assetset-1";
            view.set_building_visuals(pack / "buildings.json", oe::VisualProfileSource::Builtin);
            view.set_original_building_visuals(pack / "original-buildings.json",
                                               oe::VisualProfileSource::Builtin);
        }
        view.initialize(window, renderer);
        if (argc == 3)
            check(view.irrigation_visuals_active(),
                  "Verified irrigation pixels did not activate in the actual view");
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        event(view, focus);
        check(view.world().irrigation_enabled() && view.world().rule_version() == 26 &&
                  view.paused(),
              "Wrong explicit irrigation mode");
        const auto buy = [&](sim::CommandType type, sim::Cell cell) {
            const auto result = view.execute({type, cell});
            if (!result.accepted || !result.changed)
                throw std::runtime_error("Paid UI scene: " + result.reason);
        };
        for (int x = 96; x <= 121; ++x)
            if (!view.world().road_cell({x, 104}))
                buy(sim::CommandType::PlaceRoad, {x, 104});
        for (int y = 99; y < 104; ++y)
            buy(sim::CommandType::PlaceRoad, {117, y});
        buy(sim::CommandType::PlaceRoad, {116, 99});
        for (int x = 96; x <= 108; x += 3)
            buy(sim::CommandType::PlaceHousehold, {x, 102});
        buy(sim::CommandType::PlaceOriginalWaterWell, {108, 105});
        buy(sim::CommandType::PlaceFireWatch, {112, 105});
        zoom(view, renderer, 2);
        key(view, SDLK_P, SDL_KMOD_SHIFT);
        check(view.tool() == 47 && view.preview(origin).accepted,
              "Paid Pump tool missing valid source orientation");
        key(view, SDLK_R);
        check(!view.preview(origin).accepted, "Rotation bypassed different intake geometry");
        action(view, renderer, oe::sandbox_ui::Action::RotateIrrigationPump);
        key(view, SDLK_R);
        action(view, renderer, oe::sandbox_ui::Action::RotateIrrigationPump);
        check(view.preview(origin).accepted,
              "Four shared rotations did not restore source orientation");
        center(view, ground(origin));
        click(view, renderer, view.camera().world_to_screen(ground(origin)));
        const auto pump = view.world().building_owner_at(origin);
        check(pump && view.world().building(*pump).kind == sim::Object::OriginalIrrigationPump,
              "Actual pointer action did not build paid Pump");
        key(view, SDLK_D, SDL_KMOD_SHIFT);
        check(view.tool() == 46, "Shared keyboard Ditch action missing");
        center(view, ground({114, 100}));
        const auto before = view.world().snapshot();
        pointer(view, renderer, view.camera().world_to_screen(ground({112, 100})),
                SDL_EVENT_MOUSE_BUTTON_DOWN);
        key(view, SDLK_ESCAPE);
        pointer(view, renderer, view.camera().world_to_screen(ground({115, 100})),
                SDL_EVENT_MOUSE_BUTTON_UP);
        check(view.world().snapshot() == before, "Cancelled Ditch drag mutated World");
        const auto funds = view.world().treasury();
        pointer(view, renderer, view.camera().world_to_screen(ground({112, 100})),
                SDL_EVENT_MOUSE_BUTTON_DOWN);
        pointer(view, renderer, view.camera().world_to_screen(ground({115, 100})),
                SDL_EVENT_MOUSE_BUTTON_UP);
        view.update(0);
        check(view.world().irrigation_state()->ditches.size() == 4 &&
                  view.world().treasury() == funds - 12,
              "Actual release did not commit one paid four-cell Ditch batch");
        view.set_tool(5);
        center(view, ground(origin));
        click(view, renderer, view.camera().world_to_screen(ground(origin)));
        check(contains(view.inspection_lines(), "Pump203") &&
                  contains(view.inspection_lines(), "Workers"),
              "Pump inspector hides authority/blockers");
        const auto panel = view.inspector_layout();
        check(panel.detail_y < panel.list_y && panel.detail_row_rect(0).h > 0,
              "Pump status is hidden below the building list");
        capture(view, renderer, output / "irrigation-dry.png");
        bool wet = false;
        for (unsigned step = 0; step < 3500 && !wet; ++step) {
            view.tick_once();
            wet = view.world().irrigation_ditch_wet({112, 100});
        }
        check(wet, "Paid staffed physical scene never supplied its connected Ditches");
        capture(view, renderer, output / "irrigation-wet.png");
        key(view, SDLK_V, SDL_KMOD_SHIFT);
        capture(view, renderer, output / "irrigation-wet-overlay.png");
        const auto snapshot = view.world().snapshot();
        oe::performance::set_enabled(true);
        oe::performance::reset();
        for (unsigned frame = 0; frame < 20; ++frame)
            check(view.render(), "Prepared irrigation frame");
        for (std::size_t i = 0; i < static_cast<std::size_t>(oe::performance::Counter::Count); ++i)
            check(!oe::performance::counter(static_cast<oe::performance::Counter>(i)),
                  "Irrigation render performed World/resource/route work");
        oe::performance::set_enabled(false);
        check(view.world().snapshot() == snapshot, "Irrigation render changed World");
        view.save_now();
        check(oe::persistence::read_save(view.save_path()).source_schema_version == 42,
              "Wrong explicit save generation");
        view.tick_once();
        view.load_now();
        check(view.world().snapshot() == snapshot, "Exact irrigation F9 changed state");
        key(view, SDLK_X, SDL_KMOD_SHIFT);
        center(view, ground({114, 100}));
        click(view, renderer, view.camera().world_to_screen(ground({114, 100})));
        check(view.world().irrigation_state()->ditches.size() == 3 &&
                  !view.world().irrigation_ditch_wet({112, 100}),
              "Physical removal did not dry disconnected section");
        capture(view, renderer, output / "irrigation-cut.png");
        owner.reset();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout
            << "Irrigation explicit mode, regular tools, shared rotations, cancelled/paid drag, "
               "source water, inspector, overlay, removal, pure frames and F5/F9 PASS\n";
        return 0;
    } catch (const std::exception& e) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cerr << e.what() << '\n';
        return 1;
    }
}
