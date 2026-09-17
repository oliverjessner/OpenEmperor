#include "app/AssetBrowser.h"
#include "assets/AssetCatalog.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
constexpr std::size_t image_table = 40680;

void u16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

void u32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    u16(bytes, at, static_cast<std::uint16_t>(value));
    u16(bytes, at + 2U, static_cast<std::uint16_t>(value >> 16U));
}

void write(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output{path, std::ios::binary};
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("cannot write synthetic browser fixture");
}

class TempDirectory {
public:
    TempDirectory() {
        path = fs::temp_directory_path() /
            ("openemperor-browser-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(path);
    }
    ~TempDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    fs::path path;
};

void make_fixture(const fs::path& root) {
    constexpr std::uint32_t count = 101;
    std::vector<std::uint8_t> sg3(image_table + count * 72U, 0);
    u32(sg3, 0, static_cast<std::uint32_t>(sg3.size()));
    u32(sg3, 4, 214);
    u32(sg3, 12, count);
    u32(sg3, 16, count);
    u32(sg3, 20, 1);
    u32(sg3, 680 + 124, count);
    u32(sg3, 680 + 132, count - 1U);
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::size_t at = image_table + static_cast<std::size_t>(index) * 72U;
        u32(sg3, at, 4U + index * 8U);
        u32(sg3, at + 4U, 8);
        u16(sg3, at + 20U, 2);
        u16(sg3, at + 22U, 2);
        u16(sg3, at + 50U, index == count - 1U ? 999 : 13);
    }
    write(root / "many.sg3", sg3);
    std::vector<std::uint8_t> bitmap(4U + count * 8U, 0);
    for (std::size_t at = 4; at < bitmap.size(); at += 2U) {
        bitmap[at] = 0x1f; // Independent synthetic red RGB555 pixel.
    }
    write(root / "many.555", bitmap);
}

bool check_alpha_browser(const fs::path& root) {
    fs::create_directory(root);
    std::vector<std::uint8_t> sg3(image_table + 2U * 72U, 0);
    u32(sg3, 0, static_cast<std::uint32_t>(sg3.size()));
    u32(sg3, 4, 214);
    u32(sg3, 12, 2);
    u32(sg3, 16, 2);
    u32(sg3, 20, 1);
    u32(sg3, 680 + 124, 2);
    u32(sg3, 680 + 132, 1);
    for (std::size_t index = 0; index < 2; ++index) {
        const std::size_t at = image_table + index * 72U;
        u32(sg3, at, 4);
        u32(sg3, at + 4, 3);
        u16(sg3, at + 20, 1);
        u16(sg3, at + 22, 1);
        u16(sg3, at + 50, 256);
        u32(sg3, at + 64, index == 0 ? 10 : 0);
        u32(sg3, at + 68, 2);
    }
    write(root / "alpha.sg3", sg3);
    write(root / "alpha.555", std::vector<std::uint8_t>{0, 0, 0, 0, 1, 0x1f, 0, 1, 31});
    const auto catalog = openemperor::assets::scan_asset_catalog(root);
    if (catalog.records.size() != 2 || !catalog.records[0].decoder_supported ||
        catalog.records[1].decoder_supported) return false;
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Alpha browser test", 800, 600,
                                     SDL_WINDOW_HIDDEN, &window, &renderer)) {
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }
    openemperor::AssetBrowser browser{catalog, false};
    browser.initialize(window, renderer);
    bool passed = browser.render() && browser.decode_attempts() == 1 &&
                  browser.decode_failures() == 0;
    bool running = true;
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_4;
    browser.handle_event(event, running);
    passed = passed && browser.render() && browser.decode_attempts() == 2 &&
             browser.decode_failures() == 1;
    browser.shutdown();
    openemperor::AssetBrowser color_only{catalog, true};
    color_only.initialize(window, renderer);
    passed = passed && color_only.render() && color_only.decode_attempts() == 2 &&
             color_only.decode_failures() == 0;
    color_only.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return passed;
}

bool run_checks(const fs::path& root) {
    make_fixture(root);
    auto catalog = openemperor::assets::scan_asset_catalog(root);
    if (catalog.records.size() != 101) return false;
    openemperor::AssetBrowser browser{std::move(catalog), false};
    if (browser.decode_attempts() != 0 || browser.cache_misses() != 0) return false;
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Asset browser test", 800, 600,
                                     SDL_WINDOW_HIDDEN, &window, &renderer)) {
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }
    browser.initialize(window, renderer);
    bool passed = browser.render() && browser.decode_attempts() == 20 &&
                  browser.decode_failures() == 0 && browser.cache_misses() == 20;
    bool running = true;
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_PAGEDOWN;
    for (int page = 0; page < 4 && passed; ++page) {
        browser.handle_event(event, running);
        passed = running && browser.render();
    }
    passed = passed && browser.decode_attempts() == 100 &&
             browser.decode_failures() == 0 && browser.cache_misses() == 100 &&
             browser.cache_peak_entries() <= 64 && browser.cache_peak_entries() == 64 &&
             browser.cache_peak_bytes() <= 64U * 1024U * 1024U;
    event.key.key = SDLK_PAGEUP;
    for (int page = 0; page < 4 && passed; ++page) browser.handle_event(event, running);
    passed = passed && browser.render() && browser.decode_attempts() == 100 &&
             browser.cache_misses() > 100;
    // The all-candidates view must survive an unsupported record and show its placeholder.
    event.key.key = SDLK_4;
    browser.handle_event(event, running);
    event.key.key = SDLK_PAGEDOWN;
    for (int page = 0; page < 5 && passed; ++page) browser.handle_event(event, running);
    passed = passed && browser.render() && browser.decode_attempts() == 101 &&
             browser.decode_failures() == 1 && running;
    browser.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return passed && check_alpha_browser(root / "alpha-browser");
}

} // namespace

int main() {
    try {
        const TempDirectory temp;
        if (!run_checks(temp.path)) {
            std::cerr << "Asset browser/cache checks failed\n";
            return 1;
        }
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "Asset browser test setup failed: " << failure.what() << '\n';
        return 1;
    }
}
