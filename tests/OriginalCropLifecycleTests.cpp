// Independent literal seasonal expectations; no World or advancing clock.
#include "simulation/original/CropLifecycleRules.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace original=openemperor::original;
using original::ModelBuilding;
namespace {

std::uint64_t checks=0;
void check(bool condition,const char* message) {
    ++checks;
    if(!condition)throw std::runtime_error(message);
}

template<class F> void rejected(F operation) {
    bool failed=false;
    try { operation(); } catch(const std::out_of_range&) { failed=true; }
    check(failed,"Unsupported lifecycle input was accepted");
}

struct Season { ModelBuilding field;std::uint8_t end; };
constexpr std::array seasons{
    Season{ModelBuilding::WheatField,6},Season{ModelBuilding::MilletField,10},
    Season{ModelBuilding::RiceField,9},Season{ModelBuilding::CabbageField,11},
    Season{ModelBuilding::SoybeanField,8}
};

void all_slots_and_subslots() {
    constexpr std::array<std::uint16_t,6> words{0,0x40,0x1234,0x7fff,0x8000,0xffff};
    for(const auto season:seasons)
        for(std::uint8_t slot=0;slot<12;++slot)
            for(std::uint8_t subslot=0;subslot<16;++subslot)
                for(const bool active:{false,true})
                    for(const auto word:words) {
                        const auto expected=static_cast<std::uint16_t>(
                            active && slot==season.end && subslot==0 ? word|0x8000U:word);
                        check(original::schedule_crop_harvest_mark(season.field,slot,subslot,active,word)==expected,
                            "A literal crop/end/subslot schedule boundary changed");
                    }
}

void all_work_words_and_completed_slots() {
    for(const auto season:seasons)
        for(std::uint32_t raw=0;raw<=65535;++raw) {
            const auto word=static_cast<std::uint16_t>(raw);
            const auto quantity=static_cast<std::uint16_t>(raw%10001U);
            for(std::uint8_t slot=0;slot<12;++slot) {
                const auto marked=static_cast<std::uint16_t>(slot==season.end ? word|0x8000U:word);
                check(original::schedule_crop_harvest_mark(season.field,slot,0,true,word)==marked &&
                    original::schedule_crop_harvest_mark(season.field,slot,15,true,word)==word,
                    "Full-word schedule changed bits or marked a later subslot");
                for(const bool assignment:{false,true}) {
                    const original::CropFieldLifecycleInput input{quantity,word,assignment};
                    const auto actual=original::expire_crop_completed_slot(season.field,slot,true,input);
                    const original::CropFieldExpiration expected=slot==season.end
                        ? original::CropFieldExpiration{{0,0,false},quantity,true,assignment}
                        : original::CropFieldExpiration{input,0,false,false};
                    check(actual==expected,"Completed-slot reset lost quantity, work or assignment semantics");
                }
            }
            check(original::schedule_crop_harvest_mark(season.field,season.end,0,false,word)==word,
                "Inactive field acquired a harvest mark");
            for(const bool assignment:{false,true}) {
                const original::CropFieldLifecycleInput input{quantity,word,assignment};
                check(original::expire_crop_completed_slot(season.field,season.end,false,input)==
                        original::CropFieldExpiration{input,0,false,false},
                    "Inactive field was seasonally reset");
            }
        }
}

void independent_year_ordering_and_quantity_boundaries() {
    for(const auto season:seasons) {
        // The setter is before quantity growth. It has no quantity input:
        // a fresh zero-quantity field receives the mark but gains no quantity.
        original::CropFieldLifecycleInput empty{0,0x40,true};
        empty.work_word=original::schedule_crop_harvest_mark(season.field,season.end,0,true,empty.work_word);
        check(empty==original::CropFieldLifecycleInput{0,0x8040,true},
            "Zero-quantity schedule marking fabricated quantity or required a harvest threshold");
        check(original::expire_crop_completed_slot(season.field,season.end,true,empty)==
                original::CropFieldExpiration{{0,0,false},0,true,true},
            "Empty marked field failed its actual end-slot reset");

        for(const auto quantity:std::array<std::uint16_t,7>{0,1,999,1000,1001,9999,10000})
            for(const bool assignment:{false,true}) {
                const original::CropFieldLifecycleInput input{quantity,0x7fff,assignment};
                const auto at_start=original::schedule_crop_harvest_mark(season.field,season.end,0,true,input.work_word);
                check(at_start==0xffff,"First end subslot did not retain every original work bit");
                check(original::expire_crop_completed_slot(season.field,season.end,true,
                        {quantity,at_start,assignment})==original::CropFieldExpiration{{0,0,false},quantity,true,assignment},
                    "Quantity expiration incorrectly used1000 as a threshold or produced harvest cargo");
            }

        for(std::uint8_t slot=0;slot<12;++slot) {
            const original::CropFieldLifecycleInput input{4321,0x1234,true};
            const auto marked=original::schedule_crop_harvest_mark(season.field,slot,0,true,input.work_word);
            if(slot==season.end) {
                check(marked==0x9234 &&
                    original::schedule_crop_harvest_mark(season.field,slot,1,true,marked)==0x9234 &&
                    original::schedule_crop_harvest_mark(season.field,slot,15,true,marked)==0x9234,
                    "End-slot window did not retain its mark through the final subslot");
                const auto expired=original::expire_crop_completed_slot(season.field,slot,true,{4321,marked,true});
                check(expired==original::CropFieldExpiration{{0,0,false},4321,true,true},
                    "Completed old end slot was advanced before native quantity expiration");
                const auto next=static_cast<std::uint8_t>((slot+1U)%12U);
                check(original::schedule_crop_harvest_mark(season.field,next,0,true,expired.field.work_word)==0,
                    "Following outer slot recreated an expired field's harvest mark");
            } else {
                check(marked==0x1234 && original::expire_crop_completed_slot(season.field,slot,true,input)==
                    original::CropFieldExpiration{input,0,false,false},
                    "A non-end slot changed the field lifecycle");
            }
        }
    }
}

void invalid_inputs() {
    for(const auto season:seasons) {
        for(const auto slot:std::array<std::uint8_t,2>{12,255}) {
            rejected([&]{(void)original::schedule_crop_harvest_mark(season.field,slot,0,false,0);});
            rejected([&]{(void)original::expire_crop_completed_slot(season.field,slot,false,{});});
        }
        for(const auto subslot:std::array<std::uint8_t,2>{16,255})
            rejected([&]{(void)original::schedule_crop_harvest_mark(season.field,0,subslot,false,0);});
        for(const auto quantity:std::array<std::uint16_t,3>{10001,32767,65535})
            rejected([&]{(void)original::expire_crop_completed_slot(season.field,0,false,{quantity,0,false});});
    }
    for(const auto field:{ModelBuilding::Farmhouse,ModelBuilding::HempField,
            ModelBuilding::TeaBush,static_cast<ModelBuilding>(0),static_cast<ModelBuilding>(65535)}) {
        rejected([&]{(void)original::schedule_crop_harvest_mark(field,0,0,false,0);});
        rejected([&]{(void)original::expire_crop_completed_slot(field,0,false,{});});
    }
}

} // namespace

int main() {
    try {
        all_slots_and_subslots();all_work_words_and_completed_slots();
        independent_year_ordering_and_quantity_boundaries();invalid_inputs();
        std::cout<<"Original crop lifecycle PASS: "<<checks<<" independent checks\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
