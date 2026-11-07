#include "app/SandboxUiLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace openemperor::sandbox_ui {

std::vector<std::string> wrap_text(const std::vector<std::string>& lines,std::size_t columns) {
    columns=std::max<std::size_t>(1,columns);
    std::vector<std::string> result;
    for (auto remaining:lines) {
        while (remaining.size()>columns) {
            auto end=remaining.rfind(' ',columns);
            if (end==std::string::npos || end==0) end=columns;
            result.push_back(remaining.substr(0,end));
            remaining.erase(0,end);
            if (!remaining.empty() && remaining.front()==' ') remaining.erase(0,1);
        }
        result.push_back(std::move(remaining));
    }
    return result;
}

std::optional<Action> Layout::button_at(double x,double y) const {
    for (const auto& button:buttons)
        if (button.rect.contains(x,y)) return button.action;
    return std::nullopt;
}

bool Layout::ui_at(double x,double y) const {
    return top.contains(x,y) || toolbar.contains(x,y) || status.contains(x,y) ||
        panel.contains(x,y);
}

namespace {
int bounded_int(std::int64_t value) {
    return static_cast<int>(std::clamp(value,std::int64_t{0},
        std::int64_t{std::numeric_limits<int>::max()}));
}
int row_height(std::size_t rows,int stride) {
    const auto limit=static_cast<std::size_t>(std::numeric_limits<int>::max()/stride);
    return rows>limit ? std::numeric_limits<int>::max():static_cast<int>(rows)*stride;
}
Rect contained_rect(Rect rect,Rect panel) {
    if (rect.w<=0 || rect.h<=0 || rect.x<panel.x || rect.y<panel.y ||
        std::int64_t{rect.x}+rect.w>std::int64_t{panel.x}+panel.w ||
        std::int64_t{rect.y}+rect.h>std::int64_t{panel.y}+panel.h) return {};
    return rect;
}
Rect inspector_row(std::size_t index,std::size_t count,int origin,int stride,
                   int scroll,Rect viewport) {
    if (index>=count || index>static_cast<std::size_t>(std::numeric_limits<int>::max()/stride))
        return {};
    const auto y=std::int64_t{origin}+static_cast<int>(index)*stride-scroll;
    if (y<std::numeric_limits<int>::min() || y>std::numeric_limits<int>::max()) return {};
    return visible_text_row(static_cast<int>(y),stride,viewport);
}
} // namespace

Rect visible_text_row(int y,int height,Rect viewport) {
    if (height<=0 || viewport.w<=0 || viewport.h<=0 || y<viewport.y ||
        std::int64_t{y}+height>std::int64_t{viewport.y}+viewport.h) return {};
    return {viewport.x,y,viewport.w,height};
}

Rect InspectorLayout::list_row_rect(std::size_t index) const {
    return inspector_row(index,list_rows,list_y-2*scale,18*scale,scroll,content);
}
Rect InspectorLayout::detail_row_rect(std::size_t index) const {
    return inspector_row(index,detail_rows,detail_y,17*scale,scroll,content);
}
Rect InspectorLayout::hint_row_rect(std::size_t index) const {
    return inspector_row(index,hint_rows,hint_y,17*scale,hint_scroll,hints);
}
std::optional<std::size_t> InspectorLayout::list_row_at(double x,double y) const {
    if (!std::isfinite(x) || !std::isfinite(y) || !content.contains(x,y)) return std::nullopt;
    const double relative=y-list_y+2*scale+scroll;
    if (relative<0 || relative>=static_cast<double>(list_rows)*18*scale) return std::nullopt;
    const auto index=static_cast<std::size_t>(relative/(18*scale));
    if (list_row_rect(index).contains(x,y)) return index;
    return std::nullopt;
}

InspectorLayout make_inspector_layout(Rect panel,int scale,std::size_t buildings,
    std::size_t details,std::size_t hint_rows,bool status_first,bool operations,
    bool demolition,int scroll,int hint_scroll) {
    InspectorLayout out;
    // Actual UI scales are small; this also keeps every fixed offset bounded.
    out.scale=std::clamp(scale,1,std::numeric_limits<int>::max()/1024);
    const int unit=out.scale;
    out.list_rows=buildings;out.detail_rows=details;
    out.hint_rows=demolition ? hint_rows:0;
    panel.w=std::max(0,panel.w);panel.h=std::max(0,panel.h);
    const int bottom=bounded_int(std::int64_t{panel.y}+panel.h);
    const int fixed_top=std::min(bottom,bounded_int(std::int64_t{panel.y}+44*unit));
    const Rect fixed_bounds{panel.x,fixed_top,panel.w,std::max(0,bottom-fixed_top)};
    const int text_width=std::max(0,panel.w-20*unit);
    const int text_x=bounded_int(std::int64_t{panel.x}+10*unit);
    // Preserve the historical two-line footer and its control positions. Long
    // hints may use at most four rows / one quarter of the panel; the remainder
    // is a separate bounded scroll document instead of displacing all details.
    const int hint_budget=std::min(68*unit,panel.h/4);
    const int hint_height=demolition ? std::min(hint_budget,
        std::max(34*unit,row_height(hint_rows,17*unit))):0;
    const int extra=std::max(0,hint_height-34*unit);
    int content_bottom=bottom-8*unit;
    if (demolition) {
        out.demolition=contained_rect({text_x,bottom-78*unit-extra,text_width,28*unit},fixed_bounds);
        out.hints=contained_rect({text_x,bottom-42*unit-extra,text_width,hint_height},fixed_bounds);
        out.hint_y=out.hints.y;
        content_bottom=bottom-(operations ? 180:90)*unit-extra;
    } else if (operations) content_bottom=bottom-104*unit;
    if (operations) {
        out.operation_toggle=contained_rect({text_x,
            bottom-(demolition ? 168:92)*unit-extra,text_width,28*unit},fixed_bounds);
        const int gap=4*unit,width=std::max(0,(text_width-2*gap)/3);
        for (int i=0;i<3;++i) out.operation_priorities[static_cast<std::size_t>(i)]=
            contained_rect({text_x+i*(width+gap),bottom-(demolition ? 124:48)*unit-extra,
                width,28*unit},fixed_bounds);
    }
    const int content_top=std::min(bottom,bounded_int(std::int64_t{panel.y}+40*unit));
    out.content={text_x,content_top,text_width,std::max(0,content_bottom-content_top)};
    const int list_height=row_height(buildings,18*unit);
    const int detail_height=row_height(details,17*unit);
    out.list_y=bounded_int(std::int64_t{content_top}+(status_first ?
        std::int64_t{12*unit}+detail_height:6*unit));
    out.detail_y=bounded_int(std::int64_t{content_top}+(status_first ?
        6*unit:std::int64_t{12*unit}+list_height));
    const int document_bottom=std::max(
        buildings ? bounded_int(std::int64_t{out.list_y}+list_height):content_top,
        details ? bounded_int(std::int64_t{out.detail_y}+detail_height):content_top);
    out.scroll_limit=std::max(0,document_bottom-content_top-out.content.h);
    out.hint_scroll_limit=std::max(0,row_height(out.hint_rows,17*unit)-out.hints.h);
    out.scroll=std::clamp(scroll,0,out.scroll_limit);
    out.hint_scroll=std::clamp(hint_scroll,0,out.hint_scroll_limit);
    return out;
}

Layout make_layout(int width,int height,int window_width,int window_height,bool open,bool fire_watch,bool desirability,bool water,bool health,bool income,bool service,bool bridges,bool ramps,bool original_production,bool industry_page,bool original_fire_protection,bool original_music_venues) {
    Layout out;
    out.scale=std::max(1,static_cast<int>(std::lround(std::min(
        width/static_cast<double>(std::max(1,window_width)),
        height/static_cast<double>(std::max(1,window_height))))));
    const int unit=out.scale;
    const int top=std::min(height,(income ? 80:52)*unit);
    const int status=std::min(std::max(0,height-top),24*unit);
    const bool wrapped_tools=width<(health ? 2440:water ? 2180:fire_watch ? 1980:1800)*unit;
    std::vector<Action> tools={Action::Select,Action::Road,Action::Clay,
        Action::Pottery,Action::Warehouse,Action::RemoveRoad,Action::Household,Action::Farm,
        Action::ServicePost,Action::Market};
    if (original_production) tools[7]=Action::OriginalAncestorShrine;
    if (fire_watch) tools.push_back(original_production ? Action::OriginalWaterWell:Action::FireWatch);
    if (desirability) tools.push_back(Action::Desirability);
    if (water) { tools.push_back(Action::Well); tools.push_back(original_production ? Action::OriginalDaoistShrine:Action::Water); }
    if (health) { tools.push_back(Action::HealthPost); tools.push_back(original_production ? Action::OriginalBuddhistShrine:Action::Health); }
    if (service) tools.push_back(original_production ? Action::OriginalConfucianAcademy:Action::ServiceOverlay);
    if (bridges) tools.push_back(Action::Bridge);
    if (ramps) tools.push_back(Action::Ramp);
    if (original_production) {
        tools.push_back(Action::OriginalHempFarm);tools.push_back(Action::OriginalHempField);
        tools.push_back(Action::OriginalFoodFarm);tools.push_back(Action::OriginalWheatField);tools.push_back(Action::OriginalMilletField);
        tools.push_back(Action::OriginalMusicSchool);tools.push_back(Action::OriginalAcrobatSchool);tools.push_back(Action::OriginalDramaSchool);
    }
    if (original_production) {
        if (original_fire_protection) tools.push_back(Action::FireWatch);
        tools.push_back(Action::ToggleIndustryPage);
        if (industry_page) tools={Action::Select,Action::Road,Action::RemoveRoad,Action::Household,Action::Warehouse,Action::Market,
            Action::OriginalLoggingShed,Action::OriginalBronzeSmelter,Action::OriginalBronzewareMaker,
            Action::OriginalLacquerwareMaker,Action::OriginalPaperMaker,Action::OriginalWeaver,
            Action::OriginalTeaCuringShed,Action::OriginalLacquerRefinery,Action::OriginalSilkwormShed,
            Action::OriginalTeaBush,Action::OriginalLacquerTree,Action::OriginalMulberryTree,
            Action::OriginalElitePlot,Action::ToggleIndustryPage};
    }
    if (original_music_venues) tools.push_back(Action::OriginalTheatre);
    const int tool_count=static_cast<int>(tools.size());
    const int tool_columns=wrapped_tools ? (original_production ? 5:health ? 4:5):tool_count;
    const int tool_rows=wrapped_tools ? ((service || original_production) ? (tool_count+tool_columns-1)/tool_columns:
        (health ? 4:fire_watch ? 3:2)):1;
    const int toolbar=std::min(std::max(0,height-top-status),(tool_rows+1)*36*unit);
    out.top={0,0,width,top};
    out.status={0,height-status,width,status};
    out.toolbar={0,height-status-toolbar,width,toolbar};
    out.panel_open=open && width>=680*unit && height>=440*unit;
    const int panel_width=out.panel_open ? std::min(304*unit,width/3) : 0;
    out.panel={width-panel_width,top,panel_width,std::max(0,height-top-status-toolbar)};
    out.map={0,top,std::max(0,width-panel_width),out.panel.h};
    out.panel_toggle={std::max(0,width-96*unit),8*unit,88*unit,32*unit};
    out.buttons.resize(tools.size()+(income ? 11:10));
    constexpr std::array<Action,8> controls={Action::Pause,Action::Step,Action::Speed1,
        Action::Speed2,Action::Speed4,Action::Reset,Action::Save,Action::Load};
    const int pad=4*unit;
    const int usable=std::max(0,width-2*pad);
    const int tool_w=usable/tool_columns;
    const int control_w=usable/8;
    for (std::size_t i=0;i<tools.size();++i) {
        const int row=wrapped_tools ? static_cast<int>(i)/tool_columns:0;
        const int column=wrapped_tools ? static_cast<int>(i)%tool_columns:static_cast<int>(i);
        out.buttons[i]={tools[i],{pad+column*tool_w,
            out.toolbar.y+pad+row*36*unit,std::max(0,tool_w-pad),28*unit},true};
    }
    for (std::size_t i=0;i<controls.size();++i)
        out.buttons[tools.size()+i]={controls[i],{pad+static_cast<int>(i)*control_w,
            out.toolbar.y+tool_rows*36*unit,
            std::max(0,control_w-pad),28*unit},true};
    out.buttons[tools.size()+8]={Action::TogglePanel,out.panel_toggle,true};
    out.buttons[tools.size()+9]={Action::ToggleHelp,{std::max(0,width-186*unit),8*unit,
        84*unit,32*unit},true};
    if (income) out.buttons[tools.size()+10]={Action::ToggleIncome,
        {std::max(0,width-298*unit),8*unit,106*unit,32*unit},true};
    return out;
}

} // namespace openemperor::sandbox_ui
