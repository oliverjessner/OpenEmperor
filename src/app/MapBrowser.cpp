#include "app/MapBrowser.h"

#include "app/MapDebugView.h"
#include "maps/StoredMapSession.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <iostream>
#include <utility>

namespace openemperor {
namespace {
std::string ascii_label(const std::string& original) {
    std::string result;
    result.reserve(original.size());
    for (const char raw:original) {
        const auto ch=static_cast<unsigned char>(raw);
        result.push_back(ch>=32 && ch<127 ? raw : '?');
    }
    return result;
}
} // namespace

MapBrowser::MapBrowser(maps::MapCatalog catalog,maps::FootprintPolicy policy)
    : catalog_(std::move(catalog)),policy_(policy),statuses_(catalog_.entries.size(),"not_checked") {}
MapBrowser::~MapBrowser() { shutdown(); }
void MapBrowser::initialize(SDL_Window* window,SDL_Renderer* renderer) {
    window_=window; renderer_=renderer;
    SDL_SetWindowTitle(window_,"OpenEmperor map browser");
}
void MapBrowser::shutdown() { close_map(); window_=nullptr; renderer_=nullptr; }
void MapBrowser::close_map() {
    if (view_) { view_->shutdown(); view_.reset(); }
    if (window_) SDL_SetWindowTitle(window_,"OpenEmperor map browser");
}
bool MapBrowser::open_selected() {
    if (selected_>=catalog_.entries.size() || !renderer_) return false;
    if (view_) close_map();
    const auto& entry=catalog_.entries[selected_];
    if (!entry.map_profile) {
        statuses_[selected_]=entry.container_valid ? "unsupported_profile" : "load_failed";
        message_=entry.error.empty() ? "unsupported map profile" : entry.error;
        return false;
    }
    try {
        auto session=maps::load_stored_map_session(catalog_.data_root,entry.relative_path,policy_);
        auto view=std::make_unique<MapDebugView>(std::move(session.map),maps::RawLayer::Terrain,
            maps::MapViewMode::StoredGraphics,std::nullopt,std::move(session.plan));
        view->initialize(window_,renderer_);
        view_=std::move(view);
        message_.clear();
        return true;
    } catch (const std::exception& error) {
        statuses_[selected_]="load_failed";
        message_=error.what();
        std::cerr<<"Map browser load failed: "<<entry.relative_path<<": "<<message_<<'\n';
        close_map();
        return false;
    }
}
void MapBrowser::handle_event(const SDL_Event& event,bool& running) {
    if (event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        running=false; return;
    }
    if (view_) {
        if (event.type==SDL_EVENT_KEY_DOWN && event.key.key==SDLK_ESCAPE) {
            close_map(); return;
        }
        try { view_->handle_event(event,running); }
        catch (const std::exception& error) {
            statuses_[selected_]="render_failed";
            message_=error.what(); close_map();
        }
        return;
    }
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        switch (event.key.key) {
        case SDLK_ESCAPE: running=false; return;
        case SDLK_UP: if (selected_>0) --selected_; message_.clear(); return;
        case SDLK_DOWN: if (selected_+1<catalog_.entries.size()) ++selected_; message_.clear(); return;
        case SDLK_PAGEUP: selected_=selected_>15 ? selected_-15 : 0; message_.clear(); return;
        case SDLK_PAGEDOWN:
            if (!catalog_.entries.empty()) selected_=std::min(selected_+15,catalog_.entries.size()-1);
            message_.clear();
            return;
        case SDLK_RETURN: case SDLK_KP_ENTER: open_selected(); return;
        default: break;
        }
    }
    if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button==SDL_BUTTON_LEFT) {
        float x=event.button.x,y=event.button.y;
        if (!SDL_RenderCoordinatesFromWindow(renderer_,x,y,&x,&y)) return;
        if (y>=96.0F) {
            const auto page=selected_/15;
            const auto row=static_cast<std::size_t>((y-96.0F)/32.0F);
            const auto index=page*15+row;
            if (index<catalog_.entries.size() && row<15) {
                selected_=index;
                message_.clear();
                if (event.button.clicks>=2) open_selected();
            }
        }
    }
}
void MapBrowser::update(double seconds) { if (view_) view_->update(seconds); }
bool MapBrowser::render() {
    if (view_) {
        try {
            if (view_->render()) {
                const auto* plan=view_->stored_plan();
                if (plan) statuses_[selected_]=plan->covered_cells()==plan->cells.size() ?
                    "snapshot_complete" : "snapshot_partial";
                return true;
            }
            message_=SDL_GetError();
        } catch (const std::exception& error) { message_=error.what(); }
        statuses_[selected_]="render_failed";
        close_map();
    }
    int width=0,height=0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_,&width,&height) ||
        !SDL_SetRenderDrawColor(renderer_,17,21,28,255) || !SDL_RenderClear(renderer_) ||
        !SDL_SetRenderDrawColor(renderer_,235,240,245,255)) return false;
    const auto heading=std::string{"OPENEMPEROR MAPS | PROFILE "}+maps::stored_graphics_profile+
        " | POLICY "+maps::footprint_policy_name(policy_);
    if (!SDL_RenderDebugText(renderer_,16,12,heading.c_str()) ||
        !SDL_RenderDebugText(renderer_,16,36,"ARROWS / PAGE UP-DOWN SELECT | ENTER OR DOUBLE CLICK OPEN | ESC QUIT")) return false;
    const auto count=std::to_string(catalog_.entries.size())+" MAP FILES | "+
        (catalog_.entries.empty() ? "NO SELECTION" : "SELECTED "+std::to_string(selected_+1));
    if (!SDL_RenderDebugText(renderer_,16,60,count.c_str())) return false;
    const auto first=(selected_/15)*15;
    for (std::size_t row=0;row<15 && first+row<catalog_.entries.size();++row) {
        const auto index=first+row;
        const auto& entry=catalog_.entries[index];
        const auto raw=entry.relative_path.generic_string();
        const auto label=(index==selected_ ? "> " : "  ")+ascii_label(raw.substr(0,70))+
            " | SIZE "+(entry.declared_size ? std::to_string(*entry.declared_size) : "?")+
            " | "+statuses_[index];
        if (!SDL_SetRenderDrawColor(renderer_,index==selected_ ? 255 : 210,
                                   index==selected_ ? 220 : 215,200,255) ||
            !SDL_RenderDebugText(renderer_,16,96+static_cast<float>(row)*32.0F,label.c_str())) return false;
    }
    if (!message_.empty()) {
        const auto clipped=ascii_label(message_.substr(0,115));
        if (!SDL_SetRenderDrawColor(renderer_,255,130,130,255) ||
            !SDL_RenderDebugText(renderer_,16,static_cast<float>(height-36),clipped.c_str())) return false;
    }
    return SDL_RenderPresent(renderer_);
}
} // namespace openemperor
