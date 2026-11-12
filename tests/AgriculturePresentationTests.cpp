// Authored input and scripted SDL checks; no native or human acceptance claim.
#include "EconomyStartViewFixture.h"
#include "AgricultureFixture.h"
#include "app/SandboxUiLayout.h"
#include "app/OriginalHouseInspection.h"
#include "assets/BuildingVisualProfile.h"
#include "persistence/RecoveryStore.h"
#include <nlohmann/json.hpp>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace q=oe_agriculture_fixture;
namespace fs=std::filesystem;
using q::check;
using Menu=oe::menu::MenuSession;
class Dialog final:public oe::menu::DialogAdapter {
public:
    void open_folder(SDL_Window*,Callback) override {}
    void open_file(SDL_Window*,Callback) override {}
    void cancel_pending() override {}
};
void key(Menu& menu,SDL_Keycode code,SDL_Keymod modifiers=SDL_KMOD_NONE) {
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;event.key.mod=modifiers;
    menu.handle_event(event);menu.advance();
}
void wait_map(Menu& menu,unsigned version) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking&&
        std::chrono::steady_clock::now()<deadline) {menu.advance();std::this_thread::yield();}
    check(menu.map_rules_can_start(),"Agriculture explicit map preflight refused");
    check(menu.map_rules_result().request.rule_version==version,"Preflight changed explicit mode");
}
void capture(Menu& menu,SDL_Renderer* renderer,const fs::path& path) {
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height),"Capture dimensions");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target&&SDL_SetRenderTarget(renderer,target.get()),"Own capture target");
    check(menu.render(),"Render Agriculture capture");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface&&SDL_SavePNG(surface.get(),path.string().c_str()),"Save Agriculture scripted capture");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore capture target");
}
void pure_contracts() {
    using A=oe::sandbox_ui::Action;
    const auto legacy=oe::sandbox_ui::make_layout(1280,800,1280,800,true,true,true,true,true,true,true,true,true,true);
    const auto agriculture=oe::sandbox_ui::make_layout(1280,800,1280,800,
        true,true,true,true,true,true,true,true,true,true,false,false,false,true);
    const auto contains=[](const auto& layout,A action) {return std::any_of(layout.buttons.begin(),layout.buttons.end(),[&](const auto& b){return b.action==action;});};
    for(const auto action:{A::OriginalRiceField,A::OriginalCabbageField,A::OriginalSoybeanField}) {
        check(!contains(legacy,action)&&contains(agriculture,action),"New crop tool leaked into historical layout or is missing");
        const auto button=std::find_if(agriculture.buttons.begin(),agriculture.buttons.end(),[&](const auto& b){return b.action==action;});
        check(button->rect.w>0&&button->rect.h>0&&agriculture.button_at(button->rect.x+1,button->rect.y+1)==action,"Crop drawing/input rectangles differ");
    }
    using oe::assets::agriculture_field_quantity_stage;
    check(agriculture_field_quantity_stage(0)==0&&agriculture_field_quantity_stage(99)==0&&
        agriculture_field_quantity_stage(100)==1&&agriculture_field_quantity_stage(1999)==1&&
        agriculture_field_quantity_stage(2000)==2&&agriculture_field_quantity_stage(8000)==5&&
        agriculture_field_quantity_stage(10000)==5&&!agriculture_field_quantity_stage(10001),"Verified quantity visual boundary differs");
    q::Fixture fixture;
    const auto historical=oe::persistence::prepare_original_production_rules_model(fixture.root.data);
    for(const auto building:{oe::original::ModelBuilding::RiceField,oe::original::ModelBuilding::CabbageField,oe::original::ModelBuilding::SoybeanField})
        check(!historical->supports_building(building)&&fixture.model->supports_building(building),"Historical model gained Agriculture capability");
    check(fixture.model->building_value(oe::original::ModelBuilding::RiceField,oe::original::BuildingField::Cost)==7,
        "Rice prepared model cost differs from independent authored fixture");
}
}
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        pure_contracts();
        oe::testing::economy::Temp temp;const auto data=temp.root/"data",app=temp.root/"app";
        oe::testing::economy::write(data/"Cities/A.map",oe::testing::economy::map_file(true,true,true));
        fs::create_directories(data/"Model");
        q::f::write_text(data/"Model/EmperorBuildingModels.txt",q::authored_model());
        const auto source=q::f::read_text(data/"Cities/A.map");
        oe::menu::Settings settings;settings.data_root=data;settings.last_map="Cities/A.map";
        oe::menu::write_settings(app,settings);
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");check(SDL_Init(SDL_INIT_VIDEO),"Initialize scripted SDL");
        check(SDL_CreateWindowAndRenderer("Agriculture presentation",1280,800,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create Agriculture window");
        Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
        check(!menu.original_agriculture_setup()&&!menu.diagnostic_button_rect("A Agriculture (rule 24)"),"Agriculture became a default");
        key(menu,SDLK_U);wait_map(menu,18);key(menu,SDLK_ESCAPE);
        key(menu,SDLK_A);wait_map(menu,24);
        check(menu.original_agriculture_setup(),"A did not explicitly select Agriculture");
        key(menu,SDLK_RETURN);check(menu.state()==Menu::State::Loading&&menu.render(),"Present deferred Loading");menu.advance();
        check(menu.state()==Menu::State::Playing&&menu.sandbox(),"Agriculture candidate failed");
        auto& view=*menu.sandbox();check(view.world().original_agriculture_enabled()&&view.paused(),"Wrong active mode or unpaused startup");
        check(!sim::agriculture_production_clock_verified && view.last_message().find("Production locked")!=std::string::npos,
            "Unverified Original timing lacks its explicit production lock status");
        key(menu,SDLK_R,SDL_KMOD_SHIFT);check(view.tool()==43,"Rice keyboard action differs from shared tool path");
        key(menu,SDLK_C,SDL_KMOD_SHIFT);check(view.tool()==44,"Cabbage keyboard action differs from shared tool path");
        key(menu,SDLK_S,SDL_KMOD_SHIFT);check(view.tool()==45,"Soy keyboard action differs from shared tool path");
        const auto before=view.world().snapshot();for(int frame=0;frame<30;++frame)check(menu.render(),"Pure Agriculture frame failed");
        check(view.world().snapshot()==before,"Presentation changed paid Agriculture state");
        key(menu,SDLK_F5);const auto document=oe::persistence::read_save(menu.settings().last_save);
        check(document.source_schema_version==40&&document.world.rule_version==24,"Actual F5 saved wrong generation");
        const auto bytes=q::f::read_text(menu.settings().last_save);view.tick_once();key(menu,SDLK_F9);
        check(view.world().snapshot()==before&&q::f::read_text(menu.settings().last_save)==bytes,"F9 changed generation or source bytes");
        const auto output=fs::current_path()/".local/agri-02/presentation";fs::create_directories(output);
        capture(menu,renderer,output/"agriculture-1280.png");
        check(q::f::read_text(data/"Cities/A.map")==source,"UI changed original-map fixture bytes");
        menu.shutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Agriculture explicit menu, typed tools, pure render, model capability, F5/F9 and quantity selector PASS\n";return 0;
    } catch(const std::exception& error) {
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;
    }
}
