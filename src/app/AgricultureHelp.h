#pragma once

#include "app/SandboxUiLayout.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::sandbox_ui {

struct AgricultureHelpRow {
    std::string text;
    Rect rect;
};

struct AgricultureHelpPage {
    AgricultureHelpRow heading;
    std::vector<AgricultureHelpRow> rows;
    AgricultureHelpRow footer;
};

enum class AgricultureHelpAction { Previous, Next, Close };

// Prepare on Help open or layout change. Drawing and paging only read the
// prepared strings/complete rectangles; navigation never wraps or allocates.
class AgricultureHelp {
public:
    const Rect& panel() const { return panel_; }
    int scale() const { return scale_; }
    const std::vector<AgricultureHelpPage>& pages() const { return pages_; }
    const AgricultureHelpPage* page() const;
    std::size_t page_index() const { return selected_; }
    bool select_page(std::size_t index);
    bool previous_page();
    bool next_page();
    const Rect& previous_button() const { return previous_; }
    const Rect& next_button() const { return next_; }
    const Rect& close_button() const { return close_; }
    std::optional<AgricultureHelpAction> button_at(double x,double y) const;
private:
    friend AgricultureHelp prepare_agriculture_help(Rect available_map,int scale,bool quality_aware);
    Rect panel_,previous_,next_,close_;
    int scale_=1;
    std::vector<AgricultureHelpPage> pages_;
    std::size_t selected_=0;
};

// Rules36/37/38 controls only. An unavailable/tiny viewport produces no rectangles or
// pages instead of partially drawn text or invisible control hits.
// Only the explicit38 selection adds its gameplay guidance; older text stays exact.
AgricultureHelp prepare_agriculture_help(Rect available_map,int scale,bool quality_aware=false);

} // namespace openemperor::sandbox_ui
