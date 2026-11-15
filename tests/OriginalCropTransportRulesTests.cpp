#include "simulation/original/CropTransportRules.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace original=openemperor::original;
using original::RawFarmBin;
std::size_t checks=0;

void check(bool yes,std::string_view message) {
    ++checks;if(!yes)throw std::runtime_error(std::string(message));
}
template<class Operation>void rejected(Operation operation) {
    try {operation();}catch(const std::invalid_argument&){++checks;return;}
    catch(const std::out_of_range&){++checks;return;}
    throw std::runtime_error("invalid raw crop transport input was accepted");
}

void resource_identity_and_selection() {
    constexpr std::array<std::uint8_t,5> resources{1,3,5,6,7};
    for(const auto resource:resources) {
        std::array<RawFarmBin,5> bins{};
        bins[3]={resource,17,400};
        check(original::select_farm_raw_bin(bins,resource)==3,
              "Earlier empty slot displaced the existing typed resource bin");
        bins[3].quantity=0;
        check(original::select_farm_raw_bin(bins,resource)==3,
              "Retained zero-quantity resource identity lost its matching slot");
        bins[3].resource=0;
        check(original::select_farm_raw_bin(bins,resource)==0,
              "New resource did not choose the first completely empty slot");
    }
    std::array<RawFarmBin,5> full{{{1,1,400},{1,2,400},{1,3,400},{1,4,400},{1,5,400}}};
    const auto stable=full;
    check(!original::select_farm_raw_bin(full,6),"Five occupied foreign bins silently admitted another crop");
    check(full==stable,"Pure Farm bin selection mutated the supplied physical bins");
    for(unsigned resource=0;resource<=255;++resource) {
        bool known=false;for(const auto admitted:resources)known|=resource==admitted;
        if(known)continue;
        rejected([&]{(void)original::select_farm_raw_bin({},static_cast<std::uint8_t>(resource));});
    }
    full[4]={0,1,400};
    rejected([&]{(void)original::select_farm_raw_bin(full,1);});
}

void credit_and_explicit_overflow() {
    const auto overflow=original::credit_farm_raw_bin({6,390,400},6,35);
    check(overflow.bin==RawFarmBin{6,400,400} && overflow.accepted==10 && overflow.overflow==25,
          "Farm raw capacity clamp failed to expose the actual unaccepted quantity");
    const auto no_room=original::credit_farm_raw_bin({3,25,25},3,4);
    check(no_room.bin==RawFarmBin{3,25,25} && !no_room.accepted && no_room.overflow==4,
          "Caller-resolved capacity was replaced by the constructor default");
    const auto zero=original::credit_farm_raw_bin({0,0,0},1,0);
    check(zero.bin==RawFarmBin{1,0,0} && !zero.accepted && !zero.overflow,
          "Zero credit lost Original retained-identity assignment");
    const auto maximum=original::credit_farm_raw_bin({7,0,32767},7,32767);
    check(maximum.bin.quantity==32767 && maximum.accepted==32767 && !maximum.overflow,
          "Maximum supported raw signed-WORD capacity overflowed");
    const auto maximum_overflow=original::credit_farm_raw_bin({7,32767,32767},7,32767);
    check(maximum_overflow.bin==RawFarmBin{7,32767,32767} && !maximum_overflow.accepted &&
        maximum_overflow.overflow==32767,
        "Maximum supported sum lost typed stock or its explicitly returned overflow");
    for(std::uint16_t stock=0;stock<=400;++stock)for(std::uint16_t offered=0;offered<=401;++offered) {
        const auto result=original::credit_farm_raw_bin({5,stock,400},5,offered);
        check(result.bin.resource==5 && result.bin.quantity<=400 &&
            static_cast<unsigned>(result.bin.quantity)+result.overflow==static_cast<unsigned>(stock)+offered &&
            static_cast<unsigned>(result.accepted)+result.overflow==offered,
            "Raw Farm credit changed crop identity or broke physical quantity accounting");
    }
    for(const auto bad:std::array<RawFarmBin,4>{{{0,1,400},{6,401,400},{6,0,32768},{2,1,400}}})
        rejected([&]{(void)original::credit_farm_raw_bin(bad,6,1);});
    rejected([]{(void)original::credit_farm_raw_bin({3,0,400},6,1);});
    rejected([]{(void)original::credit_farm_raw_bin({},0,1);});
    rejected([]{(void)original::credit_farm_raw_bin({},6,32768);});
}

void debit_floor_and_empty_identity() {
    const auto part=original::debit_farm_raw_bin({6,400,400},137,true);
    check(part.bin==RawFarmBin{6,263,400} && part.removed==137,
          "Partial raw Farm debit changed its category or amount");
    const auto clear=original::debit_farm_raw_bin({3,17,400},100,true);
    check(clear.bin==RawFarmBin{0,0,400} && clear.removed==17,
          "Insufficient debit did not floor stock and clear an eligible empty identity");
    const auto retained=original::debit_farm_raw_bin({3,17,400},100,false);
    check(retained.bin==RawFarmBin{3,0,400} && retained.removed==17,
          "Empty identity was cleared despite a false caller flag");
    const auto already_empty=original::debit_farm_raw_bin({7,0,400},0,true);
    check(already_empty.bin==RawFarmBin{0,0,400} && !already_empty.removed,
          "Zero debit missed the Original empty-identity clearing branch");
    for(const auto stock:{std::uint16_t{0},std::uint16_t{1},std::uint16_t{399},std::uint16_t{400},std::uint16_t{32767}})
        for(const auto requested:{std::uint16_t{0},std::uint16_t{1},std::uint16_t{400},std::uint16_t{32767}}) {
            const auto result=original::debit_farm_raw_bin({1,stock,32767},requested,false);
            check(static_cast<unsigned>(result.bin.quantity)+result.removed==stock && result.bin.resource==1,
                  "Raw debit violated conservation or retained identity");
        }
    rejected([]{(void)original::debit_farm_raw_bin({6,401,400},1,true);});
    rejected([]{(void)original::debit_farm_raw_bin({},32768,false);});
}

void actual_target_receipt_and_residual() {
    using Branch=original::RawCartReceiptBranch;
    const auto none=original::accept_farm_raw_cart_cargo(400,0);
    check(none.accepted==0 && none.residual==400 && none.branch==Branch::ReselectDestination,
          "Refused target receipt discarded cargo or triggered a source return");
    const auto partial=original::accept_farm_raw_cart_cargo(400,137);
    check(partial.accepted==137 && partial.residual==263 && partial.branch==Branch::ReselectDestination,
          "Partial target acceptance was interpreted as overflow");
    const auto complete=original::accept_farm_raw_cart_cargo(263,263);
    check(complete.accepted==263 && !complete.residual && complete.branch==Branch::ReturnToSource,
          "Full actual target acceptance failed the source-return branch");
    check(original::accept_farm_raw_cart_cargo(0,0).branch==Branch::ReturnToSource,
          "The equality branch changed for a zero-cargo observation");
    check(original::accept_farm_raw_cart_cargo(32767,32766).residual==1,
          "Supported signed-WORD Cart residual overflowed");
    for(std::uint16_t cargo=0;cargo<=400;++cargo)for(std::uint16_t accepted=0;accepted<=cargo;++accepted) {
        const auto result=original::accept_farm_raw_cart_cargo(cargo,accepted);
        check(static_cast<unsigned>(result.accepted)+result.residual==cargo &&
            (result.branch==Branch::ReturnToSource)==(result.residual==0),
            "Cart actual receipt broke physical cargo accounting or branch selection");
    }
    rejected([]{(void)original::accept_farm_raw_cart_cargo(100,101);});
    rejected([]{(void)original::accept_farm_raw_cart_cargo(32768,0);});
    rejected([]{(void)original::accept_farm_raw_cart_cargo(400,65535);});
}

void raw_harvest_bin_cart_receipt_sequence() {
    RawFarmBin bin{};
    for(const auto amount:{std::uint16_t{87},std::uint16_t{200},std::uint16_t{113}}) {
        const auto result=original::credit_farm_raw_bin(bin,6,amount);
        check(!result.overflow,"Finite raw harvest sequence unexpectedly overflowed the Farm bin");
        bin=result.bin;
    }
    const auto load=original::debit_farm_raw_bin(bin,original::farm_raw_cart_amount,true);
    check(load.removed==400 && load.bin==RawFarmBin{},"Actual successful raw400 handoff did not drain the Farm bin");
    auto cart=original::accept_farm_raw_cart_cargo(load.removed,137);
    cart=original::accept_farm_raw_cart_cargo(cart.residual,0);
    const auto last=original::accept_farm_raw_cart_cargo(cart.residual,263);
    check(!last.residual && 137U+last.accepted==load.removed,
          "Sequential partial/refused/full receipts duplicated or lost physical raw cargo");
}
}

int main() {
    try {
        resource_identity_and_selection();credit_and_explicit_overflow();debit_floor_and_empty_identity();
        actual_target_receipt_and_residual();raw_harvest_bin_cart_receipt_sequence();
        std::cout<<"Original raw Farm bins, overflow, debit and actual Cart acceptance "<<checks<<" checks passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
