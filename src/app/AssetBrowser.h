#pragma once

#include "assets/AssetCatalog.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace openemperor {

struct RoadAtlasRange {
    std::filesystem::path archive_relative_path;
    std::uint32_t start = 0;
    std::uint32_t count = 0;
};

class AssetBrowser {
public:
    AssetBrowser(assets::AssetCatalog catalog, bool ignore_alpha,
                 std::optional<assets::Sg3ImageKind> initial_kind = std::nullopt,
                 std::optional<RoadAtlasRange> road_atlas = std::nullopt);
    AssetBrowser(const AssetBrowser&) = delete;
    AssetBrowser& operator=(const AssetBrowser&) = delete;
    ~AssetBrowser();

    void initialize(SDL_Window* window, SDL_Renderer* renderer);
    void handle_event(const SDL_Event& event, bool& running);
    bool render();
    void shutdown();
    std::size_t decode_attempts() const { return decode_attempts_; }
    std::size_t decode_failures() const { return decode_failures_; }
    std::size_t cache_misses() const { return cache_misses_; }
    std::size_t cache_peak_entries() const { return cache_peak_entries_; }
    std::uint64_t cache_peak_bytes() const { return cache_peak_bytes_; }
    std::size_t visible_count() const { return visible_.size(); }

private:
    struct CacheEntry {
        SDL_Texture* texture = nullptr;
        std::uint16_t width = 0;
        std::uint16_t height = 0;
        std::uint64_t bytes = 0;
        std::uint64_t last_used = 0;
        std::string error;
    };

    void rebuild_visible();
    void update_title();
    CacheEntry& cached(std::size_t record_index);
    void evict_for(std::uint64_t bytes);
    bool render_grid();
    bool render_detail();
    bool draw_texture(const CacheEntry& entry, const SDL_FRect& area);
    void enter_detail();

    assets::AssetCatalog catalog_;
    bool ignore_alpha_ = false;
    std::optional<assets::Sg3ImageKind> kind_;
    std::optional<RoadAtlasRange> road_atlas_;
    bool show_all_candidates_ = false;
    std::vector<std::size_t> visible_;
    std::size_t selected_ = 0;
    bool detail_ = false;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::size_t, CacheEntry> cache_;
    std::uint64_t tick_ = 0;
    std::uint64_t cache_bytes_ = 0;
    std::size_t cache_peak_entries_ = 0;
    std::uint64_t cache_peak_bytes_ = 0;
    std::size_t decode_attempts_ = 0;
    std::size_t decode_failures_ = 0;
    std::size_t cache_misses_ = 0;
};

} // namespace openemperor
