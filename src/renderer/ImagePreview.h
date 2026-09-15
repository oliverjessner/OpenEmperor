#pragma once

#include <cstdint>

struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {

bool render_image_preview(SDL_Renderer* renderer, SDL_Texture* texture,
                          std::uint16_t image_width, std::uint16_t image_height);

} // namespace openemperor
