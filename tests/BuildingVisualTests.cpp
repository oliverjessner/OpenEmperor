#include "assets/BuildingVisualProfile.h"
#include "app/SandboxVisualOrder.h"
#include "renderer/BuildingSprite.h"
#include "core/PerformanceDiagnostics.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Json=nlohmann::json;
void check(bool okay,const char* message) { if (!okay) throw std::runtime_error(message); }
void u16(Bytes& bytes,std::size_t offset,std::uint16_t value) {
    bytes.at(offset)=static_cast<std::uint8_t>(value);
    bytes.at(offset+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    u16(bytes,offset,static_cast<std::uint16_t>(value));
    u16(bytes,offset+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const fs::path& path,const Bytes& bytes) {
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(output),"synthetic file write");
}
struct Fixture {
    fs::path root=fs::temp_directory_path()/
        ("openemperor-building-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",manifest=root/"building.json";
    Bytes sg3,bitmap;
    Fixture() {
        fs::create_directories(data/"DATA");
        sg3.assign(40680U+4U*72U,0);
        u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
        u32(sg3,12,4);u32(sg3,16,4);u32(sg3,20,1);
        const std::string name="synthetic.bmp";
        std::copy(name.begin(),name.end(),sg3.begin()+680);
        u32(sg3,680+124,4);
        const std::size_t record=40680U+3U*72U;
        u32(sg3,record,4);u32(sg3,record+4,12800);u32(sg3,record+8,12800);
        u16(sg3,record+20,158);u16(sg3,record+22,90);
        u16(sg3,record+50,30);sg3[record+55]=2;
        write(data/"DATA/building.sg3",sg3);
        bitmap.assign(12804,0);
        for (std::size_t at=4;at<bitmap.size();at+=2) u16(bitmap,at,0x03e0);
        write(data/"DATA/building.555",bitmap);
    }
    ~Fixture() { std::error_code ignored;fs::remove_all(root,ignored); }
    Json valid() const {
        return {{"schema_version",1},{"mode","curated_building_preview"},
            {"buildings",{{"pottery",{{"archive","DATA/building.sg3"},
                {"image_index",3},{"ground_anchor",{79,70}},
                {"evidence","Independent synthetic Type-30 fixture"}}}}}};
    }
    void save(const Json& json) const {
        std::ofstream output(manifest);output<<json.dump();check(bool(output),"manifest write");
    }
};
template<class F> void rejects(F operation,const char* message) {
    try { operation(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
std::array<std::uint8_t,4> pixel(SDL_Renderer* renderer,int x,int y) {
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,nullptr);
    check(surface!=nullptr,"read software renderer");
    std::array<std::uint8_t,4> value{};
    const bool okay=SDL_ReadSurfacePixel(surface,x,y,&value[0],&value[1],&value[2],&value[3]);
    SDL_DestroySurface(surface);check(okay,"read software pixel");return value;
}
void profile_checks(Fixture& fixture) {
    fixture.save(fixture.valid());
    const auto loaded=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    const auto* pottery=loaded.find(openemperor::assets::BuildingVisualRole::Pottery);
    check(pottery && pottery->id.image_index==3 && loaded.unique_images.size()==1 &&
          loaded.unique_images[pottery->image_index].width==158 &&
          loaded.unique_images[pottery->image_index].height==90 &&
          pottery->ground_x==79 && pottery->ground_y==70 && pottery->footprint_side==2,
          "physical Type-30 building load");
    auto shared=fixture.valid();
    shared["buildings"]["warehouse"]=shared["buildings"]["pottery"];
    fixture.save(shared);
    const auto dedup=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(dedup.unique_images.size()==1 &&
          dedup.find(openemperor::assets::BuildingVisualRole::Warehouse)->image_index==
          dedup.find(openemperor::assets::BuildingVisualRole::Pottery)->image_index,
          "two roles did not deduplicate one physical asset");
    const auto make_copy=[&](const char* name,std::uint16_t color) {
        fs::copy_file(fixture.data/"DATA/building.sg3",fixture.data/(std::string("DATA/")+name+".sg3"));
        auto bitmap=fixture.bitmap;
        for (std::size_t at=4;at<bitmap.size();at+=2) u16(bitmap,at,color);
        write(fixture.data/(std::string("DATA/")+name+".555"),bitmap);
    };
    make_copy("clay",0x03ff);make_copy("warehouse",0x7fe0);make_copy("house",0x03e0);
    auto one_cell=fixture.sg3;
    const std::size_t one_record=40680U+3U*72U;
    u32(one_cell,one_record+4,3200);u32(one_cell,one_record+8,3200);
    u16(one_cell,one_record+20,78);u16(one_cell,one_record+22,61);
    one_cell[one_record+55]=1;
    write(fixture.data/"DATA/one-cell.sg3",one_cell);
    Bytes one_bitmap(3204,0);
    for (std::size_t at=4;at<one_bitmap.size();at+=2) u16(one_bitmap,at,0x7c00);
    write(fixture.data/"DATA/one-cell.555",one_bitmap);
    auto all=fixture.valid();
    for (const auto& [role,name]:std::array<std::pair<const char*,const char*>,3>{{
            {"clay_source","clay"},{"warehouse","warehouse"},{"household","house"}}}) {
        all["buildings"][role]=all["buildings"]["pottery"];
        all["buildings"][role]["archive"]=std::string("DATA/")+name+".sg3";
    }
    for (const char* role:{"farm","service_post","market"}) {
        all["buildings"][role]=all["buildings"]["pottery"];
        all["buildings"][role]["archive"]="DATA/one-cell.sg3";
    }
    fixture.save(all);
    const auto four=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(four.unique_images.size()==5 &&
          std::all_of(openemperor::assets::building_roles.begin(),
                      openemperor::assets::building_roles.begin()+7,
                      [&](auto role){return four.find(role)!=nullptr;}),
          "seven-role profile or one-cell deduplication failed");
    check(!four.find(openemperor::assets::BuildingVisualRole::FireWatch),
          "old seven-role profile must leave only the Fire Watch fallback");
    all["buildings"]["fire_watch"]=all["buildings"]["farm"];
    all["buildings"]["fire_watch"]["ground_anchor"]={39,41};
    fixture.save(all);
    const auto eight=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(eight.unique_images.size()==5 && eight.find(openemperor::assets::BuildingVisualRole::FireWatch)->footprint_side==1,
          "optional eighth role failed or duplicated its existing image");
    auto with_well=all;
    with_well["buildings"]["well"]=all["buildings"]["farm"];
    fixture.save(with_well);
    const auto well_profile=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(well_profile.find(openemperor::assets::BuildingVisualRole::Well)->footprint_side==1 &&
          well_profile.unique_images.size()==5,"optional Well role must deduplicate 1x1 asset");
    auto health=with_well;
    health["buildings"]["health_post"]=all["buildings"]["farm"];
    health["buildings"]["health_post"]["ground_anchor"]={39,27};
    fixture.save(health);
    const auto hp=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(hp.unique_images.size()==5 && hp.find(openemperor::assets::BuildingVisualRole::HealthPost)->footprint_side==1 &&
        hp.find(openemperor::assets::BuildingVisualRole::HealthPost)->ground_y==27,"Health role deduplication/independent anchor");
    for(bool reuse:{false,true}) {
        auto bad=fixture.valid();bad["buildings"].erase("pottery");
        bad["buildings"]["health_post"]=all["buildings"]["pottery"];
        if(reuse)bad["buildings"]["clay_source"]=all["buildings"]["pottery"];
        fixture.save(bad);
        rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "Health role accepted 2x2 geometry on first decode/dedupe");
    }
    auto invalid_well=fixture.valid();
    invalid_well["buildings"].erase("pottery");
    invalid_well["buildings"]["well"]=all["buildings"]["pottery"];
    fixture.save(invalid_well);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "first-decode 2x2 Well accepted");
    invalid_well["buildings"]["clay_source"]=all["buildings"]["pottery"];
    fixture.save(invalid_well);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "deduplicated 2x2 Well bypassed geometry check");
    using R=openemperor::assets::BuildingVisualRole;
    for (unsigned level=0;level<3;++level)
        check(eight.household_role(level)==R::Household,
              "legacy household must supply all three levels");
    all["buildings"]["household_level_0"]=all["buildings"]["household"];
    all["buildings"]["household_level_1"]=all["buildings"]["pottery"];
    all["buildings"]["household_level_2"]=all["buildings"]["warehouse"];
    fixture.save(all);
    const auto staged=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(staged.unique_images.size()==5,"house stages duplicated physical assets");
    for (unsigned level=0;level<3;++level) {
        const auto role=openemperor::assets::household_stage_roles.at(level);
        check(staged.household_role(level)==role && staged.find(role)->footprint_side==2,
              "optional 2x2 household stage not selected");
    }
    check(staged.find(R::HouseholdLevel0)->image_index==staged.find(R::Household)->image_index,
          "legacy/stage did not deduplicate the same physical record");
    auto partial=all;
    partial["buildings"].erase("household_level_1");
    partial["buildings"].erase("household_level_2");
    fixture.save(partial);
    const auto partial_loaded=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(partial_loaded.household_role(0)==R::HouseholdLevel0 &&
          partial_loaded.household_role(1)==R::Household && partial_loaded.household_role(2)==R::Household,
          "missing optional stages must use legacy household");
    fixture.save({{"schema_version",1},{"mode","curated_building_preview"},
        {"buildings",{{"household_level_0",all["buildings"]["household_level_0"]}}}});
    const auto only_stage=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(only_stage.find(only_stage.household_role(0)) &&
          !only_stage.find(only_stage.household_role(1)) &&
          !only_stage.find(only_stage.household_role(2)),
          "stage-only profile must retain diagnostic fallback for absent levels");
    check(four.find(openemperor::assets::BuildingVisualRole::Farm)->footprint_side==1 &&
          four.find(openemperor::assets::BuildingVisualRole::ServicePost)->image_index==
              four.find(openemperor::assets::BuildingVisualRole::Farm)->image_index &&
          four.find(openemperor::assets::BuildingVisualRole::Market)->image_index==
              four.find(openemperor::assets::BuildingVisualRole::Farm)->image_index,
          "new one-cell roles did not share their synthetic texture");
    using O=openemperor::simulation::Object;
    check(openemperor::building_visual_role(O::Farm)==R::Farm &&
          openemperor::building_visual_role(O::ServicePost)==R::ServicePost &&
          openemperor::building_visual_role(O::Market)==R::Market &&
          openemperor::building_visual_role(O::FireWatch)==R::FireWatch &&
          openemperor::building_visual_role(O::Well)==R::Well,
          "new building visual roles are not selected from Object.kind");
    auto wrong_footprint=fixture.valid();
    wrong_footprint["buildings"]["farm"]=wrong_footprint["buildings"]["pottery"];
    fixture.save(wrong_footprint);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "two-cell image accepted for a one-cell building role");
    wrong_footprint=fixture.valid();
    wrong_footprint["buildings"]["fire_watch"]=wrong_footprint["buildings"]["pottery"];
    fixture.save(wrong_footprint);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "first-decode two-cell Fire Watch accepted");
    wrong_footprint["buildings"]["clay_source"]=wrong_footprint["buildings"]["pottery"];
    fixture.save(wrong_footprint); // clay_source sorts first and decodes before fire_watch.
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "deduplicated two-cell Fire Watch bypassed the one-cell check");
    auto wrong_stage=fixture.valid();
    wrong_stage["buildings"].erase("pottery");
    wrong_stage["buildings"]["household_level_0"]=all["buildings"]["farm"];
    fixture.save(wrong_stage);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "first-decode 1x1 household stage accepted");
    wrong_stage["buildings"]["farm"]=all["buildings"]["farm"];
    fixture.save(wrong_stage);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "deduplicated 1x1 household stage bypassed geometry validation");
    for (const auto [side,width,base]:{std::tuple{3U,238U,28800U},std::tuple{2U,118U,7200U}}) {
        auto bad_geometry=fixture.sg3;
        u16(bad_geometry,one_record+20,static_cast<std::uint16_t>(width));
        u32(bad_geometry,one_record+8,base);
        bad_geometry[one_record+55]=static_cast<std::uint8_t>(side);
        write(fixture.data/"DATA/wrong-stage.sg3",bad_geometry);
        write(fixture.data/"DATA/wrong-stage.555",fixture.bitmap);
        wrong_stage["buildings"].erase("farm");
        wrong_stage["buildings"]["household_level_0"]["archive"]="DATA/wrong-stage.sg3";
        fixture.save(wrong_stage);
        rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
                "non-Emperor-2x2 household stage accepted");
    }
    const auto raw_duplicate=R"({"schema_version":1,"mode":"curated_building_preview","buildings":{"pottery":{},"pottery":{}}})";
    { std::ofstream out(fixture.manifest);out<<raw_duplicate; }
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "duplicate semantic role accepted");
    auto bad=fixture.valid();bad["schema_version"]=2;fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "unknown schema accepted");
    bad=fixture.valid();bad["buildings"]["unsupported"]=bad["buildings"]["pottery"];
    fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "unknown role accepted");
    bad=fixture.valid();bad["buildings"]["household_level_3"]=bad["buildings"]["pottery"];
    fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "unknown household stage accepted");
    bad=fixture.valid();bad["unverified"]=true;fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "unknown root key accepted");
    bad=fixture.valid();bad["buildings"]["pottery"]["unverified"]=true;fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "unknown entry key accepted");
    bad=fixture.valid();bad["buildings"]["pottery"]["image_index"]=4;fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "outside record accepted");
    bad=fixture.valid();bad["buildings"]["pottery"]["archive"]="../outside.sg3";
    fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "traversal accepted");
    for (const auto& value:Json::array({Json("NaN"),Json("Infinity"),Json(5000)})) {
        bad=fixture.valid();bad["buildings"]["pottery"]["ground_anchor"][0]=value;
        fixture.save(bad);
        rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
                "invalid anchor accepted");
    }
    fixture.save(fixture.valid());
    const auto outside=fixture.root/"outside.sg3";
    fs::rename(fixture.data/"DATA/building.sg3",outside);
    fs::create_symlink(outside,fixture.data/"DATA/building.sg3");
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "symlink escape accepted");
    fs::remove(fixture.data/"DATA/building.sg3");fs::rename(outside,fixture.data/"DATA/building.sg3");
    const auto outside_bitmap=fixture.root/"outside.555";
    fs::rename(fixture.data/"DATA/building.555",outside_bitmap);
    fs::create_symlink(outside_bitmap,fixture.data/"DATA/building.555");
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "bitmap symlink escape accepted");
    fs::remove(fixture.data/"DATA/building.555");
    fs::rename(outside_bitmap,fixture.data/"DATA/building.555");
    auto changed=fixture.sg3;
    u16(changed,40680U+3U*72U+50U,256);write(fixture.data/"DATA/building.sg3",changed);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "wrong image type accepted");
    changed=fixture.sg3;u32(changed,40680U+3U*72U+16U,10);
    write(fixture.data/"DATA/building.sg3",changed);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "unverified mirror accepted");
    changed=fixture.sg3;u16(changed,40680U+3U*72U+20U,5000);
    u16(changed,40680U+3U*72U+22U,5000);write(fixture.data/"DATA/building.sg3",changed);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "oversize RGBA accepted");
    write(fixture.data/"DATA/building.sg3",fixture.sg3);
    write(fixture.data/"DATA/building.555",Bytes{0,0,0,0,1});
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "truncated bitmap accepted");
    changed=fixture.sg3;u32(changed,40680U+3U*72U+4U,12801);
    write(fixture.data/"DATA/building.sg3",changed);
    auto corrupt=fixture.bitmap;corrupt.push_back(1); // Omega literal lacks RGB555 bytes.
    write(fixture.data/"DATA/building.555",corrupt);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "malformed overlay accepted");
    write(fixture.data/"DATA/building.sg3",fixture.sg3);
    write(fixture.data/"DATA/building.555",fixture.bitmap);
    fixture.save(fixture.valid());
}
void pixels_and_depth(Fixture& fixture) {
    using namespace openemperor;
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL init");
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    check(SDL_CreateWindowAndRenderer("building software",600,600,SDL_WINDOW_HIDDEN,
                                      &window,&renderer),"software renderer");
    assets::BuildingVisualProfile profile;
    assets::BuildingVisualEntry entry;entry.ground_x=60;entry.ground_y=70;
    profile.unique_images.emplace_back();
    auto& image=profile.unique_images.front();image.width=120;image.height=90;
    image.pixels.assign(120U*90U*4U,255);
    for (std::size_t at=0;at<image.pixels.size();at+=4) {
        image.pixels[at]=0;image.pixels[at+1]=255;image.pixels[at+2]=0;
    }
    image.pixels[0]=255;image.pixels[1]=0; // Asymmetric top-left red pixel.
    BuildingSprite sprite;sprite.initialize(renderer,profile);
    check(sprite.texture_count()==1 && BuildingSprite::live_texture_count()==1,
          "building texture upload count");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer) &&
          sprite.draw({100,100},1,profile,entry),"1x building draw");
    check(pixel(renderer,40,30)[0]==255 && pixel(renderer,100,100)[1]==255,
          "explicit 1x anchor or full image size changed");
    check(SDL_RenderClear(renderer) && sprite.draw({300,300},4,profile,entry),"4x building draw");
    check(pixel(renderer,60,20)[0]==255 && pixel(renderer,300,300)[1]==255,
          "4x anchor or nearest pixel changed");
    check(SDL_RenderClear(renderer) && sprite.draw({320,330},4,profile,entry),"panned draw");
    check(pixel(renderer,80,50)[0]==255 && pixel(renderer,320,330)[1]==255,
          "camera pan changed relative anchor");
    check(SDL_RenderClear(renderer) && sprite.draw({100,100},1,profile,entry) &&
          sprite.draw({300,300},1,profile,entry),"two building instances");
    check(sprite.texture_count()==1 && pixel(renderer,300,300)[1]==255,
          "two instances did not share one texture");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer) &&
          sprite.draw({100,100},1,profile,entry,true),"translucent placement draw");
    const auto preview_corner=pixel(renderer,40,30);
    check(preview_corner[0]>100 && preview_corner[0]<200 &&
          SDL_RenderClear(renderer) && sprite.draw({100,100},1,profile,entry) &&
          pixel(renderer,40,30)[0]==255,
          "placement preview and final image used different anchors");
    struct Layer { SandboxVisualKey key;int kind; };
    std::array<Layer,3> layers{{{{90,100,SandboxVisualKind::Walker,1},0},
                                 {{100,100,SandboxVisualKind::Building,2},1},
                                 {{110,100,SandboxVisualKind::Walker,2},2}}};
    std::sort(layers.begin(),layers.end(),[](const Layer& a,const Layer& b){return a.key<b.key;});
    check(layers[0].kind==0 && layers[1].kind==1 && layers[2].kind==2,
          "ground-depth sort order");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer),"depth clear");
    for (const auto& layer:layers) {
        if (layer.kind==1) check(sprite.draw({100,100},1,profile,entry),"depth building");
        else {
            const SDL_FRect rect=layer.kind==0 ? SDL_FRect{85,70,20,20}:
                                                  SDL_FRect{100,85,20,20};
            check(SDL_SetRenderDrawColor(renderer,layer.kind==0 ? 0:255,0,
                                         layer.kind==0 ? 255:0,255) &&
                  SDL_RenderFillRect(renderer,&rect),"depth walker");
        }
    }
    check(pixel(renderer,95,85)==std::array<std::uint8_t,4>{0,255,0,255} &&
          pixel(renderer,105,90)==std::array<std::uint8_t,4>{255,0,0,255},
          "actual SDL depth pixels wrong");
    check(SandboxVisualKey{100,100,SandboxVisualKind::Building,1}<
          SandboxVisualKey{100,100,SandboxVisualKind::Walker,1},"tie breaker order");
    sprite.shutdown();check(BuildingSprite::live_texture_count()==0,"texture leaked");

    // Independent one-cell ground contract: the decoded Type-30 base occupies
    // the final 40 rows of each image. Its logical cell center is ground, so
    // the base spans ground_y-20 through ground_y+19 at 1x, independently of
    // any manifest parser or anchor computation in production code.
    assets::BuildingVisualProfile one_cell;
    const std::array<int,3> heights{61,48,98};
    const std::array<assets::BuildingVisualRole,3> roles{
        assets::BuildingVisualRole::Farm,assets::BuildingVisualRole::ServicePost,
        assets::BuildingVisualRole::Market};
    const std::array<std::array<std::uint8_t,4>,3> base_colors{{
        {{240,40,20,255}},{{20,220,60,255}},{{30,80,240,255}}}};
    for (std::size_t i=0;i<heights.size();++i) {
        assets::RgbaImage ground_image;
        ground_image.width=78;
        ground_image.height=static_cast<std::uint16_t>(heights[i]);
        ground_image.pixels.assign(static_cast<std::size_t>(78*heights[i]*4),0);
        for (int y=heights[i]-40;y<heights[i];++y)
            for (int x=0;x<78;++x) {
                const auto at=static_cast<std::size_t>((y*78+x)*4);
                std::copy(base_colors[i].begin(),base_colors[i].end(),
                          ground_image.pixels.begin()+static_cast<std::ptrdiff_t>(at));
            }
        assets::BuildingVisualEntry ground_entry;
        ground_entry.image_index=i;ground_entry.ground_x=39;
        ground_entry.ground_y=heights[i]-20;ground_entry.footprint_side=1;
        one_cell.entries[assets::role_index(roles[i])]=ground_entry;
        one_cell.unique_images.push_back(std::move(ground_image));
    }
    sprite.initialize(renderer,one_cell);
    constexpr std::array<std::uint8_t,4> background{20,30,40,255};
    for (std::size_t i=0;i<roles.size();++i) {
        const auto* ground_entry=one_cell.find(roles[i]);
        check(ground_entry!=nullptr,"one-cell ground entry missing");
        for (const int zoom:{1,2,4}) {
            const scene::Point ground{300,250};
            check(SDL_SetRenderDrawColor(renderer,background[0],background[1],background[2],255) &&
                  SDL_RenderClear(renderer) &&
                  sprite.draw(ground,zoom,one_cell,*ground_entry),
                  "one-cell normal ground draw");
            const int top=250-20*zoom,bottom=250+20*zoom-1;
            const int left=300-39*zoom,right=300+39*zoom-1;
            check(pixel(renderer,300,top)==base_colors[i] &&
                  pixel(renderer,300,bottom)==base_colors[i] &&
                  pixel(renderer,left,250)==base_colors[i] &&
                  pixel(renderer,right,250)==base_colors[i] &&
                  pixel(renderer,300,top-1)==background &&
                  pixel(renderer,300,bottom+1)==background &&
                  pixel(renderer,left-1,250)==background &&
                  pixel(renderer,right+1,250)==background,
                  "78x40 base missed the independent logical-cell boundary");
        }
        const scene::Point panned{337,279};
        check(SDL_SetRenderDrawColor(renderer,background[0],background[1],background[2],255) &&
              SDL_RenderClear(renderer) && sprite.draw(panned,2,one_cell,*ground_entry) &&
              pixel(renderer,337,279-40)==base_colors[i] &&
              pixel(renderer,337,279+39)==base_colors[i],
              "camera pan changed one-cell base alignment");
        check(SDL_SetRenderDrawColor(renderer,background[0],background[1],background[2],255) &&
              SDL_RenderClear(renderer) &&
              sprite.draw({300,250},1,one_cell,*ground_entry,true),
              "one-cell placement preview draw");
        const auto preview_top=pixel(renderer,300,230);
        check(preview_top!=background && preview_top!=base_colors[i] &&
              pixel(renderer,300,229)==background && pixel(renderer,300,269)!=background,
              "placement preview used a different one-cell ground boundary");
    }
    sprite.shutdown();check(BuildingSprite::live_texture_count()==0,
                            "one-cell ground textures leaked");

    // Decode independent synthetic SG3 bytes, rather than painting an assumed
    // rectangular base. Expected pixels are derived from the logical cell.
    auto watch_json=fixture.valid();
    watch_json["buildings"]=Json::object();
    watch_json["buildings"]["fire_watch"]={{"archive","DATA/one-cell.sg3"},
        {"image_index",3},{"ground_anchor",{39,41}},{"evidence","Synthetic watch base"}};
    fixture.save(watch_json);
    namespace perf=openemperor::performance;
    perf::set_enabled(true);perf::reset();
    const auto watch=assets::load_building_visual_profile(fixture.data,fixture.manifest);
    sprite.initialize(renderer,watch);
    check(perf::counter(perf::Counter::AssetDecodes)==1 && sprite.texture_count()==1 &&
          perf::counter(perf::Counter::TextureUploads)==1,"Watch decode/upload not shared");
    const auto& watch_entry=*watch.find(assets::BuildingVisualRole::FireWatch);
    perf::reset();
    for (const int zoom:{1,2,4}) for (const bool preview:{false,true}) {
        const scene::Point ground{300,250};
        check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer),"Watch clear");
        // Adjacent road center follows the independent 80x40 logical grid.
        const float gx=static_cast<float>(ground.x+40*zoom),gy=static_cast<float>(ground.y+20*zoom);
        const SDL_Vertex road[]={{{gx,gy-20*zoom},{0,0,1,1},{}},
            {{gx+39*zoom,gy},{0,0,1,1},{}},{{gx,gy+19*zoom},{0,0,1,1},{}},
            {{gx-39*zoom,gy},{0,0,1,1},{}}};
        constexpr int indices[]{0,1,2,0,2,3};
        check(SDL_RenderGeometry(renderer,nullptr,road,4,indices,6) &&
              sprite.draw(ground,zoom,watch,watch_entry,preview),"Watch beside road draw");
        const int top=250-20*zoom,bottom=250+20*zoom-1;
        const auto red=pixel(renderer,300,top);
        check(red[0]>100 && red[1]<30 && pixel(renderer,300,bottom)==red &&
              pixel(renderer,300,top-1)==background &&
              pixel(renderer,300,bottom+1)==background &&
              pixel(renderer,300-39*zoom,250)==red &&
              pixel(renderer,300-39*zoom-1,250)==background &&
              pixel(renderer,300+40*zoom,250+20*zoom)[2]==255,
              "decoded Watch base crossed its logical cell/adjacent road or stretched 78 to 80");
        // Selection uses the one-cell contour outside the unchanged sprite base.
        check(SDL_SetRenderDrawColor(renderer,255,255,255,255) &&
              SDL_RenderLine(renderer,300,static_cast<float>(top),static_cast<float>(300+40*zoom),250),"Watch selection contour");
        check(pixel(renderer,300+40*zoom,250)[0]==255,"selection did not use logical one-cell width");
    }
    check(SDL_RenderClear(renderer) && sprite.draw({337,279},2,watch,watch_entry) &&
          pixel(renderer,337,239)[0]==255 && pixel(renderer,337,318)[0]==255,
          "decoded Watch pan alignment");
    check(sprite.draw({100,100},1,watch,watch_entry) && sprite.texture_count()==1 &&
          perf::counter(perf::Counter::AssetDecodes)==0 &&
          perf::counter(perf::Counter::TextureUploads)==0,"Watch instances uploaded per draw");
    sprite.shutdown();perf::set_enabled(false);

    assets::BuildingVisualProfile four;
    const std::array<std::array<std::uint8_t,4>,4> colors{{
        {{0,255,255,255}},{{255,0,255,255}},{{255,255,0,255}},{{0,255,0,255}}}};
    for (std::size_t i=0;i<colors.size();++i) {
        assets::BuildingVisualEntry item;item.image_index=i;item.ground_x=10;item.ground_y=10;
        four.entries[i]=item;
        assets::RgbaImage colored;colored.width=20;colored.height=20;
        for (int p=0;p<400;++p)
            colored.pixels.insert(colored.pixels.end(),colors[i].begin(),colors[i].end());
        four.unique_images.push_back(std::move(colored));
    }
    sprite.initialize(renderer,four);
    check(sprite.texture_count()==4,"four unique assets did not upload four textures");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer) &&
          sprite.draw({100,200},1,four,*four.find(assets::BuildingVisualRole::ClaySource),true),
          "building hover preview draw");
    const auto translucent=pixel(renderer,100,200);
    check(translucent[0]<20 && translucent[1]>100 && translucent[1]<200 &&
          translucent[2]>100 && translucent[2]<200,
          "hover preview did not use translucent original sprite");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer),
          "role color clear");
    for (std::size_t i=0;i<colors.size();++i) {
        const auto* selected=four.find(assets::building_roles[i]);
        check(selected && sprite.draw({50.0+50.0*i,300},1,four,*selected),"role sprite draw");
        check(pixel(renderer,50+static_cast<int>(50*i),300)==colors[i],
              "role chose wrong uploaded texture");
    }
    for (std::size_t i=0;i<colors.size();++i) {
        const auto* selected=four.find(assets::building_roles[i]);
        check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer),
              "depth role clear");
        // A farther blue walker, one shared building draw, then a nearer red walker.
        const SDL_FRect back{90,90,20,20},front{100,100,20,20};
        check(SDL_SetRenderDrawColor(renderer,0,0,255,255) &&
              SDL_RenderFillRect(renderer,&back) &&
              sprite.draw({100,100},1,four,*selected) &&
              SDL_SetRenderDrawColor(renderer,255,0,0,255) &&
              SDL_RenderFillRect(renderer,&front),"role depth draw");
        check(pixel(renderer,95,95)==colors[i] &&
              pixel(renderer,105,105)==std::array<std::uint8_t,4>{255,0,0,255},
              "shared depth path did not occlude walkers by ground order");
    }
    sprite.shutdown();check(BuildingSprite::live_texture_count()==0,"four textures leaked");
    // Independently decoded 2x2 bases of different total heights share the front
    // cell ground: 80 base rows span ground_y-60 .. ground_y+19, not h/2.
    Json housing={{"schema_version",1},{"mode","curated_building_preview"},
                  {"buildings",Json::object()}};
    const std::array<std::uint16_t,3> house_heights{96,110,134};
    for (std::size_t stage=0;stage<house_heights.size();++stage) {
        auto metadata=fixture.sg3;
        u16(metadata,40680U+3U*72U+22U,house_heights[stage]);
        auto payload=fixture.bitmap;
        for (std::size_t p=4;p<payload.size();p+=2)
            u16(payload,p,static_cast<std::uint16_t>(stage==0 ? 0x7c00:stage==1 ? 0x03e0:0x001f));
        const auto name="stage-geometry-"+std::to_string(stage);
        write(fixture.data/("DATA/"+name+".sg3"),metadata);
        write(fixture.data/("DATA/"+name+".555"),payload);
        housing["buildings"][assets::building_role_name(assets::household_stage_roles[stage])]={
            {"archive","DATA/"+name+".sg3"},{"image_index",3},
            {"ground_anchor",{79,house_heights[stage]-20}},
            {"evidence","Independent synthetic 2x2 geometry and stage color"}};
    }
    fixture.save(housing);
    const auto stage_profile=assets::load_building_visual_profile(fixture.data,fixture.manifest);
    SDL_Texture* stage_target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,800,800);
    check(stage_target && SDL_SetRenderTarget(renderer,stage_target),"2x2 geometry target");
    sprite.initialize(renderer,stage_profile);
    perf::set_enabled(true);perf::reset();
    for (unsigned stage=0;stage<3;++stage) for (const int zoom:{1,2,4}) {
        const auto* item=stage_profile.find(stage_profile.household_role(stage));
        const std::array<std::uint8_t,4> color=stage==0 ? std::array<std::uint8_t,4>{255,0,0,255}:
            stage==1 ? std::array<std::uint8_t,4>{0,255,0,255}:std::array<std::uint8_t,4>{0,0,255,255};
        check(item && SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer) &&
              sprite.draw({400,500},zoom,stage_profile,*item),"2x2 stage geometry draw");
        const int top=500-60*zoom,bottom=500+20*zoom-1;
        check(pixel(renderer,400,top)==color && pixel(renderer,400,bottom)==color &&
              pixel(renderer,400,top-1)==background && pixel(renderer,400,bottom+1)==background &&
              pixel(renderer,400-79*zoom,500-20*zoom)==color &&
              pixel(renderer,400+79*zoom-1,500-20*zoom)==color,
              "2x2 stage foundation moved relative to the front cell");
        check(SDL_RenderClear(renderer) && sprite.draw({420,520},zoom,stage_profile,*item) &&
              pixel(renderer,420,520-60*zoom)==color,"2x2 stage pan moved the ground");
        check(SDL_RenderClear(renderer) && sprite.draw({400,500},zoom,stage_profile,*item,true) &&
              pixel(renderer,400,500)[stage]>100 && pixel(renderer,400,500)[stage]<200,
              "2x2 stage placement alpha changed geometry");
    }
    check(perf::counter(perf::Counter::AssetDecodes)==0 &&
          perf::counter(perf::Counter::TextureUploads)==0,"2x2 stage draw reloaded assets");
    perf::set_enabled(false);sprite.shutdown();
    check(SDL_SetRenderTarget(renderer,nullptr),"restore 2x2 geometry target");
    SDL_DestroyTexture(stage_target);
    check(BuildingSprite::live_texture_count()==0,"stage geometry textures leaked");
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
}
}
int main() {
    try { Fixture fixture;profile_checks(fixture);pixels_and_depth(fixture);
        std::cout<<"building manifest, anchor and software depth pixels passed\n";return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
