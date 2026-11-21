#include "simulation/World.h"
#include "simulation/OriginalIndustryGoodsContext.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason){throw std::invalid_argument(reason);}
bool adjacent(Cell farm,Cell field) {
    const std::int64_t x=farm.x,y=farm.y,fx=field.x,fy=field.y;
    const auto dx=std::max({x-fx,std::int64_t{0},fx-(x+1)});
    const auto dy=std::max({y-fy,std::int64_t{0},fy-(y+1)});
    return dx+dy==1;
}
}

void World::validate_original_hemp_model() const {
    if (!original_rules_model_ || !original_rules_model_->supports_building(original::ModelBuilding::HempFarm) ||
        !original_rules_model_->supports_building(original::ModelBuilding::HempField))
        invalid("active original production requires explicitly prepared Hemp Farm192 and Hemp Field194 model capability");
    const auto workers=original_rules_model_->building_value(original::ModelBuilding::HempFarm,original::BuildingField::Employees);
    if (workers<=0 || workers>INT16_MAX ||
        original_rules_model_->building_value(original::ModelBuilding::HempFarm,original::BuildingField::Cost)<=0 ||
        original_rules_model_->building_value(original::ModelBuilding::HempField,original::BuildingField::Cost)<=0 ||
        original_rules_model_->building_value(original::ModelBuilding::HempField,original::BuildingField::Employees)!=0)
        invalid("invalid active Hemp cost or bounded Farm/Field staffing contract");
}

CommandResult World::validate_original_hemp_command(Command command) const {
    CommandResult result{false,false,"Hemp construction requires explicit original production",command_sequence_,ticks_};
    if (!original_production_enabled() || !original_production_) return result;
    const auto reject=[&](const char* why){result.reason=why;return result;};
    if (!in_bounds(command.cell)) return reject("Hemp construction outside permitted map bounds");
    if (command.type==CommandType::PlaceOriginalHempFarm) {
        if (original_production_->hemp_farms.size()>=2) return reject("Hemp Farm limit reached");
    } else if (command.type==CommandType::PlaceOriginalHempField) {
        const auto record=std::find_if(original_production_->hemp_farms.begin(),original_production_->hemp_farms.end(),
            [&](const auto& f){return f.farm_id==command.building_id;});
        if (record==original_production_->hemp_farms.end()) return reject("Hemp Field requires an explicitly selected paid Farm");
        if (record->field_id) return reject("Hemp Farm already owns its paid Field");
        const auto& farm=building(record->farm_id);
        if (!adjacent(farm.cell,command.cell)) return reject("Hemp Field must directly adjoin its selected Farm");
    } else return reject("Unknown Hemp command");
    result.accepted=true;result.changed=true;result.reason="Hemp construction facts ready";return result;
}

void World::validate_original_hemp_snapshot(const WorldSnapshot& s) {
    const auto& farms=s.original_production->hemp_farms;
    if (farms.size()>2) invalid("Hemp Farm bound exceeded");
    std::size_t actual_farms=0,actual_fields=0;std::array<BuildingId,2> fields{};std::size_t count=0;
    for (const auto& b:s.buildings) {
        if (b.kind==Object::OriginalHempFarm) ++actual_farms;
        if (b.kind==Object::OriginalHempField) ++actual_fields;
    }
    if (actual_farms!=farms.size() || actual_fields>2) invalid("missing Hemp Farm/Field ownership authority");
    for (std::size_t i=0;i<farms.size();++i) {
        const auto& record=farms[i];
        if ((i && !(farms[i-1].farm_id<record.farm_id))) invalid("Hemp Farm records must be sorted and unique");
        const auto farm=std::find_if(s.buildings.begin(),s.buildings.end(),[&](const auto& b){return b.id==record.farm_id;});
        if (farm==s.buildings.end() || farm->kind!=Object::OriginalHempFarm || farm->progress<0 || farm->progress>=(simulation::seasonal_agriculture_enabled(s.profile,s.rule_version)?81:80))
            invalid("Hemp Farm record requires its exact purchased body and bounded crop progress");
        std::int64_t held=0;
        for (const auto& stock:s.original_goods->stocks) if (stock.building_id==farm->id) held+=stock.amounts[0];
        for (const auto& cargo:s.original_goods->cargo) if (cargo.source==farm->id && cargo.good==original::Goods::Hemp) held+=cargo.amount;
        if (held<0 || static_cast<std::uint64_t>(held)>record.harvested)
            invalid("Hemp Farm stock and outgoing cargo exceed its own actual harvest origin");
        for (const auto& trip:s.original_goods->reservations) if (trip.source==farm->id) {
            if (trip.good!=original::Goods::Hemp || trip.loaded<0 || trip.delivered<0 ||
                static_cast<std::uint64_t>(trip.loaded)>record.harvested ||
                static_cast<std::uint64_t>(trip.delivered)>record.harvested-static_cast<std::uint64_t>(held))
                invalid("Hemp Farm latest transfer exceeds its own historical harvest origin");
        }
        if (!record.field_id) {
            if (record.harvested || farm->progress) invalid("Hemp has no possible origin before its paid Field");
            continue;
        }
        const auto field=std::find_if(s.buildings.begin(),s.buildings.end(),[&](const auto& b){return b.id==*record.field_id;});
        if (field==s.buildings.end() || field->kind!=Object::OriginalHempField || !(*record.field_id>record.farm_id) ||
            field->placed_tick<farm->placed_tick || !adjacent(farm->cell,field->cell) ||
            std::find(fields.begin(),fields.begin()+static_cast<std::ptrdiff_t>(count),field->id)!=fields.begin()+static_cast<std::ptrdiff_t>(count))
            invalid("Hemp Field must be uniquely paid after and adjacent to its exact Farm");
        fields[count++]=field->id;
        const auto age=s.ticks-field->placed_tick;
        if (record.harvested>age/80 || (record.harvested==age/80 && static_cast<std::uint64_t>(farm->progress)>age%80))
            invalid("Hemp harvest and progress exceed productive clock opportunities after paid Field placement");
        if (!simulation::seasonal_agriculture_enabled(s.profile,s.rule_version) && held==8 && farm->progress) invalid("full Hemp source must retain no partial crop progress");
    }
    if (count!=actual_fields) invalid("orphan or multiply owned Hemp Field");
}

void World::advance_original_hemp() {
    if(seasonal_agriculture_enabled()){advance_seasonal_hemp();return;}
    for (auto& record:original_production_->hemp_farms) {
        auto& farm=mutable_building(record.farm_id);
        if (!record.field_id || !building_staffed_for_tick(farm.id)) continue;
        if (original_market_headroom(farm.id,original::Goods::Hemp)<=0) {farm.progress=0;continue;}
        if (farm.progress<79) {++farm.progress;continue;}
        if (record.harvested==UINT64_MAX) throw std::overflow_error("Hemp harvest history exhausted");
        const original_industry_goods::Context context(buildings_,couriers_);
        auto candidate=original_goods_produce(context.view(*original_goods_,original_economy_->houses),
            farm.id,original::Goods::Hemp,1);
        original_goods_=std::move(candidate.goods);farm.progress=0;++record.harvested;
    }
}

} // namespace openemperor::simulation
