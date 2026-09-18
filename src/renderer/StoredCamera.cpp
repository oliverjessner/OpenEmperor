#include "renderer/StoredCamera.h"

#include <algorithm>

namespace openemperor {
void fit_stored_camera(const maps::StoredGraphicsPlan& plan, scene::Camera2D& camera) {
    double min_x=0,max_x=0,min_y=0,max_y=0;
    bool first=true;
    const auto include=[&](scene::Point origin,double width,double height) {
        if (first) { min_x=origin.x; max_x=origin.x+width; min_y=origin.y; max_y=origin.y+height; first=false; }
        else { min_x=std::min(min_x,origin.x); max_x=std::max(max_x,origin.x+width);
               min_y=std::min(min_y,origin.y); max_y=std::max(max_y,origin.y+height); }
    };
    for (const auto& footprint:plan.footprints) {
        if (footprint.status!=maps::StoredStatus::Rendered) continue;
        const auto& record=plan.assets[footprint.asset_index].record;
        include(footprint.image_origin,record.width,record.height);
    }
    for (const auto& cell:plan.cells)
        if (!cell.footprint_index || cell.status!=maps::StoredStatus::Rendered)
            include({cell.world.x-40,cell.world.y},80,40);
    if (first) return;
    camera.zoom=std::clamp(std::min(
        std::max(1,camera.viewport_width-40)/(max_x-min_x),
        std::max(1,camera.viewport_height-110)/(max_y-min_y)),0.05,8.0);
    camera.center_on({(min_x+max_x)*0.5,(min_y+max_y)*0.5});
    camera.offset.y+=43.0;
}
}
