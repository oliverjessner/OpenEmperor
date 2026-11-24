#pragma once

#include "scene/IsoProjection.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string_view>

namespace openemperor {

// Opt-in event-boundary observation only. No event is rewritten or replayed.
class InputDiagnostics {
public:
    enum class ActorDomain : std::uint8_t { Courier, Boat, Hunter, Animal };
    struct ActorSelection {
        ActorDomain domain=ActorDomain::Courier;
        // Courier IDs name a reusable movement channel. lifetime_id instead
        // names its actual trip/worker; other domains already use actor IDs.
        std::uint64_t id=0,lifetime_id=0;
        std::optional<std::uint32_t> source,target;
        bool operator==(const ActorSelection&) const=default;
    };
    struct State {
        std::string_view view="other";
        int menu_state=-1,tool=-1,requested_action=-1;
        std::optional<scene::Point> render_position;
        std::optional<scene::Cell> ground_cell;
        std::optional<scene::Cell> selected_cell;
        std::optional<int> ui_action;
        scene::Point camera_offset{};
        double zoom=1;
        SDL_Rect map{};
        std::uint64_t command_sequence=0,tick=0,io_generation=0,save_generation=0,load_generation=0;
        std::uint32_t selected_building=0;
        std::uint32_t selected_walker=0;
        std::optional<ActorSelection> selected_actor;
        bool selected_landscape=false;
        bool ui=false,map_pressed=false,ui_pressed=false,road_drag=false;
        bool paused=false,help=false,budget=false,demolition=false,focused=true;
    };
    static constexpr std::size_t capacity=512;
    static bool relevant(const SDL_Event& event);
    static bool belongs_to_window(const SDL_Event& event,SDL_WindowID window_id);
    static std::optional<scene::Point> window_position(const SDL_Event& event);
    void begin(const SDL_Event& event,SDL_Window* window,SDL_Renderer* renderer,const State& state);
    void end(const State& state);
    void print(std::ostream& out);
    std::size_t size() const { return size_; }
    std::uint64_t overwritten() const { return overwritten_; }
private:
    struct Record {
        std::uint64_t first_sequence=0,last_sequence=0,timestamp=0,observed_ns=0;
        std::uint32_t type=0,window_id=0,observed_window_id=0,motions=0;
        std::optional<scene::Point> raw;
        float relative_x=0,relative_y=0,wheel_x=0,wheel_y=0;
        std::uint8_t button=0;
        int navigation_key=0;
        std::string_view control_key;
        int window_w=0,window_h=0,pixel_w=0,pixel_h=0,output_w=0,output_h=0;
        int logical_w=0,logical_h=0,runtime=0;
        SDL_RendererLogicalPresentation logical_mode=SDL_LOGICAL_PRESENTATION_DISABLED;
        SDL_Rect viewport{};
        float scale_x=1,scale_y=1,density=0,display_scale=0;
        bool dimensions_valid=false,viewport_valid=false,scale_valid=false,has_target=false;
        std::string_view renderer,video_driver;
        State before,after;
    };
    Record pending_{};
    std::array<Record,capacity> records_{};
    std::size_t start_=0,size_=0;
    std::uint64_t sequence_=0,overwritten_=0;
    bool printed_=false;
};
}
