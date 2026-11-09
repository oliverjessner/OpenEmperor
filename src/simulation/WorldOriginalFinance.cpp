#include "simulation/World.h"
#include "simulation/original/OriginalRulesModel.h"
#include "simulation/original/OriginalScheduler.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}
std::uint64_t magnitude(std::int64_t negative) noexcept {
    return static_cast<std::uint64_t>(-(negative+1))+1;
}
std::int64_t negative_value(std::uint64_t value) noexcept {
    return value ? -static_cast<std::int64_t>(value-1)-1:0;
}
std::uint64_t add(std::uint64_t a,std::uint64_t b) {
    if (b>UINT64_MAX-a) throw std::overflow_error("original finance counter exhausted");
    return a+b;
}
std::uint64_t multiply(std::uint64_t a,std::uint64_t b) {
    if (b && a>UINT64_MAX/b) throw std::overflow_error("original finance product exhausted");
    return a*b;
}
template<class Rows> const auto& by_id(const Rows& rows,BuildingId id) {
    const auto it=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){
        if constexpr (requires {row.building_id;}) return row.building_id<key;
        else return row.id<key;
    });
    if (it==rows.end()) invalid("original finance has missing paid identity");
    if constexpr (requires {it->building_id;}) {if (it->building_id!=id) invalid("original finance has orphan House");}
    else if (it->id!=id) invalid("original finance has orphan body");
    return *it;
}
bool occupied_type(Object kind,original::HouseType type) {
    return original_elite_house_kind(kind) ? type>=original::HouseType::EliteAbandoned && type<=original::HouseType::HeavenlyCompound:
        type>=original::HouseType::Shelter && type<=original::HouseType::LuxuriousApartment;
}
std::size_t row_index(original::HouseType type) {return original::model_row(original::house_row(type)).value;}
std::uint64_t opportunities(std::uint64_t first,std::uint64_t through) {
    return through<first ? 0:1+(through-first)/40;
}
// The existing admission kernel can only add residents. Earlier unlocked
// capacities bound every later admission, including residents retained after
// devolution. No current-capacity clamp or footprint multiplier is introduced.
std::uint64_t population_bound(const OriginalHousingHouse& history,Object kind,
    const original::HouseEvolutionRules& rules,std::uint64_t through) {
    if (!history.first_admission_tick || through<*history.first_admission_tick) return 0;
    const auto first=*history.first_admission_tick;
    const auto initial=original_elite_house_kind(kind) ? original::HouseType::ModestSiheyuan:original::HouseType::Shelter;
    const auto& admission=history.first_transitions[row_index(initial)];
    if (!admission || admission->admitted<=0) invalid("original tax requires actual first admission");
    std::array<std::pair<original::HouseType,const OriginalHousingTransition*>,original::house_row_count> entries{};
    std::size_t count=0;
    for (std::size_t r=0;r<history.first_transitions.size();++r) if (const auto& entry=history.first_transitions[r])
        entries[count++]={static_cast<original::HouseType>(r+3),&*entry};
    std::sort(entries.begin(),entries.begin()+count,[](const auto& a,const auto& b){return a.second->evaluation.tick<b.second->evaluation.tick;});
    auto population=static_cast<std::uint64_t>(admission->admitted),previous=first+1;
    auto capacity=rules.row_capacity(original::admission_capacity_row(initial));
    const auto accumulate=[&](std::uint64_t until) {
        if (until<previous) return;
        const auto n=opportunities(first,until)-opportunities(first,previous-1);
        if (population<static_cast<std::uint64_t>(capacity))
            population=std::min(static_cast<std::uint64_t>(capacity),population+2*n);
        previous=until+1;
    };
    for (std::size_t i=0;i<count;++i) {
        const auto& [type,entry]=entries[i];
        if (entry->kind==OriginalHousingTransitionKind::Evolution && entry->evaluation.tick<through) {
            accumulate(entry->evaluation.tick);
            capacity=std::max(capacity,rules.row_capacity(original::admission_capacity_row(type)));
        }
    }
    accumulate(through);return population;
}
bool odd_population_possible(const OriginalHousingHouse& history,Object kind,
    const original::HouseEvolutionRules& rules,int population,std::uint64_t through) {
    if (population%2==0) return true;
    const auto initial=original_elite_house_kind(kind) ? original::HouseType::ModestSiheyuan:original::HouseType::Shelter;
    const auto& admission=history.first_transitions[row_index(initial)];
    if (admission && admission->admitted%2 && admission->admitted<=population) return true;
    for (std::size_t r=0;r<history.first_transitions.size();++r) if (const auto& entry=history.first_transitions[r]) {
        const auto capacity=rules.row_capacity(original::admission_capacity_row(static_cast<original::HouseType>(r+3)));
        if (entry->evaluation.tick<through && capacity%2 && capacity>entry->evaluation.input.occupants && capacity<=population) return true;
    }
    return false;
}
// Maximum population sum for a bounded count of the latest possible monthly
// inputs. Split at the at-most15 unlocks; within one interval a population of
// at most100 reaches its bound after at most100 sampled months. Long saves
// therefore never cause a loop proportional to elapsed World time.
std::uint64_t resident_units_bound(std::uint64_t events,std::uint64_t last,
    const OriginalHousingHouse& history,Object kind,const original::HouseEvolutionRules& rules,int current_population) {
    if (!events) return 0;
    const auto last_month=last/original_tax_interval_ticks;
    if (events>last_month) invalid("original tax events exceed monthly clock");
    auto month=last_month-events+1;
    std::uint64_t total=0;
    while (month<=last_month) {
        auto end=last_month;
        for (const auto& entry:history.first_transitions) if (entry) {
            const auto unlock=entry->evaluation.tick/original_tax_interval_ticks+1;
            if (unlock>month && unlock<=end) end=unlock-1;
        }
        const auto bound=[&](std::uint64_t m) {
            return std::min<std::uint64_t>(static_cast<std::uint64_t>(current_population),
                population_bound(history,kind,rules,m*original_tax_interval_ticks-1));
        };
        const auto plateau=bound(end);
        std::size_t sampled=0;
        while (month<=end) {
            const auto population=bound(month);
            if (population==plateau) {total=add(total,multiply(end-month+1,plateau));month=end+1;break;}
            if (++sampled>100) invalid("original tax population bound did not saturate");
            total=add(total,population);++month;
        }
    }
    return total;
}
template<class Homes,class Bodies>
std::uint64_t validate_records(const OriginalFinanceState& finance,const Homes& homes,
    const OriginalHousingState& housing,const Bodies& bodies,std::uint64_t ticks,
    const original::HouseEvolutionRules* rules,bool detailed,bool visited_fire=false) {
    if (finance.houses.size()!=homes.size() || housing.houses.size()!=homes.size() || homes.size()>20)
        invalid("original finance requires an exact bounded House bijection");
    std::uint64_t global=0;
    for (std::size_t i=0;i<homes.size();++i) {
        const auto& home=homes[i];const auto& record=finance.houses[i];const auto& history=housing.houses[i];
        const auto& b=by_id(bodies,home.building_id);
        if (record.building_id!=home.building_id || history.building_id!=home.building_id || !original_house_kind(b.kind))
            invalid("original finance House identity/order differs from paid body");
        if (record.last_assessment) {
            const auto& last=*record.last_assessment;
            if (!last.tick || last.tick%original_tax_interval_ticks || last.tick>ticks ||
                !occupied_type(b.kind,last.type) || last.occupants<=0 || last.occupants>home.house.occupants ||
                !history.first_admission_tick || last.tick<=*history.first_admission_tick)
                invalid("original last tax assessment has impossible time/family/residents");
            // Rule10 has no FireInspector; its retained deadline describes
            // the latest natural incident, including after expiry. Finance
            // samples pre-tick fire, so ignition itself can pay, whereas the
            // following600 ticks through the exact expiry cannot. Rule11
            // substitutes the exact, independently replayed schema27 arrival
            // and pre-tick tax journal validated before model-bound restore.
            if (!visited_fire && last.tick<=b.fire_until_tick &&
                b.fire_until_tick-last.tick<Rules::fire_incident_ticks)
                invalid("original last tax assessment occurred during the latest natural fire");
        }
        std::uint64_t events=0,units=0,paid=0;
        for (std::size_t r=0;r<record.rows.size();++r) {
            const auto& bucket=record.rows[r];
            const auto type=static_cast<original::HouseType>(r+3);
            if ((!bucket.events)!=(!bucket.resident_units) || bucket.resident_units<bucket.events)
                invalid("original tax bucket has inconsistent events/residents");
            if (bucket.events) {
                if (!record.last_assessment || !history.first_admission_tick || !occupied_type(b.kind,type) ||
                    !history.first_transitions[r]) invalid("original tax bucket precedes its actual occupied type");
                const auto& last=*record.last_assessment;
                const auto through=last.type==type ? last.tick:last.tick>=original_tax_interval_ticks ? last.tick-original_tax_interval_ticks:0;
                const auto& transition=*history.first_transitions[r];
                const auto unlocked=transition.evaluation.tick;
                const auto minimum=transition.kind==OriginalHousingTransitionKind::Admission ? transition.admitted:transition.evaluation.input.occupants;
                if (through<=unlocked || bucket.events>through/original_tax_interval_ticks-unlocked/original_tax_interval_ticks ||
                    minimum<=0 || bucket.resident_units<multiply(bucket.events,static_cast<std::uint64_t>(minimum)) ||
                    bucket.resident_units>multiply(bucket.events,static_cast<std::uint64_t>(home.house.occupants)))
                    invalid("original tax history exceeds unlocked months/current actual residents");
                if (rules && detailed) {
                    const auto maximum=last.type==type ?
                        add(resident_units_bound(bucket.events-1,through-original_tax_interval_ticks,history,b.kind,*rules,home.house.occupants),
                            static_cast<std::uint64_t>(last.occupants)):
                        resident_units_bound(bucket.events,through,history,b.kind,*rules,home.house.occupants);
                    if (bucket.resident_units>maximum) invalid("original tax residents precede their admission/capacity opportunities");
                }
            }
            events=add(events,bucket.events);units=add(units,bucket.resident_units);
            if (rules) {
                const auto rate=rules->value(static_cast<original::ModelHouse>(r),original::HouseField::TaxRate);
                if (rate<0) invalid("original finance requires nonnegative prepared TaxRate");
                paid=add(paid,multiply(bucket.resident_units,static_cast<std::uint64_t>(rate)));
            }
        }
        if (record.last_assessment) {
            const auto& last=*record.last_assessment;
            if (events>last.tick/original_tax_interval_ticks-*history.first_admission_tick/original_tax_interval_ticks)
                invalid("original last tax assessment has impossible time/family/residents");
            // Different type buckets cannot both occupy the same monthly
            // slots after their respective first actual unlocks.
            for (std::size_t r=0;r<record.rows.size();++r) if (record.rows[r].events) {
                const auto unlocked=history.first_transitions[r]->evaluation.tick;
                std::uint64_t later=0;
                for (std::size_t q=0;q<record.rows.size();++q) if (record.rows[q].events &&
                    history.first_transitions[q]->evaluation.tick>=unlocked) later=add(later,record.rows[q].events);
                if (later>last.tick/original_tax_interval_ticks-unlocked/original_tax_interval_ticks)
                    invalid("original tax type buckets overlap unavailable monthly slots");
            }
            const auto& bucket=record.rows[row_index(last.type)];
            const auto& proof=history.first_transitions[row_index(last.type)];
            if (!bucket.events || !proof) invalid("original last tax input lacks a matching unlocked bucket");
            const auto& transition=*proof;
            const auto minimum=transition.kind==OriginalHousingTransitionKind::Admission ? transition.admitted:transition.evaluation.input.occupants;
            if (last.occupants<minimum || bucket.resident_units<
                add(multiply(bucket.events-1,static_cast<std::uint64_t>(minimum)),static_cast<std::uint64_t>(last.occupants)))
                invalid("original last tax input is absent from its bucket");
            if (rules && detailed && (static_cast<std::uint64_t>(last.occupants)>population_bound(history,b.kind,*rules,last.tick-1) ||
                !odd_population_possible(history,b.kind,*rules,last.occupants,last.tick-1)))
                invalid("original last tax population was not yet possible before its tick");
        } else if (events || units || b.taxes_paid_total) invalid("original House payments lack tax history");
        // An unbound maximum is only a conservative rejection bound. If its
        // mathematical value exceeds uint64, every representable payment is
        // already below it; only the later actual model product may overflow.
        if ((rules && paid!=b.taxes_paid_total) ||
            (!rules && units<=UINT64_MAX/100 && b.taxes_paid_total>units*100))
            invalid("original House taxes differ from model-bound historical residents");
        global=add(global,b.taxes_paid_total);
    }
    return global;
}
}

OriginalFinanceBooking original_finance_book(std::int64_t funds,std::uint64_t taxes,
    std::uint64_t maintenance,std::uint64_t income,std::uint64_t bill) {
    const auto next_taxes=add(taxes,income),next_maintenance=add(maintenance,bill);
    if (funds<0) {
        const auto debt=magnitude(funds);
        if (income>debt+static_cast<std::uint64_t>(INT64_MAX)) throw std::overflow_error("original tax Funds exhausted");
        funds=income<debt ? negative_value(debt-income):static_cast<std::int64_t>(income-debt);
    } else {
        if (income>static_cast<std::uint64_t>(INT64_MAX-funds)) throw std::overflow_error("original tax Funds exhausted");
        funds+=static_cast<std::int64_t>(income);
    }
    if (funds<0) {
        const auto debt=magnitude(funds),limit=std::uint64_t{1}<<63;
        if (bill>limit-debt) throw std::overflow_error("original upkeep Funds exhausted");
        funds=negative_value(debt+bill);
    } else {
        const auto available=static_cast<std::uint64_t>(funds),limit=std::uint64_t{1}<<63;
        if (bill>available+limit) throw std::overflow_error("original upkeep Funds exhausted");
        funds=bill>available ? negative_value(bill-available):static_cast<std::int64_t>(available-bill);
    }
    return {funds,next_taxes,next_maintenance};
}

bool original_finance_accounting_valid(std::int64_t funds,std::uint64_t taxes,
    std::uint64_t construction,std::uint64_t maintenance) noexcept {
    std::array<std::uint64_t,3> credits{static_cast<std::uint64_t>(original_production_starting_treasury),
        taxes,funds<0 ? magnitude(funds):0};
    std::array<std::uint64_t,3> debits{construction,maintenance,funds>=0 ? static_cast<std::uint64_t>(funds):0};
    for (auto& credit:credits) for (auto& debit:debits) {
        const auto n=std::min(credit,debit);credit-=n;debit-=n;
    }
    return std::ranges::all_of(credits,[](auto n){return !n;}) && std::ranges::all_of(debits,[](auto n){return !n;});
}

void World::validate_original_finance_snapshot(const WorldSnapshot& s) {
    try {
    const auto taxes=validate_records(s.original_production->finance,s.original_economy->houses,
        s.original_production->housing,s.buildings,s.ticks,nullptr,false,simulation::original_fire_protection_enabled(s.profile,s.rule_version));
    if (taxes!=s.taxes_collected_total || !original_finance_accounting_valid(s.treasury,
        s.taxes_collected_total,s.construction_spent_total,s.maintenance_spent_total))
        invalid("original finance global payments or Funds do not reconcile");
    } catch (const std::overflow_error&) {invalid("original finance snapshot arithmetic exceeds supported counters");}
}

std::int64_t World::original_worker_upkeep(BuildingId id) const {
    const auto workers=workforce_required(id);
    if (workers<0) invalid("original upkeep requires nonnegative installed workers");
    return static_cast<std::int64_t>(workers)*original_worker_upkeep_funds;
}

void World::validate_original_finance_model() const {
    try {
    const auto& rules=original_rules_model_->house_rules();
    for (std::size_t r=0;r<original::house_row_count;++r)
        if (rules.value(static_cast<original::ModelHouse>(r),original::HouseField::TaxRate)<0)
            invalid("original finance requires nonnegative prepared TaxRate in every row");
    const auto taxes=validate_records(original_production_->finance,original_economy_->houses,
        original_production_->housing,buildings_,ticks_,&rules,true,original_fire_protection_enabled());
    if (taxes!=taxes_collected_total_ || !original_finance_history_valid())
        invalid("original finance does not match the bound model and actual installation ages");
    if (original_fire_protection_enabled()) {
        const auto& state=*original_production_->fire_protection;
        for (const auto& witness:state.taxes) {
            const auto& a=witness.assessment;
            const auto& history=by_id(original_production_->housing.houses,witness.building_id);
            const auto& body=building(witness.building_id);
            const auto& home=by_id(original_economy_->houses,witness.building_id);
            const auto& unlocked=history.first_transitions[row_index(a.type)];
            const auto minimum=unlocked ? (unlocked->kind==OriginalHousingTransitionKind::Admission ?
                unlocked->admitted:unlocked->evaluation.input.occupants):0;
            if (!unlocked || unlocked->evaluation.tick>=a.tick || a.occupants<minimum || a.occupants>home.house.occupants ||
                static_cast<std::uint64_t>(a.occupants)>population_bound(history,body.kind,rules,a.tick-1) ||
                !odd_population_possible(history,body.kind,rules,a.occupants,a.tick-1))
                invalid("Rule11 individual historical tax input precedes actual type/capacity/admission");
        }
        std::vector<bool> removed_route_cells(static_cast<std::size_t>(width_)*static_cast<std::size_t>(height_));
        std::uint64_t removed_route_count=0;
        const auto validate_trip=[&](const OriginalFireTrip& trip) {
            std::uint64_t possible=0;
            for (const auto& house:original_economy_->houses) {
                const auto& history=by_id(original_production_->housing.houses,house.building_id);
                possible=add(possible,std::min<std::uint64_t>(static_cast<std::uint64_t>(house.house.occupants),
                    population_bound(history,building(house.building_id).kind,rules,trip.dispatched_tick-1)));
            }
            if (static_cast<std::uint64_t>(trip.population)>possible)
                invalid("Rule11 dispatch workforce precedes model-bound actual resident capacity");
            for (std::size_t i=1;i<trip.travelled.size();++i) {
                if (!permission_edge_allowed(trip.travelled[i-1],trip.travelled[i]))
                    invalid("Rule11 historical Inspector crossed an illegal source/height/transport edge");
                if (i+1<trip.travelled.size() && !transport_cell(trip.travelled[i])) {
                    // A removed paid road may legitimately disappear after a
                    // visit. The union of such historical cells must still fit
                    // the real charged placement/removal ledger; grass alone
                    // and a fabricated revision never become transport proof.
                    if (!road_buildable(trip.travelled[i])) invalid("Rule11 historical road used an inadmissible source cell");
                    const auto at=index(trip.travelled[i]);
                    if (!removed_route_cells[at]) {removed_route_cells[at]=true;++removed_route_count;}
                    if (removed_route_count>roads_removed_total_) invalid("Rule11 historical route lacks paid roads or actual removed-road capacity");
                }
            }
        };
        for (const auto& visit:state.arrivals) validate_trip(visit.trip);
        for (const auto& trip:state.trips) validate_trip(trip);
    }
    } catch (const std::overflow_error&) {invalid("original model-bound finance arithmetic exceeds supported counters");}
}

bool World::original_finance_history_valid() const {
    try {
        const auto taxes=validate_records(original_production_->finance,original_economy_->houses,
            original_production_->housing,buildings_,ticks_,&original_rules_model_->house_rules(),false,original_fire_protection_enabled());
        std::uint64_t maintenance=0;
        for (const auto& b:buildings_) if (b.placed) {
            if (b.placed_tick>ticks_) return false;
            maintenance=add(maintenance,multiply((ticks_-b.placed_tick)/original_worker_upkeep_interval(),
                static_cast<std::uint64_t>(original_worker_upkeep(b.id))));
        }
        if (original_safe_demolition_enabled()) {
            if (!original_production_->safe_demolition) return false;
            for (const auto& entry:original_production_->safe_demolition->retired) {
                if (entry.placed_tick>entry.removed_tick || entry.removed_tick>ticks_) return false;
                const auto workers=entry.kind==OriginalRetiredKind::ClaySource ? Rules::clay_source_workers:0;
                maintenance=add(maintenance,multiply((entry.removed_tick-entry.placed_tick)/original_worker_upkeep_interval(),
                    static_cast<std::uint64_t>(workers)*static_cast<std::uint64_t>(original_worker_upkeep_funds)));
            }
        }
        return taxes==taxes_collected_total_ && maintenance==maintenance_spent_total_ &&
            original_finance_accounting_valid(treasury_,taxes_collected_total_,construction_spent_total_,maintenance_spent_total_);
    } catch (const std::invalid_argument&) {return false;}
      catch (const std::overflow_error&) {return false;}
}

void World::book_original_finance_tick(std::uint64_t next) {
    const bool assess=next%original_tax_interval_ticks==0;
    OriginalFinanceState candidate;
    std::vector<OriginalFireTaxWitness> tax_witnesses;
    std::vector<std::pair<BuildingId,std::uint64_t>> payments;
    std::uint64_t income=0,bill=0;
    if (assess) {
        candidate=original_production_->finance;
        payments.reserve(original_economy_->houses.size());
        for (std::size_t i=0;i<original_economy_->houses.size();++i) {
            const auto& home=original_economy_->houses[i];
            if (home.house.occupants<=0 || building_on_fire(home.building_id)) continue;
            const auto rate=original_rules_model_->house_rules().value(original::house_row(home.house.type),original::HouseField::TaxRate);
            if (rate<0) invalid("negative original TaxRate");
            const auto occupants=static_cast<std::uint64_t>(home.house.occupants);
            const auto tax=multiply(occupants,static_cast<std::uint64_t>(rate));
            const auto paid=add(building(home.building_id).taxes_paid_total,tax);
            auto& record=candidate.houses[i];auto& bucket=record.rows[row_index(home.house.type)];
            bucket.events=add(bucket.events,1);bucket.resident_units=add(bucket.resident_units,occupants);
            record.last_assessment=OriginalTaxAssessment{next,home.house.type,home.house.occupants};
            if (original_fire_protection_enabled()) tax_witnesses.push_back({home.building_id,*record.last_assessment});
            income=add(income,tax);payments.emplace_back(home.building_id,paid);
        }
    }
    for (const auto& b:buildings_) if (b.placed && next>b.placed_tick &&
        (next-b.placed_tick)%original_worker_upkeep_interval()==0)
        bill=add(bill,static_cast<std::uint64_t>(original_worker_upkeep(b.id)));
    const auto booked=original_finance_book(treasury_,taxes_collected_total_,maintenance_spent_total_,income,bill);
    // Every allocation, signed/counter product and invoice has passed. Commit
    // before any tick, producer, movement, incident, admission or evolution.
    if (assess && original_fire_protection_enabled()) {
        auto& witnesses=original_production_->fire_protection->taxes;
        witnesses.reserve(witnesses.size()+tax_witnesses.size());
        witnesses.insert(witnesses.end(),tax_witnesses.begin(),tax_witnesses.end());
    }
    if (assess) original_production_->finance=std::move(candidate);
    for (const auto& [id,paid]:payments) mutable_building(id).taxes_paid_total=paid;
    treasury_=booked.funds;taxes_collected_total_=booked.taxes;maintenance_spent_total_=booked.maintenance;
}

OriginalHouseTaxStatus World::original_house_tax_status(BuildingId id) const {
    OriginalHouseTaxStatus result;
    if (!original_production_enabled()) return result;
    const auto found=std::lower_bound(original_economy_->houses.begin(),original_economy_->houses.end(),id,
        [](const auto& row,BuildingId key){return row.building_id<key;});
    if (found==original_economy_->houses.end() || found->building_id!=id) return result;
    result.applicable=true;result.occupants=found->house.occupants;result.inhabited=result.occupants>0;
    result.burning=building_on_fire(id);result.eligible=result.inhabited && !result.burning;
    result.rate=original_rules_model_->house_rules().value(original::house_row(found->house.type),original::HouseField::TaxRate);
    result.amount_if_assessed_now=result.eligible ? multiply(static_cast<std::uint64_t>(result.occupants),static_cast<std::uint64_t>(result.rate)):0;
    result.paid_total=building(id).taxes_paid_total;
    const auto distance=original_tax_interval_ticks-ticks_%original_tax_interval_ticks;
    if (distance<=original::scheduler_max_tick-ticks_) result.next_assessment_tick=ticks_+distance;
    result.last_assessment=by_id(original_production_->finance.houses,id).last_assessment;
    return result;
}

OriginalGoalStatus World::original_goal_status() const {
    OriginalGoalStatus result;
    if (!original_production_enabled()) return result;
    result.applicable=true;result.population=total_population();
    result.positive_tax=taxes_collected_total_>0;result.nonnegative_funds=treasury_>=0;
    for (const auto& home:original_economy_->houses) if (home.house.occupants>0 && !building_on_fire(home.building_id)) {
        ++result.inhabited_houses;
        if (!original_elite_house_kind(building(home.building_id).kind) && home.house.type>=original::HouseType::PlainCottage)
            ++result.common_houses;
    }
    result.reached=result.inhabited_houses>=result.required_inhabited_houses &&
        result.common_houses>=result.required_common_houses && result.population>=result.required_population &&
        result.positive_tax && result.nonnegative_funds;
    return result;
}

namespace {
struct ReplayedFire {int risk=0;std::uint64_t protection=0,incident=0,cursor=0;};
void advance_fire(ReplayedFire& state,std::uint64_t placed,std::uint64_t through) {
    if (through<=state.cursor) return;
    const auto step=static_cast<std::uint64_t>(Rules::fire_risk_step_ticks);
    auto first=placed+((state.cursor-placed)/step+1)*step;
    const auto blocked=std::max(state.protection,state.incident);
    if (first<blocked) first=placed+((blocked-placed+step-1)/step)*step;
    if (first<=through) {
        const auto count=1+(through-first)/step;
        const auto needed=static_cast<std::uint64_t>(Rules::fire_risk_threshold-state.risk);
        if (count<needed) state.risk+=static_cast<int>(count);
        else {
            auto ignition=first+(needed-1)*step;
            const auto cycle=Rules::fire_incident_ticks+(Rules::fire_risk_threshold-1)*step;
            ignition+=(through-ignition)/cycle*cycle;
            state.incident=ignition+Rules::fire_incident_ticks;state.protection=0;state.risk=0;
            if (through>=state.incident) state.risk=static_cast<int>(1+(through-state.incident)/step);
        }
    }
    state.cursor=through;
}
std::size_t fire_path_cells(const OriginalFireProtectionState& state) {
    std::size_t cells=0;
    const auto include=[&](std::size_t n) {
        if (n>original_fire_path_cell_limit-cells) invalid("Rule11 historical travelled-path cell limit exceeded");
        cells+=n;
    };
    for (const auto& trip:state.trips) include(trip.travelled.size());
    for (const auto& event:state.arrivals) include(event.trip.travelled.size());return cells;
}
int footprint_distance(RulesProfile profile,std::uint32_t rule,const BuildingSnapshot& a,const BuildingSnapshot& b) {
    const auto x=building_footprint(profile,rule,a.kind),y=building_footprint(profile,rule,b.kind);
    const int dx=std::max({0,a.cell.x-(b.cell.x+y.width-1),b.cell.x-(a.cell.x+x.width-1)});
    const int dy=std::max({0,a.cell.y-(b.cell.y+y.height-1),b.cell.y-(a.cell.y+x.height-1)});
    return dx+dy;
}
std::uint64_t cell_distance(Cell a,Cell b) {
    const auto dx=static_cast<std::int64_t>(a.x)-b.x,dy=static_cast<std::int64_t>(a.y)-b.y;
    return static_cast<std::uint64_t>(std::abs(dx)+std::abs(dy));
}
}

void World::validate_original_fire_protection_snapshot(const WorldSnapshot& s) {
    if (!simulation::original_fire_protection_enabled(s.profile,s.rule_version)) {
        if (s.original_production && s.original_production->fire_protection)
            invalid("historical original economy cannot own Rule11 fire protection authority");
        return;
    }
    if (!s.original_production || !s.original_production->fire_protection || s.ticks>original::scheduler_max_tick-Rules::fire_protection_ticks)
        invalid("Rule11 requires complete bounded fire and tax provenance and replayable deadlines");
    const auto& state=*s.original_production->fire_protection;
    if (state.trips.size()>2 || state.arrivals.size()>original_fire_arrival_limit || state.taxes.size()>original_fire_tax_limit)
        invalid("Rule11 fire journal collection bound exceeded");
    if (state.travelled_cells!=fire_path_cells(state)) invalid("Rule11 travelled path counter does not reconcile");
    const bool safe=simulation::original_safe_demolition_enabled(s.profile,s.rule_version);
    if (safe) {
        if (!s.original_production->safe_demolition) invalid("Rule19 fire validation requires its retirement journal");
        for (const auto& entry:s.original_production->safe_demolition->retired) {
            if (entry.placed_tick>entry.removed_tick || entry.removed_tick>s.ticks)
                invalid("Rule19 retired fire clock has an impossible lifetime");
            // Retired IDs cannot retain an Inspector arrival or trip: every
            // such endpoint below must still resolve to a live paid body.
            // Their exact removal-time clock is therefore the natural one.
            ReplayedFire closed{0,0,0,entry.placed_tick};
            advance_fire(closed,entry.placed_tick,entry.removed_tick);
            if (closed.incident>entry.removed_tick)
                invalid("Rule19 purchase was retired during its actual natural fire");
        }
    }
    // Prepare each body's visit timeline once. Queries use a binary search
    // and one arithmetic natural-clock advance, never replay elapsed ticks or
    // scan the whole journal for every tax/dispatch witness.
    std::vector<std::vector<ReplayedFire>> timelines(s.buildings.size());
    for (std::size_t i=0;i<s.buildings.size();++i) timelines[i].push_back({0,0,0,s.buildings[i].placed_tick});
    for (std::size_t i=0;i<state.arrivals.size();++i) {
        const auto& event=state.arrivals[i];
        if (!event.tick || event.tick>s.ticks || event.tick>original::scheduler_max_tick-Rules::fire_protection_ticks ||
            (i && (state.arrivals[i-1].tick>event.tick ||
                (state.arrivals[i-1].tick==event.tick && state.arrivals[i-1].trip.courier_id>=event.trip.courier_id))))
            invalid("Rule11 arrivals must follow actual movement order and bounded deadlines");
        const auto& target=by_id(s.buildings,event.trip.target);
        if (event.tick<=target.placed_tick) invalid("Rule11 arrival precedes its paid target");
        auto& timeline=timelines[static_cast<std::size_t>(&target-s.buildings.data())];
        auto before=timeline.back();advance_fire(before,target.placed_tick,event.tick-1);
        if (event.extinguished!=(before.incident>event.tick))
            invalid("Rule11 extinguishing witness contradicts the actual historical fire clock");
        timeline.push_back({0,event.tick+Rules::fire_protection_ticks,event.tick,event.tick});
    }
    const auto replay=[&](const BuildingSnapshot& body,std::uint64_t through) {
        const auto& timeline=timelines[static_cast<std::size_t>(&body-s.buildings.data())];
        const auto after=std::upper_bound(timeline.begin(),timeline.end(),through,
            [](std::uint64_t tick,const ReplayedFire& fire){return tick<fire.cursor;});
        if (after==timeline.begin()) invalid("Rule11 event precedes its paid body's placement");
        auto result=*std::prev(after);advance_fire(result,body.placed_tick,through);return result;
    };
    std::array<const OriginalFireArrival*,Rules::original_production_courier_limit+1> previous{};
    const auto actor_by_id=[&](CourierId id) -> const CourierSnapshot& {
        const auto found=std::lower_bound(s.couriers.begin(),s.couriers.end(),id,
            [](const auto& actor,CourierId key){return actor.id<key;});
        if (found==s.couriers.end() || found->id!=id) invalid("Rule19 fire witness has an absent live Courier ID");
        return *found;
    };
    const auto validate_trip=[&](const OriginalFireTrip& trip,std::uint64_t through,bool complete) {
        const auto actor=static_cast<std::uint32_t>(trip.courier_id);
        if (!actor || (!safe && actor>s.couriers.size()) || actor>=previous.size()) invalid("Rule11 fire trip has unknown actor");
        const auto& courier=safe ? actor_by_id(trip.courier_id):s.couriers[actor-1];
        const auto& owner=by_id(s.buildings,trip.owner);const auto& target=by_id(s.buildings,trip.target);
        if (courier.id!=trip.courier_id || courier.role!=CourierRole::FireInspector || courier.owner!=trip.owner ||
            owner.kind!=Object::FireWatch || trip.owner==trip.target || !fire_eligible(target.kind,s.profile,s.rule_version) ||
            !trip.dispatched_tick || trip.dispatched_tick<=owner.placed_tick || trip.dispatched_tick<=target.placed_tick ||
            trip.dispatched_tick>through || trip.road_revision>s.road_revision || trip.assigned_workers!=Rules::fire_watch_workers ||
            trip.population<static_cast<int>(Rules::fire_watch_workers) || static_cast<std::uint64_t>(trip.population)>s.original_production->residents_admitted || !trip.operating ||
            trip.travelled.empty()) invalid("Rule11 arrival lacks a paid, staffed, operating Inspector dispatch");
        if (replay(owner,trip.dispatched_tick-1).incident>trip.dispatched_tick-1)
            invalid("Rule11 Inspector dispatched from a burning FireWatch");
        std::uint64_t possible_population=0;
        for (const auto& house:s.original_economy->houses) {
            const auto& history=by_id(s.original_production->housing.houses,house.building_id);
            if (!history.first_admission_tick || trip.dispatched_tick<=*history.first_admission_tick) continue;
            const auto admitted=opportunities(*history.first_admission_tick,trip.dispatched_tick-1)*2;
            possible_population=add(possible_population,std::min<std::uint64_t>(admitted,static_cast<std::uint64_t>(house.house.occupants)));
        }
        if (static_cast<std::uint64_t>(trip.population)>possible_population)
            invalid("Rule11 Inspector staffing precedes actual resident admission opportunities");
        if (!building_footprint_contains(s.profile,s.rule_version,owner.kind,owner.cell,trip.travelled.front()) ||
            (complete && !building_footprint_contains(s.profile,s.rule_version,target.kind,target.cell,trip.travelled.back())))
            invalid("Rule11 travelled path lacks its paid owner/target endpoints");
        for (std::size_t i=0;i<trip.travelled.size();++i) {
            const auto cell=trip.travelled[i];
            if (cell.x<0 || cell.y<0 || cell.x>=s.width || cell.y>=s.height ||
                (i && std::abs(cell.x-trip.travelled[i-1].x)+std::abs(cell.y-trip.travelled[i-1].y)!=1))
                invalid("Rule11 travelled path has an impossible grid edge");
        }
        const auto edges=trip.travelled.size()-1;
        if (edges>(through-trip.dispatched_tick+1)/static_cast<std::uint64_t>(profile_rules(s.profile,s.rule_version).courier_edge_ticks) || (complete && edges<2))
            invalid("Rule11 Inspector arrival precedes actual edge travel");
        if (previous[actor]) {
            const auto& old=*previous[actor];
            const auto minimum=static_cast<std::uint64_t>(std::max(2,footprint_distance(s.profile,s.rule_version,
                by_id(s.buildings,old.trip.target),owner)))*static_cast<std::uint64_t>(profile_rules(s.profile,s.rule_version).courier_edge_ticks);
            if (trip.dispatched_tick<=old.tick || trip.dispatched_tick-old.tick<minimum)
                invalid("Rule11 Inspector redispatched before a physical return was possible");
        }
    };
    for (std::size_t i=0;i<state.arrivals.size();++i) {
        const auto& event=state.arrivals[i];
        if (!event.tick || event.tick>s.ticks || event.tick>original::scheduler_max_tick-Rules::fire_protection_ticks ||
            (i && (state.arrivals[i-1].tick>event.tick ||
                (state.arrivals[i-1].tick==event.tick && state.arrivals[i-1].trip.courier_id>=event.trip.courier_id))))
            invalid("Rule11 arrivals must follow actual movement order and bounded deadlines");
        validate_trip(event.trip,event.tick,true);
        previous[static_cast<std::uint32_t>(event.trip.courier_id)]=&event;
    }
    for (const auto& trip:state.trips) {
        validate_trip(trip,s.ticks,false);
        const auto& actor=safe ? actor_by_id(trip.courier_id):s.couriers[static_cast<std::uint32_t>(trip.courier_id)-1];
        if (actor.phase!=CourierPhase::ToWarehouse || actor.target!=trip.target || actor.path.empty() ||
            actor.path_vertex>=actor.path.size() || actor.path[actor.path_vertex]!=trip.travelled.back())
            invalid("Rule11 active Inspector witness differs from authoritative path/progress");
        const auto moved=(trip.travelled.size()-1)*static_cast<std::uint64_t>(profile_rules(s.profile,s.rule_version).courier_edge_ticks);
        if (actor.edge_progress<0 || moved+static_cast<std::uint64_t>(actor.edge_progress)>s.ticks-trip.dispatched_tick+1)
            invalid("Rule11 active Inspector progress precedes physically available movement ticks");
    }
    std::size_t active=0;
    for (const auto& actor:s.couriers) if (actor.role==CourierRole::FireInspector && actor.phase==CourierPhase::ToWarehouse) {
        ++active;if (std::count_if(state.trips.begin(),state.trips.end(),[&](const auto& t){return t.courier_id==actor.id;})!=1)
            invalid("Rule11 outbound Inspector requires exactly one trip witness");
    }
    if (active!=state.trips.size()) invalid("Rule11 orphan Inspector trip witness");
    for (const auto& actor:s.couriers) if (actor.role==CourierRole::FireInspector) {
        const auto id=static_cast<std::uint32_t>(actor.id);
        if (!id || id>=previous.size()) invalid("Rule11 Inspector identity exceeds its bounded owner context");
        if (actor.phase==CourierPhase::ToWarehouse) {
            if (actor.last_dispatched_target!=actor.target)
                invalid("Rule11 active Inspector dispatch target differs from current target");
        } else if (const auto* last=previous[id]) {
            if (actor.target!=last->trip.target || actor.last_dispatched_target!=last->trip.target)
                invalid("Rule11 returning/idle Inspector target differs from its actual latest arrival");
            const auto elapsed=s.ticks-last->tick;
            const auto edge_ticks=static_cast<std::uint64_t>(profile_rules(s.profile,s.rule_version).courier_edge_ticks);
            const auto arrival_cell=last->trip.travelled.back();
            if (actor.phase==CourierPhase::Returning) {
                if (actor.path.empty() || actor.path_vertex>=actor.path.size() || actor.edge_progress<0 ||
                    static_cast<std::uint64_t>(actor.edge_progress)>=edge_ticks)
                    invalid("Rule11 returning Inspector lacks bounded physical progress");
                // A repaired or pending route may begin at a later actual
                // transport cell. Its omitted prefix still needs at least
                // grid-distance travel; the retained prefix and partial edge
                // consume their ordinary movement ticks as well.
                const auto prefix=cell_distance(arrival_cell,actor.path.front());
                const auto available=elapsed/edge_ticks;
                if (prefix>available || actor.path_vertex>available-prefix ||
                    static_cast<std::uint64_t>(actor.edge_progress)>
                        elapsed-(prefix+actor.path_vertex)*edge_ticks)
                    invalid("Rule11 returning Inspector progress precedes its actual latest arrival");
            } else if (actor.phase==CourierPhase::IdleAtWorkshop) {
                const auto minimum=std::max<std::uint64_t>(2,cell_distance(arrival_cell,by_id(s.buildings,actor.owner).cell));
                if (minimum>elapsed/edge_ticks)
                    invalid("Rule11 Inspector became idle before a physical return was possible");
            }
        } else if (actor.phase!=CourierPhase::IdleAtWorkshop || actor.target!=actor.owner || actor.last_dispatched_target) {
            invalid("Rule11 unvisited Inspector must retain its paid initial idle relation");
        }
    }
    if (simulation::original_music_venues_enabled(s.profile,s.rule_version)) {
        if (!s.original_production->culture ||
            s.original_production->culture->schools.size()>original_music_school_limit ||
            s.original_production->culture->venues.size()>original_culture_venue_limit)
            invalid("Rule22 fire binding requires its complete bounded culture authority");
        // Dispatch precedes every movement and this tick's natural fire update.
        // Arrivals see the same pre-update clock plus actual earlier-ID
        // Inspector arrivals. Reuse the already validated fire timeline; only
        // same-tick arrivals can intervene before the selected movement channel.
        const auto burning_before_event=[&](BuildingId id,std::uint64_t tick,
                                            std::optional<CourierId> arriving_channel) {
            const auto& target=by_id(s.buildings,id);
            if (!tick || tick<=target.placed_tick || tick>s.ticks)
                invalid("Rule22 culture fire event precedes its paid body or saved clock");
            const auto fire=replay(target,tick-1);
            if (arriving_channel) {
                auto event=std::lower_bound(state.arrivals.begin(),state.arrivals.end(),tick,
                    [](const OriginalFireArrival& row,std::uint64_t at){return row.tick<at;});
                for (;event!=state.arrivals.end() && event->tick==tick &&
                       event->trip.courier_id<*arriving_channel;++event)
                    if (event->trip.target==id) return false;
            }
            // An incident expiring exactly now is no longer active. Natural
            // ignition at this tick happens later, after every real arrival.
            return fire.incident>tick;
        };
        const auto check_dispatch=[&](BuildingId school,BuildingId venue,std::uint64_t tick) {
            if (burning_before_event(school,tick,std::nullopt) ||
                burning_before_event(venue,tick,std::nullopt))
                invalid("Rule22 performer dispatched from or into an actually burning body");
        };
        const auto check_arrival=[&](BuildingId venue,CourierId channel,std::uint64_t tick) {
            if (burning_before_event(venue,tick,channel))
                invalid("Rule22 accepted performer arrived during actual pre-movement venue fire");
        };
        const auto& culture=*s.original_production->culture;
        for (const auto& school:culture.schools) if (school.latest_trip) {
            const auto& trip=*school.latest_trip;
            check_dispatch(school.building_id,trip.venue_id,trip.dispatched_tick);
            if (trip.accepted) {
                if (!trip.arrival_tick) invalid("Rule22 accepted music trip lacks its actual arrival clock");
                check_arrival(trip.venue_id,school.courier_id,*trip.arrival_tick);
            }
        }
        for (const auto& venue:culture.venues) if (venue.last_arrival) {
            const auto& arrival=*venue.last_arrival;
            check_dispatch(arrival.school_id,venue.building_id,arrival.dispatched_tick);
            check_arrival(venue.building_id,arrival.courier_id,arrival.arrival_tick);
        }
        if (simulation::original_venue_house_service_enabled(s.profile,s.rule_version)) {
            if (!culture.venue_house_service)
                invalid("Rule23 fire binding requires its complete venue House authority");
            // Retained inputs can outlive both latest School trips and the
            // Venue's current arrival. They remain actual historical events,
            // checked against the same prepared fire timeline as Rule22.
            const auto input=[&](const OriginalMusicArrival& m,BuildingId venue) {
                check_dispatch(m.school_id,venue,m.dispatched_tick);
                check_arrival(venue,m.courier_id,m.arrival_tick);
            };
            for (const auto& source:culture.venue_house_service->sources)
                if (source.first_music) input(*source.first_music,source.building_id);
            const auto walker=[&](const std::optional<OriginalVenueWalkerEvidence>& evidence,
                                  BuildingId venue,BuildingId house,CourierId channel,
                                  std::optional<std::uint64_t> arrival,bool applied) {
                if (!evidence) return;
                input(evidence->dispatch_music,venue);
                if (evidence->arrival_music) input(*evidence->arrival_music,venue);
                if (burning_before_event(venue,evidence->dispatched_tick,std::nullopt) ||
                    burning_before_event(house,evidence->dispatched_tick,std::nullopt))
                    invalid("Rule23 venue walker dispatched from or toward an actually burning body");
                if (applied && (!arrival || burning_before_event(venue,*arrival,channel) ||
                    burning_before_event(house,*arrival,channel)))
                    invalid("Rule23 House Music was applied during actual pre-movement fire");
            };
            for (const auto& trip:s.original_production->services.trips) if (trip.venue_walker)
                walker(trip.venue_walker,actor_by_id(trip.transfer.courier_id).owner,trip.target,
                    trip.transfer.courier_id,trip.arrival_tick,trip.visit_applied);
            const auto witness=[&](const OriginalServiceWitness& w) {
                walker(w.venue_walker,w.provider_id,w.building_id,w.transfer.courier_id,w.arrival_tick,true);
            };
            for (const auto& w:s.original_production->services.witnesses) witness(w);
            for (const auto& h:s.original_production->housing.houses) {
                const auto evaluation=[&](const OriginalHousingEvaluation& e) {
                    for (const auto& w:e.service_witnesses) if (w) witness(*w);
                };
                if (h.last_evaluation) evaluation(*h.last_evaluation);
                for (const auto& t:h.first_transitions) if (t) evaluation(t->evaluation);
            }
        }
    }
    for (const auto& body:s.buildings) if (fire_eligible(body.kind,s.profile,s.rule_version)) {
        const auto fire=replay(body,s.ticks);
        if (body.fire_risk!=fire.risk || body.fire_until_tick!=fire.incident || body.fire_protection_until_tick!=fire.protection)
            invalid("Rule11 fire state differs from paid-body natural clocks and actual arrivals");
    }
    std::vector<OriginalFinanceHouse> accumulated;
    accumulated.reserve(s.original_production->finance.houses.size());
    for (const auto& house:s.original_production->finance.houses) accumulated.push_back(OriginalFinanceHouse{house.building_id,{},std::nullopt});
    for (std::size_t i=0;i<state.taxes.size();++i) {
        const auto& event=state.taxes[i];const auto& a=event.assessment;
        if (!a.tick || a.tick>s.ticks || a.tick%original_tax_interval_ticks || a.occupants<=0 ||
            !occupied_type(by_id(s.buildings,event.building_id).kind,a.type) ||
            (i && (state.taxes[i-1].assessment.tick>a.tick ||
                (state.taxes[i-1].assessment.tick==a.tick && state.taxes[i-1].building_id>=event.building_id))))
            invalid("Rule11 exact tax witnesses have impossible order, identity or input");
        const auto before=replay(by_id(s.buildings,event.building_id),a.tick-1);
        if (before.incident>a.tick-1) invalid("Rule11 tax assessment occurred during actual pre-tick fire");
        auto found=std::lower_bound(accumulated.begin(),accumulated.end(),event.building_id,
            [](const auto& x,BuildingId id){return x.building_id<id;});
        if (found==accumulated.end() || found->building_id!=event.building_id) invalid("Rule11 orphan tax input");
        if (found->last_assessment && found->last_assessment->occupants>a.occupants)
            invalid("Rule11 historical tax residents contradict monotone actual immigration");
        auto& bucket=found->rows[row_index(a.type)];++bucket.events;bucket.resident_units=add(bucket.resident_units,static_cast<std::uint64_t>(a.occupants));
        found->last_assessment=a;
    }
    if (accumulated!=s.original_production->finance.houses)
        invalid("Rule11 exact pre-tick tax witnesses differ from household finance buckets");
}

void World::preflight_original_fire_tick() const {
    const auto& state=*original_production_->fire_protection;
    if (ticks_>=original::scheduler_max_tick-Rules::fire_protection_ticks ||
        state.arrivals.size()>original_fire_arrival_limit-2 ||
        state.travelled_cells>original_fire_path_cell_limit-4 ||
        ((ticks_+1)%original_tax_interval_ticks==0 && state.taxes.size()>original_fire_tax_limit-original_economy_->houses.size()))
        throw std::overflow_error("Rule11 bounded fire/tax provenance exhausted before tick");
}
void World::record_original_fire_dispatch(const CourierState& actor) {
    auto& state=*original_production_->fire_protection;
    if (state.trips.size()>=2 || actor.path.empty() || !building(actor.owner).operating_enabled)
        throw std::logic_error("invalid Rule11 Inspector dispatch provenance");
    OriginalFireTrip trip;trip.courier_id=actor.id;trip.owner=actor.owner;trip.target=actor.target;
    trip.dispatched_tick=ticks_;trip.road_revision=road_revision_;
    trip.assigned_workers=building_staffed_for_tick(actor.owner) ? workforce_required(actor.owner):0;trip.population=total_population();
    trip.operating=building(actor.owner).operating_enabled;trip.travelled.push_back(actor.path.front());
    state.trips.push_back(std::move(trip));++state.travelled_cells;
}
void World::record_original_fire_step(const CourierState& actor) {
    auto& trips=original_production_->fire_protection->trips;
    auto found=std::find_if(trips.begin(),trips.end(),[&](const auto& t){return t.courier_id==actor.id;});
    if (found==trips.end()) throw std::logic_error("Rule11 physical Inspector edge lacks dispatch proof");
    found->travelled.push_back(actor.path.at(actor.path_vertex));
    ++original_production_->fire_protection->travelled_cells;
}
void World::record_original_fire_arrival(const CourierState& actor) {
    auto& state=*original_production_->fire_protection;
    const auto found=std::find_if(state.trips.begin(),state.trips.end(),[&](const auto& t){return t.courier_id==actor.id;});
    if (found==state.trips.end() || found->target!=actor.target) throw std::logic_error("Rule11 arrival lacks an actual matching dispatch");
    state.arrivals.push_back(OriginalFireArrival{std::move(*found),ticks_,building_on_fire(actor.target)});
    state.trips.erase(found);
}

} // namespace openemperor::simulation
