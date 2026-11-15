#include "simulation/original/CropGrowerRules.h"

#include <stdexcept>

namespace openemperor::original {
namespace {
void validate_actor(const CropGrowerActor& actor) {
    if(actor.raw_source_word==0 || actor.raw_source_word>32767 || actor.raw_field_word>32767)
        throw std::invalid_argument("Food Grower references require a positive source and nonnegative signed WORDs");
    if((actor.raw_status!=1 && actor.raw_status!=2) ||
       (actor.raw_role!=1 && actor.raw_role!=2))
        throw std::invalid_argument("unsupported Food Grower status or role");
}
void validate_counters(CropGrowerCounters counters) {
    if(counters.harvest<0 || counters.tending<0)
        throw std::invalid_argument("negative retained Food Grower counter");
}
void debit(CropGrowerCounters& counters,std::uint8_t role) {
    auto& value=role==2 ? counters.harvest:counters.tending;
    if(value>0)--value;
}
std::int32_t signed_word(std::uint16_t raw) noexcept {
    return raw<0x8000U ? static_cast<std::int32_t>(raw):static_cast<std::int32_t>(raw)-65536;
}
}

CropGrowerActor begin_crop_grower_callback(CropGrowerActor actor,std::uint8_t source_status) {
    validate_actor(actor);
    if(actor.raw_state!=4 && (actor.raw_state<6 || actor.raw_state>13))
        throw std::invalid_argument("Food Grower callback begin requires state4 or6..13");
    actor.raw_flag_6f=0;
    if(source_status!=1)actor.raw_status=2;
    return actor;
}

CropGrowerActor advance_crop_grower_state4_wait(CropGrowerActor actor) {
    validate_actor(actor);
    if(actor.raw_state!=4)
        throw std::invalid_argument("Food Grower state4 wait requires actual state4");
    const auto nonnegative=actor.wait<0 ? std::uint16_t{0}:static_cast<std::uint16_t>(actor.wait);
    const auto next=static_cast<std::uint16_t>(static_cast<std::uint32_t>(nonnegative)+1U);
    actor.wait=static_cast<std::int16_t>(signed_word(next));
    if(actor.wait>=128) { actor.wait=127;actor.raw_status=2; }
    return actor;
}

CropGrowerWorkStep advance_crop_grower_work(CropGrowerActor actor,bool field_compatible) {
    validate_actor(actor);
    const auto state=actor.raw_state;
    if(state!=6 && state!=8 && state!=10 && state!=12)
        throw std::invalid_argument("Food Grower work requires state6/8/10/12");
    if(field_compatible && actor.raw_field_word==0 && (state==8 || state==12))
        throw std::invalid_argument("compatible Food Grower work requires an actual field reference");
    actor.raw_flag_6f=static_cast<std::uint8_t>(state==6 || state==10);
    const auto wait=advance_crop_work_wait(actor.wait);
    actor.wait=wait.remaining;
    CropGrowerWorkStep result{actor,false,CropGrowerFieldCallback::None};
    if(!wait.callback_due)return result;
    if(state==6 || state==10) {
        result.actor.wait=0;
        result.select_target=true;
    } else {
        result.actor.raw_state=static_cast<std::uint8_t>(state==8 ? 6:10);
        if(field_compatible)
            result.field_callback=state==8 ? CropGrowerFieldCallback::Tending:CropGrowerFieldCallback::Harvest;
    }
    return result;
}

CropGrowerTargetStep apply_crop_grower_target(CropGrowerActor actor,
    CropGrowerCounters counters,const std::optional<CropGrowerTarget>& actual_target,
    bool target_in_growing_period,CropGrowerCell source_return) {
    validate_actor(actor);validate_counters(counters);
    if(actor.raw_state!=6 && actor.raw_state!=10)
        throw std::invalid_argument("Food Grower target result requires state6/10");
    if(actual_target && (actual_target->raw_field_word==0 || actual_target->raw_field_word>32767))
        throw std::invalid_argument("actual Food Grower target requires a positive signed-WORD identity");
    const bool tending=actor.raw_state==6;
    if(actual_target && (!tending || target_in_growing_period)) {
        actor.raw_field_word=actual_target->raw_field_word;
        actor.target=actual_target->position;
        actor.raw_state=static_cast<std::uint8_t>(tending ? 7:11);
    } else {
        actor.target=source_return;
        actor.raw_state=static_cast<std::uint8_t>(!tending && signed_word(actor.raw_yield_word)>0 ? 13:9);
        debit(counters,static_cast<std::uint8_t>(tending ? 1:2));
    }
    return {actor,counters,true};
}

CropGrowerMovementStep apply_crop_grower_movement(CropGrowerActor actor,
    const CropGrowerMovement& movement) {
    validate_actor(actor);
    const auto state=actor.raw_state;
    if(state!=7 && state!=9 && state!=11 && state!=13)
        throw std::invalid_argument("Food Grower movement requires state7/9/11/13");
    if(movement.food_field)(void)crop_rule(*movement.food_field);
    if((state==7 || state==11) && movement.outcome==8 && movement.field_compatible &&
       (!movement.food_field || actor.raw_field_word==0))
        throw std::invalid_argument("accepted Food Grower arrival requires actual typed field context");
    actor.raw_flag_6f=0;
    CropGrowerMovementStep result{actor,false,false};
    if(movement.outcome==9) {
        result.clear_route=true;
    } else if(movement.outcome==10) {
        result.actor.raw_status=2;
        if(state==7 || state==11)result.actor.raw_flag_6f=1;
    } else if(movement.outcome==8) {
        if(state==7 || state==11) {
            result.actor.raw_state=static_cast<std::uint8_t>(state==7 ? 8:12);
            result.actor.wait=movement.food_field
                ? initial_crop_work_wait(*movement.food_field,state==7 ? CropWork::Tending:CropWork::Harvest,
                    movement.field_compatible,movement.field_assignment_nonzero,movement.terrain_flag_100)
                : 0;
        } else {
            result.actor.raw_status=2;
            result.actor.raw_flag_6f=static_cast<std::uint8_t>(state==9);
            result.source_receipt=state==13;
        }
    }
    return result;
}

CropGrowerCallbackFinish finish_crop_grower_callback(CropGrowerActor actor,
    CropGrowerCounters counters) {
    validate_actor(actor);validate_counters(counters);
    if(actor.raw_state!=4 && (actor.raw_state<6 || actor.raw_state>13))
        throw std::invalid_argument("Food Grower callback epilogue requires state4 or6..13");
    const bool retiring=actor.raw_status==2;
    if(retiring)debit(counters,actor.raw_role);
    return {actor,counters,retiring};
}

std::size_t clear_crop_grower_assignments(std::span<CropGrowerFieldAssignment> fields,
    std::uint16_t actual_actor_word) {
    if(actual_actor_word==0 || actual_actor_word>32767)
        throw std::invalid_argument("Food Grower cleanup requires a positive signed-WORD actor identity");
    std::size_t cleared=0;
    for(auto& field:fields)
        if(field.raw_status==1 && field.raw_actor_word==actual_actor_word) {
            field.raw_actor_word=0;++cleared;
        }
    return cleared;
}

CropGrowerReset reset_retired_crop_grower(CropGrowerActor actor) {
    validate_actor(actor);
    if(actor.raw_status!=2)
        throw std::invalid_argument("Food Grower reset requires actual retiring status2");
    return {{},actor.raw_yield_word};
}

} // namespace openemperor::original
