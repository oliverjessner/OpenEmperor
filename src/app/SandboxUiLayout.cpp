#include "app/SandboxUiLayout.h"

#include <algorithm>
#include <cmath>

namespace openemperor::sandbox_ui {

std::optional<Action> Layout::button_at(double x,double y) const {
    for (const auto& button:buttons)
        if (button.rect.contains(x,y)) return button.action;
    return std::nullopt;
}

bool Layout::ui_at(double x,double y) const {
    return top.contains(x,y) || toolbar.contains(x,y) || status.contains(x,y) ||
        panel.contains(x,y);
}

Layout make_layout(int width,int height,int window_width,int window_height,bool open) {
    Layout out;
    out.scale=std::max(1,static_cast<int>(std::lround(std::min(
        width/static_cast<double>(std::max(1,window_width)),
        height/static_cast<double>(std::max(1,window_height))))));
    const int unit=out.scale;
    const int top=std::min(height,52*unit);
    const int status=std::min(std::max(0,height-top),24*unit);
    const int toolbar=std::min(std::max(0,height-top-status),72*unit);
    out.top={0,0,width,top};
    out.status={0,height-status,width,status};
    out.toolbar={0,height-status-toolbar,width,toolbar};
    out.panel_open=open && width>=680*unit && height>=440*unit;
    const int panel_width=out.panel_open ? std::min(304*unit,width/3) : 0;
    out.panel={width-panel_width,top,panel_width,std::max(0,height-top-status-toolbar)};
    out.map={0,top,std::max(0,width-panel_width),out.panel.h};
    out.panel_toggle={std::max(0,width-96*unit),8*unit,88*unit,32*unit};
    constexpr std::array<Action,7> tools={Action::Select,Action::Road,Action::Clay,
        Action::Pottery,Action::Warehouse,Action::RemoveRoad,Action::Household};
    constexpr std::array<Action,8> controls={Action::Pause,Action::Step,Action::Speed1,
        Action::Speed2,Action::Speed4,Action::Reset,Action::Save,Action::Load};
    const int pad=4*unit;
    const int usable=std::max(0,width-2*pad);
    const int tool_w=usable/7;
    const int control_w=usable/8;
    for (std::size_t i=0;i<tools.size();++i)
        out.buttons[i]={tools[i],{pad+static_cast<int>(i)*tool_w,
            out.toolbar.y+pad,std::max(0,tool_w-pad),28*unit},true};
    for (std::size_t i=0;i<controls.size();++i)
        out.buttons[7+i]={controls[i],{pad+static_cast<int>(i)*control_w,
            out.toolbar.y+36*unit,std::max(0,control_w-pad),28*unit},true};
    out.buttons[15]={Action::TogglePanel,out.panel_toggle,true};
    out.buttons[16]={Action::ToggleDebug,{std::max(0,width-186*unit),8*unit,
        84*unit,32*unit},true};
    return out;
}

} // namespace openemperor::sandbox_ui
