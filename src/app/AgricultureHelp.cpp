#include "app/AgricultureHelp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace openemperor::sandbox_ui {
namespace {
struct Topic {
    const char* title;
    std::vector<std::string> lines;
};

std::array<Topic,9> topics() {
    return {{
        {"Controls",{
            "5 Select: visible-pixel inspection; construction picks ground.",
            "F1: read-only visual diagnostics; not a construction tool.",
            "Arrows pan | Wheel zoom | Z: 1x / 2x / 4x.",
            "Space pause | . single step | + / - speed.",
            "F5 save | F9 load exact state | Esc menu.",
            "H or ? closes Help | PageUp / PageDown changes Help page.",
            "Focus loss, resize or release over UI cancels a placement."}},
        {"Food fields",{
            "1 Road | 7 House | 0 Market | 4 Warehouse.",
            "Q Water Well | Y Ancestor Shrine | F Fire Watch.",
            "O Food Farm | W Wheat | L Millet.",
            "Select a paid Food Farm before placing nearby Food Fields.",
            "Shift-R Rice | Shift-C Cabbage | Shift-S Soy.",
            "Up to eight active fields share each Farm's labor.",
            "Rice needs source fertility 100; Soy produces Bean Curd.",
            "N Hemp Farm | M Hemp Field; select the paid source first."}},
        {"Work and seasons",{
            "Select Food/Hemp/plantation Growers to inspect their real work.",
            "Read source, lifetime ID, phase, cargo and physical route.",
            "A crop inspector shows its growing window and next opportunity.",
            "Growth needs staff, road access, operation and an active season.",
            "Ready crops still need actual harvest or pickup before expiry.",
            "Field harvest, picked cargo and source return are distinct.",
            "Paused sources still settle genuinely loaded returns."}},
        {"Water",{
            "Shift-D Ditch | Shift-P Pump | R rotates the Pump tool.",
            "Shift-X removes Ditch | Shift-V shows supplied water.",
            "A Pump needs legal shore/intake geometry and real staff.",
            "Operating Pumps refill connected paid Ditches on the water phase.",
            "Eligible wet growth earns one bonus per four actual work ticks.",
            "Water creates no work by itself and does not boost harvest cargo.",
            "Inspect wet status, bonus carry and concrete supply blockers."}},
        {"Fish and hunting",{
            "Shift-F Fishing Quay | R rotates the Quay tool.",
            "A Quay needs legal shore/dock geometry and real staff.",
            "Select a Boat: fishing work, actual cargo and dock unload.",
            "Shift-H Hunters Tent; select Hunter or living wildlife.",
            "Fishing grounds and prey are finite and recover through time.",
            "Fish and Meat enter Food only after real return and unload."}},
        {"Food processing",{
            "Shift-M Mill | Shift-K Food Shop | Shift-G changes recipe.",
            "A Food Shop binds an earlier cardinal-adjacent paid Market.",
            "Ingredients commit once; staffed work completes prepared meals.",
            "4 Warehouse: shared Food 32, at most four portions per kind.",
            "Inspect seven raw Foods and five Dinners grade columns.",
            "Recipes use grades 1-4; grade 5 remains unavailable.",
            "Actual trips connect producers, Mill, Market and Food Shop.",
            "A dispatch or reserved slot is not a delivered meal."}},
        {"Materials",{
            "Industry toolbar opens material buildings and plantations.",
            "Choose Tea Shed / Tea Bush, Refinery / Lacquer Tree,",
            "or Silkworm Shed / Mulberry, then select the paid source.",
            "Paid plants share source staff, seasonal work and supplied water.",
            "Tea, Lacquer and Raw Silk require real source return.",
            "Industry recipes process materials into household Goods.",
            "Inspect source inputs, outputs, workers and actual cargo."}},
        {"Inspect and needs",{
            "Select Farm, Field, Quay, Tent, Mill, Shop or Warehouse.",
            "Panel wheel scrolls actual inventory, work and blockers.",
            "Read incoming/outgoing trips; a planned route is no arrival.",
            "House sections: State / Needs / Goods / Visits.",
            "PageUp/Down changes House section; F10 cycles overlays.",
            "Check current/next House needs and luxury consumption witnesses.",
            "Tea, Silk and Lacquerware receipts differ from consumption.",
            "The selected actor's identity, cargo and route stay separate."}},
        {"Reconfigure",{
            "Select a paid Food Field, plantation plant or Pump.",
            "Choose Drain: stop new cycles; settle actual work and cargo.",
            "When settled, confirm Demolish for that selected leaf.",
            "Farms and other stock owners remain protected.",
            "A rebuild pays full price and receives a new identity.",
            "There is no refund, stock transfer or erased work history.",
            "Paused, burning, unstaffed or disconnected? Read the blocker."}}
    }};
}

bool valid_bounds(Rect rect) {
    return rect.w>0 && rect.h>0 &&
        std::int64_t{rect.x}+rect.w<=std::numeric_limits<int>::max() &&
        std::int64_t{rect.y}+rect.h<=std::numeric_limits<int>::max();
}
} // namespace

const AgricultureHelpPage* AgricultureHelp::page() const {
    return selected_<pages_.size() ? &pages_[selected_] : nullptr;
}

bool AgricultureHelp::select_page(std::size_t index) {
    if (index>=pages_.size() || index==selected_) return false;
    selected_=index;
    return true;
}

bool AgricultureHelp::previous_page() {
    return selected_>0 && select_page(selected_-1);
}

bool AgricultureHelp::next_page() {
    return selected_<pages_.size() && select_page(selected_+1);
}

std::optional<AgricultureHelpAction> AgricultureHelp::button_at(double x,double y) const {
    if (!std::isfinite(x) || !std::isfinite(y) || !page()) return std::nullopt;
    if (close_.contains(x,y)) return AgricultureHelpAction::Close;
    if (selected_>0 && previous_.contains(x,y)) return AgricultureHelpAction::Previous;
    if (selected_+1<pages_.size() && next_.contains(x,y)) return AgricultureHelpAction::Next;
    return std::nullopt;
}

AgricultureHelp prepare_agriculture_help(Rect available_map,int scale) {
    AgricultureHelp out;
    out.scale_=std::clamp(scale,1,1024);
    if (!valid_bounds(available_map)) return out;
    const int unit=out.scale_;
    const int margin=12*unit,pad=12*unit,stride=17*unit;
    const int width=std::min(760*unit,std::max(0,available_map.w-2*margin));
    const int height=std::min(640*unit,std::max(0,available_map.h-2*margin));
    const int text_width=width-2*pad;
    // Three real 80-unit controls plus gaps must fit. All headings and the
    // prepared page counter also fit these26 complete character columns.
    if (text_width<260*unit || height<132*unit) return out;
    const int x=available_map.x+(available_map.w-width)/2;
    const int y=available_map.y+margin;
    const int body_y=y+pad+24*unit;
    const int button_y=y+height-pad-28*unit;
    const int footer_y=button_y-24*unit;
    const int body_height=footer_y-6*unit-body_y;
    const int rows=body_height/stride;
    if (rows<=0) return out;
    out.panel_={x,y,width,height};
    out.previous_={x+pad,button_y,80*unit,28*unit};
    out.next_={x+pad+88*unit,button_y,80*unit,28*unit};
    out.close_={x+width-pad-80*unit,button_y,80*unit,28*unit};
    const auto columns=static_cast<std::size_t>(text_width/(10*unit));
    for (auto& topic:topics()) {
        auto lines=wrap_text(topic.lines,columns);
        for (std::size_t first=0;first<lines.size();first+=static_cast<std::size_t>(rows)) {
            AgricultureHelpPage page;
            page.heading={std::string("Help - ")+topic.title,{x+pad,y+pad,text_width,stride}};
            const auto end=std::min(lines.size(),first+static_cast<std::size_t>(rows));
            page.rows.reserve(end-first);
            for (std::size_t i=first;i<end;++i) {
                const int row=static_cast<int>(i-first);
                const auto rect=visible_text_row(body_y+row*stride,stride,
                    {x+pad,body_y,text_width,body_height});
                page.rows.push_back({std::move(lines[i]),rect});
            }
            page.footer.rect={x+pad,footer_y,text_width,stride};
            out.pages_.push_back(std::move(page));
        }
    }
    for (std::size_t i=0;i<out.pages_.size();++i)
        out.pages_[i].footer.text="Page "+std::to_string(i+1)+"/"+
            std::to_string(out.pages_.size())+" | PgUp/PgDn";
    return out;
}

} // namespace openemperor::sandbox_ui
