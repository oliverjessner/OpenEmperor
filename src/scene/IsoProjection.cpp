#include "scene/IsoProjection.h"

#include <algorithm>
#include <cmath>

namespace openemperor::scene {

Point project(Point tile) { return {(tile.x - tile.y) * 40.0, (tile.x + tile.y) * 20.0}; }
Point unproject(Point world) { return {world.x / 80.0 + world.y / 40.0,
                                       world.y / 40.0 - world.x / 80.0}; }
std::optional<Cell> pick_cell(Point world, int width, int height) {
    const Point tile = unproject(world);
    if (!std::isfinite(tile.x) || !std::isfinite(tile.y) || tile.x < 0 || tile.y < 0 ||
        tile.x >= width || tile.y >= height) return std::nullopt;
    return Cell{static_cast<int>(std::floor(tile.x)), static_cast<int>(std::floor(tile.y))};
}
Point Camera2D::world_to_screen(Point world) const {
    return {world.x * zoom + offset.x, world.y * zoom + offset.y};
}
Point Camera2D::screen_to_world(Point screen) const {
    return {(screen.x - offset.x) / zoom, (screen.y - offset.y) / zoom};
}
void Camera2D::zoom_at(Point screen, double factor) {
    if (!std::isfinite(factor) || factor <= 0) return;
    const Point before = screen_to_world(screen);
    zoom = std::clamp(zoom * factor, 0.5, 4.0);
    offset = {screen.x - before.x * zoom, screen.y - before.y * zoom};
}
void Camera2D::center_on(Point world) {
    offset = {viewport_width * 0.5 - world.x * zoom,
              viewport_height * 0.5 - world.y * zoom};
}

} // namespace openemperor::scene
