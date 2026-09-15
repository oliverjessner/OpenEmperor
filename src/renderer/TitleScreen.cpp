#include "renderer/TitleScreen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <string_view>

namespace openemperor {
namespace {

using Glyph = std::array<std::string_view, 7>;

constexpr Glyph glyph_for(char letter) {
    switch (letter) {
    case 'O': return {"01110", "10001", "10001", "10001", "10001", "10001", "01110"};
    case 'E': return {"11111", "10000", "10000", "11110", "10000", "10000", "11111"};
    case 'p': return {"00000", "00000", "11110", "10001", "11110", "10000", "10000"};
    case 'e': return {"00000", "00000", "01110", "10001", "11111", "10000", "01111"};
    case 'n': return {"00000", "00000", "11110", "10001", "10001", "10001", "10001"};
    case 'm': return {"00000", "00000", "11010", "10101", "10101", "10101", "10101"};
    case 'r': return {"00000", "00000", "10110", "11001", "10000", "10000", "10000"};
    case 'o': return {"00000", "00000", "01110", "10001", "10001", "10001", "01110"};
    default: return {"00000", "00000", "00000", "00000", "00000", "00000", "00000"};
    }
}

bool draw_title(SDL_Renderer* renderer, int width, int height) {
    constexpr std::string_view title = "OpenEmperor";
    constexpr int glyph_columns = 5;
    constexpr int glyph_rows = 7;
    constexpr int advance = glyph_columns + 1;
    const int cells_wide = static_cast<int>(title.size()) * advance - 1;
    const int scale = std::max(1, std::min({8, width / (cells_wide + 4), height / (glyph_rows + 4)}));
    const float left = static_cast<float>((width - cells_wide * scale) / 2);
    const float top = static_cast<float>((height - glyph_rows * scale) / 2);

    if (!SDL_SetRenderDrawColor(renderer, 232, 234, 240, 255)) {
        return false;
    }
    for (std::size_t letter_index = 0; letter_index < title.size(); ++letter_index) {
        const Glyph glyph = glyph_for(title[letter_index]);
        for (int row = 0; row < glyph_rows; ++row) {
            for (int column = 0; column < glyph_columns; ++column) {
                if (glyph[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] != '1') {
                    continue;
                }
                const SDL_FRect pixel{
                    left + static_cast<float>((static_cast<int>(letter_index) * advance + column) * scale),
                    top + static_cast<float>(row * scale),
                    static_cast<float>(scale),
                    static_cast<float>(scale),
                };
                if (!SDL_RenderFillRect(renderer, &pixel)) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace

bool render_title_screen(SDL_Renderer* renderer) {
    int width = 0;
    int height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer, &width, &height) ||
        !SDL_SetRenderDrawColor(renderer, 22, 29, 43, 255) ||
        !SDL_RenderClear(renderer) ||
        !draw_title(renderer, width, height)) {
        return false;
    }
    return SDL_RenderPresent(renderer);
}

} // namespace openemperor
