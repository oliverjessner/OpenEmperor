#pragma once

#include "maps/MapCatalog.h"
#include "maps/StoredGraphicsPlan.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace openemperor {
class MapDebugView;

class MapBrowser {
public:
    MapBrowser(maps::MapCatalog catalog,maps::FootprintPolicy policy,
               maps::StoredGraphicsProfile profile = maps::StoredGraphicsProfile::Base);
    ~MapBrowser();
    void initialize(SDL_Window* window,SDL_Renderer* renderer);
    void shutdown();
    void handle_event(const SDL_Event& event,bool& running);
    void update(double seconds);
    bool render();
    std::size_t selected_index() const { return selected_; }
    bool map_open() const { return static_cast<bool>(view_); }
    const std::vector<std::string>& statuses() const { return statuses_; }
    bool open_selected();
    void close_map();
private:
    maps::MapCatalog catalog_;
    maps::FootprintPolicy policy_;
    maps::StoredGraphicsProfile profile_;
    std::vector<std::string> statuses_;
    std::size_t selected_=0;
    std::string message_;
    std::unique_ptr<MapDebugView> view_;
    SDL_Window* window_=nullptr;
    SDL_Renderer* renderer_=nullptr;
};
} // namespace openemperor
