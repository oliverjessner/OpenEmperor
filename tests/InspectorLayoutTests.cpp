#include "app/SandboxUiLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
namespace ui=openemperor::sandbox_ui;
void check(bool value,const char* message) {
    if (!value) throw std::runtime_error(message);
}
bool empty(ui::Rect r) { return r.w<=0 || r.h<=0; }
bool inside(ui::Rect outer,ui::Rect inner) {
    return !empty(inner) && inner.x>=outer.x && inner.y>=outer.y &&
        inner.x+inner.w<=outer.x+outer.w && inner.y+inner.h<=outer.y+outer.h;
}
bool overlaps(ui::Rect a,ui::Rect b) {
    return !empty(a) && !empty(b) && a.x<b.x+b.w && b.x<a.x+a.w &&
        a.y<b.y+b.h && b.y<a.y+a.h;
}
void bounded(const ui::InspectorLayout& layout,ui::Rect panel) {
    check(inside(panel,layout.content),"Inspector has no contained content viewport");
    const std::array fixed{layout.operation_toggle,layout.operation_priorities[0],
        layout.operation_priorities[1],layout.operation_priorities[2],layout.demolition,layout.hints};
    for (std::size_t i=0;i<fixed.size();++i) if (!empty(fixed[i])) {
        check(inside(panel,fixed[i]),"Fixed control or hint escaped its panel");
        check(!overlaps(layout.content,fixed[i]),"Scrollable content overlaps a fixed control/hint");
        for (std::size_t j=i+1;j<fixed.size();++j)
            check(!overlaps(fixed[i],fixed[j]),"Fixed inspector regions overlap");
        check(!layout.list_row_at(fixed[i].x+fixed[i].w*.5,fixed[i].y+fixed[i].h*.5),
            "A fixed control/hint is also a building row hit");
    }
}
void exercise(ui::Rect panel,int scale,bool first,bool operations,bool demolition) {
    constexpr std::size_t buildings=128,details=137,hints=96;
    const auto make=[&](int scroll=0,int hint_scroll=0) {
        return ui::make_inspector_layout(panel,scale,buildings,details,hints,
            first,operations,demolition,scroll,hint_scroll);
    };
    const auto initial=make();bounded(initial,panel);
    check(initial.scroll_limit>0,"Large inspector document does not scroll");
    check(initial.hints.h<=panel.h/4 && initial.hints.h<=68*scale,
        "Long hints displaced the bounded content budget");
    check((first && initial.detail_y<initial.list_y) ||
          (!first && initial.list_y<initial.detail_y),"Inspector document order changed");
    check(make(-100,-100).scroll==0 && make(-100,-100).hint_scroll==0,
        "Negative inspector scroll was not clamped");
    const auto final=make(std::numeric_limits<int>::max(),std::numeric_limits<int>::max());
    check(final.scroll==initial.scroll_limit && final.hint_scroll==initial.hint_scroll_limit,
        "Extreme inspector scrolling escaped its document");
    for (const int offset:{0,1,17*scale,initial.scroll_limit/2,initial.scroll_limit}) {
        const auto layout=make(offset);bounded(layout,panel);
        check(layout.list_row_rect(buildings).w==0 && layout.detail_row_rect(details).w==0,
            "An out-of-document inspector row became visible");
        for (std::size_t i=0;i<buildings;++i) {
            const int y=layout.list_y-2*scale+static_cast<int>(i)*18*scale-layout.scroll;
            const bool complete=y>=layout.content.y && y+18*scale<=layout.content.y+layout.content.h;
            const auto rect=layout.list_row_rect(i);
            check(!empty(rect)==complete,"A partial building row was drawn or a complete row hidden");
            if (complete) {
                check(inside(layout.content,rect),"Visible building row escaped its clip");
                check(layout.list_row_at(rect.x+.5,rect.y+.5)==i &&
                      layout.list_row_at(rect.x+rect.w-.5,rect.y+rect.h-.5)==i,
                    "Visible row drawing and its hit rectangle disagree");
            } else if (y<layout.content.y+layout.content.h && y+18*scale>layout.content.y) {
                const double sample=std::max(y,layout.content.y)+.5;
                check(!layout.list_row_at(layout.content.x+.5,sample),
                    "A clipped building row retained an invisible hitbox");
            }
        }
        for (std::size_t i=0;i<details;++i) {
            const auto rect=layout.detail_row_rect(i);
            if (!empty(rect)) check(inside(layout.content,rect) &&
                !layout.list_row_at(rect.x+.5,rect.y+.5),"Inspection details became building hits");
        }
        check(!layout.list_row_at(layout.content.x+.5,layout.content.y-.5) &&
              !layout.list_row_at(layout.content.x+.5,layout.content.y+layout.content.h) &&
              !layout.list_row_at(layout.content.x+layout.content.w,layout.content.y+.5),
            "Inspector row hit escaped a half-open clip boundary");
    }
    // Align every real row with its viewport, including the document's final
    // row. This checks reachability independently of a finite wheel sample.
    for (std::size_t i=0;i<buildings;++i) {
        const auto layout=make(initial.list_y-2*scale+static_cast<int>(i)*18*scale-initial.content.y);
        check(!empty(layout.list_row_rect(i)),"A building is unreachable by bounded scrolling");
    }
    for (std::size_t i=0;i<details;++i) {
        const auto layout=make(initial.detail_y+static_cast<int>(i)*17*scale-initial.content.y);
        check(!empty(layout.detail_row_rect(i)),"Inspection information is unreachable by scrolling");
    }
    if (demolition) {
        check(initial.hint_scroll_limit>0,"Long demolition hints have no bounded scrolling");
        for (std::size_t i=0;i<hints;++i) {
            const auto layout=make(0,static_cast<int>(i)*17*scale);
            check(!empty(layout.hint_row_rect(i)) && inside(layout.hints,layout.hint_row_rect(i)),
                "A demolition hint is unreachable or escaped its clip");
        }
        check(empty(initial.hint_row_rect(hints)),"An absent hint became visible");
    } else check(empty(initial.hints) && initial.hint_scroll_limit==0,
        "An unsupported demolition footer retained a scroll region");
    const auto nan=std::numeric_limits<double>::quiet_NaN();
    check(!initial.list_row_at(nan,100) && !initial.list_row_at(100,nan) &&
          !initial.list_row_at(std::numeric_limits<double>::infinity(),100),
        "Nonfinite inspector input reached a row hit");
}
void historical_controls(ui::Rect panel,int scale) {
    const int bottom=panel.y+panel.h;
    for (const std::size_t hints:{1U,2U,3U,4U}) {
        const auto layout=ui::make_inspector_layout(panel,scale,20,32,hints,false,true,true);
        const int extra=std::max(0,static_cast<int>(hints)-2)*17*scale;
        check(layout.operation_toggle.y==bottom-168*scale-extra &&
              layout.operation_toggle.h==28*scale &&
              layout.operation_priorities[0].y==bottom-124*scale-extra &&
              layout.demolition.y==bottom-78*scale-extra &&
              layout.hints.y==layout.demolition.y+36*scale,
            "Ordinary operation/priority/demolition hitboxes moved");
        check(layout.list_y==panel.y+46*scale && layout.detail_y==panel.y+(52+20*18)*scale,
            "Ordinary list-first inspector text origins moved");
    }
    const auto operations=ui::make_inspector_layout(panel,scale,20,32,0,true,true,false);
    check(operations.operation_toggle.y==bottom-92*scale &&
          operations.operation_priorities[0].y==bottom-48*scale,
        "Operation-only historical hitboxes moved");
    check(operations.detail_y==panel.y+46*scale && operations.list_y==panel.y+(52+32*17)*scale,
        "Status-first inspector text order/origins moved");
}
void small_panels() {
    for (int scale:{1,2}) {
        const auto make=[&](int height) {
            return ui::make_inspector_layout({100*scale,50*scale,304*scale,height*scale},
                scale,128,137,96,false,true,true,std::numeric_limits<int>::max(),
                std::numeric_limits<int>::max());
        };
        // During a resize, each surviving fixed region still has a real visible
        // rectangle even when an earlier control no longer fits. Rendering and
        // input must consume these rectangles independently.
        const auto compact=make(180);
        check(empty(compact.operation_toggle) && !empty(compact.operation_priorities[0]) &&
              !empty(compact.operation_priorities[1]) && !empty(compact.operation_priorities[2]) &&
              !empty(compact.demolition) && !empty(compact.hints),
            "Compact panel lost valid priority/demolition/hint rectangles with its toggle");
        const auto shorter=make(120);
        check(empty(shorter.demolition) && !empty(shorter.hints),
            "A surviving hint viewport incorrectly depends on its clipped demolition button");
        for (const int height:{420,240,180,120,80,180,420,80}) {
            const ui::Rect panel{100*scale,50*scale,304*scale,height*scale};
            const auto layout=make(height);
            check(layout.content.w>=0 && layout.content.h>=0 &&
                  layout.scroll>=0 && layout.scroll<=layout.scroll_limit &&
                  layout.hint_scroll>=0 && layout.hint_scroll<=layout.hint_scroll_limit,
                "Resize produced negative bounds or retained an out-of-document scroll");
            const std::array fixed{layout.operation_toggle,layout.operation_priorities[0],
                layout.operation_priorities[1],layout.operation_priorities[2],layout.demolition,layout.hints};
            for (std::size_t i=0;i<fixed.size();++i) if (!empty(fixed[i])) {
                check(inside(panel,fixed[i]) && fixed[i].y>=panel.y+44*scale &&
                      !overlaps(layout.content,fixed[i]),"Resize placed a fixed inspector region over its header/content");
                for (std::size_t j=i+1;j<fixed.size();++j)
                    check(!overlaps(fixed[i],fixed[j]),"Compact fixed inspector regions overlap");
                check(!layout.list_row_at(fixed[i].x+1,fixed[i].y+1),
                    "Compact fixed inspector region is also an invisible building hit");
            }
            for (std::size_t i=0;i<layout.list_rows;++i) {
                const auto row=layout.list_row_rect(i);
                if (!empty(row)) check(inside(layout.content,row),"Resized building row escaped its clip");
                if (empty(layout.content)) check(empty(row),"Zero-height content retained a visible building row");
            }
            if (!empty(layout.hints)) check(!empty(layout.hint_row_rect(layout.hint_rows-1)),
                "Compact hint scrolling cannot reach its final complete row");
        }
    }
}
} // namespace

int main() {
    try {
        for (const auto size:std::array{std::array{1280,800},std::array{1600,900},std::array{1920,1080}})
            for (int scale:{1,2}) {
                // Original-production toolbar is the tightest maintained
                // layout at these window sizes, including the OE41 Market7 repro.
                const auto outer=ui::make_layout(size[0]*scale,size[1]*scale,size[0],size[1],
                    true,true,true,true,true,true,true,false,false,true,false,true);
                check(outer.panel_open && outer.scale==scale,"Required inspector layout is unavailable");
                historical_controls(outer.panel,scale);
                for (bool first:{false,true}) for (bool operations:{false,true})
                    for (bool demolition:{false,true}) exercise(outer.panel,scale,first,operations,demolition);
            }
        const auto partial=ui::make_inspector_layout({100,50,304,420},1,200,50,2,false,true,true,7);
        check(empty(partial.list_row_rect(0)) &&
              !partial.list_row_at(partial.content.x+1,partial.content.y+1),
            "A top-clipped row can still be selected");
        const ui::Rect viewport{20,30,100,40};
        check(empty(ui::visible_text_row(29,17,viewport)) &&
              empty(ui::visible_text_row(54,17,viewport)) &&
              !empty(ui::visible_text_row(53,17,viewport)),"Full text-row clip boundary is incorrect");
        const auto hidden=ui::make_inspector_layout({},1,0,0,0,false,true,true);
        check(hidden.content.h==0 && hidden.operation_toggle.h==0 && hidden.demolition.h==0 &&
              !hidden.list_row_at(0,0),"Hidden inspector retained controls or building hits");
        const auto short_panel=ui::make_inspector_layout({100,50,304,80},1,20,30,2,false,true,true);
        check(short_panel.content.h==0 && short_panel.operation_toggle.h==0 &&
              short_panel.demolition.h==0 && short_panel.hints.h==0 &&
              short_panel.operation_priorities[0].h==0,
            "Short inspector controls overwrite its header or retain hidden hitboxes");
        small_panels();
        for (const int scale:{1,2}) for (const bool operations:{false,true}) {
            const ui::Rect panel{600*scale,80*scale,300*scale,208*scale};
            const auto layout=ui::make_inspector_layout(panel,scale,82,90,20,true,operations,true,0,0,true);
            bounded(layout,panel);
            check(!empty(layout.detail_row_rect(0))&&!empty(layout.detail_row_rect(1))&&!empty(layout.detail_row_rect(2)),
                "Rule36 compact footer hid the required three complete detail rows");
            check(!empty(layout.demolition)&&!empty(layout.hint_row_rect(0))&&!empty(layout.hint_row_rect(1)),
                "Rule36 compact action or two-row drain explanation is unavailable");
            for (std::size_t row=0;row<20;++row) {
                const auto scrolled=ui::make_inspector_layout(panel,scale,82,90,20,true,operations,true,0,static_cast<int>(row)*17*scale,true);
                check(!empty(scrolled.hint_row_rect(row)),"A precise Rule36 drain blocker became unreachable");
            }
        }
        const auto wrapped=ui::wrap_text({"A complete demolition warning with "
            "a_very_long_unbroken_token_that_must_remain_accessible"},28);
        for (const auto& line:wrapped) check(line.size()<=28,"A long hint escaped its glyph column budget");
        std::cout<<"Inspector layout: required sizes/2x, shared clipping and hits, complete scroll reachability, "
            "bounded hints and historical fixed controls PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"Inspector layout: "<<error.what()<<'\n';return 1;
    }
}
