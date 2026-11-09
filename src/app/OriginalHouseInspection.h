#pragma once

#include "simulation/World.h"
#include "simulation/original/OriginalScheduler.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace openemperor::original_house_ui {

enum class Section : std::uint8_t { State,Needs,Goods,Visits };
enum class Overlay : std::uint8_t { Off,Supply,Desirability,Risk };
enum class Status : std::uint8_t { Unknown,Good,Warning,Bad };
inline constexpr std::size_t section_line_limit=80;

struct CheckSchedule {
    std::optional<std::uint64_t> housing,countdown,water,consumption;
    bool operator==(const CheckSchedule&) const = default;
};
struct OverlayObservation {
    bool applicable=false;
    Status status=Status::Unknown;
    char symbol='?';
    std::string label;
    simulation::Cell origin{};
    simulation::BuildingFootprint footprint{};
    int value=0;
    bool operator==(const OverlayObservation&) const = default;
};
struct GoodsObservation {
    original::Goods good=original::Goods::Hemp;
    int stock=0;
    std::uint64_t incoming_reserved=0,in_transit=0,delivered=0,consumed=0;
    simulation::OriginalGoodsBalance city;
    bool operator==(const GoodsObservation&) const = default;
};
struct FoodObservation {
    simulation::OriginalFoodAmounts stock{};
    simulation::OriginalFoodCounters received{},departed{},returned{},consumed{},incoming{},city_produced{};
    std::uint64_t missed_demands=0;
    std::uint8_t quality=0;
    std::optional<simulation::OriginalFoodQualityWitness> quality_witness;
    bool operator==(const FoodObservation&) const = default;
};
// Bounded, disposable presentation data. It owns no World, route, model,
// saved authority, resource handle or mutable cache.
struct HouseInspection {
    bool applicable=false;
    simulation::BuildingId building_id=static_cast<simulation::BuildingId>(0);
    simulation::Cell origin{};
    simulation::BuildingFootprint footprint{};
    std::uint64_t tick=0;
    original::HouseType type=original::HouseType::Vacant;
    simulation::OriginalHousingDiagnosis housing;
    CheckSchedule checks;
    std::array<GoodsObservation,original::goods_count> goods{};
    FoodObservation food;
    std::array<simulation::OriginalServiceAvailability,9> visits{};
    std::array<std::vector<std::string>,4> sections;
    std::array<OverlayObservation,3> overlays;
};

inline std::string_view section_name(Section section) {
    constexpr std::array<std::string_view,4> names{"State","Needs","Goods","Visits"};
    const auto index=static_cast<std::size_t>(section);
    if (index>=names.size()) throw std::out_of_range("unknown Original House inspector section");
    return names[index];
}
inline std::string_view overlay_name(Overlay overlay) {
    constexpr std::array<std::string_view,4> names{"Off","Supply","DES","Risk"};
    const auto index=static_cast<std::size_t>(overlay);
    if (index>=names.size()) throw std::out_of_range("unknown Original House overlay");
    return names[index];
}
// A display ratio, not an original stored percentage or a needs decision.
inline std::optional<unsigned> relative_need_percentage(std::int32_t actual,std::int32_t required) {
    if (required<=0) return std::nullopt;
    if (actual<=0) return 0;
    if (actual>=required) return 100;
    return static_cast<unsigned>(static_cast<std::int64_t>(actual)*100/required);
}
inline CheckSchedule upcoming_checks(std::uint64_t tick) {
    CheckSchedule result;
    if (tick>=original::scheduler_max_tick) return result;
    // One authored month is the finite maximum search. Inspect only the
    // existing pure dispatch description; never advance a World or House.
    constexpr auto bound=static_cast<std::uint64_t>(original::scheduler_phase_count)*original::scheduler_steps_per_month;
    for (std::uint64_t distance=1;distance<=bound && distance<=original::scheduler_max_tick-tick;++distance) {
        const auto dispatch=original::next_scheduler_dispatch(tick+distance-1);
        if (!dispatch) break;
        const auto next=tick+distance;
        if (!result.housing && dispatch->evolution) result.housing=next;
        if (!result.countdown && dispatch->service_countdown) result.countdown=next;
        if (!result.water && dispatch->position.phase==28) result.water=next;
        if (!result.consumption && dispatch->consumption) result.consumption=next;
        if (result.housing && result.countdown && result.water && result.consumption) break;
    }
    return result;
}

namespace detail {
inline constexpr std::array roles{original::ServiceRole::Herbalist,original::ServiceRole::Acupuncture,
    original::ServiceRole::Ancestor,original::ServiceRole::Confucian,original::ServiceRole::Daoist,
    original::ServiceRole::Buddhist,original::ServiceRole::Music,original::ServiceRole::Acrobat,original::ServiceRole::Drama};
inline constexpr std::array<std::string_view,9> role_names{"Herbalist","Acupuncture","Ancestor","Confucian","Daoist","Buddhist","Music","Acrobat","Drama"};
inline constexpr std::array<std::string_view,6> goods_names{"Hemp","Ceramics","Tea","Silk","Bronzeware","Lacquerware"};
inline std::uint64_t add(std::uint64_t a,std::uint64_t b) {
    if (b>UINT64_MAX-a) throw std::overflow_error("Original House observation sum overflow");
    return a+b;
}
inline void line(std::vector<std::string>& lines,std::string value) {
    if (lines.size()>=section_line_limit) throw std::logic_error("Original House inspector line bound exceeded");
    lines.push_back(std::move(value));
}
inline std::string yes(bool value) {return value ? "YES":"NO";}
inline std::string tick_label(std::optional<std::uint64_t> tick) {return tick ? std::to_string(*tick):"unavailable";}
inline std::string blocker(const simulation::OriginalHousingDiagnosis& d) {
    if (d.materialization_pending && !d.materialization_ready) return "Plot4x4 geometry blocked";
    if (d.awaiting_admission) return "Await actual immigration";
    if (!d.current.needs_met) return "Current requirements missing";
    if (d.current.verdict==original::Verdict::Devolve && d.actual.type!=original::HouseType::EliteAbandoned &&
        d.cached_desirability<=d.current.low) return "DES at/below current low";
    if (d.common_terminal || d.elite_terminal) return "Family terminal; no higher type";
    if (!d.next.needs_met) return "Next requirements missing";
    if (d.current.verdict==original::Verdict::Stay && d.actual.type!=original::HouseType::EliteAbandoned &&
        d.cached_desirability<d.current.high) return "DES below promotion high";
    if (d.current.verdict==original::Verdict::Evolve) return "None; growth eligible if checked now";
    return "Current World verdict Stay";
}
inline const simulation::OriginalHouseState* house(const simulation::World& world,simulation::BuildingId id) {
    if (!world.original_production_enabled() || !world.original_economy_state() || !world.original_production_state()) return nullptr;
    const auto& homes=world.original_economy_state()->houses;
    if (homes.size()>simulation::Rules::city_v10_household_limit) throw std::logic_error("Original House observation bound exceeded");
    const auto found=std::lower_bound(homes.begin(),homes.end(),id,[](const auto& row,auto key){return row.building_id<key;});
    return found!=homes.end() && found->building_id==id ? &*found:nullptr;
}
inline OverlayObservation overlay(const simulation::World& world,simulation::BuildingId id,Overlay mode,
                                  const simulation::OriginalHousingDiagnosis& diagnosis) {
    OverlayObservation result;result.applicable=true;const auto& body=world.building(id);
    result.origin=body.cell;result.footprint=simulation::building_footprint(world.profile(),world.rule_version(),body.kind);
    result.value=diagnosis.cached_desirability;
    if (mode==Overlay::Supply) {
        result.status=diagnosis.awaiting_admission ? Status::Unknown:diagnosis.current.needs_met ? Status::Good:Status::Bad;
        result.label=diagnosis.awaiting_admission ? "Empty; await actual immigration":
            diagnosis.current.needs_met ? "Current needs met":"Current needs missing";
    } else if (mode==Overlay::Desirability) {
        result.status=result.value<=diagnosis.current.low ? Status::Bad:
            result.value>=diagnosis.current.high ? Status::Good:Status::Warning;
        result.label="Current DES "+std::to_string(result.value);
    } else if (mode==Overlay::Risk) {
        if (diagnosis.materialization_pending && !diagnosis.materialization_ready) {
            result.status=Status::Bad;result.label="Plot4x4 geometry blocked";
        } else if (diagnosis.awaiting_admission) {
            result.status=Status::Unknown;result.label="Empty; await actual immigration";
        } else if (static_cast<unsigned>(diagnosis.pure_result_type)<static_cast<unsigned>(diagnosis.actual.type)) {
            result.status=Status::Bad;result.label="Lower type if checked now";
        } else if (diagnosis.current.verdict==original::Verdict::Devolve) {
            result.status=Status::Warning;result.label="Current devolution pressure";
        } else {
            result.status=Status::Good;result.label="No current devolution pressure";
        }
    } else {result.applicable=false;return result;}
    result.symbol=result.status==Status::Good ? 'G':result.status==Status::Warning ? '!':result.status==Status::Bad ? 'X':'?';
    return result;
}
inline void needs_lines(std::vector<std::string>& lines,std::string_view title,const simulation::OriginalHousingRowDiagnosis& row) {
    line(lines,std::string(title)+" row "+std::to_string(static_cast<unsigned>(row.row))+(row.needs_met ? ": met":": missing"));
    for (const auto& need:row.requirements) {
        const auto name=std::string(simulation::original_housing_requirement_name(need.field));
        const bool either=need.field==original::HouseField::DaoistOrBuddhist || need.field==original::HouseField::BronzewareOrLacquerware;
        const auto raw=std::to_string(need.actual)+(either ? "|"+std::to_string(need.alternative):"");
        if (need.presence) {
            line(lines,name+" actual "+yes(std::max(need.actual,need.alternative)!=0)+" ("+raw+")");
            line(lines,"  model requires "+yes(need.required!=0)+(need.met ? " | met":" | MISSING"));
        } else {
            line(lines,name+" actual "+raw+" / need "+std::to_string(need.required));
            const auto percent=relative_need_percentage(std::max(need.actual,need.alternative),need.required);
            line(lines,percent ? "  relative need "+std::to_string(*percent)+"%"+(need.met ? " | met":" | MISSING"):
                "  numeric need not positive | met");
        }
    }
}
}

// Lightweight overlay path: one existing World diagnosis, no goods ledger,
// provider loop, schedule search or four-section string preparation.
inline OverlayObservation overlay_observation(const simulation::World& world,simulation::BuildingId id,Overlay mode) {
    if (mode==Overlay::Off || !detail::house(world,id)) return {};
    if (static_cast<std::size_t>(mode)>static_cast<std::size_t>(Overlay::Risk)) throw std::out_of_range("unknown Original House overlay");
    return detail::overlay(world,id,mode,world.original_housing_diagnosis(id));
}

inline HouseInspection inspect_house(const simulation::World& world,simulation::BuildingId id,
                                    std::optional<Section> requested=std::nullopt) {
    HouseInspection result;const auto* home=detail::house(world,id);if (!home) return result;
    if (requested) (void)section_name(*requested);
    const auto wants=[&](Section section){return !requested || *requested==section;};
    result.applicable=true;result.building_id=id;result.tick=world.ticks();result.type=home->house.type;
    const auto& body=world.building(id);result.origin=body.cell;
    result.footprint=simulation::building_footprint(world.profile(),world.rule_version(),body.kind);
    result.housing=world.original_housing_diagnosis(id);
    if (wants(Section::State)) result.checks=upcoming_checks(result.tick);
    const auto& d=result.housing;const auto& production=*world.original_production_state();
    const auto& ledger=*world.original_goods_state();
    const auto consumption=std::lower_bound(production.house_consumption.begin(),production.house_consumption.end(),id,
        [](const auto& row,auto key){return row.building_id<key;});
    const auto food=std::lower_bound(production.food.stocks.begin(),production.food.stocks.end(),id,
        [](const auto& row,auto key){return row.building_id<key;});
    const auto water=std::lower_bound(production.water.houses.begin(),production.water.houses.end(),id,
        [](const auto& row,auto key){return row.building_id<key;});
    if (consumption==production.house_consumption.end() || consumption->building_id!=id ||
        food==production.food.stocks.end() || food->building_id!=id || water==production.water.houses.end() || water->building_id!=id)
        throw std::logic_error("Original House inspection lost its actual accounts");
    if (wants(Section::Goods)) {
    const auto balances=world.original_goods_balances();
    for (std::size_t g=0;g<result.goods.size();++g) {
        auto& value=result.goods[g];value.good=static_cast<original::Goods>(g);value.city=balances[g];
        value.stock=original::goods_stock(home->house,value.good);value.consumed=consumption->consumed[g];
        if (value.stock<0) throw std::logic_error("negative Original House inspection stock");
        value.delivered=detail::add(static_cast<std::uint64_t>(value.stock),value.consumed);
        for (const auto& trip:ledger.reservations) if (trip.target==id && trip.good==value.good) {
            if (trip.phase==simulation::OriginalGoodsTransferPhase::Reserved)
                value.incoming_reserved=detail::add(value.incoming_reserved,static_cast<std::uint64_t>(trip.reserved));
            else if (trip.phase==simulation::OriginalGoodsTransferPhase::Loaded) {
                const auto remaining=std::int64_t(trip.loaded)-trip.delivered-trip.returned;
                if (remaining<0) throw std::logic_error("negative Original House inbound commitment");
                value.incoming_reserved=detail::add(value.incoming_reserved,static_cast<std::uint64_t>(remaining));
                const auto cargo=std::find_if(ledger.cargo.begin(),ledger.cargo.end(),[&](const auto& row){return row.transfer==trip.transfer;});
                if (cargo!=ledger.cargo.end()) value.in_transit=detail::add(value.in_transit,static_cast<std::uint64_t>(cargo->amount));
            }
        }
    }
    auto& observed_food=result.food;
    observed_food.stock=food->amounts;observed_food.received=food->received;observed_food.departed=food->departed;
    observed_food.returned=food->returned;observed_food.consumed=food->consumed;
    observed_food.missed_demands=food->missed_demands;observed_food.quality=home->house.food_quality;observed_food.quality_witness=food->quality_witness;
    for (const auto& farm:production.food.farms) {
        const auto crop=static_cast<std::size_t>(farm.crop);
        if (crop>=observed_food.city_produced.size()) throw std::logic_error("unknown observed Food crop");
        observed_food.city_produced[crop]=detail::add(observed_food.city_produced[crop],farm.harvested);
    }
    for (const auto& trip:production.food.trips) if (trip.target==id && trip.phase==simulation::OriginalGoodsTransferPhase::Loaded)
        for (std::size_t crop=0;crop<observed_food.incoming.size();++crop) {
            const auto remaining=std::int64_t(trip.loaded[crop])-trip.delivered[crop]-trip.returned[crop];
            if (remaining<0) throw std::logic_error("negative observed Food cargo");
            observed_food.incoming[crop]=detail::add(observed_food.incoming[crop],static_cast<std::uint64_t>(remaining));
        }
    }
    if (wants(Section::Visits)) for (std::size_t r=0;r<result.visits.size();++r)
        result.visits[r]=world.original_service_availability(id,detail::roles[r]);
    for (std::size_t i=0;i<result.overlays.size();++i) result.overlays[i]=detail::overlay(world,id,static_cast<Overlay>(i+1),d);

    if (wants(Section::State)) {
    auto& state=result.sections[static_cast<std::size_t>(Section::State)];
    detail::line(state,std::string(simulation::original_housing_type_name(result.type))+" | type "+std::to_string(static_cast<unsigned>(result.type)));
    detail::line(state,std::string(d.elite_family ? "Elite":"Common")+" actual "+std::to_string(result.footprint.width)+"x"+std::to_string(result.footprint.height));
    detail::line(state,"Blocker: "+detail::blocker(d));
    detail::line(state,"Residents "+std::to_string(d.actual.occupants)+" | admit cap "+std::to_string(d.admission_capacity));
    detail::line(state,"Census capacity "+std::to_string(d.census_capacity));
    detail::line(state,"Current cached DES "+std::to_string(d.cached_desirability)+" | last checked "+std::to_string(d.actual.desirability));
    if (world.original_cardinal_water_enabled())
        detail::line(state,"Cell DES "+std::to_string(d.raw_desirability)+" + water "+std::to_string(d.cardinal_water_bonus)+" = "+std::to_string(d.cached_desirability));
    detail::line(state,"DES low "+std::to_string(d.current.low)+" | high "+std::to_string(d.current.high));
    detail::line(state,"Devolve counter "+std::to_string(d.actual.devolve_counter)+" / "+std::to_string(d.devolve_limit));
    if (d.awaiting_admission) detail::line(state,"Await actual immigration; empty checks skip");
    else {
        detail::line(state,"World verdict: "+std::string(d.current.verdict==original::Verdict::Devolve ? "Devolve":d.current.verdict==original::Verdict::Evolve ? "Evolve":"Stay"));
        detail::line(state,"If checked now: type "+std::to_string(static_cast<unsigned>(d.pure_result_type))+" counter "+std::to_string(d.pure_result_counter));
    }
    if (d.materialization_pending) {
        detail::line(state,d.materialization_ready ? "Plot4x4 ready; claims await actual immigration":
            "Plot4x4 blocked: "+std::string(simulation::build_blocker_reason(d.materialization_diagnostic.blocker)));
        if (d.materialization_diagnostic.cell) detail::line(state,"Growth cell "+std::to_string(d.materialization_diagnostic.cell->x)+","+std::to_string(d.materialization_diagnostic.cell->y));
    }
    if (world.building_on_fire(id)) detail::line(state,"FIRE: arrivals/immigration interrupted");
    if (world.original_fire_protection_enabled()) {
        detail::line(state,"Fire risk "+std::to_string(world.building(id).fire_risk)+"/"+
            std::to_string(simulation::Rules::fire_risk_threshold));
        detail::line(state,world.building_on_fire(id) ?
            "ON FIRE: "+std::to_string(world.fire_remaining(id))+" ticks remaining":
            world.building_fire_protected(id) ?
            "Fire protection: "+std::to_string(world.fire_protection_remaining(id))+" ticks remaining":"Fire protection: none");
    }
    detail::line(state,"Next housing slot "+detail::tick_label(result.checks.housing));
    detail::line(state,"Next water check "+detail::tick_label(result.checks.water));
    detail::line(state,"Next countdown "+detail::tick_label(result.checks.countdown));
    detail::line(state,"Next consumption slot "+detail::tick_label(result.checks.consumption));
    for (const auto& history:production.housing.houses) if (history.building_id==id)
        detail::line(state,"Last actual housing check "+(history.last_evaluation ? std::to_string(history.last_evaluation->tick):"never"));
    detail::line(state,"Slots are scheduled; arrivals and growth unknown");
    const auto tax=world.original_house_tax_status(id);
    detail::line(state,"Tax model rate "+std::to_string(tax.rate)+" x residents "+std::to_string(tax.occupants));
    detail::line(state,"Tax if assessed now "+std::to_string(tax.amount_if_assessed_now)+" | eligible "+detail::yes(tax.eligible));
    detail::line(state,"House tax paid total "+std::to_string(tax.paid_total));
    detail::line(state,"Next tax slot "+detail::tick_label(tax.next_assessment_tick));
    detail::line(state,"Last tax tick "+(tax.last_assessment ? std::to_string(tax.last_assessment->tick):"never"));
    if (tax.last_assessment) detail::line(state,"Last tax type "+std::to_string(static_cast<unsigned>(tax.last_assessment->type))+
        " | residents "+std::to_string(tax.last_assessment->occupants));
    }

    if (wants(Section::Needs)) {
    auto& needs=result.sections[static_cast<std::size_t>(Section::Needs)];
    detail::line(needs,"Numeric % = relative need, capped at100");
    detail::line(needs,"Presence uses actual YES/NO, not a percentage");
    detail::needs_lines(needs,"Current",d.current);detail::needs_lines(needs,"Next",d.next);
    }

    if (wants(Section::Goods)) {
    const auto& observed_food=result.food;
    auto& goods=result.sections[static_cast<std::size_t>(Section::Goods)];
    detail::line(goods,"House delivered = stock + House consumed");
    detail::line(goods,"Incoming commitment includes inbound cargo");
    for (std::size_t g=0;g<result.goods.size();++g) {
        const auto& value=result.goods[g];detail::line(goods,std::string(detail::goods_names[g])+" House stock "+std::to_string(value.stock));
        detail::line(goods,"  House delivered "+std::to_string(value.delivered));
        detail::line(goods,"  House consumed "+std::to_string(value.consumed));
        detail::line(goods,"  House reserved "+std::to_string(value.incoming_reserved)+" | in transit "+std::to_string(value.in_transit));
        detail::line(goods,"  City produced "+std::to_string(value.city.totals.produced));
        detail::line(goods,"  City source reserved "+std::to_string(value.city.source_reserved)+" | cargo "+std::to_string(value.city.cargo));
    }
    detail::line(goods,"Food quality "+std::to_string(observed_food.quality)+" (actual score)");
    for (std::size_t crop=0;crop<observed_food.stock.size();++crop) {
        const auto name=crop==0 ? "Wheat":"Millet";
        detail::line(goods,std::string(name)+" House stock "+std::to_string(observed_food.stock[crop])+" | inbound "+std::to_string(observed_food.incoming[crop]));
        detail::line(goods,"  House received "+std::to_string(observed_food.received[crop]));
        detail::line(goods,"  House departed "+std::to_string(observed_food.departed[crop]));
        detail::line(goods,"  House consumed "+std::to_string(observed_food.consumed[crop]));
        detail::line(goods,"  House returned "+std::to_string(observed_food.returned[crop]));
        detail::line(goods,"  City harvested "+std::to_string(observed_food.city_produced[crop]));
    }
    detail::line(goods,"Food missed demands "+std::to_string(observed_food.missed_demands));
    if (observed_food.quality_witness) {
        const auto& witness=*observed_food.quality_witness;
        detail::line(goods,"Quality arrival witness tick "+std::to_string(witness.tick));
        detail::line(goods,"Accepted Wheat "+std::to_string(witness.accepted[0])+" | Millet "+std::to_string(witness.accepted[1]));
        detail::line(goods,"Previous quality "+std::to_string(witness.previous_quality));
    } else detail::line(goods,"Quality arrival witness: none");
    }

    if (wants(Section::Visits)) {
    auto& visits=result.sections[static_cast<std::size_t>(Section::Visits)];
    detail::line(visits,"Actual timers are raw countdowns");
    detail::line(visits,"Source existence never grants a visit");
    detail::line(visits,"Water evaluated YES/NO: "+detail::yes(home->house.water)+" | raw "+std::to_string(water->normal_timer));
    detail::line(visits,"Water actual arrival "+(water->current_witness ? std::to_string(water->current_witness->arrival_tick):"never"));
    detail::line(visits,"Water last check witness "+(water->evaluation_witness ? std::to_string(water->evaluation_witness->arrival_tick):"none"));
    detail::line(visits,"Alternate Water branch: unsupported");
    for (std::size_t r=0;r<result.visits.size();++r) {
        const auto& value=result.visits[r];detail::line(visits,std::string(detail::role_names[r])+" actual timer "+std::to_string(value.delivered_timer));
        detail::line(visits,"  Last actual arrival "+(value.last_arrival_tick ? std::to_string(*value.last_arrival_tick):"never"));
        detail::line(visits,"  Sources "+std::to_string(value.provider_count)+" | available "+std::to_string(value.available_source_count));
        detail::line(visits,"  Cached reach "+std::to_string(value.reachable_source_count)+" | travelling "+std::to_string(value.travelling_to_target_count));
        if (world.original_venue_house_service_enabled() && detail::roles[r]==original::ServiceRole::Music) {
            for (const auto& witness:world.original_production_state()->services.witnesses)
                if (witness.building_id==id && witness.role==original::ServiceRole::Music && witness.venue_walker) {
                    detail::line(visits,"  Theatre #"+std::to_string(static_cast<std::uint32_t>(witness.provider_id))+
                        " | venue walker #"+std::to_string(static_cast<std::uint64_t>(witness.venue_walker->walker_id)));
                    if (witness.venue_walker->arrival_music)
                        detail::line(visits,"  Actual Music input performer #"+
                            std::to_string(static_cast<std::uint64_t>(witness.venue_walker->arrival_music->performer_id)));
                    break;
                }
        }
    }
    detail::line(visits,"Cached reach0 may be stale; next arrival unknown");
    }
    return result;
}

inline std::vector<std::string> section_lines(const simulation::World& world,simulation::BuildingId id,Section section) {
    const auto index=static_cast<std::size_t>(section);(void)section_name(section);
    auto inspection=inspect_house(world,id,section);return std::move(inspection.sections[index]);
}

} // namespace openemperor::original_house_ui
