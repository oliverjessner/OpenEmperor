#include "app/AgricultureActorPresentation.h"
#include <algorithm>
#include <stdexcept>

namespace openemperor {
namespace {
namespace sim=simulation;
using Action=AgricultureActorAction;
using Kind=AgricultureActorKind;
bool direction(AgricultureActorPose& pose,sim::Cell from,sim::Cell to) {
    const auto dx=static_cast<std::int64_t>(to.x)-from.x;
    const auto dy=static_cast<std::int64_t>(to.y)-from.y;
    if((dx==0&&dy!=0)||(dy==0&&dx!=0)) {
        pose.dx=static_cast<std::int8_t>((dx>0)-(dx<0));
        pose.dy=static_cast<std::int8_t>((dy>0)-(dy<0));return true;
    }
    return false;
}
template<class Actor> void edge(AgricultureActorPose& pose,const Actor& actor) {
    if(actor.path_vertex>=actor.path.size()||actor.path_vertex+1>=actor.path.size())return;
    if(!direction(pose,actor.path[actor.path_vertex],actor.path[actor.path_vertex+1]))return;
    // Begun edges remain genuine movement even after a source pause or cut.
    pose.moving=actor.edge_progress>0;
    if(pose.moving)pose.phase=static_cast<std::uint8_t>(actor.edge_progress%4);
}
void work(AgricultureActorPose& pose,std::uint64_t counter,Action action) {
    pose.action=action;pose.moving=false;pose.working=counter!=0;
    pose.phase=static_cast<std::uint8_t>(counter%4);
}
bool food_courier(sim::CourierRole role) {
    return role==sim::CourierRole::Food||role==sim::CourierRole::MarketFoodInbound||
        role==sim::CourierRole::MarketFoodDistribution||role==sim::CourierRole::OriginalFoodToMarket||
        role==sim::CourierRole::OriginalFoodToHouse||role==sim::CourierRole::OriginalMealsToShop||
        role==sim::CourierRole::OriginalMealsToHouse||role==sim::CourierRole::OriginalWarehouseFood;
}
}

void AgricultureActorPresentationCache::prepare(const simulation::World& world) {
    if(!world.agriculture_reconfiguration_enabled()){clear();return;}
    std::array<AgricultureActorVisual,capacity> next{};std::size_t count=0;
    const auto append=[&](const AgricultureActorVisual& row){
        if(count==next.size())throw std::logic_error("Agriculture actor presentation exceeds existing movement-channel budget");
        next[count++]=row;
    };
    for(const auto& courier:world.couriers()) {
        const bool food=food_courier(courier.role);
        if(!food&&courier.role!=sim::CourierRole::OriginalFoodGrower&&
            courier.role!=sim::CourierRole::OriginalSeasonalHempGrower&&courier.role!=sim::CourierRole::OriginalPlantationGrower)continue;
        // Every admitted Agriculture source channel hides at its actual idle
        // terminal, matching the existing live walker visibility contract.
        if(courier.phase==sim::CourierPhase::IdleAtWorkshop)continue;
        const auto position=world.courier_position(courier.id);if(!position)continue;
        AgricultureActorVisual row;row.id=static_cast<std::uint32_t>(courier.id);row.owner=courier.owner;
        row.target=courier.target;row.position=*position;
        row.pose.action=courier.phase==sim::CourierPhase::Returning?Action::Returning:Action::Walking;
        edge(row.pose,courier);
        if(courier.route_pending&&!courier.edge_progress)row.pose.action=Action::Waiting;
        if(food) {
            row.pose.kind=Kind::FoodCourier;row.pose.cargo=world.original_food_cargo(courier.id);
            if(world.processed_food_state())for(const auto& trip:world.processed_food_state()->trips)
                if(trip.transfer.courier_id==courier.id){row.lifetime_id=trip.transfer.sequence;break;}
        }else if(courier.role==sim::CourierRole::OriginalFoodGrower) {
            row.pose.kind=Kind::FoodGrower;row.pose.cargo=world.physical_food_grower_cargo(courier.id);
            if(!world.physical_food_growers_state())continue;
            const auto& farms=world.physical_food_growers_state()->farms;
            const auto farm=std::find_if(farms.begin(),farms.end(),[&](const auto& f){return f.courier_id==courier.id;});
            if(farm==farms.end()||!farm->latest_trip||farm->latest_trip->returned_tick)continue;
            const auto& trip=*farm->latest_trip;row.lifetime_id=trip.grower_id.value;row.target=trip.field_id;
            if(trip.phase==sim::PhysicalFoodGrowerPhase::Waiting)row.pose.action=Action::Waiting;
            if(trip.phase==sim::PhysicalFoodGrowerPhase::Working) {
                auto counter=static_cast<std::uint64_t>(trip.work_ticks);
                if(world.seasonal_agriculture_state())for(const auto& field:world.seasonal_agriculture_state()->fields)
                    if(field.field_id==trip.field_id){counter+=field.harvest_progress;break;}
                work(row.pose,counter,Action::Working);
            }
        }else if(courier.role==sim::CourierRole::OriginalSeasonalHempGrower) {
            row.pose.kind=Kind::HempGrower;row.pose.cargo=world.seasonal_hemp_grower_cargo(courier.id);
            if(!world.seasonal_agriculture_state())continue;
            const auto& farms=world.seasonal_agriculture_state()->hemp_farms;
            const auto farm=std::find_if(farms.begin(),farms.end(),[&](const auto& f){return f.courier_id==courier.id;});
            if(farm==farms.end()||!farm->latest_trip||farm->latest_trip->returned_tick)continue;
            row.lifetime_id=farm->latest_trip->grower_id.value;row.target=farm->latest_trip->field_id;
        }else {
            row.pose.kind=Kind::PlantationGrower;row.pose.cargo=world.plantation_grower_cargo(courier.id);
            if(!world.plantations_state())continue;
            const auto& sources=world.plantations_state()->sources;
            const auto source=std::find_if(sources.begin(),sources.end(),[&](const auto& s){return s.courier_id==courier.id;});
            if(source==sources.end()||!source->latest_trip||source->latest_trip->returned_tick)continue;
            row.lifetime_id=static_cast<std::uint64_t>(source->latest_trip->grower_id);row.target=source->latest_trip->plant_id;
        }
        append(row);
    }
    if(world.fishing_state())for(const auto& boat:world.fishing_state()->boats) {
        AgricultureActorVisual row;row.domain=AgricultureActorDomain::Boat;row.id=row.lifetime_id=boat.id.value;
        row.owner=boat.quay_id;row.position=world.fishing_boat_position(boat.id);row.pose.kind=Kind::Boat;row.pose.cargo=boat.cargo;
        row.pose.action=boat.phase==sim::FishingBoatPhase::Returning?Action::Returning:Action::Walking;edge(row.pose,boat);
        if(!row.pose.moving&&(boat.movement==sim::FishingMovementResult::Retry||boat.movement==sim::FishingMovementResult::Failed))row.pose.action=Action::Waiting;
        if(boat.phase==sim::FishingBoatPhase::Fishing)work(row.pose,boat.work_progress,Action::Working);
        if(boat.phase==sim::FishingBoatPhase::Unloading)work(row.pose,boat.work_progress,Action::Unloading);
        append(row);
    }
    if(world.hunting_state()) {
        for(const auto& hunter:world.hunting_state()->hunters) {
            AgricultureActorVisual row;row.domain=AgricultureActorDomain::Hunter;row.id=row.lifetime_id=hunter.id.value;
            row.owner=hunter.tent_id;row.position=world.hunting_hunter_position(hunter.id);row.pose.kind=Kind::Hunter;row.pose.cargo=hunter.cargo;
            row.pose.action=hunter.phase==sim::HuntingHunterPhase::Returning?Action::Returning:Action::Walking;edge(row.pose,hunter);
            if(!row.pose.moving&&(hunter.movement==sim::HuntingMovementResult::Retry||hunter.movement==sim::HuntingMovementResult::Failed))row.pose.action=Action::Waiting;
            if(hunter.phase==sim::HuntingHunterPhase::Hunting)work(row.pose,hunter.work_progress,Action::Working);
            if(hunter.phase==sim::HuntingHunterPhase::Unloading)work(row.pose,hunter.work_progress,Action::Unloading);
            append(row);
        }
        for(const auto& animal:world.hunting_state()->animals) {
            AgricultureActorVisual row;row.domain=AgricultureActorDomain::Animal;row.id=row.lifetime_id=animal.id.value;
            row.position=world.hunting_animal_position(animal.id);
            switch(animal.species){case sim::HuntingSpecies::WildPig75:row.pose.kind=Kind::WildPig;break;case sim::HuntingSpecies::Pheasant76:row.pose.kind=Kind::Pheasant;break;case sim::HuntingSpecies::Antelope77:row.pose.kind=Kind::Antelope;break;default:continue;}
            if(animal.next&&direction(row.pose,animal.current,*animal.next)) {
                row.pose.moving=animal.edge_progress>0;row.pose.action=row.pose.moving?Action::Walking:Action::Standing;
                if(row.pose.moving)row.pose.phase=static_cast<std::uint8_t>(animal.edge_progress%4);
            }
            append(row);
        }
    }
    entries_=next;count_=count;
}
const AgricultureActorVisual* AgricultureActorPresentationCache::find(AgricultureActorDomain domain,std::uint64_t id)const noexcept {
    const auto rows=entries();const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.domain==domain&&row.id==id;});
    return at==rows.end()?nullptr:&*at;
}
const AgricultureActorVisual* AgricultureActorPresentationCache::find_courier(sim::CourierId id)const noexcept{return find(AgricultureActorDomain::Courier,static_cast<std::uint32_t>(id));}
const AgricultureActorVisual* AgricultureActorPresentationCache::find_boat(sim::FishingBoatId id)const noexcept{return find(AgricultureActorDomain::Boat,id.value);}
const AgricultureActorVisual* AgricultureActorPresentationCache::find_hunter(sim::HuntingHunterId id)const noexcept{return find(AgricultureActorDomain::Hunter,id.value);}
const AgricultureActorVisual* AgricultureActorPresentationCache::find_animal(sim::HuntingAnimalId id)const noexcept{return find(AgricultureActorDomain::Animal,id.value);}
} // namespace openemperor
