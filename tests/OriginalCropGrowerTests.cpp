#include "simulation/original/CropGrowerRules.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace original=openemperor::original;
namespace {
std::uint64_t checks=0;
void check(bool condition,std::string_view message) {
    ++checks;
    if(!condition)throw std::runtime_error(std::string(message));
}
template<class Operation> void refuses(Operation operation) {
    try { operation(); }
    catch(const std::logic_error&) { ++checks;return; }
    throw std::runtime_error("Unsupported Food Grower context was admitted");
}
original::CropGrowerActor actor(std::uint8_t state,std::int16_t wait=1) {
    return {17,23,state,1,static_cast<std::uint8_t>(state>=10 ? 2:1),wait,333,{8,9},77};
}
constexpr std::array fields{original::ModelBuilding::WheatField,original::ModelBuilding::MilletField,
    original::ModelBuilding::RiceField,original::ModelBuilding::CabbageField,
    original::ModelBuilding::SoybeanField};

void complete_wait_domain() {
    for(std::uint32_t raw=0;raw<=65535;++raw) {
        const auto value=raw<32768 ? static_cast<std::int32_t>(raw):static_cast<std::int32_t>(raw)-65536;
        const auto before=static_cast<std::int16_t>(value);
        const auto after=value==-32768 ? 32767:value-1;
        for(const auto state:std::array<std::uint8_t,4>{6,8,10,12})
            for(const bool compatible:{false,true}) {
                const auto initial=actor(state,before);
                auto expected=initial;
                const bool selecting=state==6 || state==10;
                expected.raw_flag_6f=static_cast<std::uint8_t>(selecting);
                expected.wait=static_cast<std::int16_t>(after);
                bool select=false;
                auto callback=original::CropGrowerFieldCallback::None;
                if(after<=0) {
                    if(selecting) { expected.wait=0;select=true; }
                    else {
                        expected.raw_state=static_cast<std::uint8_t>(state==8 ? 6:10);
                        if(compatible) callback=state==8 ? original::CropGrowerFieldCallback::Tending:
                            original::CropGrowerFieldCallback::Harvest;
                    }
                }
                check(original::advance_crop_grower_work(initial,compatible)==
                    original::CropGrowerWorkStep{expected,select,callback},
                    "Complete signed-WORD wait gate or callback/reset ordering changed");
                check(initial==actor(state,before),"Wait operation changed its input observation");
            }
    }
    auto expired=actor(12,0);
    const auto completion=original::advance_crop_grower_work(expired,true);
    check(completion.actor.wait==-1 && completion.actor.raw_state==10 &&
        completion.field_callback==original::CropGrowerFieldCallback::Harvest &&
        completion.actor.raw_yield_word==333,
        "Expiration's zero wait must request the callback, without generating raw yield");
    auto unresolved=actor(8,1);unresolved.raw_field_word=0;
    check(original::advance_crop_grower_work(unresolved,false).field_callback==
        original::CropGrowerFieldCallback::None,"Incompatible field became actual tending");
    refuses([&]{(void)original::advance_crop_grower_work(unresolved,true);});
}

void explicit_state4_word_operation() {
    for(std::uint32_t raw=0;raw<=65535;++raw) {
        const auto value=raw<32768 ? static_cast<std::int32_t>(raw):static_cast<std::int32_t>(raw)-65536;
        for(const auto status:std::array<std::uint8_t,2>{1,2}) {
            auto initial=actor(4,static_cast<std::int16_t>(value));initial.raw_status=status;
            auto expected=initial;
            if(value<0)expected.wait=1;
            else if(value==32767)expected.wait=-32768;
            else if(value>=127) { expected.wait=127;expected.raw_status=2; }
            else expected.wait=static_cast<std::int16_t>(value+1);
            check(original::advance_crop_grower_state4_wait(initial)==expected,
                "Standalone state4 changed wait semantics or a retained nonzero flag");
            expected.raw_flag_6f=0;
            check(original::advance_crop_grower_state4_wait(
                    original::begin_crop_grower_callback(initial,1))==expected,
                "Begin owns flag clearing before the independent state4 helper");
        }
    }
}

void actual_movement_results() {
    for(const auto field:fields)
        for(const auto state:std::array<std::uint8_t,4>{7,9,11,13})
            for(unsigned outcome=0;outcome<=255;++outcome)
                for(unsigned flags=0;flags<8;++flags) {
                    const bool compatible=(flags&1U)!=0;
                    const bool assigned=(flags&2U)!=0;
                    const bool terrain=(flags&4U)!=0;
                    const auto initial=actor(state,19);
                    auto expected=initial;expected.raw_flag_6f=0;
                    bool clear=false,receipt=false;
                    if(outcome==9)clear=true;
                    else if(outcome==10) {
                        expected.raw_status=2;
                        expected.raw_flag_6f=static_cast<std::uint8_t>(state==7 || state==11);
                    } else if(outcome==8) {
                        if(state==7 || state==11) {
                            expected.raw_state=static_cast<std::uint8_t>(state==7 ? 8:12);
                            expected.wait=static_cast<std::int16_t>(compatible && assigned && !terrain
                                ? (state==7 ? 51:25):0);
                        } else {
                            expected.raw_status=2;
                            expected.raw_flag_6f=static_cast<std::uint8_t>(state==9);
                            receipt=state==13;
                        }
                    }
                    const original::CropGrowerMovement movement{
                        static_cast<std::uint8_t>(outcome),field,compatible,assigned,terrain};
                    check(original::apply_crop_grower_movement(initial,movement)==
                        original::CropGrowerMovementStep{expected,clear,receipt},
                        "Literal movement8/9/10 and ordinary-state effects changed");
                    check(initial==actor(state,19),"Movement event changed input or created cargo");
                }
    const auto no_field=original::apply_crop_grower_movement(actor(7),{8,{},false,true,false});
    check(no_field.actor.raw_state==8 && no_field.actor.wait==0 && !no_field.source_receipt,
        "Incompatible movement arrival fabricated a field wait or source receipt");
    refuses([]{(void)original::apply_crop_grower_movement(actor(7),{8,{},true,true,false});});
    auto zero=actor(11);zero.raw_field_word=0;
    refuses([&]{(void)original::apply_crop_grower_movement(zero,{8,fields[0],true,true,false});});
}

void actual_target_results_and_signed_cargo() {
    const original::CropGrowerCounters initial_counters{5,7};
    constexpr original::CropGrowerTarget target{111,{12,13}};
    constexpr original::CropGrowerCell source{3,4};
    for(const auto state:std::array<std::uint8_t,2>{6,10})
        for(const bool present:{false,true})
            for(const bool growing:{false,true}) {
                const auto initial=actor(state);
                auto expected=initial;
                auto counters=initial_counters;
                const bool chosen=present && (state==10 || growing);
                if(chosen) {
                    expected.raw_field_word=111;expected.target={12,13};
                    expected.raw_state=static_cast<std::uint8_t>(state==6 ? 7:11);
                } else {
                    expected.target=source;
                    expected.raw_state=static_cast<std::uint8_t>(state==6 ? 9:13);
                    if(state==6)counters.tending=6;else counters.harvest=4;
                }
                const auto actual=original::apply_crop_grower_target(initial,initial_counters,
                    present ? std::optional{target}:std::nullopt,growing,source);
                check(actual==original::CropGrowerTargetStep{expected,counters,true},
                    "Actual target result changed assignment, period recheck or return selection");
            }
    for(std::uint32_t raw=0;raw<=65535;++raw) {
        auto initial=actor(10);initial.raw_yield_word=static_cast<std::uint16_t>(raw);
        const auto result=original::apply_crop_grower_target(initial,initial_counters,{},true,source);
        const auto expected_state=static_cast<std::uint8_t>(raw>0 && raw<32768 ? 13:9);
        check(result.actor.raw_state==expected_state && result.actor.raw_yield_word==raw &&
            result.actor.raw_field_word==23 && result.counters==original::CropGrowerCounters{4,7},
            "Failed harvest selection changed signed cargo semantics or last field identity");
    }
    for(const auto amount:std::array<std::int32_t,4>{0,1,2,std::numeric_limits<std::int32_t>::max()}) {
        const original::CropGrowerCounters counters{amount,amount};
        for(const auto state:std::array<std::uint8_t,2>{6,10}) {
            const auto result=original::apply_crop_grower_target(actor(state),counters,{},true,source);
            const auto expected=amount==0 ? 0:amount-1;
            check(result.counters.harvest==(state==10 ? expected:amount) &&
                result.counters.tending==(state==6 ? expected:amount),
                "Retained selection counter failed to floor at zero or retain unrelated role");
        }
    }
}

void epilogue_and_double_debit() {
    for(unsigned source_status=0;source_status<=255;++source_status)
        for(const auto status:std::array<std::uint8_t,2>{1,2})
            for(const auto role:std::array<std::uint8_t,2>{1,2})
                for(std::uint8_t state=6;state<=13;++state)
                    for(const auto amount:std::array<std::int32_t,4>{0,1,2,std::numeric_limits<std::int32_t>::max()}) {
                        auto initial=actor(state);initial.raw_status=status;initial.raw_role=role;
                        auto expected=initial;expected.raw_flag_6f=0;
                        const bool retiring=status==2 || source_status!=1;
                        if(retiring)expected.raw_status=2;
                        original::CropGrowerCounters counters{amount,amount};
                        if(retiring && amount>0) {
                            if(role==2)--counters.harvest;else --counters.tending;
                        }
                        check(original::finish_crop_grower_callback(original::begin_crop_grower_callback(
                            initial,static_cast<std::uint8_t>(source_status)),{amount,amount})==
                            original::CropGrowerCallbackFinish{expected,counters,retiring},
                            "Retained raw source status or callback retirement counter changed");
                    }
    const auto selected=original::apply_crop_grower_target(actor(10),{5,7},{},true,{3,4});
    const auto returned=original::apply_crop_grower_movement(selected.actor,{8,{},false,false,false});
    const auto finish=original::finish_crop_grower_callback(returned.actor,selected.counters);
    check(selected.counters.harvest==4 && returned.source_receipt &&
        finish.counters.harvest==3 && finish.actor.raw_yield_word==333 && finish.retirement_requested,
        "Failed-selection and actual-return retirement must retain two native counter debits");
    const auto failed_return=original::apply_crop_grower_movement(selected.actor,{10,{},false,false,false});
    check(failed_return.actor.raw_status==2 && !failed_return.source_receipt &&
        failed_return.actor.raw_yield_word==333,"Failed return became a source receipt or silently removed cargo");
    const auto begun=original::begin_crop_grower_callback(actor(12,1),0);
    const auto work=original::advance_crop_grower_work(begun,true);
    const auto inactive=original::finish_crop_grower_callback(work.actor,{5,7});
    check(begun.raw_status==2 && work.actor.raw_status==2 &&
        work.field_callback==original::CropGrowerFieldCallback::Harvest && inactive.actor.raw_status==2 &&
        inactive.counters.harvest==4,"Source inactivity incorrectly skipped the native callback body");
}

void all_active_assignments_and_reset() {
    for(unsigned status=0;status<=255;++status) {
        std::array<original::CropGrowerFieldAssignment,5> assignments{{
            {static_cast<std::uint8_t>(status),99},{1,99},{1,100},{0,99},{1,65535}}};
        auto expected=assignments;
        expected[1].raw_actor_word=0;
        if(status==1)expected[0].raw_actor_word=0;
        check(original::clear_crop_grower_assignments(assignments,99)==(status==1 ? 2U:1U) &&
            assignments==expected,"Cleanup failed to clear every active match or changed inactive/other references");
    }
    for(const auto invalid:std::array<std::uint16_t,3>{0,32768,65535}) {
        std::array<original::CropGrowerFieldAssignment,2> fields_to_preserve{{{1,99},{1,100}}};
        const auto before=fields_to_preserve;
        refuses([&]{(void)original::clear_crop_grower_assignments(fields_to_preserve,invalid);});
        check(fields_to_preserve==before,"Rejected cleanup changed a retained assignment");
    }
    for(std::uint32_t raw=0;raw<=65535;++raw) {
        auto retiring=actor(13);retiring.raw_status=2;retiring.raw_yield_word=static_cast<std::uint16_t>(raw);
        check(original::reset_retired_crop_grower(retiring)==
            original::CropGrowerReset{{},static_cast<std::uint16_t>(raw)},
            "Manager reset fabricated a refund or lost the observed cleared raw word");
    }
    refuses([]{(void)original::reset_retired_crop_grower(actor(13));});
}

void unsupported_contexts() {
    for(unsigned state=0;state<=255;++state) {
        const auto raw=static_cast<std::uint8_t>(state);
        if(state!=6 && state!=8 && state!=10 && state!=12)
            refuses([&]{(void)original::advance_crop_grower_work(actor(raw),false);});
        if(state!=7 && state!=9 && state!=11 && state!=13)
            refuses([&]{(void)original::apply_crop_grower_movement(actor(raw),{});});
        if(state!=6 && state!=10)
            refuses([&]{(void)original::apply_crop_grower_target(actor(raw),{},{},false,{});});
        if(state!=4 && (state<6 || state>13)) {
            refuses([&]{(void)original::begin_crop_grower_callback(actor(raw),1);});
            refuses([&]{(void)original::finish_crop_grower_callback(actor(raw),{});});
        }
        if(state!=4)
            refuses([&]{(void)original::advance_crop_grower_state4_wait(actor(raw));});
    }
    for(const auto invalid:std::array<std::uint16_t,3>{0,32768,65535}) {
        auto bad=actor(6);bad.raw_source_word=invalid;
        refuses([&]{(void)original::advance_crop_grower_work(bad,false);});
    }
    for(const auto invalid:std::array<std::uint16_t,2>{32768,65535}) {
        auto bad=actor(8);bad.raw_field_word=invalid;
        refuses([&]{(void)original::advance_crop_grower_work(bad,false);});
    }
    for(const auto invalid:std::array<std::uint8_t,3>{0,3,255}) {
        auto bad=actor(6);bad.raw_status=invalid;
        refuses([&]{(void)original::advance_crop_grower_work(bad,false);});
        bad=actor(6);bad.raw_role=invalid;
        refuses([&]{(void)original::advance_crop_grower_work(bad,false);});
    }
    for(const auto invalid:std::array<std::uint16_t,3>{0,32768,65535})
        refuses([&]{(void)original::apply_crop_grower_target(actor(6),{},
            original::CropGrowerTarget{invalid,{}},true,{});});
    for(const auto counters:std::array{original::CropGrowerCounters{-1,0},
            original::CropGrowerCounters{0,-1},original::CropGrowerCounters{-1,-1}}) {
        refuses([&]{(void)original::apply_crop_grower_target(actor(6),counters,{},false,{});});
        refuses([&]{(void)original::finish_crop_grower_callback(actor(6),counters);});
    }
    for(const auto model:{original::ModelBuilding::Farmhouse,original::ModelBuilding::TeaBush,
            static_cast<original::ModelBuilding>(0),static_cast<original::ModelBuilding>(65535)})
        refuses([&]{(void)original::apply_crop_grower_movement(actor(7),{0,model,false,false,false});});
}
}
int main() {
    try {
        complete_wait_domain();explicit_state4_word_operation();actual_movement_results();actual_target_results_and_signed_cargo();
        epilogue_and_double_debit();all_active_assignments_and_reset();unsupported_contexts();
        std::cout<<"Original Food Grower rules: "<<checks<<" independent checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Original Food Grower rules failed: "<<error.what()<<'\n';return 1;
    }
}
