#include "maps/RegeneratedMapRenderPlan.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace openemperor::maps;
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
WaterNeighborhood wet(unsigned mask) {return {bool(mask&1),bool(mask&2),bool(mask&4),bool(mask&8),
    bool(mask&16),bool(mask&32),bool(mask&64),bool(mask&128)};}
constexpr std::size_t count=stored_grid_width*stored_grid_height;
constexpr GridCell center{114,114};
constexpr auto cell_index=std::size_t(114)*stored_grid_width+114;
}
int main() {try {
    const auto isolated=match_water(wet(0),0,255);
    check(isolated && isolated->row==1 && isolated->orientation_offset==71 && isolated->variant==0,
        "isolated water matches land cardinal requirements, diagonals ignored");
    const auto channel=match_water(wet(0x44),0,255);
    check(channel && channel->row==6 && channel->orientation_offset==32 && channel->variant_count==2 && channel->variant==1,
        "one-cell east-west channel uses straight bank and modulo two");
    check(match_water(wet(0x11),0,2)->row==7,"north-south channel rotates the bank");
    check(match_water(wet(0x40),0,4)->orientation_offset==39 &&
        match_water(wet(0x40),1,4)->orientation_offset==38,"four orientation offsets retain original order");
    check(!match_water(wet(0xff),0,0) && !match_water(wet(0),4,0),"interior bypass and invalid orientation");
    check(match_water(wet(0x80),0,0)->row==1,"cardinal rule has priority over diagonal-only water");
    for(unsigned mask=0;mask<256;++mask) for(unsigned orientation=0;orientation<4;++orientation) {
        const auto a=match_water(wet(mask),orientation,193),b=match_water(wet(mask),orientation,193);
        check(a==b,"all neighbourhoods and orientations are deterministic");
        if(a) check(a->row>=1 && a->row<=46 && a->variant<a->variant_count &&
            unsigned(a->orientation_offset)+a->variant<72,"bounded shore family");
    }
    std::vector<std::uint32_t> terrain(count,0),objects(count,0);
    std::vector<std::uint8_t> variation(count,190),fertility(count,100);
    LandscapeSelectorInput input{terrain,objects,variation,fertility,false,0};
    terrain[cell_index]=4;
    auto selection=select_landscape(input,center);
    check(selection.group.value==0x605 && selection.variant==71,"isolated selector preserves table offset");
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
        terrain[std::size_t(114+y)*stored_grid_width+std::size_t(114+x)]=4;
    selection=select_landscape(input,center);
    check(selection.group.value==0x61c && selection.variant==22,"lake interior uses modulo24 plus parity");
    variation[cell_index]=191;
    check(select_landscape(input,center).variant==24,"odd modulo24 includes parity increment");
    terrain[cell_index]=0x4000004;
    check(select_landscape(input,center).variant==47,"special water boundary modulo48");
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
        terrain[std::size_t(114+y)*stored_grid_width+std::size_t(114+x)]=0x4000004;
    check(select_landscape(input,center).variant==71,"special interior offset48 plus modulo24");
    input.flood_context=true;
    check(select_landscape(input,center).variant==71,"active context has the same special interior phase");
    terrain.assign(count,0);terrain[cell_index]=0x100;
    check(select_landscape(input,center).family==LandscapeFamily::Ground &&
        select_landscape(input,center).evidence==SelectorEvidence::Unresolved,"flood never enters water selector");
    terrain[cell_index]=0x40000;
    check(select_landscape(input,center).family!=LandscapeFamily::Water,"marsh is separate from water");
    input.flood_context=false;terrain[cell_index]=0x80;variation[cell_index]=57;
    selection=select_landscape(input,center);
    check(selection.group.value==0x602 && selection.variant==39,"fertility100 uses bank four and modulo9");
    fertility[cell_index]=40;
    check(select_landscape(input,center).variant==12,"even fertility boundary and odd variation select lower bank");
    variation[cell_index]=58;
    check(select_landscape(input,center).variant==22,"even variation selects upper boundary bank");
    variation[cell_index]=6;
    check(select_landscape(input,center).evidence==SelectorEvidence::Unresolved,"low variation packing remains explicit");
    terrain[cell_index]=0x82;variation[cell_index]=57;
    check(select_landscape(input,center).group.value==0x606 && select_landscape(input,center).variant==1,
        "raw rock chooses rock resource group independent of fertile flag");
    terrain[cell_index+1]=2;terrain[cell_index+stored_grid_width]=2;terrain[cell_index+stored_grid_width+1]=2;
    check(select_landscape(input,center).evidence==SelectorEvidence::Unresolved,"possible large rock placement is not invented");
    terrain[cell_index]=0x81;objects[cell_index]=2;
    check(select_landscape(input,center).group.value==0x625,"object bit two only selects vegetation family inside vegetation branch");
    terrain[cell_index]=0x80;variation[cell_index]=200;
    check(select_landscape(input,center).group.value==0x602,"object bit two never becomes global bamboo meaning");
    terrain.assign(count,0);terrain[228]=4;
    check(!terrain_neighborhood(terrain,{227,0},4).e,"neighbour probe cannot wrap into another storage row");
    check(select_landscape(input,{228,0}).evidence==SelectorEvidence::Unresolved,"outside storage fails closed");
    RuntimeArchiveLayout layout;layout.slot=3;layout.verified_registration=true;layout.sg3_version=213;
    layout.runtime_image_count=300;layout.groups={{0,80,20,20},{1,82,92,92}};
    std::map<std::uint32_t,GroupRegistration> groups{{3,{&layout}}};
    check(resolve_landscape_variant({0x601},71,groups)->value==3*0x4000+91,
        "large variation resolved through bounded resource group, not physical constant");
    check(!resolve_landscape_variant({0x601},72,groups) && !resolve_landscape_variant({0x80000601},0,groups) &&
        !resolve_landscape_variant({0x600},0,groups),"group boundary, high bit and missing position fail closed");
    // No original bytes: fake metadata is enough to test load-plan isolation,
    // physical dedupe and independence from the discarded saved identity.
    StoredArchiveRegistration registration;registration.layout=layout;
    auto& l=*registration.layout;l.system_record_skip=0;l.first_physical_record=1;
    l.image_capacity=101;l.runtime_image_count=100;l.groups={{0,80,1,1},{1,82,1,1},{2,84,1,1},{3,86,1,1},{4,88,1,1},{5,90,73,73}};
    registration.catalog.emplace();
    for(unsigned n=0;n<101;++n) {
        openemperor::assets::AssetRecord r;r.id={"DATA/synthetic.sg3",n};r.image_type=30;r.width=78;r.height=40;
        r.uncompressed_length=3200;r.data_length=3200;r.isometric_size_flag=1;
        r.payload_in_bounds=true;r.decoder_supported=true;r.color_decoder_supported=true;
        r.color_bounds=openemperor::assets::AssetRangeStatus::InBounds;
        registration.catalog->records.push_back(r);
    }
    StoredArchiveRegistrations registrations{{3,registration}};
    StoredGraphicsPlan plan;plan.landscape_layers_available=true;
    plan.raw_terrain.assign(count,0);plan.raw_terrain[cell_index]=4;plan.raw_objects.assign(count,0);
    plan.variation_bytes.assign(count,190);plan.fertility_bytes.assign(count,100);plan.raw_saved_ids.assign(count,123);
    plan.raw_candidate_bytes.assign(count,64);plan.height_bytes.assign(count,3);
    StoredCell c;c.storage=center;c.cell_index=cell_index;c.slot=3;c.stored_id=123;c.footprint_index=0;
    c.asset_index=0;c.status=StoredStatus::Rendered;plan.cells.push_back(c);
    plan.assets.push_back({registration.catalog->records[1],StoredStatus::Rendered,true,true,{}});
    PlacedFootprint f;f.id=0;f.origin=center;f.cell_indices={0};plan.footprints.push_back(f);
    const auto original_ids=plan.raw_saved_ids;
    build_regenerated_map_render_plan(plan,registrations);
    check(plan.regenerated && plan.regenerated->cells[0].asset_index && plan.assets.size()==2,
        "load plan resolves and eagerly declares one distinct new physical asset");
    check(plan.raw_saved_ids==original_ids && plan.cells[0].stored_id==123 && plan.cells[0].status==StoredStatus::Rendered &&
        plan.footprints[0].asset_index==0,"historical geometry/status/buildability inputs remain exact");
    auto second=plan;second.regenerated.reset();second.assets.resize(1);
    second.raw_saved_ids.assign(count,999999);second.cells[0].stored_id=999999;
    second.raw_candidate_bytes.assign(count,7);second.height_bytes.assign(count,255);
    build_regenerated_map_render_plan(second,registrations);
    check(second.regenerated->cells[0].graphic && plan.regenerated->cells[0].graphic &&
        second.regenerated->cells[0].graphic->value==plan.regenerated->cells[0].graphic->value &&
        second.regenerated->cells[0].selection==plan.regenerated->cells[0].selection,
        "identity independent of saved IDs, candidate byte and height; elevation remains separate");
    const auto published=plan.regenerated;build_regenerated_map_render_plan(plan,registrations);
    check(plan.regenerated==published && plan.assets.size()==2,"published plan never rebuilt or duplicated");
    std::cout<<"landscape selectors passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
