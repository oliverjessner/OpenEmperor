// Independent literal staffing/counter controls for the bounded Original
// dispatch operation. These tests do not create a World, worker or inventory.
#include "simulation/original/CropRules.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace original=openemperor::original;
std::size_t checks=0;
void check(bool condition,std::string_view message) {
    ++checks;if(!condition)throw std::runtime_error(std::string(message));
}
template<class Error,class Operation> void rejected(Operation operation) {
    try {operation();}catch(const Error&){++checks;return;}
    throw std::runtime_error("Unsupported crop dispatch operand was accepted");
}

void literal_staffing_boundaries() {
    struct Case {std::uint8_t staff,threshold;std::int32_t harvest,tending;};
    constexpr std::array cases{
        Case{0,0,0,0},Case{1,32,0,0},Case{24,32,0,0},Case{25,16,1,1},
        Case{49,16,1,1},Case{50,8,2,1},Case{74,8,2,1},Case{75,4,3,1},
        Case{99,4,3,1},Case{100,1,5,2}
    };
    for(const auto& row:cases) {
        check(original::crop_dispatch_rule(row.staff,{5,2})==original::CropDispatchRule{
            row.staff!=0,row.threshold,row.harvest,row.tending},
            "Food staffing tier lost a literal cap, tending minimum or attempt threshold");
        // Exhaust every retained byte, including255. Due is based on the old
        // byte; Original's increment cannot postpone a255 attempt by wrapping.
        for(unsigned counter=0;counter<=255;++counter) {
            const auto input=static_cast<std::uint8_t>(counter);
            const auto result=original::advance_crop_dispatch_attempt(input,row.staff,{5,2});
            if(!row.staff) check(result==original::CropDispatchAttempt{input,false},
                "Unstaffed source changed its retained attempt byte");
            else if(counter<row.threshold) check(result==original::CropDispatchAttempt{
                static_cast<std::uint8_t>(counter+1),false},"Crop dispatch fired before its old-byte threshold");
            else check(result==original::CropDispatchAttempt{0,true},
                "Due old-byte dispatch failed to reset, including255 wrap");
        }
    }
    struct Cadence {std::uint8_t staff;unsigned calls;};
    constexpr std::array cadence{Cadence{100,2},Cadence{75,5},Cadence{50,9},Cadence{25,17},Cadence{1,33}};
    for(const auto row:cadence) {
        auto counter=std::uint8_t{0};
        for(unsigned call=1;call<=row.calls*3;++call) {
            const auto result=original::advance_crop_dispatch_attempt(counter,row.staff,{5,2});
            check(result.attempt_due==(call%row.calls==0),
                  "Repeated dispatch cadence changed its literal eligible-call period");
            counter=result.counter;
        }
    }
    check(original::advance_crop_dispatch_attempt(0,100,{5,2})==original::CropDispatchAttempt{1,false} &&
          original::advance_crop_dispatch_attempt(1,100,{5,2})==original::CropDispatchAttempt{0,true},
          "Crop dispatch adopted Hunter's different counter comparison order");
    check(original::advance_crop_dispatch_attempt(32,1,{5,2}).attempt_due &&
          !original::crop_dispatch_rule(1,{5,2}).harvest_limit &&
          !original::crop_dispatch_rule(1,{5,2}).tending_limit,
          "An attempt boundary was incorrectly equated with a permitted or successful actor spawn");
}

void independently_supplied_configuration() {
    using original::CropDispatchRule;
    check(original::crop_dispatch_rule(100,{7,11})==CropDispatchRule{true,1,7,11} &&
          original::crop_dispatch_rule(75,{7,11})==CropDispatchRule{true,4,5,8} &&
          original::crop_dispatch_rule(50,{7,11})==CropDispatchRule{true,8,3,5} &&
          original::crop_dispatch_rule(25,{7,11})==CropDispatchRule{true,16,1,2},
          "Dispatch ignored supplied configuration or changed truncation in a partial tier");
    check(original::crop_dispatch_rule(100,{0,0})==CropDispatchRule{true,1,0,0},
          "Full-staff dispatch invented a quota from zero configured limits");
    for(const auto staff:{std::uint8_t{25},std::uint8_t{50},std::uint8_t{75}}) {
        const auto result=original::crop_dispatch_rule(staff,{0,0});
        check(!result.harvest_limit && result.tending_limit==1,
              "Partial-staff tending minimum was applied to harvest or omitted");
    }
    check(original::crop_dispatch_rule(24,{7,11})==CropDispatchRule{true,32,0,0},
          "Below-quarter staffing gained a positive quota from configuration");
    constexpr auto high=std::numeric_limits<std::int32_t>::max();
    check(original::crop_dispatch_rule(100,{high,high})==CropDispatchRule{true,1,high,high} &&
          original::crop_dispatch_rule(50,{high,high})==CropDispatchRule{true,8,1073741823,1073741823} &&
          original::crop_dispatch_rule(25,{high,high})==CropDispatchRule{true,16,536870911,536870911} &&
          original::crop_dispatch_rule(75,{715827882,715827882})==CropDispatchRule{true,4,536870911,536870911},
          "Supported nonnegative signed-DWORD boundaries changed or overflowed");
    for(const auto configured:{original::CropDispatchConfig{715827883,0},
                               original::CropDispatchConfig{0,715827883},
                               original::CropDispatchConfig{high,high}}) {
        rejected<std::overflow_error>([&]{(void)original::crop_dispatch_rule(75,configured);});
        rejected<std::overflow_error>([&]{(void)original::advance_crop_dispatch_attempt(255,75,configured);});
    }
    for(const auto configured:{original::CropDispatchConfig{-1,2},original::CropDispatchConfig{5,-1},
                               original::CropDispatchConfig{std::numeric_limits<std::int32_t>::min(),0}})
        rejected<std::out_of_range>([&]{(void)original::crop_dispatch_rule(100,configured);});
    for(const auto staff:{std::uint8_t{101},std::uint8_t{255}}) {
        rejected<std::out_of_range>([&]{(void)original::crop_dispatch_rule(staff,{5,2});});
        rejected<std::out_of_range>([&]{(void)original::advance_crop_dispatch_attempt(0,staff,{5,2});});
    }
}
} // namespace

int main() {
    try {
        literal_staffing_boundaries();independently_supplied_configuration();
        std::cout<<"Original crop dispatch: resolved quotas and old-byte attempt order "<<checks<<" checks passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
