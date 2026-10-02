#include "app/SandboxUiLayout.h"

#include <algorithm>
#include <cmath>

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

Layout make_layout(int width,int height,int window_width,int window_height,bool open,bool fire_watch) {
    Layout out;
    out.scale=std::max(1,static_cast<int>(std::lround(std::min(
        width/static_cast<double>(std::max(1,window_width)),
        height/static_cast<double>(std::max(1,window_height))))));
    const int unit=out.scale;
    const int top=std::min(height,52*unit);
    const int status=std::min(std::max(0,height-top),24*unit);
    const bool wrapped_tools=width<(fire_watch ? 1980:1800)*unit;
    const int tool_rows=wrapped_tools ? (fire_watch ? 3:2):1;
    const int toolbar=std::min(std::max(0,height-top-status),(tool_rows+1)*36*unit);
    out.top={0,0,width,top};
    out.status={0,height-status,width,status};
    out.toolbar={0,height-status-toolbar,width,toolbar};
    out.panel_open=open && width>=680*unit && height>=440*unit;
    const int panel_width=out.panel_open ? std::min(304*unit,width/3) : 0;
    out.panel={width-panel_width,top,panel_width,std::max(0,height-top-status-toolbar)};
    out.map={0,top,std::max(0,width-panel_width),out.panel.h};
    out.panel_toggle={std::max(0,width-96*unit),8*unit,88*unit,32*unit};
    std::vector<Action> tools={Action::Select,Action::Road,Action::Clay,
        Action::Pottery,Action::Warehouse,Action::RemoveRoad,Action::Household,Action::Farm,
        Action::ServicePost,Action::Market};
    if (fire_watch) tools.push_back(Action::FireWatch);
    out.buttons.resize(tools.size()+10);
    constexpr std::array<Action,8> controls={Action::Pause,Action::Step,Action::Speed1,
        Action::Speed2,Action::Speed4,Action::Reset,Action::Save,Action::Load};
    const int pad=4*unit;
    const int usable=std::max(0,width-2*pad);
    const int tool_columns=wrapped_tools ? 5:static_cast<int>(tools.size());
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
    return out;
}

} // namespace openemperor::sandbox_ui
