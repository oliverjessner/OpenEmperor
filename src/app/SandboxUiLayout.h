#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::sandbox_ui {

struct Rect {
    int x=0,y=0,w=0,h=0;
    bool contains(double px,double py) const {
        return px>=x && py>=y && px<x+w && py<y+h;
    }
};

enum class Action {
    Select, Road, Clay, Pottery, Warehouse, RemoveRoad, Household, Farm, ServicePost,
    Market, Pause, Step, Speed1, Speed2, Speed4, Reset, Save, Load, TogglePanel, ToggleHelp,
    FireWatch, Desirability, Well, Water, HealthPost, Health
};

struct Button { Action action; Rect rect; bool enabled=true; };

struct Layout {
    Rect map,top,toolbar,status,panel,panel_toggle;
    std::vector<Button> buttons;
    bool panel_open=true;
    int scale=1;
    std::optional<Action> button_at(double x,double y) const;
    bool ui_at(double x,double y) const;
};

Layout make_layout(int output_width,int output_height,int window_width,int window_height,
                   bool panel_open,bool fire_watch=false,bool desirability=false,bool water=false,bool health=false);
// Wrap every character, including long tokens, without ellipsis.
std::vector<std::string> wrap_text(const std::vector<std::string>& lines,std::size_t columns);

} // namespace openemperor::sandbox_ui
