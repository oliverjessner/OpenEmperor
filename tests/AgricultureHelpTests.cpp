#include "app/AgricultureHelp.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
namespace ui=openemperor::sandbox_ui;
void check(bool value,const char* message) {
    if (!value) throw std::runtime_error(message);
}
bool contains(ui::Rect outer,ui::Rect inner) {
    return inner.w>0 && inner.h>0 && inner.x>=outer.x && inner.y>=outer.y &&
        std::int64_t{inner.x}+inner.w<=std::int64_t{outer.x}+outer.w &&
        std::int64_t{inner.y}+inner.h<=std::int64_t{outer.y}+outer.h;
}
bool overlaps(ui::Rect a,ui::Rect b) {
    return a.w>0 && a.h>0 && b.w>0 && b.h>0 &&
        std::int64_t{a.x}<std::int64_t{b.x}+b.w &&
        std::int64_t{b.x}<std::int64_t{a.x}+a.w &&
        std::int64_t{a.y}<std::int64_t{b.y}+b.h &&
        std::int64_t{b.y}<std::int64_t{a.y}+a.h;
}
ui::Layout agriculture_layout(int width,int height,int density,bool industry=false) {
    return ui::make_layout(width*density,height*density,width,height,true,
        true,true,true,true,true,true,false,false,true,industry,true,false,
        true,true,true,true,true);
}
std::string document(const ui::AgricultureHelp& help) {
    std::string text;
    for (const auto& page:help.pages())
        for (const auto& row:page.rows) { text+=row.text; text+=' '; }
    return text;
}
void bounded_page(const ui::AgricultureHelp& help,const ui::AgricultureHelpPage& page,ui::Rect available) {
    check(contains(available,help.panel()),"Help panel escapes the actual map viewport");
    check(contains(help.panel(),page.heading.rect) && contains(help.panel(),page.footer.rect),
        "Help heading/footer is clipped");
    check(!overlaps(page.heading.rect,page.footer.rect),"Help heading overlaps footer");
    check(!page.rows.empty(),"Prepared Help page has no body text");
    const std::array controls{help.previous_button(),help.next_button(),help.close_button()};
    for (std::size_t i=0;i<controls.size();++i) {
        check(contains(help.panel(),controls[i]),"Help button has no contained visible rectangle");
        check(!overlaps(controls[i],page.heading.rect) && !overlaps(controls[i],page.footer.rect),
            "Help button overlaps heading/footer text");
        for (std::size_t j=i+1;j<controls.size();++j)
            check(!overlaps(controls[i],controls[j]),"Prepared Help controls overlap");
    }
    check(page.heading.text.size()*static_cast<std::size_t>(10*help.scale())<=
            static_cast<std::size_t>(page.heading.rect.w),"Help heading would be ellipsized");
    check(page.footer.text.size()*static_cast<std::size_t>(10*help.scale())<=
            static_cast<std::size_t>(page.footer.rect.w),"Page counter would be ellipsized");
    for (std::size_t i=0;i<page.rows.size();++i) {
        const auto& row=page.rows[i];
        check(contains(help.panel(),row.rect),"Help retained a partial text row");
        check(row.rect.h==17*help.scale(),"Help body row does not retain its complete line height");
        check(row.text.size()*static_cast<std::size_t>(10*help.scale())<=
            static_cast<std::size_t>(row.rect.w),"Help body text would be silently ellipsized");
        check(!overlaps(row.rect,page.heading.rect) && !overlaps(row.rect,page.footer.rect),
            "Help body overlaps heading/footer");
        for (const auto control:controls)
            check(!overlaps(row.rect,control),"Help body overlaps a control");
        if (i) check(!overlaps(row.rect,page.rows[i-1].rect),"Help body rows overlap");
    }
}
void layouts_and_paging() {
    for (const auto size:std::array<std::array<int,2>,4>{{{900,600},{1100,700},{1280,800},{1920,1080}}})
        for (const int density:{1,2}) for (const bool industry:{false,true}) {
            const auto layout=agriculture_layout(size[0],size[1],density,industry);
            auto help=ui::prepare_agriculture_help(layout.map,layout.scale);
            check(help.page()!=nullptr && help.pages().size()>=9,"Latest Agriculture Help is unavailable at supported window dimensions");
            check(!help.previous_page() && help.page_index()==0,"First Help page wrapped backwards");
            const auto* storage=help.pages().data();
            const auto count=help.pages().size();
            for (std::size_t i=0;i<count;++i) {
                check(help.page_index()==i && help.page()==&help.pages()[i],"Help skipped a prepared page");
                bounded_page(help,*help.page(),layout.map);
                check(help.page()->footer.text=="Page "+std::to_string(i+1)+"/"+
                    std::to_string(count)+" | PgUp/PgDn","Prepared counter differs from reachable page");
                check(help.pages().data()==storage,"Read-only Help paging rebuilt its prepared pages");
                if (i+1<count) check(help.next_page(),"Help body page is unreachable by Next");
            }
            check(!help.next_page() && help.page_index()==count-1,"Last Help page wrapped forwards");
            check(!help.select_page(std::numeric_limits<std::size_t>::max()) && help.page_index()==count-1,
                "Out-of-range Help selection changed the current page");
            while (help.page_index()) check(help.previous_page(),"Previous cannot reach an earlier prepared page");
            check(help.pages().data()==storage && help.pages().size()==count,"Navigation rebuilt Help preparation");
        }
}
void hits_are_visible_and_bounded() {
    const auto layout=agriculture_layout(900,600,1);
    auto help=ui::prepare_agriculture_help(layout.map,layout.scale);
    const auto hit=[&](ui::Rect r){return help.button_at(r.x+r.w*.5,r.y+r.h*.5);};
    check(!hit(help.previous_button()) && hit(help.next_button())==ui::AgricultureHelpAction::Next &&
        hit(help.close_button())==ui::AgricultureHelpAction::Close,"First-page Help button enablement disagrees with its visible bounds");
    check(help.next_page() && hit(help.previous_button())==ui::AgricultureHelpAction::Previous,
        "Second-page Previous control cannot act");
    check(help.select_page(help.pages().size()-1) && !hit(help.next_button()),"Last-page Next remains active");
    for (const auto& page:help.pages()) for (const auto& row:page.rows)
        check(!help.button_at(row.rect.x+.5,row.rect.y+.5),"Help body text acts as a navigation button");
    const auto close=help.close_button();
    check(!help.button_at(close.x+close.w,close.y+close.h*.5) &&
        !help.button_at(close.x+close.w*.5,close.y+close.h),"Help button includes its outside half-open boundary");
    check(!help.button_at(layout.map.x-1,layout.map.y) &&
        !help.button_at(layout.map.x+layout.map.w,layout.map.y),"Outside map input reached Help controls");
    const auto nan=std::numeric_limits<double>::quiet_NaN();
    const auto infinity=std::numeric_limits<double>::infinity();
    check(!help.button_at(nan,close.y) && !help.button_at(close.x,nan) &&
        !help.button_at(infinity,close.y),"Nonfinite input hit Help navigation");
}
void content_survives_wrapping() {
    const auto small_layout=agriculture_layout(900,600,1);
    const auto large_layout=agriculture_layout(1920,1080,1);
    const auto small=ui::prepare_agriculture_help(small_layout.map,1);
    const auto large=ui::prepare_agriculture_help(large_layout.map,1);
    const auto text=document(small);
    // Space-delimited wrapping may introduce a space only when a long token
    // was split. Every control/token below fits even the minimum26 columns.
    for (const char* essential:{"5 Select","F1:","Wheel zoom","Z: 1x / 2x / 4x","Space pause","single step","F5 save","F9 load","Esc menu",
        "O Food Farm","W Wheat","L Millet","Shift-R Rice","Shift-C Cabbage","Shift-S Soy","eight active fields",
        "N Hemp Farm","M Hemp Field","lifetime ID","growing window","Shift-D Ditch","Shift-P Pump","R rotates",
        "Shift-X","Shift-V","one bonus per four","Shift-F Fishing Quay","Shift-H Hunters Tent","Shift-M Mill",
        "Shift-K Food Shop","Shift-G","Food 32","seven raw Foods","five Dinners","Industry toolbar","Tea Bush",
        "Lacquer Tree","Mulberry","current/next House needs","luxury consumption","Drain:","Demolish","new identity"})
        check(text.find(essential)!=std::string::npos,"Latest Help lost an essential control or gameplay explanation");
    check(text==document(large),"Responsive Help wrapping or pagination discarded/reordered content");
    check(small.pages().size()>large.pages().size(),"Short viewport did not paginate instead of clipping");
}
void unavailable_layouts() {
    for (const auto map:std::array<ui::Rect,8>{{{}, {0,0,900,0}, {0,0,0,600}, {0,0,-1,600},
        {0,0,200,600}, {0,0,900,100},
        {std::numeric_limits<int>::max()-10,0,900,600},
        {0,std::numeric_limits<int>::max()-10,900,10}}}) {
        const auto help=ui::prepare_agriculture_help(map,1);
        check(help.pages().empty() && !help.page() && help.panel().w==0 &&
            help.previous_button().w==0 && help.next_button().w==0 && help.close_button().w==0 &&
            !help.button_at(0,0),"Unavailable viewport retained clipped Help or invisible controls");
    }
    const auto invalid_scale=ui::prepare_agriculture_help({0,0,900,600},std::numeric_limits<int>::max());
    check(invalid_scale.pages().empty(),"Extreme scale overflows Help geometry");
    check(ui::prepare_agriculture_help({0,0,900,600},0).scale()==1,"Zero scale was not bounded");
}
} // namespace

int main() {
    try {
        layouts_and_paging();hits_are_visible_and_bounded();content_survives_wrapping();unavailable_layouts();
        std::cout<<"Agriculture Help: all controls/content reachable; complete contained rows/buttons at900x600 through1920x1080,1x/2x density; navigation reuses prepared pages PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
