#include "app/AssetBrowser.h"

#include "assets/Sg3ImageLoader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace openemperor {
namespace {

constexpr std::size_t columns = 5;
constexpr std::size_t rows = 4;
constexpr std::size_t page_size = columns * rows;
constexpr std::size_t max_cache_entries = 64;
constexpr std::uint64_t max_cache_bytes = 64U * 1024U * 1024U;
constexpr std::uint64_t max_single_texture_bytes = 16U * 1024U * 1024U;

std::string short_text(const std::string& text, std::size_t max_length) {
    return text.size() <= max_length ? text : text.substr(0, max_length - 3U) + "...";
}

} // namespace

AssetBrowser::AssetBrowser(assets::AssetCatalog catalog, bool ignore_alpha,
                           std::optional<assets::Sg3ImageKind> initial_kind)
    : catalog_(std::move(catalog)), ignore_alpha_(ignore_alpha), kind_(initial_kind) {
    rebuild_visible();
}

AssetBrowser::~AssetBrowser() {
    shutdown();
}

void AssetBrowser::initialize(SDL_Window* window, SDL_Renderer* renderer) {
    window_ = window;
    renderer_ = renderer;
    update_title();
}

void AssetBrowser::rebuild_visible() {
    visible_.clear();
    // Zero-sized/empty records remain in the CLI catalog but have no thumbnail.
    for (std::size_t index = 0; index < catalog_.records.size(); ++index) {
        const auto& record = catalog_.records[index];
        if (record.width <= 0 || record.height <= 0 || record.data_length == 0) continue;
        if (kind_ && record.image_kind != *kind_) continue;
        if (!show_all_candidates_ &&
            (!record.color_decoder_supported || !record.payload_in_bounds)) continue;
        if (!show_all_candidates_ && !ignore_alpha_ &&
            (!record.decoder_supported ||
             (record.alpha_length != 0 && record.alpha_bounds != assets::AssetRangeStatus::InBounds))) continue;
        visible_.push_back(index);
    }
    selected_ = 0;
    detail_ = false;
    update_title();
}

void AssetBrowser::update_title() {
    if (window_ == nullptr) return;
    std::string title = "OpenEmperor Assets";
    if (!visible_.empty()) {
        const auto& record = catalog_.records[visible_[selected_]];
        title += " — " + record.id.archive_relative_path.filename().string() +
                 " #" + std::to_string(record.id.image_index) +
                 " — Type " + std::to_string(record.image_type) +
                 " — " + std::to_string(record.width) + "x" + std::to_string(record.height);
        if (detail_) {
            title += " — group " + std::to_string(record.group_id) +
                     " — alpha " + (record.alpha_length != 0 ? "yes" : "no") +
                     " — policy " + (ignore_alpha_ ? "ignored_diagnostic" :
                         assets::alpha_policy_name(record.alpha_policy)) +
                     " — effective alpha " + (!ignore_alpha_ && record.effective_alpha_offset
                         ? std::to_string(*record.effective_alpha_offset) : "none") +
                     " — mirror " + std::to_string(record.horizontal_mirror_offset);
        }
    }
    SDL_SetWindowTitle(window_, title.c_str());
}

void AssetBrowser::enter_detail() {
    if (visible_.empty()) return;
    detail_ = true;
    const auto& record = catalog_.records[visible_[selected_]];
    std::cout << "Asset " << record.id.archive_relative_path.generic_string()
              << '#' << record.id.image_index
              << " kind=" << assets::sg3_image_kind_name(record.image_kind)
              << " type=" << record.image_type
              << " dimensions=" << record.width << 'x' << record.height
              << " group=" << static_cast<unsigned int>(record.group_id)
              << " alpha=" << (record.alpha_length != 0 ? "yes" : "no")
              << " alpha_policy=" << (ignore_alpha_ ? "ignored_diagnostic" :
                  assets::alpha_policy_name(record.alpha_policy))
              << " effective_alpha_offset=" << (!ignore_alpha_ && record.effective_alpha_offset
                  ? std::to_string(*record.effective_alpha_offset) : "none")
              << " mirror_offset=" << record.horizontal_mirror_offset << '\n';
    update_title();
}

void AssetBrowser::handle_event(const SDL_Event& event, bool& running) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        running = false;
        return;
    }
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;
    const SDL_Keycode key = event.key.key;
    if (key == SDLK_ESCAPE) {
        if (detail_) { detail_ = false; update_title(); }
        else running = false;
        return;
    }
    if (key == SDLK_0 || key == SDLK_1 || key == SDLK_2 || key == SDLK_3) {
        if (key == SDLK_0) kind_.reset();
        else if (key == SDLK_1) kind_ = assets::Sg3ImageKind::Plain;
        else if (key == SDLK_2) kind_ = assets::Sg3ImageKind::Sprite;
        else kind_ = assets::Sg3ImageKind::Isometric;
        rebuild_visible();
        return;
    }
    if (key == SDLK_4) {
        show_all_candidates_ = !show_all_candidates_;
        rebuild_visible();
        return;
    }
    if (visible_.empty()) return;
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) { enter_detail(); return; }
    if (detail_) return;
    const std::size_t previous = selected_;
    if (key == SDLK_LEFT || key == SDLK_A) {
        if (selected_ != 0) --selected_;
    } else if (key == SDLK_RIGHT || key == SDLK_D) {
        if (selected_ + 1U < visible_.size()) ++selected_;
    } else if (key == SDLK_UP || key == SDLK_W) {
        selected_ = selected_ >= columns ? selected_ - columns : 0;
    } else if (key == SDLK_DOWN || key == SDLK_S) {
        selected_ = std::min(visible_.size() - 1U, selected_ + columns);
    } else if (key == SDLK_PAGEUP) {
        selected_ = selected_ >= page_size ? selected_ - page_size : 0;
    } else if (key == SDLK_PAGEDOWN) {
        selected_ = std::min(visible_.size() - 1U, selected_ + page_size);
    }
    if (selected_ != previous) update_title();
}

void AssetBrowser::evict_for(std::uint64_t bytes) {
    while (!cache_.empty() &&
           (cache_.size() >= max_cache_entries || cache_bytes_ > max_cache_bytes - bytes)) {
        const auto oldest = std::min_element(cache_.begin(), cache_.end(),
            [](const auto& left, const auto& right) {
                return left.second.last_used < right.second.last_used;
            });
        cache_bytes_ -= oldest->second.bytes;
        if (oldest->second.texture != nullptr) SDL_DestroyTexture(oldest->second.texture);
        cache_.erase(oldest);
    }
}

AssetBrowser::CacheEntry& AssetBrowser::cached(std::size_t record_index) {
    auto found = cache_.find(record_index);
    if (found != cache_.end()) {
        found->second.last_used = ++tick_;
        return found->second;
    }
    ++cache_misses_;
    auto& record = catalog_.records[record_index];
    CacheEntry entry;
    entry.last_used = ++tick_;
    const bool first_attempt = !record.decode_attempted;
    record.decode_attempted = true;
    if (first_attempt) ++decode_attempts_;
    try {
        if (!record.color_decoder_supported) {
            throw std::runtime_error("color decoder does not support this record's metadata");
        }
        const std::uint64_t width = static_cast<std::uint64_t>(record.width);
        const std::uint64_t height = static_cast<std::uint64_t>(record.height);
        if (width * height * 4U > max_single_texture_bytes) {
            throw std::runtime_error("image exceeds browser texture limit (16 MiB)");
        }
        const auto rgba = assets::load_sg3_image({
            catalog_.data_root / record.id.archive_relative_path,
            record.id.image_index, ignore_alpha_});
        const std::uint64_t bytes = static_cast<std::uint64_t>(rgba.width) * rgba.height * 4U;
        if (bytes > max_single_texture_bytes ||
            static_cast<std::uint64_t>(rgba.width) * 4U >
                static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("decoded image exceeds browser texture limit");
        }
        evict_for(bytes);
        entry.texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STATIC, rgba.width, rgba.height);
        if (entry.texture == nullptr ||
            !SDL_UpdateTexture(entry.texture, nullptr, rgba.pixels.data(),
                               static_cast<int>(rgba.width) * 4) ||
            !SDL_SetTextureBlendMode(entry.texture, SDL_BLENDMODE_BLEND) ||
            !SDL_SetTextureScaleMode(entry.texture, SDL_SCALEMODE_NEAREST)) {
            throw std::runtime_error(std::string{"SDL texture failed: "} + SDL_GetError());
        }
        entry.width = rgba.width;
        entry.height = rgba.height;
        entry.bytes = bytes;
        record.decode_succeeded = true;
    } catch (const std::exception& failure) {
        if (entry.texture != nullptr) {
            SDL_DestroyTexture(entry.texture);
            entry.texture = nullptr;
        }
        entry.error = failure.what();
        record.decode_error = entry.error;
        record.decode_succeeded = false;
        if (first_attempt) ++decode_failures_;
    }
    evict_for(entry.bytes);
    cache_bytes_ += entry.bytes;
    auto [inserted, success] = cache_.emplace(record_index, std::move(entry));
    (void)success;
    cache_peak_entries_ = std::max(cache_peak_entries_, cache_.size());
    cache_peak_bytes_ = std::max(cache_peak_bytes_, cache_bytes_);
    return inserted->second;
}

bool AssetBrowser::draw_texture(const CacheEntry& entry, const SDL_FRect& area) {
    if (entry.texture == nullptr || entry.width == 0 || entry.height == 0) return true;
    const float scale = std::min(area.w / static_cast<float>(entry.width),
                                 area.h / static_cast<float>(entry.height));
    const float draw_width = static_cast<float>(entry.width) * scale;
    const float draw_height = static_cast<float>(entry.height) * scale;
    const SDL_FRect destination{area.x + (area.w - draw_width) / 2.0F,
                                area.y + (area.h - draw_height) / 2.0F,
                                draw_width, draw_height};
    return SDL_RenderTexture(renderer_, entry.texture, nullptr, &destination);
}

bool AssetBrowser::render_grid() {
    int width = 0;
    int height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_, &width, &height) ||
        !SDL_SetRenderDrawColor(renderer_, 22, 29, 43, 255) || !SDL_RenderClear(renderer_)) {
        return false;
    }
    const std::string heading = "Assets " + std::to_string(visible_.size()) +
        " | 0 all, 1 plain, 2 sprite, 3 iso, 4 " +
        (show_all_candidates_ ? "ready" : "all candidates") +
        " | arrows/WASD, PgUp/PgDn, Enter, Esc";
    if (!SDL_SetRenderDrawColor(renderer_, 230, 236, 247, 255) ||
        !SDL_RenderDebugText(renderer_, 16.0F, 12.0F, heading.c_str())) return false;
    if (visible_.empty()) {
        return SDL_RenderDebugText(renderer_, 16.0F, 48.0F, "No images match this view") &&
               SDL_RenderPresent(renderer_);
    }
    const float cell_width = static_cast<float>(width - 32) / static_cast<float>(columns);
    const float cell_height = static_cast<float>(height - 100) / static_cast<float>(rows);
    const std::size_t page_start = (selected_ / page_size) * page_size;
    for (std::size_t slot = 0; slot < page_size && page_start + slot < visible_.size(); ++slot) {
        const std::size_t record_index = visible_[page_start + slot];
        const auto& record = catalog_.records[record_index];
        const float x = 16.0F + static_cast<float>(slot % columns) * cell_width;
        const float y = 44.0F + static_cast<float>(slot / columns) * cell_height;
        const SDL_FRect frame{x + 2.0F, y + 2.0F, cell_width - 6.0F, cell_height - 6.0F};
        const bool selected = page_start + slot == selected_;
        if (!SDL_SetRenderDrawColor(renderer_, selected ? 51 : 32,
                                    selected ? 76 : 43, selected ? 110 : 61, 255) ||
            !SDL_RenderFillRect(renderer_, &frame) ||
            !SDL_SetRenderDrawColor(renderer_, selected ? 130 : 65,
                                    selected ? 180 : 79, selected ? 240 : 103, 255) ||
            !SDL_RenderRect(renderer_, &frame)) return false;
        const CacheEntry& entry = cached(record_index);
        if (entry.texture != nullptr) {
            const SDL_FRect area{frame.x + 8.0F, frame.y + 8.0F,
                                 frame.w - 16.0F, frame.h - 38.0F};
            if (!draw_texture(entry, area)) return false;
        } else {
            if (!SDL_SetRenderDrawColor(renderer_, 230, 110, 105, 255) ||
                !SDL_RenderDebugText(renderer_, frame.x + 10.0F, frame.y + 18.0F,
                                     "DECODE FAILED")) return false;
        }
        const std::string label = "#" + std::to_string(record.id.image_index) +
            " T" + std::to_string(record.image_type) + " " +
            std::to_string(record.width) + "x" + std::to_string(record.height);
        if (!SDL_SetRenderDrawColor(renderer_, 230, 236, 247, 255) ||
            !SDL_RenderDebugText(renderer_, frame.x + 8.0F, frame.y + frame.h - 20.0F,
                                 short_text(label, 24).c_str())) return false;
    }
    const std::string footer = "Page " + std::to_string(page_start / page_size + 1U) +
        "/" + std::to_string((visible_.size() + page_size - 1U) / page_size) +
        " | decode attempts " + std::to_string(decode_attempts_) +
        " | failures " + std::to_string(decode_failures_) +
        " | cache " + std::to_string(cache_.size()) + "/64";
    return SDL_SetRenderDrawColor(renderer_, 230, 236, 247, 255) &&
           SDL_RenderDebugText(renderer_, 16.0F, static_cast<float>(height - 29), footer.c_str()) &&
           SDL_RenderPresent(renderer_);
}

bool AssetBrowser::render_detail() {
    if (visible_.empty()) { detail_ = false; return render_grid(); }
    int width = 0;
    int height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_, &width, &height) ||
        !SDL_SetRenderDrawColor(renderer_, 22, 29, 43, 255) || !SDL_RenderClear(renderer_)) {
        return false;
    }
    const std::size_t record_index = visible_[selected_];
    const auto& record = catalog_.records[record_index];
    const std::string heading = record.id.archive_relative_path.generic_string() +
        " #" + std::to_string(record.id.image_index) +
        " | " + assets::sg3_image_kind_name(record.image_kind) +
        " T" + std::to_string(record.image_type) +
        " | " + std::to_string(record.width) + "x" + std::to_string(record.height);
    const std::string details = "Group " + std::to_string(record.group_id) +
        " | alpha " + (record.alpha_length != 0 ? "yes" : "no") +
        " | policy " + (ignore_alpha_ ? "ignored_diagnostic" :
            assets::alpha_policy_name(record.alpha_policy)) +
        " | offset " + (!ignore_alpha_ && record.effective_alpha_offset
            ? std::to_string(*record.effective_alpha_offset) : "none") +
        " | mirror offset " + std::to_string(record.horizontal_mirror_offset) +
        " | Esc grid";
    if (!SDL_SetRenderDrawColor(renderer_, 230, 236, 247, 255) ||
        !SDL_RenderDebugText(renderer_, 16.0F, 12.0F, short_text(heading, 115).c_str()) ||
        !SDL_RenderDebugText(renderer_, 16.0F, 31.0F, details.c_str())) return false;
    const CacheEntry& entry = cached(record_index);
    if (entry.texture != nullptr) {
        const SDL_FRect area{30.0F, 66.0F, static_cast<float>(width - 60),
                             static_cast<float>(height - 95)};
        if (!draw_texture(entry, area)) return false;
    } else if (!SDL_SetRenderDrawColor(renderer_, 230, 110, 105, 255) ||
               !SDL_RenderDebugText(renderer_, 30.0F, 94.0F,
                                    short_text("Decode failed: " + entry.error, 110).c_str())) {
        return false;
    }
    return SDL_RenderPresent(renderer_);
}

bool AssetBrowser::render() {
    return detail_ ? render_detail() : render_grid();
}

void AssetBrowser::shutdown() {
    for (auto& [index, entry] : cache_) {
        (void)index;
        if (entry.texture != nullptr) SDL_DestroyTexture(entry.texture);
    }
    cache_.clear();
    cache_bytes_ = 0;
    renderer_ = nullptr;
    window_ = nullptr;
}

} // namespace openemperor
