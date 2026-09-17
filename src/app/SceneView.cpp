#include "app/SceneView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace openemperor {

SceneView::SceneView(scene::Scene scene) : renderer_(std::move(scene)) {}
void SceneView::initialize(SDL_Window* window, SDL_Renderer* renderer) {
    window_ = window;
    sdl_renderer_ = renderer;
    if (!SDL_GetCurrentRenderOutputSize(renderer, &camera_.viewport_width, &camera_.viewport_height))
        throw std::runtime_error(SDL_GetError());
    reset_camera();
    renderer_.initialize(renderer);
    update_title();
}
void SceneView::shutdown() { renderer_.shutdown(); sdl_renderer_ = nullptr; window_ = nullptr; }
void SceneView::reset_camera() {
    camera_.zoom = 1.0;
    const auto& scene = renderer_.scene_data();
    camera_.center_on(scene::project({scene.width * 0.5, scene.height * 0.5}));
}
void SceneView::update_title() {
    std::string title = "OpenEmperor — scene";
    if (selected_) title += " — tile (" + std::to_string(selected_->x) + ", " +
                            std::to_string(selected_->y) + ")";
    SDL_SetWindowTitle(window_, title.c_str());
}
void SceneView::handle_event(const SDL_Event& event, bool& running) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
        (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) { running = false; return; }
    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        const scene::Point center = camera_.screen_to_world(
            {camera_.viewport_width * 0.5, camera_.viewport_height * 0.5});
        if (SDL_GetCurrentRenderOutputSize(sdl_renderer_, &camera_.viewport_width,
                                            &camera_.viewport_height)) camera_.center_on(center);
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.key == SDLK_G) grid_ = !grid_;
        if (event.key.key == SDLK_R) reset_camera();
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        float x = event.wheel.mouse_x, y = event.wheel.mouse_y;
        if (SDL_RenderCoordinatesFromWindow(sdl_renderer_, x, y, &x, &y))
            camera_.zoom_at({x, y}, std::pow(1.15, event.wheel.y));
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        float x = event.button.x, y = event.button.y;
        if (SDL_RenderCoordinatesFromWindow(sdl_renderer_, x, y, &x, &y)) {
            selected_ = scene::pick_cell(camera_.screen_to_world({x, y}),
                                         renderer_.scene_data().width, renderer_.scene_data().height);
            update_title();
        }
    }
}
void SceneView::update(double seconds) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const double pixels = 360.0 * std::clamp(seconds, 0.0, 0.05);
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) camera_.offset.x += pixels;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) camera_.offset.x -= pixels;
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) camera_.offset.y += pixels;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) camera_.offset.y -= pixels;
}
bool SceneView::render() {
    int width = 0, height = 0;
    if (!SDL_GetCurrentRenderOutputSize(sdl_renderer_, &width, &height)) return false;
    if (width != camera_.viewport_width || height != camera_.viewport_height) {
        const scene::Point center = camera_.screen_to_world(
            {camera_.viewport_width * 0.5, camera_.viewport_height * 0.5});
        camera_.viewport_width = width;
        camera_.viewport_height = height;
        camera_.center_on(center);
    }
    return renderer_.render(camera_, grid_, selected_);
}

} // namespace openemperor
