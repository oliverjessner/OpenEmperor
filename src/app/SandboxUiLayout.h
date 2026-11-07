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
    FireWatch, Desirability, Well, Water, HealthPost, Health, ToggleIncome, ServiceOverlay, Bridge, Ramp,
    OriginalHempFarm, OriginalHempField, OriginalFoodFarm, OriginalWheatField, OriginalMilletField,
    OriginalAncestorShrine, OriginalWaterWell,
    OriginalDaoistShrine, OriginalBuddhistShrine, OriginalConfucianAcademy,
    OriginalMusicSchool, OriginalAcrobatSchool, OriginalDramaSchool,
    OriginalLoggingShed, OriginalBronzeSmelter, OriginalBronzewareMaker, OriginalLacquerwareMaker,
    OriginalPaperMaker, OriginalWeaver, OriginalTeaCuringShed, OriginalLacquerRefinery,
    OriginalSilkwormShed, OriginalTeaBush, OriginalLacquerTree, OriginalMulberryTree, ToggleIndustryPage,
    OriginalElitePlot, OriginalTheatre
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

// One scroll document for building rows and inspection text, followed by fixed
// controls. All rectangles use the same render coordinates as Layout::panel.
struct InspectorLayout {
    Rect content,operation_toggle,demolition,hints;
    std::array<Rect,3> operation_priorities{};
    int list_y=0,detail_y=0,hint_y=0;
    int scroll=0,scroll_limit=0,hint_scroll=0,hint_scroll_limit=0;
    int scale=1;
    std::size_t list_rows=0,detail_rows=0,hint_rows=0;

    // List rows include the highlight's two-pixel top inset; draw list text at
    // row.y+2*scale. Detail/hint y is the text origin. Empty rows never hit.
    Rect list_row_rect(std::size_t index) const;
    Rect detail_row_rect(std::size_t index) const;
    Rect hint_row_rect(std::size_t index) const;
    std::optional<std::size_t> list_row_at(double x,double y) const;
};

// Return only complete rows within their shared drawing/input viewport.
Rect visible_text_row(int y,int height,Rect viewport);
InspectorLayout make_inspector_layout(Rect panel,int scale,std::size_t buildings,
    std::size_t details,std::size_t hints,bool status_first,bool operations,
    bool demolition,int scroll=0,int hint_scroll=0);

Layout make_layout(int output_width,int output_height,int window_width,int window_height,
                   bool panel_open,bool fire_watch=false,bool desirability=false,bool water=false,bool health=false,
                   bool income=false,bool service=false,bool bridges=false,bool ramps=false,
                   bool original_production=false,bool industry_page=false,bool original_fire_protection=false,
                   bool original_music_venues=false);
// Wrap every character, including long tokens, without ellipsis.
std::vector<std::string> wrap_text(const std::vector<std::string>& lines,std::size_t columns);

} // namespace openemperor::sandbox_ui
