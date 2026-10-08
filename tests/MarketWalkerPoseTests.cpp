#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace assets=oe::assets;
namespace perf=oe::performance;
void check(bool okay,const char* message) { if (!okay) throw std::runtime_error(message); }
constexpr std::array market_roles{sim::CourierRole::MarketFoodInbound,sim::CourierRole::MarketPotteryInbound,
    sim::CourierRole::MarketFoodDistribution,sim::CourierRole::MarketPotteryDistribution};
constexpr std::array food_roles{sim::CourierRole::Food,sim::CourierRole::MarketFoodInbound,
    sim::CourierRole::MarketPotteryInbound,sim::CourierRole::MarketFoodDistribution,
    sim::CourierRole::MarketPotteryDistribution};
assets::WalkerVisualProfile profile() {
    assets::WalkerVisualProfile result;result.schema_version=4;
    for (auto& entry:result.roles) {
        auto& role=entry.emplace();role.ticks_per_frame=2;role.idle_frame=0;role.frames.resize(8);
        for (std::size_t d=0;d<4;++d) role.clips[d]={2*d,2*d+1};
    }
    return result;
}
void contracts(const assets::WalkerVisualProfile& visuals) {
    static_assert(assets::walker_role_index(assets::WalkerVisualRole::FireInspector)==3);
    static_assert(assets::walker_role_index(assets::WalkerVisualRole::Supplier)==4);
    static_assert(assets::walker_role_index(assets::WalkerVisualRole::Distributor)==5);
    for (const auto role:food_roles) {
        const bool supplier=role==sim::CourierRole::Food || role==sim::CourierRole::MarketFoodInbound ||
            role==sim::CourierRole::MarketPotteryInbound;
        check(oe::walker_visual_role(role)==(supplier ? assets::WalkerVisualRole::Supplier:
            assets::WalkerVisualRole::Distributor),"Food/Market logical family mapping differs.");
        sim::CourierState c;c.id=static_cast<sim::CourierId>(901);c.owner=static_cast<sim::BuildingId>(800);
        c.target=static_cast<sim::BuildingId>(1);c.role=role;c.enabled=true;c.phase=sim::CourierPhase::ToWarehouse;
        c.path={{2,2},{3,2},{3,3},{2,3},{2,2}};c.cargo=3;
        constexpr std::array dirs{assets::StorageDirection::PosX,assets::StorageDirection::PosY,
            assets::StorageDirection::NegX,assets::StorageDirection::NegY};
        for (std::size_t edge=0;edge<4;++edge) for (const auto phase:{sim::CourierPhase::ToWarehouse,sim::CourierPhase::Returning}) {
            c.path_vertex=edge;c.phase=phase;c.cargo=phase==sim::CourierPhase::Returning ? 0:3;
            const auto a=oe::walker_pose(c,0,visuals),b=oe::walker_pose(c,2,visuals);
            const auto d=assets::direction_index(dirs[edge]);
            check(a.role==oe::walker_visual_role(role)&&a.moving&&a.direction==dirs[edge]&&
                a.loaded==(c.cargo>0)&&a.frame==2*d&&b.frame==2*d+1,
                "Current authoritative edge/cargo/tick does not select the family pose.");
            check(oe::walker_pose(c,0,visuals).frame==a.frame&&oe::walker_pose(c,4,visuals).frame==a.frame,
                "Render frequency/pause/loop changed World-tick phase.");
            c.route_pending=true;c.edge_progress=1;
            check(oe::walker_pose(c,2,visuals).moving&&oe::walker_live_visible(c),"Protected begun edge disappeared.");
            c.edge_progress=0;
            const auto wait=oe::walker_pose(c,2,visuals);
            check(!wait.moving&&wait.direction==dirs[edge]&&wait.frame==2*d&&
                oe::walker_pose(c,999,visuals).frame==wait.frame&&oe::walker_live_visible(c),
                "Pending route walked in place/lost static facing/visibility.");
            c.route_pending=false;
        }
        c.phase=sim::CourierPhase::Returning;c.path={{3,3}};c.path_vertex=0;c.route_pending=true;c.edge_progress=0;
        check(oe::walker_live_visible(c)&&!oe::walker_pose(c,100,visuals).moving&&
            oe::walker_pose(c,100,visuals).frame==0,"One-cell pending route confused with hidden home Idle.");
        c.path={{3,3},{4,4}};c.route_pending=false;
        check(oe::walker_pose(c,0,visuals).fallback==oe::WalkerFallback::InvalidEdge,"Diagonal fabricated a direction.");
        c.path={{std::numeric_limits<int>::max(),0},{std::numeric_limits<int>::min(),0}};
        check(oe::walker_pose(c,0,visuals).fallback==oe::WalkerFallback::InvalidEdge,"Extreme edge overflowed.");
        c.path={{3,3},{4,3}};auto partial=visuals;
        partial.roles[assets::walker_role_index(*oe::walker_visual_role(role))]->clips[0].clear();
        check(oe::walker_pose(c,0,partial).fallback==oe::WalkerFallback::UnmappedDirection&&
            !oe::walker_pose(c,0,partial).frame,"Partial custom direction lost diagnostic fallback.");
        c.phase=sim::CourierPhase::IdleAtWorkshop;
        check(!oe::walker_pose(c,10,visuals).moving&&oe::walker_pose(c,10,visuals).frame==0&&
            oe::walker_live_visible(c)==(role==sim::CourierRole::Food),"Market idle ghost or legacy Food idle changed.");
    }
    for (const auto role:market_roles) for (bool enabled:{false,true}) for (int cargo:{0,3})
        for (const auto phase:{sim::CourierPhase::IdleAtWorkshop,sim::CourierPhase::ToWarehouse,sim::CourierPhase::Returning}) {
            sim::CourierState c;c.role=role;c.enabled=enabled;c.cargo=cargo;c.phase=phase;
            check(oe::walker_live_visible(c)==(phase!=sim::CourierPhase::IdleAtWorkshop),
                "Market visibility incorrectly depends on enabled/cargo.");
        }
    for (const auto role:{sim::CourierRole::Clay,sim::CourierRole::Pottery,sim::CourierRole::Household}) {
        sim::CourierState c;c.role=role;c.enabled=true;c.phase=sim::CourierPhase::Returning;
        c.path={{3,3},{4,3}};c.route_pending=true;
        const auto a=oe::walker_pose(c,0,visuals),b=oe::walker_pose(c,999,visuals);
        check(!a.direction&&!a.moving&&a.frame==0&&b.frame==a.frame&&oe::walker_live_visible(c),
            "New family changed old waiting/idle contract.");
    }
    check(oe::walker_visual_role(sim::CourierRole::Service)==assets::WalkerVisualRole::Service&&
          oe::walker_visual_role(sim::CourierRole::HealthWorker)==assets::WalkerVisualRole::HealthWorker,
        "Service/Health append-only visual mapping is wrong.");
}
constexpr int extent=24;
auto gate_permissions() {
    std::vector<sim::MapCellPermission> cells(extent*extent,{true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});
    const auto index=[](sim::Cell p){return static_cast<std::size_t>(p.y*extent+p.x);};
    sim::FixedGatePassage gate;gate.id={91};
    for (int y=7;y<=9;++y) for (int x=5;x<=9;++x) {
        const sim::Cell p{x,y};gate.protected_footprint.push_back(p);
        cells[index(p)]={false,false,true,0,sim::BuildBlocker::GateSolidPart,sim::BuildBlocker::OriginalStructure};
    }
    for (int y=7;y<=9;++y) {gate.corridor.push_back({7,y});cells[index({7,y})].road_allowed=true;}
    gate.openings={sim::Cell{7,6},sim::Cell{7,10}};
    return std::make_shared<const sim::MapPermissions>(extent,extent,1,std::move(cells),std::vector{gate});
}
void actual_transport(const assets::WalkerVisualProfile& visuals) {
    const auto policy=gate_permissions();sim::World decorated(policy,sim::RulesProfile::CityV16,3);
    const auto put=[&](sim::CommandType type,sim::Cell cell){check(decorated.execute({type,cell}).accepted,"Paid gate fixture placement.");};
    for (const auto cell:{sim::Cell{1,12},sim::Cell{1,15},sim::Cell{4,12},sim::Cell{4,15}}) put(sim::CommandType::PlaceHousehold,cell);
    put(sim::CommandType::PlaceClaySource,{0,1});put(sim::CommandType::PlacePottery,{3,1});
    put(sim::CommandType::PlaceWarehouse,{6,1});put(sim::CommandType::PlaceFarm,{6,4});
    put(sim::CommandType::PlaceMarket,{10,12});put(sim::CommandType::PlaceServicePost,{14,12});
    for (int x=0;x<=7;++x) put(sim::CommandType::PlaceRoad,{x,3});
    for (int y=4;y<=11;++y) put(sim::CommandType::PlaceRoad,{7,y});
    for (int x=1;x<=14;++x) put(sim::CommandType::PlaceRoad,{x,11});
    for (int y=12;y<=14;++y) put(sim::CommandType::PlaceRoad,{7,y});
    for (int x=1;x<=7;++x) put(sim::CommandType::PlaceRoad,{x,14});
    auto plain=sim::World::restore(decorated.snapshot(),policy);
    std::array<bool,4> roles{},directions{},returned{};bool in_gate=false,simultaneous=false,cut_done=false,wait_seen=false;
    std::optional<sim::CourierId> interrupted;int waiting_ticks=0;bool repaired=false;
    std::optional<sim::BuildingId> paused_market;std::uint64_t resume_at=0;bool paused_trip_seen=false;
    const auto identical=[&] {check(decorated.snapshot()==plain.snapshot()&&decorated.map_permissions()==policy&&
        plain.map_permissions()==policy&&decorated.navigation_valid()&&decorated.production_balance_valid()&&
        decorated.food_balance_valid()&&decorated.city_economy_valid(),"Complete transport/policy/conservation diverged.");};
    for (int ticks=0;ticks<1400;++ticks) {
        decorated.tick();plain.tick();identical();bool food=false,pottery=false;
        for (const auto& c:decorated.couriers()) {
            const auto found=std::find(market_roles.begin(),market_roles.end(),c.role);if(found==market_roles.end())continue;
            const auto r=static_cast<std::size_t>(found-market_roles.begin());
            const auto before=decorated.snapshot();perf::set_enabled(true);perf::reset();
            const auto pose=oe::walker_pose(c,decorated.ticks(),visuals);const bool visible=oe::walker_live_visible(c);
            for(const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,perf::Counter::WorldExecutes,
                perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,
                perf::Counter::TextureUploads,perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::SimulationTicks})
                check(perf::counter(counter)==0,"Pure pose/visibility performed extra work.");
            perf::set_enabled(false);check(before==decorated.snapshot(),"Pose changed full authoritative snapshot.");
            check(visible==(c.phase!=sim::CourierPhase::IdleAtWorkshop),"Real live phase visibility differs.");
            if(paused_market&&c.owner==*paused_market&&!decorated.building(c.owner).operating_enabled&&
                c.phase!=sim::CourierPhase::IdleAtWorkshop){
                check(visible,"Pausing the owner hid an existing transport.");paused_trip_seen=true;
            }
            if(pose.moving){roles[r]=true;directions[assets::direction_index(*pose.direction)]=true;
                if(c.path_vertex<c.path.size()&&decorated.fixed_passage(c.path[c.path_vertex]))in_gate=true;}
            if(c.phase==sim::CourierPhase::Returning&&visible){check(c.cargo==0&&!pose.loaded,"Returning figure carries invented cargo.");returned[r]=true;}
            food=food||(c.role==sim::CourierRole::MarketFoodDistribution&&c.phase==sim::CourierPhase::ToWarehouse);
            pottery=pottery||(c.role==sim::CourierRole::MarketPotteryDistribution&&c.phase==sim::CourierPhase::ToWarehouse);
            if(!cut_done&&c.role==sim::CourierRole::MarketFoodInbound&&c.phase==sim::CourierPhase::ToWarehouse&&
                c.path_vertex<c.path.size()&&c.path[c.path_vertex]==sim::Cell{7,7}&&c.edge_progress==1)interrupted=c.id;
        }
        simultaneous=simultaneous||(food&&pottery);
        if(!paused_market&&food&&pottery){
            for(const auto& c:decorated.couriers())if(c.role==sim::CourierRole::MarketFoodDistribution)paused_market=c.owner;
            check(decorated.execute(sim::set_building_operation(*paused_market,false)).accepted&&
                plain.execute(sim::set_building_operation(*paused_market,false)).accepted,"Ordinary Market pause failed.");
            resume_at=decorated.ticks()+20;identical();
        }
        if(paused_market&&!decorated.building(*paused_market).operating_enabled&&decorated.ticks()>=resume_at){
            check(decorated.execute(sim::set_building_operation(*paused_market,true)).accepted&&
                plain.execute(sim::set_building_operation(*paused_market,true)).accepted,"Ordinary Market resume failed.");identical();
        }
        if(interrupted&&!cut_done){
            const auto position=decorated.courier_position(*interrupted);
            check(decorated.execute({sim::CommandType::RemoveRoad,{7,10}}).accepted&&
                plain.execute({sim::CommandType::RemoveRoad,{7,10}}).accepted,"Allowed future gate road cut failed.");
            check(decorated.courier_position(*interrupted)==position&&decorated.courier(*interrupted).route_pending&&
                oe::walker_pose(decorated.courier(*interrupted),decorated.ticks(),visuals).moving,"Begun edge was not retained.");cut_done=true;identical();
        }
        if(cut_done&&!repaired&&interrupted&&decorated.courier(*interrupted).route_pending&&
            decorated.courier(*interrupted).edge_progress==0){
            const auto& c=decorated.courier(*interrupted);check(oe::walker_live_visible(c)&&!oe::walker_pose(c,decorated.ticks(),visuals).moving,
                "Stopped delivery disappeared or walked in place.");wait_seen=true;++waiting_ticks;
            auto restored=sim::World::restore(decorated.snapshot(),policy);check(restored.snapshot()==decorated.snapshot()&&
                restored.map_permissions()==policy&&oe::walker_pose(restored.courier(*interrupted),restored.ticks(),visuals).frame==
                oe::walker_pose(c,decorated.ticks(),visuals).frame,"Waiting restore lost policy/state/frame.");
            if(waiting_ticks==20){check(decorated.execute({sim::CommandType::PlaceRoad,{7,10}}).accepted&&
                plain.execute({sim::CommandType::PlaceRoad,{7,10}}).accepted,"Paid repair failed.");repaired=true;identical();}
        }
        if(ticks%31==0){auto restored=sim::World::restore(decorated.snapshot(),policy);check(restored.snapshot()==decorated.snapshot(),"Active restore changed full snapshot.");
            for(const auto& c:decorated.couriers())if(oe::walker_visual_role(c.role))check(oe::walker_pose(c,decorated.ticks(),visuals).frame==
                oe::walker_pose(restored.courier(c.id),restored.ticks(),visuals).frame,"Load changed tick phase.");}
    }
    check(std::all_of(roles.begin(),roles.end(),[](bool x){return x;})&&std::all_of(returned.begin(),returned.end(),[](bool x){return x;})&&
        std::all_of(directions.begin(),directions.end(),[](bool x){return x;})&&in_gate&&simultaneous&&cut_done&&wait_seen&&repaired&&
        paused_trip_seen&&decorated.taxes_collected_total()>0,
        "Paid gate fixture did not close four roles/return/directions/tax/cut-repair/ownerpause.");
}
void understaffed_existing_trip(const assets::WalkerVisualProfile& visuals) {
    const std::vector<std::uint8_t> mask(24U*8U,1);
    sim::World world(24,8,mask,sim::RulesProfile::CityV11,3);
    const auto put=[&](sim::CommandType type,sim::Cell cell){check(world.execute({type,cell}).accepted,
        "Understaffed ordinary paid fixture placement.");return *world.building_owner_at(cell);};
    const auto clay=put(sim::CommandType::PlaceClaySource,{0,1});
    const auto pottery=put(sim::CommandType::PlacePottery,{3,1});
    put(sim::CommandType::PlaceWarehouse,{6,1});put(sim::CommandType::PlaceFarm,{9,4});
    put(sim::CommandType::PlaceServicePost,{18,1});const auto market=put(sim::CommandType::PlaceMarket,{14,4});
    for(const auto p:{sim::Cell{1,5},sim::Cell{4,5},sim::Cell{7,5}})put(sim::CommandType::PlaceHousehold,p);
    for(int x=0;x<=18;++x)check(world.execute({sim::CommandType::PlaceRoad,{x,3}}).accepted,"Understaffed road.");
    for(const auto p:{sim::Cell{1,4},sim::Cell{4,4},sim::Cell{7,4},sim::Cell{18,2}})
        check(world.execute({sim::CommandType::PlaceRoad,p}).accepted,"Understaffed entrance.");
    auto control=sim::World::restore(world.snapshot(),mask);
    const auto step=[&]{world.tick();control.tick();check(world.snapshot()==control.snapshot(),
        "Understaffed transport changed complete stock/reservation/path/counter state.");};
    const auto operation=[&](sim::BuildingId id,bool enabled){
        check(world.execute(sim::set_building_operation(id,enabled)).accepted&&
            control.execute(sim::set_building_operation(id,enabled)).accepted&&world.snapshot()==control.snapshot(),
            "Ordinary operation control diverged.");};
    check(world.workforce_supply()==18&&!world.building_staffed(market),"Market fixture was not genuinely unstaffed.");
    for(int i=0;i<200&&world.building(market).food_stock==0;++i)step();
    check(world.building(market).food_stock>0,"Unstaffed Market did not receive actual Farm supply.");
    operation(clay,false);operation(pottery,false);check(world.building_staffed(market),"Normal pauses did not release workforce.");
    std::optional<sim::CourierId> id;
    for(int i=0;i<30&&!id;++i){step();for(const auto& c:world.couriers())if(c.role==sim::CourierRole::MarketFoodDistribution&&
        c.phase==sim::CourierPhase::ToWarehouse&&c.cargo>0)id=c.id;}
    check(id.has_value(),"Real Market distribution did not dispatch.");
    operation(clay,true);operation(pottery,true);check(!world.building_staffed(market),"Market did not lose actual staffing.");
    bool empty_return=false;int budget=300;
    while(budget--&&world.courier(*id).phase!=sim::CourierPhase::IdleAtWorkshop){
        const auto& c=world.courier(*id);check(oe::walker_live_visible(c)&&oe::walker_pose(c,world.ticks(),visuals).frame,
            "Existing genuinely understaffed trip disappeared.");
        if(c.phase==sim::CourierPhase::Returning){check(c.cargo==0,"Empty return carries invented supply.");empty_return=true;}
        step();
    }
    check(budget>=0&&empty_return&&!oe::walker_live_visible(world.courier(*id))&&world.food_balance_valid(),
        "Unstaffed travelling/emptyreturn/home visibility did not follow the existing delivery.");
}
void legacy_food_transport(const assets::WalkerVisualProfile& visuals) {
    const std::vector<std::uint8_t> mask(16U*10U,1);
    sim::World decorated(16,10,mask,sim::RulesProfile::CityV7,1);
    const auto put=[&](sim::CommandType type,sim::Cell cell){
        check(decorated.execute({type,cell}).accepted,"Legacy Food paid fixture placement.");};
    put(sim::CommandType::PlaceClaySource,{1,2});put(sim::CommandType::PlacePottery,{4,2});
    put(sim::CommandType::PlaceWarehouse,{7,2});put(sim::CommandType::PlaceFarm,{9,5});
    put(sim::CommandType::PlaceHousehold,{12,2});put(sim::CommandType::PlaceHousehold,{12,5});
    for(int x=1;x<=12;++x)put(sim::CommandType::PlaceRoad,{x,3});
    put(sim::CommandType::PlaceRoad,{9,4});put(sim::CommandType::PlaceRoad,{12,4});
    auto plain=sim::World::restore(decorated.snapshot(),mask);
    bool outbound=false,returning=false;std::array<bool,4> directions{};
    for(int tick=0;tick<1800;++tick){decorated.tick();plain.tick();
        for(const auto& c:decorated.couriers())if(c.role==sim::CourierRole::Food){
            const auto pose=oe::walker_pose(c,decorated.ticks(),visuals);
            check(pose.role==assets::WalkerVisualRole::Supplier&&oe::walker_live_visible(c),
                "Legacy direct Food lost Supplier/old idle visibility.");
            check(decorated.building(c.owner).kind==sim::Object::Farm,
                "Legacy Food ownership was inferred from the visual family.");
            if(pose.moving)directions[assets::direction_index(*pose.direction)]=true;
            if(c.phase==sim::CourierPhase::ToWarehouse&&c.cargo>0){
                check(decorated.building(c.target).kind==sim::Object::Household,
                    "Legacy direct Food was rerouted through Market.");outbound=true;
            }
            if(c.phase==sim::CourierPhase::Returning){check(c.cargo==0&&!pose.loaded,"Legacy return invented cargo.");returning=true;}
        }
        check(decorated.snapshot()==plain.snapshot()&&decorated.food_balance_valid()&&
            decorated.production_balance_valid()&&decorated.city_economy_valid(),
            "Legacy Food complete snapshot/stock/reservations/tax changed.");
    }
    check(outbound&&returning&&std::all_of(directions.begin(),directions.end(),[](bool x){return x;})&&
        decorated.taxes_collected_total()>0,"Legacy Food path did not deliver/return/tax in all directions.");
}
}
int main(){try{const auto visuals=profile();contracts(visuals);actual_transport(visuals);
    understaffed_existing_trip(visuals);legacy_food_transport(visuals);
    std::cout<<"Food/Market pose/live rules and complete paid gate-curve transport snapshot neutrality PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
