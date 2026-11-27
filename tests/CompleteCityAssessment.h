#pragma once

// Shared, read-only paid-city QA. No gameplay state, commands or grants live here.
#include "LuxuryProgressionFixture.h"

namespace oe_complete_city {
namespace l=oe_luxury_progression;
namespace sim=l::sim;
namespace orig=l::orig;
using Json=l::Json;
using Id=sim::BuildingId;
inline constexpr std::uint64_t full_cycle_ticks=51U*16U*12U;
inline constexpr auto year_ticks=full_cycle_ticks;
inline constexpr std::array<const char*,12> food_names{"Wheat7","Millet5","Rice6","Cabbage3","BeanCurd1","Fish2","Meat4","Dinners28-grade1","Dinners28-grade2","Dinners28-grade3","Dinners28-grade4","Dinners28-grade5"};

struct Totals {
    sim::ProcessedFoodCounters origin{},held{},used{},carried{},house_consumed{};
    std::array<std::uint64_t,orig::goods_count> house_goods{};
};
inline Totals totals(const sim::World& world) {
    l::check((world.rule_version()==36||world.rule_version()==37||world.rule_version()==38)&&world.processed_food_state()&&!world.hunting_food_state()&&!world.fishing_food_state()&&!world.original_agriculture_state(),"Single selected reconfiguration Food authority required");
    Totals result;
    for(const auto& field:world.agriculture_fields()) {
        const auto& receipts=world.physical_food_growers_state()->fields;
        const auto row=std::find_if(receipts.begin(),receipts.end(),[&](const auto& item){return item.field_id==field.field_id;});
        l::check(row!=receipts.end(),"Missing physical Field return history");
        result.origin.at(static_cast<std::size_t>(field.crop))+=row->returned;
    }
    for(const auto& quay:world.fishing_state()->quays)result.origin[5]+=quay.unloaded;
    for(const auto& tent:world.hunting_state()->tents)result.origin[6]+=tent.unloaded;
    for(const auto& mill:world.food_processing_state()->mills)for(std::size_t grade=0;grade<4;++grade)result.origin[7+grade]+=(grade+1)*mill.completed_by_grade[grade];
    for(const auto& stock:world.processed_food_state()->stocks) {
        const bool house=sim::original_house_kind(world.building(stock.building_id).kind);
        for(std::size_t i=0;i<12;++i) {
            l::check(stock.amounts[i]>=0,"Negative physical Food stock");
            result.held[i]+=static_cast<unsigned>(stock.amounts[i]);result.used[i]+=stock.consumed[i];
            if(house)result.house_consumed[i]+=stock.consumed[i];
        }
    }
    for(const auto& trip:world.processed_food_state()->trips)for(std::size_t i=0;i<12;++i) {
        const auto cargo=trip.loaded[i]-trip.delivered[i]-trip.returned[i];l::check(cargo>=0&&cargo<=4,"Invalid physical Food cargo");result.carried[i]+=static_cast<unsigned>(cargo);
    }
    for(std::size_t i=0;i<12;++i)l::check(result.origin[i]==result.held[i]+result.used[i]+result.carried[i],"Food origin != held + consumed/processor-debited + physical cargo");
    for(const auto& home:world.original_production_state()->housing.houses)for(const auto& row:home.consumption_rows)
        for(std::size_t i=0;i<orig::goods_count;++i)result.house_goods[i]+=row.consumed[i];
    return result;
}
template<std::size_t N>std::array<std::uint64_t,N> delta(const std::array<std::uint64_t,N>& after,const std::array<std::uint64_t,N>& before) {
    std::array<std::uint64_t,N> result{};
    for(std::size_t i=0;i<N;++i){l::check(after[i]>=before[i],"Lifetime receipt/consumption counter decreased");result[i]=after[i]-before[i];}return result;
}

// A separately selected QA contract, never saved gameplay authority. All rows
// are bounded by the existing paid-body/House limits, not by elapsed ticks.
inline constexpr const char* complete_city_contract="paid-complete-city-acceptance-v1";
inline constexpr const char* fresh_city_contract="paid-complete-city-from-zero-v2";
class Assessment {
    struct Home {
        std::array<std::uint64_t,orig::goods_count> initial_goods{},goods{};
        std::array<std::uint64_t,16> type_ticks{};
        orig::HouseType first_type=orig::HouseType::Vacant,last_type=orig::HouseType::Vacant;
        bool elite=false,mature_occupied=false,luxury_consumer=false;
        std::uint64_t new_evolutions=0,new_devolutions=0,occupied_ticks=0,empty_ticks=0,
            family_loss_ticks=0,food_deficit_ticks=0,food_deficit_evaluations=0,
            food_deficit_devolutions=0,seen_evaluation=0;
        std::int16_t minimum_occupancy=0;
    };
    std::map<Id,Home> homes_;
    Totals initial_,window_start_;
    std::array<std::uint64_t,9> initial_crop_returns_{},window_crop_returns_{};
    std::uint64_t begin_=0,observed_=0,maturity_=0,window_begin_=0,maximum_wet_ditches_=0;
    std::optional<std::uint64_t> last_water_phase_;
    std::optional<std::uint64_t> minimum_reserve_;
    std::uint64_t zero_reserve_ticks_=0,maximum_reserve_=0,maximum_mill_raw_=0,
        retained_progressions_=0,minimum_population_=0,maximum_population_=0;
    std::int64_t minimum_funds_=0,maturity_funds_=0;
    std::uint64_t maturity_taxes_=0,maturity_upkeep_=0;
    int minimum_worker_supply_=0,maximum_active_workers_=0,maximum_used_workers_=0;
    bool labor_bounds_=true;
    Json setup_,missing_setup_=Json::array(),windows_=Json::array();
    // Only the named factory can select this observation origin. These values
    // seal one actual World and immutable source objects; they are never saved.
    bool fresh_=false;unsigned fresh_rule_=37;
    const sim::World* fresh_world_=nullptr;
    std::shared_ptr<const sim::MapPermissions> fresh_permissions_;
    std::shared_ptr<const orig::OriginalRulesModel> fresh_model_;
    std::shared_ptr<const sim::OriginalTerrainFacts> fresh_terrain_;
    std::uint64_t fresh_last_tick_=0,fresh_last_command_=0,maturity_command_=0;
    Totals fresh_previous_,maturity_counts_;
    std::array<std::optional<std::uint64_t>,12> first_food_ticks_{};
    std::array<std::optional<std::uint64_t>,orig::goods_count> first_goods_ticks_{};
    std::array<std::optional<std::uint64_t>,9> first_crop_ticks_{};
    std::array<std::uint64_t,9> maturity_crop_returns_{};
    std::optional<std::uint64_t> first_all_crop_returns_;
    Json fresh_origin_,fresh_identity_;

    void fresh_identity(const sim::World& world)const {
        l::check(&world==fresh_world_&&world.profile()==sim::RulesProfile::CityV16&&world.rule_version()==fresh_rule_&&
            world.map_permissions()==fresh_permissions_&&world.original_rules_model()==fresh_model_&&
            world.original_terrain_facts()==fresh_terrain_,
            "Fresh complete-city observation changed its actual World or immutable source identities");
    }
    void refresh_fresh_homes(const sim::World& world) {
        for(const auto& house:world.original_economy_state()->houses)if(!homes_.contains(house.building_id)) {
            l::check(!maturity_,"Fresh complete-city paid House cohort changed after joint maturity");
            Home row;row.first_type=row.last_type=house.house.type;
            row.elite=orig::is_elite(house.house.type);row.minimum_occupancy=house.house.occupants;
            // The origin is actual empty World0, including for later purchases.
            // Never rebase their lifetime goods counters at discovery time.
            homes_.emplace(house.building_id,row);
        }
        l::check(homes_.size()==world.original_economy_state()->houses.size()&&
            homes_.size()<=sim::Rules::city_v10_household_limit,
            "Fresh complete-city paid House identities disappeared or exceeded the existing bound");
    }

    static std::array<std::uint64_t,orig::goods_count> house_goods(const sim::World& world,Id id) {
        for(const auto& row:world.original_production_state()->house_consumption)if(row.building_id==id)return row.consumed;
        throw std::runtime_error("Missing sole paid House consumption row");
    }
    static bool placed(const sim::World& world,Id id,sim::Object kind) {
        const auto& body=world.building(id);return body.placed&&body.kind==kind;
    }
    void prepare_setup(const sim::World& world) {
        Json objects=Json::array(),fields=Json::array(),plants=Json::array();
        const auto require_kind=[&](sim::Object kind,const char* name) {
            Json ids=Json::array();for(const auto& body:world.buildings())if(body.placed&&body.kind==kind)ids.push_back(static_cast<unsigned>(body.id));
            if(ids.empty())missing_setup_.push_back(std::string("Missing paid ")+name);
            objects.push_back({{"name",name},{"kind",static_cast<unsigned>(kind)},{"actual_ids",ids}});
        };
        for(const auto& item:std::array{
            std::pair{sim::Object::OriginalFoodFarm,"Food Farm"},std::pair{sim::Object::OriginalHempFarm,"Hemp source"},
            std::pair{sim::Object::OriginalLoggingShed38,"Wood source"},std::pair{sim::Object::OriginalWeaver47,"Silk processor"},
            std::pair{sim::Object::OriginalLacquerwareMaker44,"Lacquerware processor"},std::pair{sim::Object::OriginalFishingQuay,"Quay"},
            std::pair{sim::Object::OriginalHuntersTent,"Hunter Tent"},std::pair{sim::Object::OriginalMill53,"Mill"},
            std::pair{sim::Object::OriginalFoodShop66,"FoodShop"},std::pair{sim::Object::Warehouse,"Food Warehouse"},
            std::pair{sim::Object::Market,"Market"},std::pair{sim::Object::OriginalIrrigationPump,"Pump"}})require_kind(item.first,item.second);
        constexpr std::array kinds{sim::Object::OriginalWheatField,sim::Object::OriginalMilletField,sim::Object::OriginalRiceField,
            sim::Object::OriginalCabbageField,sim::Object::OriginalSoybeanField};
        std::array<bool,5> crops{};std::map<Id,unsigned> per_farm;
        for(const auto& field:world.agriculture_fields())if(world.building(field.field_id).placed) {
            const auto index=static_cast<std::size_t>(field.crop);
            l::check(index<kinds.size()&&placed(world,field.field_id,kinds[index])&&placed(world,field.farm_id,sim::Object::OriginalFoodFarm),"Complete city Field lost actual parent/type binding");
            crops[index]=true;++per_farm[field.farm_id];fields.push_back({{"field",static_cast<unsigned>(field.field_id)},
                {"farm",static_cast<unsigned>(field.farm_id)},{"crop",index}});
        }
        for(std::size_t i=0;i<crops.size();++i)if(!crops[i])missing_setup_.push_back(std::string("Missing bound Food field ")+food_names[i]);
        if(std::none_of(per_farm.begin(),per_farm.end(),[](const auto& entry){return entry.second>=2;}))missing_setup_.push_back("No paid Farm owns multiple active fields");
        bool hemp=false;for(const auto& row:world.original_production_state()->hemp_farms)
            hemp|=row.field_id&&placed(world,row.farm_id,sim::Object::OriginalHempFarm)&&placed(world,*row.field_id,sim::Object::OriginalHempField);
        if(!hemp)missing_setup_.push_back("Missing actual Hemp field/source binding");
        constexpr std::array source_kinds{sim::Object::OriginalTeaCuringShed237,sim::Object::OriginalLacquerRefinery238,sim::Object::OriginalSilkwormShed239};
        constexpr std::array plant_kinds{sim::Object::OriginalTeaBush26,sim::Object::OriginalLacquerTree27,sim::Object::OriginalMulberryTree28};
        std::array<bool,3> bound_plants{};
        for(const auto& plant:world.plantations_state()->plants)if(world.building(plant.plant_id).placed) {
            for(std::size_t i=0;i<plant_kinds.size();++i)if(placed(world,plant.plant_id,plant_kinds[i])&&placed(world,plant.source_id,source_kinds[i])) {
                bound_plants[i]=true;plants.push_back({{"plant",static_cast<unsigned>(plant.plant_id)},{"source",static_cast<unsigned>(plant.source_id)},{"kind",i}});
            }
        }
        for(std::size_t i=0;i<bound_plants.size();++i)if(!bound_plants[i])missing_setup_.push_back("Missing actual plantation source/plant binding index "+std::to_string(i));
        bool shop=false;for(const auto& row:world.food_processing_state()->shops)
            shop|=placed(world,row.shop_id,sim::Object::OriginalFoodShop66)&&placed(world,row.market_id,sim::Object::Market);
        if(!shop)missing_setup_.push_back("Missing actual FoodShop/earlier Market binding");
        bool warehouse=false;for(const auto& stock:world.processed_food_state()->stocks)warehouse|=placed(world,stock.building_id,sim::Object::Warehouse);
        if(!warehouse)missing_setup_.push_back("Missing Warehouse owner row in the sole Food ledger");
        if(world.irrigation_state()->ditches.size()<21)missing_setup_.push_back("Fewer than21 actual paid ditches");
        setup_={{"paid_objects",objects},{"active_bound_fields",fields},{"active_bound_plants",plants},
            {"actual_hemp_binding",hemp},{"actual_shop_market_binding",shop},{"actual_warehouse_food_row",warehouse},
            {"paid_ditch_count",world.irrigation_state()->ditches.size()},{"missing",missing_setup_}};
    }
    void water_epoch(const sim::World& world) {
        std::optional<std::uint64_t> phase;
        for(const auto& pump:world.irrigation_state()->pumps) {
            const auto status=world.irrigation_pump_status(pump.building_id);
            if(status.last_phase_tick&&(!phase||*status.last_phase_tick>*phase))phase=status.last_phase_tick;
        }
        if(!phase||phase==last_water_phase_)return;
        last_water_phase_=phase;std::uint64_t wet=0;
        // Scan only paid ditch rows at a new real published phase, never map cells.
        for(const auto& ditch:world.irrigation_state()->ditches)wet+=world.irrigation_ditch_wet(ditch.cell);
        maximum_wet_ditches_=std::max(maximum_wet_ditches_,wet);
    }
    std::pair<std::uint64_t,std::uint64_t> reserves(const sim::World& world)const {
        std::uint64_t edible=0,mill_raw=0;
        for(const auto& stock:world.processed_food_state()->stocks)if(world.building(stock.building_id).placed) {
            const bool mill=world.building(stock.building_id).kind==sim::Object::OriginalMill53;
            for(std::size_t i=0;i<stock.amounts.size();++i) {
                l::check(stock.amounts[i]>=0,"Negative actual reserve");const auto amount=static_cast<unsigned>(stock.amounts[i]);
                if(mill&&i<7)mill_raw+=amount;else edible+=amount;
            }
        }
        return {edible,mill_raw}; // Never live cargo, incoming claims or debited recipe inputs.
    }
    static bool raw_food_sink(const sim::ProcessedFoodCounters& values) {
        return std::any_of(values.begin(),values.begin()+7,[](auto amount){return amount>0;});
    }
    static bool meal_sink(const sim::ProcessedFoodCounters& values) {
        return std::any_of(values.begin()+7,values.begin()+11,[](auto amount){return amount>0;});
    }
    static bool luxury_sinks(const std::array<std::uint64_t,orig::goods_count>& values) {
        return std::all_of(l::luxury_goods.begin(),l::luxury_goods.end(),[&](auto good){return values[static_cast<std::size_t>(good)]>0;});
    }
    static std::array<std::uint64_t,9> crop_returns(const sim::World& world,const Totals& counts) {
        std::array<std::uint64_t,9> result{};for(std::size_t i=0;i<5;++i)result[i]=counts.origin[i];
        result[5]=world.original_goods_balance(orig::Goods::Hemp).totals.produced;
        for(const auto& plant:world.plantations_state()->plants) {
            const auto kind=world.building(plant.plant_id).kind;
            if(kind==sim::Object::OriginalTeaBush26)result[6]+=plant.returned;
            else if(kind==sim::Object::OriginalLacquerTree27)result[7]+=plant.returned;
            else if(kind==sim::Object::OriginalMulberryTree28)result[8]+=plant.returned;
        }
        return result;
    }
public:
    explicit Assessment(const sim::World& world):initial_(totals(world)),window_start_(initial_),begin_(world.ticks()),
        minimum_population_(static_cast<std::uint64_t>(world.total_population())),maximum_population_(minimum_population_),
        minimum_funds_(world.treasury()),minimum_worker_supply_(world.workforce_supply()) {
        prepare_setup(world);water_epoch(world);initial_crop_returns_=window_crop_returns_=crop_returns(world,initial_);
        const auto [edible,mill_raw]=reserves(world);maximum_reserve_=edible;maximum_mill_raw_=mill_raw;
        maximum_active_workers_=world.active_workforce_required();maximum_used_workers_=world.workforce_used();
        labor_bounds_=world.workforce_used()>=0&&world.workforce_used()<=world.workforce_supply()&&world.workforce_used()<=world.active_workforce_required();
        for(const auto& house:world.original_economy_state()->houses) {
            Home row;row.initial_goods=row.goods=house_goods(world,house.building_id);row.first_type=row.last_type=house.house.type;
            row.elite=orig::is_elite(house.house.type);row.minimum_occupancy=house.house.occupants;homes_.emplace(house.building_id,row);
        }
        for(const auto& row:world.original_production_state()->housing.houses) {
            if(row.last_evaluation)homes_.at(row.building_id).seen_evaluation=row.last_evaluation->tick;
            for(std::size_t index=0;index<row.first_transitions.size();++index) {
                const auto& entry=row.first_transitions[index];
                const auto target=static_cast<orig::HouseType>(index+3);
                retained_progressions_+=entry&&entry->kind==sim::OriginalHousingTransitionKind::Evolution&&
                    entry->evaluation.input.occupants>0&&target>entry->evaluation.input.type;
            }
        }
        l::check(homes_.size()<=sim::Rules::city_v10_household_limit,"Complete-city House audit exceeded its existing bound");
    }
    static Assessment fresh_from_zero(const sim::World& world) {
        l::check(world.profile()==sim::RulesProfile::CityV16&&(world.irrigated_harvest_enabled()||world.quality_aware_food_dispatch_enabled())&&
            world.map_permissions()&&world.map_permissions()->policy_version()==3&&world.original_rules_model()&&
            world.original_terrain_facts(),"Fresh complete-city assessment requires explicit source-bound Rule37/policy3");
        l::check(world.ticks()==0&&world.command_sequence()==0&&world.buildings().empty()&&world.couriers().empty()&&
            world.total_population()==0&&world.treasury()==sim::original_production_starting_treasury&&
            world.taxes_collected_total()==0&&world.construction_spent_total()==0&&world.maintenance_spent_total()==0&&
            world.processed_food_state()&&world.processed_food_state()->stocks.empty(),
            "Fresh complete-city assessment requires empty unpaid World0 with normal starting Funds");
        const auto pristine=world.rule_version()==38?sim::World::create_quality_aware_food_dispatch(world.map_permissions(),world.original_rules_model(),world.original_terrain_facts()):sim::World::create_irrigated_harvest(world.map_permissions(),world.original_rules_model(),world.original_terrain_facts());
        l::check(world.snapshot()==pristine.snapshot(),
            "Fresh complete-city origin contains roads, Food, paid identities, history or other earned state");
        Assessment result(world);result.fresh_=true;result.fresh_rule_=world.rule_version();result.fresh_world_=&world;
        result.fresh_permissions_=world.map_permissions();result.fresh_model_=world.original_rules_model();
        result.fresh_terrain_=world.original_terrain_facts();result.fresh_previous_=result.initial_;
        const auto& model=result.fresh_model_->provenance();const auto& map_hash=result.fresh_terrain_->source_map_sha256();
        result.fresh_identity_={{"model_sha256",model.sha256},{"model_byte_count",model.byte_count},
            {"difficulty",static_cast<unsigned>(result.fresh_model_->difficulty())},{"source_map_sha256",map_hash},
            {"map_permissions_sha256",l::save::map_permissions_fingerprint(*result.fresh_permissions_,map_hash)},
            {"rule",world.rule_version()},{"policy",3},{"schema",world.rule_version()+16}};
        result.fresh_origin_={{"tick",0},{"command_sequence",0},{"buildings",0},{"couriers",0},{"population",0},
            {"starting_funds",world.treasury()},{"taxes",0},{"construction",0},{"upkeep",0},
            {"food_origin",result.initial_.origin},{"house_food_consumed",result.initial_.house_consumed},
            {"house_goods_consumed",result.initial_.house_goods},{"pristine_world_snapshot_exact",true}};
        return result;
    }
    std::optional<std::uint64_t> maturity_tick()const {return maturity_?std::optional{maturity_}:std::nullopt;}
    std::size_t complete_window_count()const {return windows_.size();}
    std::optional<std::uint64_t> last_window_end_tick()const {
        return windows_.empty()?std::nullopt:std::optional{windows_.back().at("end_tick").get<std::uint64_t>()};
    }
    void after(const sim::World& world) {
        if(fresh_) {
            fresh_identity(world);
            l::check(observed_<200000&&world.ticks()==fresh_last_tick_+1&&world.command_sequence()>=fresh_last_command_,
                "Fresh complete-city assessment requires each contiguous actual World tick within its bounded horizon");
            if(world.command_sequence()!=fresh_last_command_) {
                missing_setup_=Json::array();prepare_setup(world);
                fresh_last_command_=world.command_sequence();
            }
            refresh_fresh_homes(world);fresh_last_tick_=world.ticks();
        }
        ++observed_;water_epoch(world);minimum_funds_=std::min(minimum_funds_,world.treasury());
        // World remains const throughout this observation. Reuse the same
        // actual queries for every unchanged per-tick minimum/maximum/gate.
        const auto population=static_cast<std::uint64_t>(world.total_population());
        const auto supply=world.workforce_supply();
        const auto active=world.active_workforce_required();
        const auto used=world.workforce_used();
        minimum_population_=std::min(minimum_population_,population);
        maximum_population_=std::max(maximum_population_,population);
        minimum_worker_supply_=std::min(minimum_worker_supply_,supply);
        maximum_active_workers_=std::max(maximum_active_workers_,active);maximum_used_workers_=std::max(maximum_used_workers_,used);
        labor_bounds_&=used>=0&&used<=supply&&used<=active;
        const auto actual=totals(world);const auto food=delta(actual.house_consumed,initial_.house_consumed);const auto goods=delta(actual.house_goods,initial_.house_goods);
        if(fresh_) {
            (void)delta(actual.origin,fresh_previous_.origin);(void)delta(actual.house_consumed,fresh_previous_.house_consumed);
            (void)delta(actual.house_goods,fresh_previous_.house_goods);fresh_previous_=actual;
            for(std::size_t i=0;i<food.size();++i)if(food[i]&&!first_food_ticks_[i])first_food_ticks_[i]=world.ticks();
            for(const auto good:l::luxury_goods){const auto i=static_cast<std::size_t>(good);
                if(goods[i]&&!first_goods_ticks_[i])first_goods_ticks_[i]=world.ticks();}
            const auto returned=crop_returns(world,actual);
            for(std::size_t i=0;i<returned.size();++i)if(returned[i]&&!first_crop_ticks_[i])first_crop_ticks_[i]=world.ticks();
            if(!first_all_crop_returns_&&std::all_of(returned.begin(),returned.end(),[](auto amount){return amount>0;}))
                first_all_crop_returns_=world.ticks();
        }
        const auto [edible,mill_raw]=reserves(world);maximum_reserve_=std::max(maximum_reserve_,edible);maximum_mill_raw_=std::max(maximum_mill_raw_,mill_raw);
        for(const auto& house:world.original_economy_state()->houses) {
            auto& row=homes_.at(house.building_id);const auto current_goods=house_goods(world,house.building_id);
            const auto earned=delta(current_goods,row.initial_goods);row.luxury_consumer|=
                std::any_of(l::luxury_goods.begin(),l::luxury_goods.end(),[&](auto good){return earned[static_cast<std::size_t>(good)]>0;});
            row.goods=current_goods;row.new_devolutions+=house.house.type<row.last_type;
            if(maturity_) {
                const auto type=static_cast<unsigned>(house.house.type);l::check(type>=2&&type<=17,"Actual retained House type outside domain");++row.type_ticks[type-2];
                row.occupied_ticks+=house.house.occupants>0;row.empty_ticks+=row.mature_occupied&&house.house.occupants<=0;
                row.family_loss_ticks+=row.luxury_consumer&&orig::is_elite(house.house.type)!=row.elite;
                row.minimum_occupancy=std::min(row.minimum_occupancy,house.house.occupants);
                const auto required=world.original_rules_model()->house_rules().value(orig::house_row(house.house.type),orig::HouseField::FoodQuality);
                row.food_deficit_ticks+=house.house.occupants>0&&int(house.house.food_quality)<required;
            }
            for(const auto& proof:world.original_production_state()->housing.houses)if(proof.building_id==house.building_id&&proof.last_evaluation&&
                proof.last_evaluation->tick!=row.seen_evaluation) {
                const auto& evaluation=*proof.last_evaluation;
                // Admission is not evolution. Count an actual retained occupied
                // phase39 input and its real higher result, never an empty plot.
                row.new_evolutions+=evaluation.tick>=begin_&&evaluation.input.occupants>0&&house.house.type>evaluation.input.type;
                if(maturity_&&evaluation.tick>maturity_) {
                    const auto need=world.original_rules_model()->house_rules().value(orig::house_row(evaluation.input.type),orig::HouseField::FoodQuality);
                    const bool deficit=evaluation.input.occupants>0&&int(evaluation.input.food_quality)<need;
                    row.food_deficit_evaluations+=deficit;row.food_deficit_devolutions+=deficit&&house.house.type<evaluation.input.type;
                }
                row.seen_evaluation=evaluation.tick;
            }
            row.last_type=house.house.type;
        }
        const bool all_food=std::all_of(food.begin(),food.begin()+7,[](auto amount){return amount>0;});
        if(!maturity_&&missing_setup_.empty()&&maximum_wet_ditches_>=21&&all_food&&meal_sink(food)&&luxury_sinks(goods)&&
            (!fresh_||first_all_crop_returns_.has_value())) {
            maturity_=world.ticks();window_begin_=maturity_;window_start_=actual;window_crop_returns_=crop_returns(world,actual);maturity_funds_=world.treasury();
            maturity_taxes_=world.taxes_collected_total();maturity_upkeep_=world.maintenance_spent_total();
            for(const auto& house:world.original_economy_state()->houses) {auto& row=homes_.at(house.building_id);row.mature_occupied=house.house.occupants>0;row.minimum_occupancy=house.house.occupants;
                if(fresh_)row.elite=orig::is_elite(house.house.type);}
            if(fresh_){maturity_counts_=actual;maturity_crop_returns_=window_crop_returns_;maturity_command_=world.command_sequence();}
        } else if(maturity_) {
            if(!minimum_reserve_||edible<*minimum_reserve_)minimum_reserve_=edible;
            zero_reserve_ticks_+=edible==0;
            if(world.ticks()-window_begin_==year_ticks) {
                l::check(windows_.size()<21,"Complete-city yearly audit exceeded its tick-derived bound");
                const auto used_food=delta(actual.house_consumed,window_start_.house_consumed);const auto used_goods=delta(actual.house_goods,window_start_.house_goods);
                const auto returned=crop_returns(world,actual);const auto crop_delta=delta(returned,window_crop_returns_);
                windows_.push_back({{"begin_tick",window_begin_},{"end_tick",world.ticks()},{"new_house_food",used_food},{"new_house_goods",used_goods},
                    {"actual_source_crop_returns",crop_delta},{"all_nine_crop_returns_observed",std::all_of(crop_delta.begin(),crop_delta.end(),[](auto amount){return amount>0;})},
                    {"renewed_raw_food",raw_food_sink(used_food)},{"renewed_meals",meal_sink(used_food)},{"renewed_all_three_luxuries",luxury_sinks(used_goods)}});
                window_begin_=world.ticks();window_start_=actual;window_crop_returns_=returned;
            }
        }
    }
    Json report(const sim::World& world)const {
        if(fresh_){fresh_identity(world);l::check(world.ticks()==fresh_last_tick_&&world.command_sequence()==fresh_last_command_,
            "Fresh complete-city report has unobserved World ticks or commands");}
        Json homes=Json::array();bool cohort=false,cohort_retained=true,luxury_retained=false;std::uint64_t food_devolutions=0,new_progressions=0;
        for(const auto& [id,row]:homes_) {
            Json types=Json::array();for(std::size_t i=0;i<row.type_ticks.size();++i)if(row.type_ticks[i])types.push_back({{"type",i+2},{"ticks",row.type_ticks[i]}});
            cohort|=row.mature_occupied;cohort_retained&=!row.mature_occupied||!row.empty_ticks;
            luxury_retained|=row.luxury_consumer&&row.mature_occupied&&!row.empty_ticks&&!row.family_loss_ticks;
            food_devolutions+=row.food_deficit_devolutions;new_progressions+=row.new_evolutions;
            homes.push_back({{"id",static_cast<unsigned>(id)},{"initial_type",static_cast<unsigned>(row.first_type)},{"final_type",static_cast<unsigned>(row.last_type)},
                {"mature_occupied",row.mature_occupied},{"actual_new_luxury_consumer",row.luxury_consumer},{"minimum_occupancy",row.minimum_occupancy},
                {"post_maturity_occupied_ticks",row.occupied_ticks},{"cohort_empty_ticks",row.empty_ticks},{"luxury_family_loss_ticks",row.family_loss_ticks},
                {"new_evolutions",row.new_evolutions},{"new_devolutions",row.new_devolutions},{"post_maturity_type_ticks",types},
                {"actual_food_quality_deficit_house_ticks",row.food_deficit_ticks},{"actual_food_quality_deficit_evaluations",row.food_deficit_evaluations},
                {"devolutions_with_actual_evaluation_food_deficit",row.food_deficit_devolutions}});
        }
        const auto after=maturity_?world.ticks()-maturity_:0;const auto taxes=maturity_?world.taxes_collected_total()-maturity_taxes_:0;
        const auto upkeep=maturity_?world.maintenance_spent_total()-maturity_upkeep_:0;
        const bool windows_live=std::all_of(windows_.begin(),windows_.end(),[](const auto& row){return row.at("renewed_raw_food")==true&&row.at("renewed_meals")==true&&row.at("renewed_all_three_luxuries")==true;});
        Json gates={{"installed_all_chains",missing_setup_.empty()},{"actually_supplied_21_ditches",maximum_wet_ditches_>=21},
            {"joint_maturity_earned",maturity_!=0},{"post_maturity_20400_ticks",after>=20400},{"two_full_seasonal_cycle_windows",windows_.size()>=2},
            {"renewed_house_food_meals_and_luxuries_in_every_full_window",!windows_.empty()&&windows_live},
            {"positive_edible_physical_reserves_after_maturity",minimum_reserve_&&*minimum_reserve_>0&&!zero_reserve_ticks_},
            {"paid_occupied_cohort_retained",cohort&&cohort_retained},{"actual_luxury_consumer_family_retained",luxury_retained},
            {"no_devolution_with_actual_food_deficit",maturity_&&!food_devolutions},{"real_progression_witness",retained_progressions_+new_progressions>0},
            {"funds_never_negative",minimum_funds_>=0},{"post_maturity_operating_cashflow_nonnegative",maturity_&&taxes>=upkeep}};
        gates["real_worker_supply_bounds"]=labor_bounds_;
        Json blockers=Json::array();for(auto at=gates.begin();at!=gates.end();++at)if(at.value()!=true)blockers.push_back(at.key());
        Json result={{"contract",complete_city_contract},{"status",blockers.empty()?"PASS":"PARTIAL"},{"criteria",gates},{"failed_criteria",blockers},
            {"installed_bindings",setup_},{"maturity_tick",maturity_?Json(maturity_):Json(nullptr)},{"actual_post_joint_maturity_ticks",after},
            {"begin_tick",begin_},{"observed_ticks",observed_},{"full_cycle_ticks",year_ticks},{"complete_post_maturity_windows",windows_},
            {"maximum_actual_wet_ditches",maximum_wet_ditches_},{"minimum_edible_physical_reserve_after_maturity",minimum_reserve_?Json(*minimum_reserve_):Json(nullptr)},
            {"new_actual_source_crop_returns",delta(crop_returns(world,totals(world)),initial_crop_returns_)},
            {"source_crop_return_order",{"Wheat","Millet","Rice","Cabbage","BeanCurd","Hemp","Tea","Lacquer","Mulberry"}},
            {"zero_edible_reserve_ticks_after_maturity",zero_reserve_ticks_},{"maximum_edible_physical_reserve",maximum_reserve_},{"maximum_separate_mill_raw_inputs",maximum_mill_raw_},
            {"minimum_funds",minimum_funds_},{"funds_at_maturity",maturity_?Json(maturity_funds_):Json(nullptr)},{"post_maturity_taxes",taxes},{"post_maturity_installed_upkeep",upkeep},
            {"minimum_population",minimum_population_},{"maximum_population",maximum_population_},{"minimum_worker_supply",minimum_worker_supply_},
            {"maximum_active_worker_demand",maximum_active_workers_},{"maximum_workers_used",maximum_used_workers_},{"retained_paid_progression_history",retained_progressions_},{"houses",homes},
            {"reserve_scope","Actual sole-ledger held Food at placed producers, Markets, Shops, Warehouses and completed Mill meals. Mill raw inputs, active/debited recipes, cargo and incoming claims are excluded."},
            {"retention_scope","Finite occupied paid cohort and actual luxury-consumer family retention, renewed annual House sinks, and no actual devolution accompanied by a source-row FoodQuality deficit. No fixed highest House type or universal zero-devolution requirement."},
            {"authority","Read-only bounded QA. A valid seed still requires independent paid-creator provenance; no saved authority, clock, source model, work, stock, grant or default changes."}};
        if(fresh_) {
            Json food_ticks=Json::array(),goods_ticks=Json::array(),crop_ticks=Json::array();
            for(const auto tick:first_food_ticks_)food_ticks.push_back(tick?Json(*tick):Json(nullptr));
            for(const auto tick:first_goods_ticks_)goods_ticks.push_back(tick?Json(*tick):Json(nullptr));
            for(const auto tick:first_crop_ticks_)crop_ticks.push_back(tick?Json(*tick):Json(nullptr));
            const auto actual=totals(world);
            result["contract"]=fresh_city_contract;result["fresh_origin"]=fresh_origin_;result["source_identity"]=fresh_identity_;
            result["first_actual_food_consumption_ticks"]=food_ticks;
            result["first_actual_luxury_consumption_ticks_by_goods_index"]=goods_ticks;
            result["first_actual_source_crop_return_ticks"]=crop_ticks;
            result["first_all_nine_source_returns_tick"]=first_all_crop_returns_?Json(*first_all_crop_returns_):Json(nullptr);
            result["whole_lifecycle_house_food_consumed"]=delta(actual.house_consumed,initial_.house_consumed);
            result["whole_lifecycle_house_goods_consumed"]=delta(actual.house_goods,initial_.house_goods);
            result["maturity_house_food_consumed"]=maturity_?Json(maturity_counts_.house_consumed):Json(nullptr);
            result["maturity_house_goods_consumed"]=maturity_?Json(maturity_counts_.house_goods):Json(nullptr);
            result["maturity_actual_source_crop_returns"]=maturity_?Json(maturity_crop_returns_):Json(nullptr);
            result["maturity_command_sequence"]=maturity_?Json(maturity_command_):Json(nullptr);
            result["authority"]="Read-only bounded whole paid-game QA from exact empty source-bound Rule"+std::to_string(fresh_rule_)+" World0. Every earned counter and first epoch is observed on contiguous real World ticks; no seed prehistory, rebase, saved authority, commands, grants or default changes.";
        }
        return result;
    }
};

} // namespace oe_complete_city
