#include "renderer/ImagePreview.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace openemperor {

bool render_image_preview(SDL_Renderer* renderer, SDL_Texture* texture,
                          std::uint16_t image_width, std::uint16_t image_height) {
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height) ||
        !SDL_SetRenderDrawColor(renderer, 22, 29, 43, 255) ||
        !SDL_RenderClear(renderer)) {
        return false;
    }
    const float available_width = static_cast<float>(std::max(1, output_width - 32));
    const float available_height = static_cast<float>(std::max(1, output_height - 32));
    const float max_scale = std::min(available_width / image_width, available_height / image_height);
    const float scale = max_scale >= 1.0F ? std::max(1.0F, static_cast<float>(static_cast<int>(max_scale)))
                                          : max_scale;
    const float draw_width = image_width * scale;
    const float draw_height = image_height * scale;
    const SDL_FRect destination{
        (static_cast<float>(output_width) - draw_width) / 2.0F,
        (static_cast<float>(output_height) - draw_height) / 2.0F,
        draw_width, draw_height,
    };
    return SDL_RenderTexture(renderer, texture, nullptr, &destination) && SDL_RenderPresent(renderer);
}

} // namespace openemperor
