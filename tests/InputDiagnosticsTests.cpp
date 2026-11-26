#include "app/InputDiagnostics.h"
#include <nlohmann/json.hpp>

#include <cmath>
#include <array>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using openemperor::InputDiagnostics;
using Json=nlohmann::json;
void check(bool okay,const char* message) {
    if (!okay) throw std::runtime_error(message);
}
std::vector<Json> lines(const std::string& text) {
    std::vector<Json> result;std::istringstream input(text);std::string line;
    while (std::getline(input,line)) {
        const auto offset=line.find(' ');check(offset!=std::string::npos,"diagnostic prefix");
        result.push_back(Json::parse(line.substr(offset+1)));
    }
    return result;
}
SDL_Event event(Uint32 type,SDL_Window* window) {
    SDL_Event e{};e.type=type;e.common.timestamp=123456789;
    if (type>=SDL_EVENT_WINDOW_FIRST&&type<=SDL_EVENT_WINDOW_LAST)
        e.window.windowID=SDL_GetWindowID(window);
    else if (type==SDL_EVENT_MOUSE_MOTION) e.motion.windowID=SDL_GetWindowID(window);
    else e.button.windowID=SDL_GetWindowID(window);
    return e;
}
}
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),"dummy hint");
        check(SDL_Init(SDL_INIT_VIDEO),"SDL init");
        window=SDL_CreateWindow("input diagnostics test",800,600,0);check(window,"window");
        renderer=SDL_CreateRenderer(window,"software");check(renderer,"software renderer");
        check(std::string(SDL_GetRendererName(renderer))=="software","actual software");
        const auto window_id=SDL_GetWindowID(window);
        for (const Uint32 type:{SDL_EVENT_MOUSE_MOTION,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP,
            SDL_EVENT_MOUSE_WHEEL,SDL_EVENT_KEY_DOWN,SDL_EVENT_KEY_UP,SDL_EVENT_WINDOW_FOCUS_LOST}) {
            auto e=event(type,window);
            check(InputDiagnostics::relevant(e)&&InputDiagnostics::belongs_to_window(e,window_id),
                "matching app-window event admitted");
            // All these SDL payloads have the same initial windowID field.
            e.window.windowID=window_id+1;
            check(!InputDiagnostics::belongs_to_window(e,window_id),"foreign-window event excluded");
            e.window.windowID=0;
            check(!InputDiagnostics::belongs_to_window(e,window_id),"unknown-window event excluded");
        }
        auto quit=event(SDL_EVENT_QUIT,window);
        check(InputDiagnostics::relevant(quit)&&InputDiagnostics::belongs_to_window(quit,window_id),
            "global Quit admitted without a window ID");
        auto text_event=event(SDL_EVENT_TEXT_INPUT,window);
        check(!InputDiagnostics::belongs_to_window(text_event,window_id),"non-observed text event excluded");
        InputDiagnostics::State before;before.view="sandbox";before.tool=1;
        before.render_position=openemperor::scene::Point{123.5,246.25};
        before.ground_cell=openemperor::scene::Cell{110,114};
        before.map={0,52,496,416};
        InputDiagnostics diagnostic;
        const auto record=[&](const SDL_Event& e,InputDiagnostics::State after) {
            diagnostic.begin(e,window,renderer,before);diagnostic.end(after);
        };
        auto motion=event(SDL_EVENT_MOUSE_MOTION,window);
        for (int i=0;i<1000;++i) {
            motion.motion.x=100.25f+static_cast<float>(i);motion.motion.y=51.5f;
            motion.motion.xrel=.25f;motion.motion.yrel=-.5f;record(motion,before);
        }
        check(diagnostic.size()==1&&diagnostic.overwritten()==0,"consecutive motion coalescing");
        auto down=event(SDL_EVENT_MOUSE_BUTTON_DOWN,window);
        down.button.button=SDL_BUTTON_LEFT;down.button.x=123.5f;down.button.y=246.25f;
        auto pressed=before;pressed.map_pressed=true;pressed.road_drag=true;
        record(down,pressed);
        auto focus=event(SDL_EVENT_WINDOW_FOCUS_LOST,window);auto unfocused=before;unfocused.focused=false;
        record(focus,unfocused);
        std::ostringstream output;diagnostic.print(output);const auto parsed=lines(output.str());
        check(parsed.size()==4&&parsed[0]["records"]==3&&parsed[0]["events"]==1002,"header counts");
        check(parsed[1]["first_sequence"]==1&&parsed[1]["sequence"]==1000&&parsed[1]["motions"]==1000,
            "coalesced first/last motion sequences");
        check(parsed[1]["raw"][0]==1099.25&&parsed[1]["raw"][1]==51.5,"latest fractional raw position");
        check(parsed[2]["type"]==SDL_EVENT_MOUSE_BUTTON_DOWN&&parsed[2]["raw"][0]==123.5&&
            parsed[3]["type"]==SDL_EVENT_WINDOW_FOCUS_LOST,"button and focus retained separately");
        check(parsed[2]["actions"]==Json::array({"gesture"})&&parsed[3]["after"]["focused"]==false,
            "observed gesture/focus state");
        check(parsed[2]["window"]==Json::array({800,600})&&parsed[2]["pixels"]==Json::array({800,600})&&
            parsed[2]["dimensions_valid"]==true&&parsed[2]["renderer"]=="software","measured renderer metadata");
        const auto printed=output.str();diagnostic.print(output);check(output.str()==printed,"stdout emitted once");
        for (const auto type:{SDL_EVENT_TEXT_INPUT,SDL_EVENT_TEXT_EDITING,SDL_EVENT_TEXT_EDITING_CANDIDATES}) {
            auto text=event(type,window);check(!InputDiagnostics::relevant(text),"text excluded from diagnostic routing");
            check(!InputDiagnostics::window_position(text),"text has no positional payload");
        }
        InputDiagnostics ring;
        for (std::size_t i=0;i<InputDiagnostics::capacity+17;++i) {
            auto e=event(i%2 ? SDL_EVENT_MOUSE_BUTTON_UP:SDL_EVENT_MOUSE_BUTTON_DOWN,window);
            e.button.x=static_cast<float>(i)+.5f;ring.begin(e,window,renderer,before);ring.end(before);
        }
        check(ring.size()==512&&ring.overwritten()==17,"fixed capacity ring");
        std::ostringstream bounded;ring.print(bounded);const auto rows=lines(bounded.str());
        check(rows.size()==513&&rows[1]["sequence"]==18&&rows.back()["sequence"]==529,
            "ring chronological retained range");
        InputDiagnostics invalid;
        motion.motion.x=std::numeric_limits<float>::quiet_NaN();
        motion.motion.y=std::numeric_limits<float>::infinity();
        auto invalid_state=before;invalid_state.render_position=openemperor::scene::Point{
            std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()};
        invalid_state.camera_offset.x=std::numeric_limits<double>::infinity();
        invalid_state.zoom=std::numeric_limits<double>::quiet_NaN();
        invalid.begin(motion,window,renderer,invalid_state);invalid.end(invalid_state);
        std::ostringstream safe;invalid.print(safe);const auto nonfinite=lines(safe.str());
        check(nonfinite[1]["raw"][0].is_null()&&nonfinite[1]["raw"][1].is_null()&&
            nonfinite[1]["before"]["render"][0].is_null()&&nonfinite[1]["before"]["zoom"].is_null(),
            "nonfinite floats serialize as valid JSON null");
        auto ordinary_key=event(SDL_EVENT_KEY_DOWN,window);ordinary_key.key.scancode=SDL_SCANCODE_Q;
        ordinary_key.key.key=SDLK_Q;InputDiagnostics keys;
        keys.begin(ordinary_key,window,renderer,before);keys.end(before);
        ordinary_key.key.scancode=SDL_SCANCODE_LEFT;ordinary_key.key.key=SDLK_LEFT;
        keys.begin(ordinary_key,window,renderer,before);keys.end(before);
        std::ostringstream keyboard;keys.print(keyboard);const auto key_rows=lines(keyboard.str());
        check(key_rows[1]["navigation_scancode"]==0&&key_rows[2]["navigation_scancode"]==SDL_SCANCODE_LEFT,
            "only navigation key identity retained");
        check(!key_rows[1].contains("text")&&!key_rows[1].contains("keycode"),"no typed content field");
        check(key_rows[1]["control_key"].is_null(),"ordinary typed key is not a control identity");
        InputDiagnostics controls;
        const std::array<std::pair<SDL_Scancode,const char*>,5> control_cases{{
            {SDL_SCANCODE_F5,"F5"},{SDL_SCANCODE_F9,"F9"},{SDL_SCANCODE_SPACE,"Space"},
            {SDL_SCANCODE_PAGEUP,"PageUp"},{SDL_SCANCODE_PAGEDOWN,"PageDown"}}};
        for (const auto& [scancode,name]:control_cases) {
            (void)name;
            ordinary_key.key.scancode=scancode;
            controls.begin(ordinary_key,window,renderer,before);controls.end(before);
        }
        std::ostringstream control_output;controls.print(control_output);const auto control_rows=lines(control_output.str());
        for (std::size_t i=0;i<control_cases.size();++i)
            check(control_rows[i+1]["control_key"]==control_cases[i].second &&
                !control_rows[i+1].contains("keycode") && !control_rows[i+1].contains("text"),
                "only whitelisted control identity is exported");

        InputDiagnostics actors;
        const std::array<InputDiagnostics::ActorDomain,4> domains{{InputDiagnostics::ActorDomain::Courier,
            InputDiagnostics::ActorDomain::Boat,InputDiagnostics::ActorDomain::Hunter,InputDiagnostics::ActorDomain::Animal}};
        const std::array<const char*,4> domain_names{{"courier","boat","hunter","animal"}};
        constexpr std::uint64_t lifetime=(std::uint64_t{1}<<40U)+3;
        auto selected=before;selected.selected_cell=openemperor::scene::Cell{110,114};
        for (const auto domain:domains) {
            auto after=selected;
            after.selected_actor=InputDiagnostics::ActorSelection{domain,7,lifetime,42,46};
            if (domain==InputDiagnostics::ActorDomain::Animal) {
                after.selected_actor->source.reset();after.selected_actor->target.reset();
            }
            actors.begin(down,window,renderer,selected);actors.end(after);
        }
        std::ostringstream actor_output;actors.print(actor_output);const auto actor_rows=lines(actor_output.str());
        for (std::size_t i=0;i<domains.size();++i) {
            const auto& row=actor_rows[i+1];const auto& actor=row["after"]["selected_actor"];
            check(row["before"]["selected_actor"].is_null() && actor["domain"]==domain_names[i] &&
                actor["id"]==7 && actor["lifetime_id"]==lifetime &&
                actor["id_kind"]==(i==0 ? "courier_channel":"actor") && row["actions"]==Json::array({"selection"}),
                "same-cell typed actor selection retains distinct channel and64-bit lifetime");
            check(i==3 ? actor["source"].is_null() && actor["target"].is_null():
                actor["source"]==42 && actor["target"]==46,"cached actor source/target or unowned wildlife");
        }
        InputDiagnostics reuse;
        selected.selected_actor=InputDiagnostics::ActorSelection{InputDiagnostics::ActorDomain::Courier,7,lifetime,42,46};
        auto successor=selected;++successor.selected_actor->lifetime_id;
        reuse.begin(down,window,renderer,selected);reuse.end(successor);
        auto cleared=selected;cleared.selected_actor.reset();
        reuse.begin(down,window,renderer,selected);reuse.end(cleared);
        std::ostringstream reuse_output;reuse.print(reuse_output);const auto reuse_rows=lines(reuse_output.str());
        check(reuse_rows[1]["actions"]==Json::array({"selection"}) &&
            reuse_rows[1]["before"]["selected_actor"]["id"]==reuse_rows[1]["after"]["selected_actor"]["id"] &&
            reuse_rows[1]["after"]["selected_actor"]["lifetime_id"]==lifetime+1 &&
            reuse_rows[2]["actions"]==Json::array({"selection"}) && reuse_rows[2]["after"]["selected_actor"].is_null(),
            "channel reuse and selection retirement are observable without changing cells");

        InputDiagnostics persistence;
        auto attempted=before;attempted.io_generation=1;
        persistence.begin(down,window,renderer,before);persistence.end(attempted);
        auto saved=attempted;saved.save_generation=1;
        persistence.begin(down,window,renderer,before);persistence.end(saved);
        auto loaded=saved;++loaded.io_generation;++loaded.load_generation;
        persistence.begin(down,window,renderer,saved);persistence.end(loaded);
        std::ostringstream persistence_output;persistence.print(persistence_output);const auto io_rows=lines(persistence_output.str());
        check(io_rows[1]["actions"]==Json::array({"persistence_attempt"}) &&
            io_rows[2]["actions"]==Json::array({"persistence_attempt","save"}) &&
            io_rows[3]["actions"]==Json::array({"persistence_attempt","load"}) &&
            io_rows[3]["after"]["io_generation"]==2 && io_rows[3]["after"]["save_generation"]==1 &&
            io_rows[3]["after"]["load_generation"]==1,
            "failed attempts and successful same-state save/load remain distinct");
        SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();
        std::cout<<"bounded input diagnostics checks passed\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        if(renderer)SDL_DestroyRenderer(renderer);
        if(window)SDL_DestroyWindow(window);
        SDL_Quit();return 1;
    }
}
