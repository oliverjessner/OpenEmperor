#pragma once

#include <optional>

namespace openemperor::scene {

struct Point { double x = 0; double y = 0; };
struct Cell { int x = 0; int y = 0; bool operator==(const Cell&) const = default; };

Point project(Point tile);
Point unproject(Point world);
std::optional<Cell> pick_cell(Point world, int width, int height);

class Camera2D {
public:
    double zoom = 1.0;
    Point offset{};
    int viewport_width = 1;
    int viewport_height = 1;
    Point world_to_screen(Point world) const;
    Point screen_to_world(Point screen) const;
    void zoom_at(Point screen, double factor);
    void center_on(Point world);
};

} // namespace openemperor::scene
