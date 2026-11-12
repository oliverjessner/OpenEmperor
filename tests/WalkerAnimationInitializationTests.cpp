// Real paid Original startup under independently authored presentation pressure.
// The supplied installation is read-only; the only extra SG3 lives in a fresh
// ignored byte-preserved mirror. No cells, claims, permissions or World change.
#include "app/SandboxView.h"
#include "app/VisualSelection.h"
#include "assets/FireVisualProfile.h"
#include "assets/RoadVisualProfile.h"
#include "core/PerformanceDiagnostics.h"
#include "renderer/StoredGraphicsRenderer.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
namespace oe=openemperor;namespace assets=oe::assets;namespace maps=oe::maps;
namespace fs=std::filesystem;namespace perf=oe::performance;
using J=nlohmann::json;using Bytes=std::vector<std::uint8_t>;
constexpr std::uint64_t limit=64U*1024U*1024U;
void check(bool value,const std::string& why) {if(!value)throw std::runtime_error(why);}
bool contains(const fs::path& root,const fs::path& path) {
    auto a=root.begin(),b=path.begin();
    for(;a!=root.end();++a,++b)if(b==path.end()||*a!=*b)return false;
    return true;
}
void no_symlinks(const fs::path& path) {
    fs::path current;
    for(const auto& component:fs::absolute(path).lexically_normal()) {
        current/=component;std::error_code error;const auto status=fs::symlink_status(current,error);
        check(!error||error==std::errc::no_such_file_or_directory,"Inspect fixture ancestry");
        check(error||!fs::is_symlink(status),"Fixture paths may not traverse symlinks");
    }
}
std::string sha(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);check(bool(input),"Read supplied file "+path.string());
    auto context=std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>(EVP_MD_CTX_new(),EVP_MD_CTX_free);
    check(context&&EVP_DigestInit_ex(context.get(),EVP_sha256(),nullptr)==1,"Initialize SHA256");
    std::array<char,65536> buffer{};
    while(input) {input.read(buffer.data(),buffer.size());const auto count=input.gcount();
        if(count)check(EVP_DigestUpdate(context.get(),buffer.data(),static_cast<std::size_t>(count))==1,"Hash source bytes");}
    check(input.eof(),"Read complete supplied file");std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};unsigned size=0;
    check(EVP_DigestFinal_ex(context.get(),digest.data(),&size)==1&&size==32,"Finish SHA256");
    std::ostringstream result;result<<std::hex<<std::setfill('0');
    for(unsigned i=0;i<size;++i)result<<std::setw(2)<<static_cast<unsigned>(digest[i]);
    return result.str();
}
void write(const fs::path& path,const Bytes& bytes) {
    check(!fs::exists(path),"Preserve existing private output");std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));check(bool(out),"Write authored pressure fixture");
}
void u16(Bytes& bytes,std::size_t at,std::uint16_t value) {
    bytes.at(at)=static_cast<std::uint8_t>(value);bytes.at(at+1)=static_cast<std::uint8_t>(value>>8);
}
void u32(Bytes& bytes,std::size_t at,std::uint32_t value) {
    for(unsigned n=0;n<4;++n)bytes.at(at+n)=static_cast<std::uint8_t>(value>>(8*n));
}
template<class Profile> std::uint64_t image_bytes(const Profile& profile) {
    std::uint64_t result=0;for(const auto& image:profile.unique_images)result+=image.pixels.size();return result;
}
void configure(oe::SandboxView& view,const oe::VisualSelection& art,bool complete) {
    view.set_walker_visuals(art.walker,art.walker_source);
    view.set_building_visuals(art.building,art.building_source);
    view.set_road_visuals(art.road,art.road_source);
    view.set_fire_visuals(art.fire,art.fire_source,art.fire_fallback_reason);
    view.set_fire_inspector_visuals(art.fire_inspector,art.fire_inspector_source,art.fire_inspector_fallback_reason);
    view.set_market_walker_visuals(art.market_walker,art.market_walker_source,art.market_walker_fallback_reason);
    view.set_service_walker_visuals(art.service_walker,art.service_walker_source,art.service_walker_fallback_reason);
    view.set_health_walker_visuals(art.health_walker,art.health_walker_source,art.health_walker_fallback_reason);
    view.set_original_house_visuals(art.original_house,art.original_house_source,art.original_house_fallback_reason);
    view.set_original_building_visuals(art.original_building,art.original_building_source,art.original_building_fallback_reason);
    if(complete)view.set_walker_animation_visuals(art.walker_animation,art.walker_animation_source,art.walker_animation_fallback_reason);
}
void assert_historical(const oe::SandboxView& view,std::uint64_t walker_bytes) {
    check(view.building_texture_count()==64,"Complete legacy core/House/Original body pool lost its64 textures");
    const auto bodies=view.original_building_display_stats();
    check(bodies.active&&std::all_of(bodies.configured_roles.begin(),bodies.configured_roles.end(),[](bool v){return v;}),"Original native body/field tail yielded to optional walkers");
    const auto walkers=view.walker_display_stats();
    check(view.walker_texture_count()==176&&walkers.logical_rgba_bytes==walker_bytes,"Historical walker physical prefix changed");
    for(std::size_t i=0;i<walkers.roles.size();++i)
        check(walkers.roles[i].configured==(i<assets::walker_schema6_role_count),"Failed optional whole pack changed historical role ownership");
}
// Append actual eagerly decoded, uploaded RGBA owners without placing them in a
// cell or footprint. Their logical bytes come only from the production decoder.
std::size_t pressure_assets(maps::StoredGraphicsPlan& plan,std::uint64_t bytes) {
    check(bytes>0&&bytes%4==0,"Pressure must contain complete real RGBA pixels");
    std::vector<std::array<int,2>> dimensions;auto pixels=bytes/4;
    while(pixels>=1024U*4096U){dimensions.push_back({1024,4096});pixels-=1024U*4096U;}
    if(pixels>=1024){dimensions.push_back({1024,static_cast<int>(pixels/1024)});pixels%=1024;}
    if(pixels)dimensions.push_back({static_cast<int>(pixels),1});
    check(plan.assets.size()+dimensions.size()<=maps::stored_max_assets,"Pressure cannot raise the historical asset limit");
    Bytes archive(40680+(dimensions.size()+1)*72,0),bitmap{0,0,0,0};
    u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
    u32(archive,12,static_cast<std::uint32_t>(dimensions.size()+1));u32(archive,16,static_cast<std::uint32_t>(dimensions.size()+1));u32(archive,20,1);
    const std::string group="OE62Budget.bmp";std::copy(group.begin(),group.end(),archive.begin()+680);
    u32(archive,804,static_cast<std::uint32_t>(dimensions.size()+1));
    for(std::size_t i=0;i<dimensions.size();++i) {
        const auto width=dimensions[i][0],height=dimensions[i][1];Bytes payload;
        // Independently authored solid Omega run: positive count then RGB555.
        for(int y=0;y<height;++y)for(int remaining=width;remaining>0;) {
            const auto count=std::min(remaining,254);payload.push_back(static_cast<std::uint8_t>(count));
            for(int x=0;x<count;++x){payload.push_back(0x21);payload.push_back(0x04);}remaining-=count;
        }
        const auto record=static_cast<std::uint32_t>(i+1);const auto at=40680+std::size_t(record)*72;
        u32(archive,at,static_cast<std::uint32_t>(bitmap.size()));u32(archive,at+4,static_cast<std::uint32_t>(payload.size()));
        u16(archive,at+20,static_cast<std::uint16_t>(width));u16(archive,at+22,static_cast<std::uint16_t>(height));u16(archive,at+50,256);
        bitmap.insert(bitmap.end(),payload.begin(),payload.end());
        maps::StoredAsset asset;asset.record.id={"DATA/OE62Budget.sg3",record};asset.record.image_type=256;
        asset.record.width=static_cast<std::int16_t>(width);asset.record.height=static_cast<std::int16_t>(height);
        asset.record.data_length=static_cast<std::uint32_t>(payload.size());plan.assets.push_back(std::move(asset));
    }
    write(plan.data_root/"DATA/OE62Budget.sg3",archive);write(plan.data_root/"DATA/OE62Budget.555",bitmap);return dimensions.size();
}
} // namespace

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==4,"Expected explicit DATA REPO_ROOT FRESH_IGNORED_OUTPUT");
        for(int i=1;i<4;++i)no_symlinks(argv[i]);
        const auto supplied=fs::canonical(argv[1]),repo=fs::canonical(argv[2]);
        const auto output=fs::absolute(argv[3]).lexically_normal(),allowed=repo/".local/oe62";
        check(output!=allowed&&contains(allowed,output)&&!contains(supplied,output)&&!fs::exists(output),"Use a fresh ignored OE62 output separate from source data");
        check(fs::create_directories(output),"Reserve fresh private output");const auto data=output/"data";
        std::map<fs::path,std::string> sources;
        const auto copy_file=[&](const fs::path& relative) {
            const auto input=supplied/relative;no_symlinks(input);check(fs::is_regular_file(input),"Expected supplied regular file");
            const auto hash=sha(input);fs::create_directories((data/relative).parent_path());
            check(fs::copy_file(input,data/relative)&&sha(data/relative)==hash,"Copy source bytes without mutation");sources.emplace(relative,hash);
        };
        // Explicit asset/model directories only; do not enumerate personal saves.
        for(const auto* directory:{"DATA","Model"})for(const auto& entry:fs::recursive_directory_iterator(supplied/directory)) {
            check(!entry.is_symlink(),"Data mirror refuses symlink entries");
            if(entry.is_regular_file())copy_file(entry.path().lexically_relative(supplied));
        }
        copy_file("Cities/Xia.map");
        const auto art=oe::select_visual_profiles(assets::detect_compatibility(data,repo/"resources/compatibility"));
        check(!art.walker_animation.empty()&&!art.original_house.empty()&&!art.original_building.empty(),"Explicit data must qualify for historical bodies and complete walkers");
        auto buildings=assets::load_building_visual_profile(data,art.building);
        assets::append_original_house_visual_profile(data,art.original_house,buildings);
        assets::append_original_building_visual_profile(data,art.original_building,buildings);
        check(buildings.unique_images.size()==64,"Independent current body oracle must have64 physical images");
        auto walkers=assets::load_walker_visual_profile(data,art.walker);
        assets::append_fire_inspector_visual_profile(data,art.fire_inspector,walkers);
        assets::append_market_visual_profile(data,art.market_walker,walkers);
        assets::append_service_visual_profile(data,art.service_walker,walkers);
        assets::append_health_visual_profile(data,art.health_walker,walkers);
        const auto body_bytes=image_bytes(buildings),walker_bytes=image_bytes(walkers);
        const auto complete_bytes=image_bytes(assets::load_walker_visual_profile(data,art.walker_animation));
        const auto road_bytes=image_bytes(assets::load_road_visual_profile(data,art.road));
        const auto fire_bytes=assets::fire_rgba_bytes(assets::load_fire_visual_profile(data,art.fire));
        check(walkers.unique_images.size()==176&&complete_bytes>walker_bytes,"Actual complete replacement must consume more than the old176-image pool");
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software")&&SDL_Init(SDL_INIT_VIDEO),"Initialize isolated dummy/software renderer");
        check(SDL_CreateWindowAndRenderer("Original walker initialization priority",1280,800,SDL_WINDOW_HIDDEN,&window,&renderer),"Create real renderer");
        const auto session=[&] {return maps::load_stored_map_session(data,"Cities/Xia.map",maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8,maps::GreatWallPresentationMode::Automatic,{},true);};
        auto reference=oe::SandboxView::create_original_deadline_food_dispatch(session(),oe::original::Difficulty::Normal,true);
        reference->configure_save(data,"Cities/Xia.map",output/"reference.json");configure(*reference,art,false);reference->initialize(window,renderer);
        assert_historical(*reference,walker_bytes);const auto expected=reference->world().snapshot();const auto expected_message=reference->last_message();
        check(expected_message.starts_with("Paid 4-house starter:")&&reference->paused(),"Historical paid starter status changed");
        reference->save_now();const auto expected_save=sha(output/"reference.json");reference->shutdown();
        auto normal=oe::SandboxView::create_original_deadline_food_dispatch(session(),oe::original::Difficulty::Normal,true);
        normal->configure_save(data,"Cities/Xia.map",output/"normal-complete.json");configure(*normal,art,true);normal->initialize(window,renderer);
        check(normal->walker_animations_active()&&normal->walker_texture_count()==240&&normal->building_texture_count()==64,
            "With sufficient real headroom the complete twelve-role pack must activate after all historical bodies");
        check(normal->world().snapshot()==expected&&normal->last_message()==expected_message&&normal->paused(),
            "Successful optional startup replacement changed paid World or starter status");
        normal->save_now();check(sha(output/"normal-complete.json")==expected_save,"Successful optional startup changed canonical save bytes");normal->shutdown();
        auto pressured=session();std::uint64_t background_bytes=0;std::size_t background_uploads=0;
        {oe::StoredGraphicsRenderer background(pressured.plan);background.initialize(renderer);background_bytes=background.plan().logical_texture_bytes;background_uploads=background.upload_count();background.shutdown();}
        const auto other=background_bytes+body_bytes+road_bytes+fire_bytes+walker_bytes;
        check(other<limit,"Historical real session needs headroom for authored pressure");const auto filler_bytes=limit-other;
        const auto pressure_count=pressure_assets(pressured.plan,filler_bytes);
        // Prove every byte actually passed source containment, decode and upload.
        {oe::StoredGraphicsRenderer background(pressured.plan);background.initialize(renderer);
            check(background.plan().logical_texture_bytes==background_bytes+filler_bytes&&background.upload_count()==background_uploads+pressure_count,"Authored unused assets did not reserve actual eager RGBA bytes");
            for(std::size_t i=pressured.plan.assets.size()-pressure_count;i<pressured.plan.assets.size();++i)
                check(background.plan().assets[i].status==maps::StoredStatus::Rendered&&background.plan().assets[i].decode_succeeded,"Pressure asset must decode through production reader");
            background.shutdown();}
        auto restore_session=pressured;
        auto view=oe::SandboxView::create_original_deadline_food_dispatch(std::move(pressured),oe::original::Difficulty::Normal,true);
        view->configure_save(data,"Cities/Xia.map",output/"pressured.json");configure(*view,art,true);view->initialize(window,renderer);
        assert_historical(*view,walker_bytes);
        check(!view->walker_animations_active()&&!view->walker_animation_fallback_reason().empty(),"Whole optional pack must fall back atomically when old native bodies reserve the budget");
        check(view->world().snapshot()==expected&&view->last_message()==expected_message&&view->paused(),"Optional startup replacement changed paid World or starter status");
        view->save_now();check(sha(output/"pressured.json")==expected_save,"Presentation pressure changed canonical complete save bytes");
        view->load_now();check(view->world().snapshot()==expected&&view->paused(),"Pressure save restore changed complete World");
        const auto loaded_message=view->last_message();view->set_walker_animation_visuals(art.walker_animation,art.walker_animation_source);
        check(view->last_message()==loaded_message&&view->world().snapshot()==expected,"Failed optional replacement changed loaded status or World");
        perf::set_enabled(true);perf::reset();check(view->render(),"Render unchanged real paid state");
        for(std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)check(perf::counter(static_cast<perf::Counter>(i))==0,"Ordinary pressured frame performed preparation or gameplay work");
        perf::set_enabled(false);check(view->world().snapshot()==expected,"Ordinary pressured render mutated World");view->shutdown();
        auto restored=oe::SandboxView::create_original_deadline_food_dispatch(std::move(restore_session),oe::original::Difficulty::Normal,false);
        restored->configure_save(data,"Cities/Xia.map",output/"restored.json",oe::persistence::read_save(output/"pressured.json"));
        configure(*restored,art,true);restored->initialize(window,renderer);assert_historical(*restored,walker_bytes);
        check(!restored->walker_animations_active()&&restored->world().snapshot()==expected&&restored->paused()&&
            restored->last_message()=="Loaded tick 0 (paused)","Deferred optional restore lost canonical paid state or loaded status");
        restored->save_now();check(sha(output/"restored.json")==expected_save,"Deferred optional restore changed canonical save bytes");restored->shutdown();
        J source_rows=J::array();for(const auto& [relative,hash]:sources){check(sha(supplied/relative)==hash&&sha(data/relative)==hash,"Supplied or mirrored source bytes changed");source_rows.push_back({{"path",relative.generic_string()},{"sha256",hash}});}
        J report={{"status","PASS"},{"scope","Actual Original Rule18 paid startup and restore; independently authored unused presentation pressure; scripted dummy/software, no human acceptance"},
            {"budget",limit},{"background_bytes",background_bytes},{"body_bytes",body_bytes},{"legacy_walker_bytes",walker_bytes},{"complete_walker_bytes",complete_bytes},{"road_bytes",road_bytes},{"fire_bytes",fire_bytes},{"authored_pressure_bytes",filler_bytes},{"authored_pressure_textures",pressure_count},{"retained_body_textures",64},{"retained_walker_textures",176},{"complete_pack_active_under_pressure",false},{"complete_pack_active_with_headroom",true},{"full_World_equal",true},{"canonical_save_equal",true},{"initial_save_restore_equal",true},{"source_files_unchanged",source_rows}};
        const auto text=report.dump(2)+"\n";write(output/"report.json",Bytes(text.begin(),text.end()));
        SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();
        std::cout<<"Original startup native-body/walker priority, atomic whole-pack fallback and source-byte guards PASS\n";return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<"Walker initialization priority: "<<error.what()<<'\n';return 1;
    }
}
