// Composition of explicitly resolved Original events. Movement outcomes and
// admissions are supplied observations; this is not a World or route simulator.
#include "simulation/original/CropGrowerRules.h"
#include "simulation/original/CropHarvestRules.h"
#include "simulation/original/CropLifecycleRules.h"
#include "simulation/original/CropSelectionRules.h"
#include "simulation/original/CropTransportRules.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace original=openemperor::original;
namespace {
std::uint64_t checks=0;
void check(bool value,const char* message) {
    ++checks;
    if(!value)throw std::runtime_error(message);
}
original::CropHarvestContext context() {
    original::CropHarvestContext result;
    result.runtime_fertility=100;
    result.region_factor_binary32=0x3f800000U;
    result.raw_date_operand=-300;
    result.x87_control_word=0x027f;
    return result;
}
original::CropGrowerActor spawned(original::CropWork work) {
    const auto commit=original::commit_crop_spawn(work,true,0,0);
    check(commit.retained_counter==1 && commit.actor.has_value(),"actual spawn arithmetic missing");
    original::CropGrowerActor actor;
    actor.raw_source_word=1;
    actor.raw_state=commit.actor->state;
    actor.raw_status=1;
    actor.raw_role=commit.actor->role;
    actor.wait=commit.actor->initial_wait;
    return actor;
}
void select(original::CropGrowerActor& actor,original::CropGrowerCounters& counters,
            std::optional<original::CropGrowerTarget> target) {
    actor=original::begin_crop_grower_callback(actor,1);
    const auto work=original::advance_crop_grower_work(actor,false);
    check(work.select_target,"selection must follow the actual due wait gate");
    const auto selected=original::apply_crop_grower_target(work.actor,counters,target,true,{0,0});
    check(selected.clear_route,"actual target result must release old route ownership");
    const auto finished=original::finish_crop_grower_callback(selected.actor,selected.counters);
    actor=finished.actor;counters=finished.counters;
}
original::CropGrowerMovementStep movement_callback(original::CropGrowerActor& actor,
    original::CropGrowerCounters& counters,const original::CropGrowerMovement& event) {
    const auto body=original::apply_crop_grower_movement(
        original::begin_crop_grower_callback(actor,1),event);
    const auto finished=original::finish_crop_grower_callback(body.actor,counters);
    actor=finished.actor;counters=finished.counters;
    return {actor,body.clear_route,body.source_receipt};
}
void arrive_for_work(original::CropGrowerActor& actor,original::CropGrowerCounters& counters,
                     original::ModelBuilding field) {
    const auto state=actor.raw_state;
    const auto blocked=movement_callback(actor,counters,{9,field,true,true,false});
    check(blocked.clear_route && !blocked.source_receipt && blocked.actor.raw_state==state,
        "route retry must neither perform field work nor receive source stock");
    const auto arrival=movement_callback(actor,counters,{8,field,true,true,false});
    check(!arrival.source_receipt && (arrival.actor.raw_state==8 || arrival.actor.raw_state==12),
        "field arrival must enter work rather than credit raw yield");
}
void finish_work(original::CropGrowerActor& actor,original::CropGrowerCounters& counters,
                 original::ModelBuilding field,original::CropHarvestFieldState& state) {
    const auto calls=actor.raw_state==8 ? 51:25;
    for(int call=1;call<=calls;++call) {
        actor=original::begin_crop_grower_callback(actor,1);
        auto work=original::advance_crop_grower_work(actor,true);
        check((work.field_callback!=original::CropGrowerFieldCallback::None)==(call==calls),
            "field callback must wait for the complete eligible invocation count");
        if(work.field_callback==original::CropGrowerFieldCallback::Tending)
            state=original::apply_crop_tending_callback(field,state);
        else if(work.field_callback==original::CropGrowerFieldCallback::Harvest) {
            const auto harvest=original::apply_crop_harvest_callback(field,original::crop_rule(field).end_slot,
                state,work.actor.raw_yield_word,context());
            check(harvest.yield.has_value() && !harvest.numeric_word_narrowed && !harvest.actor_word_wrapped,
                "safe composed harvest must retain one exact actor amount");
            state=harvest.field;work.actor.raw_yield_word=harvest.actor_yield_word;
        }
        const auto finished=original::finish_crop_grower_callback(work.actor,counters);
        actor=finished.actor;counters=finished.counters;
    }
}
struct EventResult {
    std::array<original::CropHarvestFieldState,2> fields;
    original::CropGrowerActor actor_after_reset;
    original::CropGrowerCounters counters;
    std::array<original::RawFarmBin,original::farm_raw_bin_count> bins;
    bool operator==(const EventResult&) const = default;
};
EventResult same_crop_event_chain(original::ModelBuilding field) {
    original::CropGrowerCounters counters{0,1};
    auto tending=spawned(original::CropWork::Tending);
    std::array<original::CropHarvestFieldState,2> fields{};
    for(std::size_t index=0;index<fields.size();++index) {
        auto& state=fields[index];state.assigned_actor_word=19;state.body_counter=99;
        state.history.fill(999);
        const original::CropGrowerTarget target{static_cast<std::uint16_t>(10+index),
            {static_cast<std::int16_t>(3+index),4}};
        select(tending,counters,target);
        arrive_for_work(tending,counters,field);
        finish_work(tending,counters,field,state);
        check(state.work_word==64 && state.quantity==0 && !state.assigned_actor_word && !state.body_counter,
            "one Grower must complete actual resolved tending on each of two fields");
    }
    select(tending,counters,std::nullopt);
    const auto tending_return=movement_callback(tending,counters,{8,std::nullopt,false,false,false});
    check(tending_return.actor.raw_status==2 && !tending_return.source_receipt && counters.tending==0,
        "tending return cannot create raw stock");

    // Explicit phase20 visits followed by completed-cycle work decay. These
    // events do not assert an admitted live World clock or route chronology.
    for(unsigned cycle=0;cycle<64;++cycle) {
        const auto slot=static_cast<std::uint8_t>(original::crop_rule(field).start_slot+cycle/16U);
        for(auto& state:fields) {
            const auto growth=original::advance_crop_field_growth(field,slot,state.work_word,state.quantity,0,false,false);
            state.quantity=growth.quantity;
            state.work_word=original::decay_crop_work(state.work_word);
        }
    }
    for(auto& state:fields) {
        check(state.quantity==7552 && state.work_word==0,"literal64-step field growth/decay quantity");
        state.work_word=original::schedule_crop_harvest_mark(field,original::crop_rule(field).end_slot,0,true,state.work_word);
        state.assigned_actor_word=23;
    }
    auto harvesting=spawned(original::CropWork::Harvest);counters.harvest=1;
    for(std::size_t index=0;index<fields.size();++index) {
        select(harvesting,counters,original::CropGrowerTarget{static_cast<std::uint16_t>(10+index),
            {static_cast<std::int16_t>(3+index),4}});
        arrive_for_work(harvesting,counters,field);
        finish_work(harvesting,counters,field,fields[index]);
        check(harvesting.raw_yield_word==75*(index+1) && fields[index].quantity==0 &&
            fields[index].work_word==0x4000 && fields[index].history[original::crop_rule(field).end_slot]==75,
            "marked harvest must move exact raw results into the same carried actor WORD");
    }
    const auto last_field=harvesting.raw_field_word;
    select(harvesting,counters,std::nullopt);
    check(harvesting.raw_state==13 && harvesting.raw_field_word==last_field && counters.harvest==0,
        "last-field resource binding and failed-selection counter debit must remain observable");
    const auto retry=movement_callback(harvesting,counters,{9,std::nullopt,false,false,false});
    check(retry.clear_route && retry.actor.raw_yield_word==150 && !retry.source_receipt,
        "blocked return retry preserves carried raw amount without receipt");
    auto failed_actor=harvesting;
    auto failed_counters=counters;
    const auto returned=movement_callback(harvesting,counters,{8,std::nullopt,false,false,false});
    check(returned.source_receipt && returned.actor.raw_status==2 && returned.actor.raw_yield_word==150,
        "actual resolved source receipt leaves the native actor WORD retained until reset");
    std::array<original::RawFarmBin,original::farm_raw_bin_count> bins{};
    const auto resource=original::crop_rule(field).resource_id;
    const auto bin=original::select_farm_raw_bin(bins,resource);
    check(bin.has_value(),"returned source must select a compatible physical bin");
    const auto credit=original::credit_farm_raw_bin(bins[*bin],resource,returned.actor.raw_yield_word);
    bins[*bin]=credit.bin;
    check(credit.accepted==150 && !credit.overflow && bins[*bin].quantity==150 && bins[*bin].resource==resource,
        "same-crop source event retains its exact resource and accepted raw balance");
    const auto reset=original::reset_retired_crop_grower(returned.actor);
    check(returned.actor.raw_status==2 && counters.harvest==0 && reset.cleared_raw_yield_word==150 &&
        reset.actor.raw_yield_word==0 && bins[*bin].quantity==150,
        "post-receipt reset is not a second stock credit or a second active cargo account");
    auto full_actor=failed_actor;
    auto full_counters=failed_counters;
    const auto full_return=movement_callback(full_actor,full_counters,{8,std::nullopt,false,false,false});
    const auto full=original::credit_farm_raw_bin({resource,400,400},resource,full_return.actor.raw_yield_word);
    const auto full_reset=original::reset_retired_crop_grower(full_return.actor);
    check(full_return.source_receipt && !full.accepted && full.overflow==150 && full.bin.quantity==400 &&
        full_reset.cleared_raw_yield_word==full.accepted+full.overflow && !full_reset.actor.raw_yield_word,
        "full-bin receipt and reset must report unaccepted overflow rather than a stock grant");
    const auto failed_return=movement_callback(failed_actor,failed_counters,{10,std::nullopt,false,false,false});
    check(!failed_return.source_receipt && failed_return.actor.raw_status==2 &&
        original::reset_retired_crop_grower(failed_return.actor).cleared_raw_yield_word==150,
        "native failed return cannot be translated into receipt or an invented refund");
    std::array assignments{original::CropGrowerFieldAssignment{1,23},original::CropGrowerFieldAssignment{1,23},
        original::CropGrowerFieldAssignment{0,23},original::CropGrowerFieldAssignment{1,24}};
    check(original::clear_crop_grower_assignments(assignments,23)==2 && assignments[2].raw_actor_word==23 &&
        assignments[3].raw_actor_word==24,"removal clears every matching active field and preserves other references");
    return {fields,reset.actor,counters,bins};
}
void expiry_and_source_inactive_boundaries() {
    constexpr auto field=original::ModelBuilding::RiceField;
    original::CropHarvestFieldState state{7552,0x8000,99,23,{}};
    state.history.fill(111);
    const auto expired=original::expire_crop_completed_slot(field,9,true,{state.quantity,state.work_word,true});
    state.quantity=expired.field.quantity;state.work_word=expired.field.work_word;state.assigned_actor_word=0;
    auto actor=spawned(original::CropWork::Harvest);actor.raw_field_word=10;actor.raw_state=12;actor.wait=0;
    auto work=original::advance_crop_grower_work(original::begin_crop_grower_callback(actor,1),true);
    check(work.field_callback==original::CropGrowerFieldCallback::Harvest,
        "expired assignment must not invent an extra compatibility gate in the actual worker callback");
    const auto no_yield=original::apply_crop_harvest_callback(field,9,state,0,{});
    check(!no_yield.yield && !no_yield.actor_yield_word && no_yield.field.history==state.history,
        "late harvest request cannot recreate an expired harvest mark or quantity");
    const auto late_tending=original::apply_crop_tending_callback(field,state);
    check(late_tending.work_word==64 && original::advance_crop_field_growth(field,10,late_tending.work_word,0,0,false,false).quantity==157,
        "explicit late tending and next non-end growth must not acquire an invented growing-period veto");

    state={1000,0x8000,0,23,{}};actor.wait=1;
    actor=original::begin_crop_grower_callback(actor,2);
    check(actor.raw_status==2,"source-inactive retirement status must precede body effects");
    work=original::advance_crop_grower_work(actor,true);
    const auto harvest=original::apply_crop_harvest_callback(field,9,state,0,context());
    work.actor.raw_yield_word=harvest.actor_yield_word;
    const auto finished=original::finish_crop_grower_callback(work.actor,{1,0});
    check(work.field_callback==original::CropGrowerFieldCallback::Harvest && harvest.actor_yield_word==10 &&
        finished.retirement_requested && !finished.counters.harvest &&
        original::reset_retired_crop_grower(finished.actor).cleared_raw_yield_word==10,
        "source-inactive body can still harvest before retirement; it cannot imply source receipt");
}
} // namespace
int main() {
    try {
        for(const auto field:{original::ModelBuilding::WheatField,original::ModelBuilding::MilletField,
            original::ModelBuilding::RiceField,original::ModelBuilding::CabbageField,original::ModelBuilding::SoybeanField})
        {
            const auto first=same_crop_event_chain(field);
            check(same_crop_event_chain(field)==first,
                "complete resolved-event replay must preserve fields, actor reset, counters and typed bins");
        }
        expiry_and_source_inactive_boundaries();
        std::cout<<"Original resolved crop events PASS: "<<checks<<" checks\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
