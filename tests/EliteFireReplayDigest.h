#pragma once

// Explicit external OE45 QA only. No World/snapshot copy, parser, route search,
// production hook or input-file read. The digest combines production canonical
// output with typed framing and the saved fire journal omitted by that output.
#include "simulation/World.h"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace oe_elite_fire_replay_digest {
namespace sim=openemperor::simulation;
using Json=nlohmann::json;

class Observation {
    static constexpr std::size_t row_limit=262144;
    using Context=std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>;
public:
    Observation(const sim::World& w,const std::filesystem::path& output):
        file_(output/"oe45-state-digests.csv"),context_(EVP_MD_CTX_new(),EVP_MD_CTX_free),
        file_context_(EVP_MD_CTX_new(),EVP_MD_CTX_free),begin_(w.ticks()),last_seen_(w.ticks()),rule_(w.rule_version()) {
        require(w.profile()==sim::RulesProfile::CityV16&&
            (w.original_inspector_coordination_enabled()||w.original_deadline_fire_dispatch_enabled()||w.original_deadline_service_dispatch_enabled()||w.original_deadline_food_dispatch_enabled())&&
            w.original_production_state()&&w.original_production_state()->fire_protection&&
            w.original_terrain_facts(),"OE45 digest requires exact source-bound Rule15 or16");
        require(!std::filesystem::exists(file_),"Preserve previous OE45 digest trace");
        require(context_&&file_context_&&EVP_DigestInit_ex(file_context_.get(),EVP_sha256(),nullptr)==1,
            "Cannot initialize OE45 digest contexts");
        stream_.open(file_,std::ios::out|std::ios::binary);
        require(bool(stream_),"Cannot create OE45 digest trace");
        append_file("tick,sha256\n");record(w);
    }
    void after(const sim::World& w) {
        require(!finished_&&last_seen_!=UINT64_MAX&&w.ticks()==last_seen_+1,
            "OE45 digest must observe each ordinary tick exactly once");
        last_seen_=w.ticks();record(w);
    }
    Json finish() {
        require(!finished_,"OE45 digest already finished");finished_=true;
        stream_.flush();require(bool(stream_),"Cannot flush OE45 digest trace");
        const auto file_sha=finalize(file_context_.get());
        return {{"format","openemperor-elite-fire-replay-digest-v1"},
            {"technical_status",overflow_?"PARTIAL":"PASS"},{"overflow",overflow_},
            {"row_limit",row_limit},{"rows",rows_},{"begin_tick",begin_},
            {"last_recorded_tick",last_recorded_},{"last_observed_tick",last_seen_},
            {"file",file_.filename().string()},{"file_sha256",file_sha},
            {"first_state_sha256",first_},{"last_state_sha256",last_},
            {"algorithm","SHA256 of domain tag and two length-framed UTF-8 values: production canonical_state and typed JSON supplement"},
            {"world_copy_or_snapshot",false},{"BFS_or_route_refresh",false},
            {"production_hook",false},{"input_file_reads",false},
            {"saved_authority_scope",rule_==18?"Complete validated WorldSnapshot authority for exact City-v16 Rule18; no additional saved authority; recursive field audit against World::snapshot and state headers":rule_==17?"Complete validated WorldSnapshot authority for exact City-v16 Rule17; no additional saved authority; recursive field audit against World::snapshot and state headers":"Complete validated WorldSnapshot authority for exact City-v16 Rule15/16; recursive field audit against World::snapshot and state headers"},
            {"excluded_saved_authority",Json::array()},
            {"canonical_coverage",Json::array({"scalar counters and balances; complete buildings and couriers",
                "roads encoded by complete object grid; complete bridge/ramp geometry and next IDs",
                "original economy including every HouseState field and model source identity",
                "original influence source and removed-source records; all goods accounts/transfers",
                "production hemp/consumption/food/services/water/housing/industry/finance recursively",
                "bound terrain full-map SHA256, model SHA256/difficulty, complete map-permission canonical context"})},
            {"supplement_coverage",Json::array({"width/height/profile/rule; explicit legacy workshop/warehouse/path",
                "all top-level optional-state presence, collection lengths and courier optional values",
                "all fire_protection fields: travelled_cells, trips, arrivals and individual tax assessments"})},
            {"derived_state_scope","canonical_state additionally contains its historical derived grid/cache values; other derived navigation/influence caches are not saved authority and are not claimed covered"},
            {"comparison_requirement","Compare complete CSV bytes for separate processes with the same initial tick and command plan; a digest is not a gameplay acceptance claim"}};
    }
private:
    static void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
    template<class T> static Json optional_number(const std::optional<T>& value) {
        return value ? Json(static_cast<std::uint64_t>(*value)):Json(nullptr);
    }
    static Json cell(sim::Cell value) {return Json::array({value.x,value.y});}
    static Json optional_cell(const std::optional<sim::Cell>& value) {return value?cell(*value):Json(nullptr);}
    static Json cells(const auto& values) {
        Json rows=Json::array();for(const auto value:values)rows.push_back(cell(value));return rows;
    }
    static Json fire_trip(const sim::OriginalFireTrip& value) {
        return {{"courier_id",static_cast<std::uint32_t>(value.courier_id)},
            {"owner",static_cast<std::uint32_t>(value.owner)},{"target",static_cast<std::uint32_t>(value.target)},
            {"dispatched_tick",value.dispatched_tick},{"road_revision",value.road_revision},
            {"assigned_workers",value.assigned_workers},{"population",value.population},
            {"operating",value.operating},{"travelled",cells(value.travelled)}};
    }
    static Json supplement(const sim::World& w) {
        Json result{{"width",w.width()},{"height",w.height()},{"profile",static_cast<unsigned>(w.profile())},
            {"rule_version",w.rule_version()},{"workshop",optional_cell(w.workshop())},
            {"warehouse",optional_cell(w.warehouse())},{"legacy_path",cells(w.courier_path())},
            {"buildings_count",w.buildings().size()},{"couriers_count",w.couriers().size()},
            {"bridges_count",w.bridges().size()},{"ramps_count",w.ramps().size()},
            {"original_economy_present",bool(w.original_economy_state())},
            {"original_influence_present",bool(w.original_influence_state())},
            {"original_goods_present",bool(w.original_goods_state())},
            {"original_production_present",bool(w.original_production_state())}};
        Json couriers=Json::array();for(const auto& c:w.couriers())couriers.push_back({
            {"id",static_cast<std::uint32_t>(c.id)},{"path_size",c.path.size()},
            {"route_checked_revision",optional_number(c.route_checked_revision)},
            {"last_dispatched_pottery",optional_number(c.last_dispatched_pottery)},
            {"last_dispatched_target",optional_number(c.last_dispatched_target)}});
        result["courier_framing"]=std::move(couriers);
        Json bridges=Json::array();for(const auto& b:w.bridges())bridges.push_back({
            {"id",b.id.value},{"water_cell_count",b.water_cells.size()}});
        result["bridge_framing"]=std::move(bridges);
        if(const auto& state=w.original_economy_state();state)result["economy_framing"]={
            {"model_source_present",bool(state->model_source)},{"houses",state->houses.size()}};
        if(const auto& state=w.original_influence_state();state)result["influence_framing"]={
            {"sources",state->sources.size()},{"removed_sources",state->removed_sources.size()}};
        if(const auto& state=w.original_goods_state();state)result["goods_framing"]={
            {"totals",state->totals.size()},{"stocks",state->stocks.size()},
            {"cargo",state->cargo.size()},{"reservations",state->reservations.size()}};
        if(const auto& state=w.original_production_state();state) {
            result["production_framing"]={{"hemp_farms",state->hemp_farms.size()},
                {"house_consumption",state->house_consumption.size()},
                {"food_farms",state->food.farms.size()},{"food_stocks",state->food.stocks.size()},
                {"food_trips",state->food.trips.size()},{"service_providers",state->services.providers.size()},
                {"service_trips",state->services.trips.size()},{"service_witnesses",state->services.witnesses.size()},
                {"water_providers",state->water.providers.size()},{"water_trips",state->water.trips.size()},
                {"water_houses",state->water.houses.size()},{"housing_source_origins",state->housing.neutral_source_origins.size()},
                {"housing_houses",state->housing.houses.size()},{"industry_sources",state->industry.sources.size()},
                {"managed_ore",state->industry.managed_ore.size()},{"auxiliary",state->industry.auxiliary.size()},
                {"aux_trips",state->industry.aux_trips.size()},{"clay_trips",state->industry.clay_trips.size()},
                {"finance_houses",state->finance.houses.size()},{"fire_protection_present",bool(state->fire_protection)}};
            if(const auto& fire=state->fire_protection;fire) {
                Json trips=Json::array(),arrivals=Json::array(),taxes=Json::array();
                for(const auto& row:fire->trips)trips.push_back(fire_trip(row));
                for(const auto& row:fire->arrivals)arrivals.push_back({{"trip",fire_trip(row.trip)},
                    {"tick",row.tick},{"extinguished",row.extinguished}});
                for(const auto& row:fire->taxes)taxes.push_back({
                    {"building_id",static_cast<std::uint32_t>(row.building_id)},
                    {"assessment",{{"tick",row.assessment.tick},
                        {"type",static_cast<unsigned>(row.assessment.type)},
                        {"occupants",row.assessment.occupants}}}});
                result["fire_protection"]={{"travelled_cells",fire->travelled_cells},
                    {"trips",std::move(trips)},{"arrivals",std::move(arrivals)},{"taxes",std::move(taxes)}};
            }
        }
        return result;
    }
    void update(std::string_view value) {
        require(EVP_DigestUpdate(context_.get(),value.data(),value.size())==1,"Cannot update OE45 state digest");
    }
    void frame(std::string_view value) {
        std::array<unsigned char,8> length{};auto size=static_cast<std::uint64_t>(value.size());
        for(auto& byte:length){byte=static_cast<unsigned char>(size&255U);size>>=8;}
        require(EVP_DigestUpdate(context_.get(),length.data(),length.size())==1,"Cannot frame OE45 state digest");
        update(value);
    }
    static std::string finalize(EVP_MD_CTX* context) {
        std::array<unsigned char,EVP_MAX_MD_SIZE> bytes{};unsigned size=0;
        require(EVP_DigestFinal_ex(context,bytes.data(),&size)==1&&size==32,"Cannot finish OE45 SHA256");
        constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);
        for(unsigned i=0;i<size;++i){result.push_back(hex[bytes[i]>>4]);result.push_back(hex[bytes[i]&15]);}
        return result;
    }
    void append_file(std::string_view value) {
        stream_.write(value.data(),static_cast<std::streamsize>(value.size()));
        require(bool(stream_)&&EVP_DigestUpdate(file_context_.get(),value.data(),value.size())==1,
            "Cannot write/hash OE45 digest trace");
    }
    void record(const sim::World& w) {
        if(overflow_)return;
        if(rows_>=row_limit){overflow_=true;return;}
        require(EVP_DigestInit_ex(context_.get(),EVP_sha256(),nullptr)==1,"Cannot initialize OE45 state SHA256");
        update("openemperor-oe45-complete-state-v1");
        const auto canonical=w.canonical_state();frame(canonical);
        const auto extra=supplement(w).dump();frame(extra);
        const auto digest=finalize(context_.get());
        append_file(std::to_string(w.ticks())+","+digest+"\n");
        if(rows_==0)first_=digest;last_=digest;last_recorded_=w.ticks();++rows_;
    }
    std::filesystem::path file_;std::ofstream stream_;Context context_,file_context_;
    std::uint64_t begin_=0,last_seen_=0,last_recorded_=0;std::size_t rows_=0;
    unsigned rule_=0;
    bool overflow_=false,finished_=false;std::string first_,last_;
};
} // namespace oe_elite_fire_replay_digest
