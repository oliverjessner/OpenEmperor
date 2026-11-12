#include "simulation/FoodBatchArithmetic.h"
#include "simulation/OriginalFoodState.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>

namespace {
namespace sim=openemperor::simulation;
namespace batch=sim::food_batch;
template<std::size_t N> using Amounts=std::array<std::int32_t,N>;

// Generic arithmetic must not widen the historical runtime or saved catalog.
static_assert(static_cast<unsigned>(sim::OriginalFoodCrop::Wheat)==0);
static_assert(static_cast<unsigned>(sim::OriginalFoodCrop::Millet)==1);
static_assert(sim::original_food_crop_count==2);
static_assert(std::is_same_v<sim::OriginalFoodAmounts,Amounts<2>>);
static_assert(std::is_same_v<sim::OriginalFoodCounters,std::array<std::uint64_t,2>>);

void check(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void refuses(F action,const char* message) {
    bool rejected=false;
    try {action();} catch (const std::invalid_argument&) {rejected=true;}
    check(rejected,message);
}
template<std::size_t N> std::int64_t independent_sum(const Amounts<N>& amounts) {
    return std::accumulate(amounts.begin(),amounts.end(),std::int64_t{0});
}
template<std::size_t N> void bounded_selection(const Amounts<N>& source,
        const Amounts<N>& selected,std::uint64_t requested) {
    check(independent_sum(selected)==static_cast<std::int64_t>(requested),
        "Selection did not preserve the requested physical quantity");
    for (std::size_t i=0;i<N;++i)
        check(selected[i]>=0 && selected[i]<=source[i],
            "Selection manufactured a kind or overran its source quantity");
}

void explicit_catalogs_and_orders() {
    check(batch::volume(Amounts<1>{7})==7 &&
          batch::prefix_portions(Amounts<1>{7},4)==Amounts<1>{4} &&
          batch::mixed_portions(Amounts<1>{7},4)==Amounts<1>{4},
        "A one-kind catalog retained a two-kind dependency");
    check(batch::prefix_portions(Amounts<3>{2,0,5},4)==Amounts<3>{2,0,2},
        "Prefix loading did not exhaust earlier physical stock first");
    check(batch::prefix_portions(Amounts<5>{0,3,0,2,4},6)==Amounts<5>{0,3,0,2,1},
        "Five-kind canonical prefix skipped a sparse stock");
    check(batch::mixed_portions(Amounts<3>{4,4,4},2)==Amounts<3>{1,1,0} &&
          batch::mixed_portions(Amounts<3>{4,4,4},3)==Amounts<3>{1,1,1} &&
          batch::mixed_portions(Amounts<3>{4,4,4},4)==Amounts<3>{2,1,1} &&
          batch::mixed_portions(Amounts<3>{4,4,4},10)==Amounts<3>{4,4,2},
        "Three-kind authored diversity or canonical tie order changed");
    check(batch::mixed_portions(Amounts<5>{0,3,0,2,4},2)==Amounts<5>{0,1,0,1,0} &&
          batch::mixed_portions(Amounts<5>{0,3,0,2,4},3)==Amounts<5>{0,1,0,1,1} &&
          batch::mixed_portions(Amounts<5>{0,3,0,2,4},4)==Amounts<5>{0,2,0,1,1} &&
          batch::mixed_portions(Amounts<5>{0,3,0,2,4},7)==Amounts<5>{0,3,0,2,2},
        "Five-kind sparse authored selection lost a kind or order");
    Amounts<16> wide{};wide.front()=2;wide[7]=1;wide.back()=3;
    Amounts<16> expected{};expected.front()=2;expected[7]=1;expected.back()=1;
    check(batch::mixed_portions(wide,4)==expected && batch::volume(wide)==6,
        "A later catalog entry was omitted by a fixed small loop bound");
    check(batch::prefix_portions(wide,6)==wide && batch::mixed_portions(wide,6)==wide,
        "Loading all physical portions lost the catalog tail");
}

void historical_two_kind_examples() {
    struct Load {Amounts<2> source;std::uint64_t count;Amounts<2> expected;};
    for (const auto& row:std::array{
            Load{{4,4},0,{0,0}},Load{{4,4},1,{1,0}},Load{{4,4},2,{1,1}},
            Load{{4,4},3,{2,1}},Load{{4,4},4,{3,1}},Load{{4,4},5,{4,1}},
            Load{{4,4},7,{4,3}},Load{{4,4},8,{4,4}},Load{{0,6},4,{0,4}},
            Load{{1,6},4,{1,3}},Load{{6,1},4,{3,1}}}) {
        check(batch::mixed_portions(row.source,row.count)==row.expected,
            "Frozen Wheat/Millet authored cart choice changed");
    }
    constexpr std::array<std::uint8_t,2> scores{20,80};
    struct Quality {Amounts<2> amounts;std::uint8_t expected;};
    for (const auto& row:std::array{
            Quality{{0,0},0},Quality{{4,0},20},Quality{{0,4},80},
            Quality{{3,1},35},Quality{{1,2},60},Quality{{1,4},68}})
        check(batch::weighted_quality(row.amounts,scores)==row.expected,
            "Frozen authored two-kind batch quality changed");
}

// These checks are mass/order properties, not another selection implementation.
template<std::size_t N> void selection_properties(const Amounts<N>& input) {
    const auto before=input;
    const auto total=independent_sum(input);
    check(batch::volume(input)==total,"Catalog volume disagrees with physical sum");
    const auto available_kinds=static_cast<std::uint64_t>(
        std::count_if(input.begin(),input.end(),[](auto n){return n>0;}));
    for (std::uint64_t count=0;count<=static_cast<std::uint64_t>(total);++count) {
        const auto prefix=batch::prefix_portions(input,count);
        const auto mixed=batch::mixed_portions(input,count);
        bounded_selection(input,prefix,count);bounded_selection(input,mixed,count);
        if (count==1) check(mixed==prefix,"A single mixed portion bypassed the first physical kind");
        for (std::size_t i=0;i<N;++i) {
            if (prefix[i]<input[i])
                check(std::all_of(prefix.begin()+static_cast<std::ptrdiff_t>(i+1),prefix.end(),
                    [](auto n){return n==0;}),"Prefix consumed a later kind before exhausting an earlier one");
            if (mixed[i]>1)
                check(std::equal(input.begin(),input.begin()+static_cast<std::ptrdiff_t>(i),mixed.begin()),
                    "Mixed extra portions bypassed remaining earlier stock");
            if (count>=2 && input[i]>0 && mixed[i]==0)
                check(std::all_of(mixed.begin()+static_cast<std::ptrdiff_t>(i+1),mixed.end(),
                    [](auto n){return n==0;}),"Mixed tie order skipped an earlier available kind");
        }
        if (count>=2) {
            const auto selected_kinds=static_cast<std::uint64_t>(
                std::count_if(mixed.begin(),mixed.end(),[](auto n){return n>0;}));
            check(selected_kinds==std::min(count,available_kinds),
                "Mixed selection lost available diversity within its physical quantity");
        }
        check(input==before,"Successful pure selection mutated its source");
    }
    check(batch::prefix_portions(input,0)==Amounts<N>{} &&
          batch::mixed_portions(input,0)==Amounts<N>{},"Zero request loaded physical portions");
    check(batch::prefix_portions(input,static_cast<std::uint64_t>(total))==input &&
          batch::mixed_portions(input,static_cast<std::uint64_t>(total))==input,
        "Complete loading did not preserve every source kind");
    refuses([&]{(void)batch::prefix_portions(input,static_cast<std::uint64_t>(total)+1);},
        "Prefix manufactured stock for an oversized request");
    refuses([&]{(void)batch::mixed_portions(input,static_cast<std::uint64_t>(total)+1);},
        "Mixed loading manufactured stock for an oversized request");
    check(input==before,"Rejected pure selection mutated its source");
}
template<std::size_t N> void small_catalogs() {
    std::size_t cases=1;for (std::size_t i=0;i<N;++i) cases*=3;
    for (std::size_t encoded=0;encoded<cases;++encoded) {
        auto digits=encoded;Amounts<N> values{};
        for (auto& value:values) {value=static_cast<std::int32_t>(digits%3);digits/=3;}
        selection_properties(values);
    }
}

void caller_supplied_quality() {
    check(batch::weighted_quality(Amounts<3>{2,1,3},std::array<std::uint8_t,3>{0,255,90})==87,
        "Three-kind quality did not use all caller-supplied scores and integer truncation");
    check(batch::weighted_quality(Amounts<5>{1,2,3,4,5},std::array<std::uint8_t,5>{10,20,30,40,50})==36,
        "Five-kind weighted quality lost a catalog entry");
    constexpr auto maximum=std::numeric_limits<std::int32_t>::max();
    check(batch::weighted_quality(Amounts<1>{maximum},std::array<std::uint8_t,1>{255})==255 &&
          batch::weighted_quality(Amounts<2>{maximum-1,1},std::array<std::uint8_t,2>{0,255})==0,
        "Quality overflowed or rounded instead of truncating at the int32 quantity boundary");
    for (unsigned score=0;score<=255;++score) {
        const auto quality=static_cast<std::uint8_t>(score);
        check(batch::weighted_quality(Amounts<1>{9},std::array<std::uint8_t,1>{quality})==quality &&
              batch::weighted_quality(Amounts<1>{0},std::array<std::uint8_t,1>{quality})==0,
            "Single-kind or empty quality depends on an implicit resource mapping");
    }
    Amounts<16> quantities{};quantities.fill(1);
    std::array<std::uint8_t,16> scores{};
    for (std::size_t i=0;i<scores.size();++i) scores[i]=static_cast<std::uint8_t>(i);
    check(batch::weighted_quality(quantities,scores)==7,"Wide catalog quality lost its tail");
    const Amounts<5> original{1,2,3,4,5},doubled{2,4,6,8,10};
    constexpr std::array<std::uint8_t,5> arbitrary_scores{17,61,93,174,239};
    const auto before_scores=arbitrary_scores;
    const auto value=batch::weighted_quality(original,arbitrary_scores);
    check(value>=17 && value<=239 && value==batch::weighted_quality(doubled,arbitrary_scores) &&
          arbitrary_scores==before_scores,"Quality is not scale invariant, bounded or pure");
}

void hostile_domains_and_immutable_inputs() {
    constexpr auto maximum=std::numeric_limits<std::int32_t>::max();
    check(batch::volume(Amounts<1>{maximum})==maximum &&
          batch::volume(Amounts<5>{maximum-3,0,1,0,2})==maximum,
        "Exact int32 volume limit was rejected");
    for (auto invalid:std::array{
            Amounts<3>{-1,0,1},Amounts<3>{0,std::numeric_limits<std::int32_t>::min(),0},
            Amounts<3>{maximum,1,0},Amounts<3>{maximum,maximum,maximum}}) {
        const auto before=invalid;
        std::array<std::uint8_t,3> scores{0,127,255};const auto before_scores=scores;
        refuses([&]{(void)batch::volume(invalid);},"Invalid source volume was accepted");
        refuses([&]{(void)batch::prefix_portions(invalid,0);},"Zero prefix request bypassed invalid source validation");
        refuses([&]{(void)batch::mixed_portions(invalid,0);},"Zero mixed request bypassed invalid source validation");
        refuses([&]{(void)batch::weighted_quality(invalid,scores);},"Invalid quantities granted a quality");
        check(invalid==before && scores==before_scores,"Rejected arithmetic changed caller-owned inputs");
    }
    Amounts<5> source{1,0,2,0,3};const auto before=source;
    constexpr auto counter_max=std::numeric_limits<std::uint64_t>::max();
    refuses([&]{(void)batch::prefix_portions(source,counter_max);},"UINT64_MAX prefix request narrowed into a valid quantity");
    refuses([&]{(void)batch::mixed_portions(source,counter_max);},"UINT64_MAX mixed request narrowed into a valid quantity");
    check(source==before,"Oversized request changed its physical source");
    check(batch::history_total(std::array<std::uint64_t,1>{counter_max})==counter_max &&
          batch::history_total(std::array<std::uint64_t,3>{counter_max-6,1,5})==counter_max &&
          batch::history_total(std::array<std::uint64_t,5>{0,1,2,3,4})==10,
        "History lost a kind or rejected the exact uint64 boundary");
    for (auto counters:std::array{
            std::array<std::uint64_t,3>{counter_max,1,0},
            std::array<std::uint64_t,3>{counter_max-1,1,1}}) {
        const auto prior=counters;
        refuses([&]{(void)batch::history_total(counters);},"History sum wrapped at UINT64_MAX");
        check(counters==prior,"Rejected history sum changed its counters");
    }
}
} // namespace

int main() {
    try {
        explicit_catalogs_and_orders();historical_two_kind_examples();
        small_catalogs<1>();small_catalogs<3>();small_catalogs<5>();
        Amounts<16> wide{};
        for (std::size_t i=0;i<wide.size();++i) wide[i]=static_cast<std::int32_t>(i%3);
        selection_properties(wide);
        caller_supplied_quality();hostile_domains_and_immutable_inputs();
        std::cout<<"Food batch arithmetic: original-free multi-kind mass/order/quality and frozen two-kind guards PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"Food batch arithmetic: "<<error.what()<<'\n';return 1;
    }
}
