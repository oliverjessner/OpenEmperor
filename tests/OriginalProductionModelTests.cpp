#include "OriginalProductionFixture.h"
#include "simulation/original/OriginalRulesModel.h"
#include "simulation/original/OriginalModelMapping.h"

#include <array>
#include <iostream>
#include <limits>
#include <tuple>

namespace {
namespace f=oe_influence_fixture;
namespace orig=openemperor::original;
using f::check;
constexpr std::array legacy{orig::ModelBuilding::ClayPit,orig::ModelBuilding::Kiln,
    orig::ModelBuilding::Warehouse,orig::ModelBuilding::CommonMarketSquare,
    orig::ModelBuilding::Well,orig::ModelBuilding::Farmhouse,orig::ModelBuilding::HerbalistsStall};
constexpr std::array extra{orig::ModelBuilding::HempFarm,orig::ModelBuilding::HempField,
    orig::ModelBuilding::WheatField,orig::ModelBuilding::MilletField,orig::ModelBuilding::AcupuncturistsClinic,
    orig::ModelBuilding::AncestralShrine,orig::ModelBuilding::DaoistShrine,
    orig::ModelBuilding::BuddhistShrine,orig::ModelBuilding::ConfucianAcademy,
    orig::ModelBuilding::MusicSchool,orig::ModelBuilding::AcrobatSchool,orig::ModelBuilding::DramaSchool,
    orig::ModelBuilding::LoggingShed,
    orig::ModelBuilding::BronzeSmelter,
    orig::ModelBuilding::BronzewareMaker,
    orig::ModelBuilding::LacquerwareMaker,
    orig::ModelBuilding::PaperMaker,
    orig::ModelBuilding::Weaver,
    orig::ModelBuilding::TeaCuringShed,
    orig::ModelBuilding::LacquerRefinery,
    orig::ModelBuilding::SilkwormShed,
    orig::ModelBuilding::TeaBush,
    orig::ModelBuilding::LacquerTree,
    orig::ModelBuilding::MulberryTree};
template<class F> void refuses(F action) {
    bool denied=false;try {action();} catch (const std::exception&) {denied=true;}
    check(denied,"Active model admitted an absent/invalid explicit production capability");
}
std::shared_ptr<const orig::OriginalRulesModel> prepared(const std::filesystem::path& root,
        bool active,orig::Difficulty difficulty=orig::Difficulty::Normal) {
    auto input=orig::load_model_data(root);check(bool(input),"Model source did not parse");
    return active ? orig::OriginalRulesModel::prepare_production(std::move(*input.model),difficulty):
        orig::OriginalRulesModel::prepare(std::move(*input.model),difficulty);
}
void equivalent_legacy(const orig::OriginalRulesModel& old,const orig::OriginalRulesModel& active) {
    check(old.difficulty()==active.difficulty(),"Active preparation changed difficulty identity");
    for (const auto building:legacy) for (std::size_t field=0;field<orig::building_field_count;++field) {
        const auto column=static_cast<orig::BuildingField>(field);
        check(old.supports_building(building) && active.supports_building(building) &&
            old.building_value(building,column)==active.building_value(building,column),
            "Historical seven prepared building rows changed");
    }
    for (std::size_t row=0;row<orig::house_row_count;++row)
        for (std::size_t field=0;field<orig::house_field_count;++field)
            check(old.house_rules().value(static_cast<orig::ModelHouse>(row),static_cast<orig::HouseField>(field))==
                active.house_rules().value(static_cast<orig::ModelHouse>(row),static_cast<orig::HouseField>(field)),
                "Active production preparation changed House rules");
}
void synthetic() {
    for(const auto [object,model]:std::array{
            std::pair{f::sim::Object::OriginalHempFarm,orig::ModelBuilding::HempFarm},
            std::pair{f::sim::Object::OriginalHempField,orig::ModelBuilding::HempField},
            std::pair{f::sim::Object::OriginalFoodFarm,orig::ModelBuilding::Farmhouse},
            std::pair{f::sim::Object::OriginalWheatField,orig::ModelBuilding::WheatField},
            std::pair{f::sim::Object::OriginalMilletField,orig::ModelBuilding::MilletField},
            std::pair{f::sim::Object::OriginalAncestorShrine214,orig::ModelBuilding::AncestralShrine},
            std::pair{f::sim::Object::OriginalDaoistShrine215,orig::ModelBuilding::DaoistShrine},
            std::pair{f::sim::Object::OriginalBuddhistShrine217,orig::ModelBuilding::BuddhistShrine},
            std::pair{f::sim::Object::OriginalConfucianAcademy219,orig::ModelBuilding::ConfucianAcademy},
            std::pair{f::sim::Object::OriginalMusicSchool211,orig::ModelBuilding::MusicSchool},
            std::pair{f::sim::Object::OriginalAcrobatSchool212,orig::ModelBuilding::AcrobatSchool},
            std::pair{f::sim::Object::OriginalDramaSchool213,orig::ModelBuilding::DramaSchool},
            std::pair{f::sim::Object::OriginalLoggingShed38,orig::ModelBuilding::LoggingShed},
            std::pair{f::sim::Object::OriginalBronzeSmelter39,orig::ModelBuilding::BronzeSmelter},
            std::pair{f::sim::Object::OriginalBronzewareMaker42,orig::ModelBuilding::BronzewareMaker},
            std::pair{f::sim::Object::OriginalLacquerwareMaker44,orig::ModelBuilding::LacquerwareMaker},
            std::pair{f::sim::Object::OriginalPaperMaker45,orig::ModelBuilding::PaperMaker},
            std::pair{f::sim::Object::OriginalWeaver47,orig::ModelBuilding::Weaver},
            std::pair{f::sim::Object::OriginalTeaCuringShed237,orig::ModelBuilding::TeaCuringShed},
            std::pair{f::sim::Object::OriginalLacquerRefinery238,orig::ModelBuilding::LacquerRefinery},
            std::pair{f::sim::Object::OriginalSilkwormShed239,orig::ModelBuilding::SilkwormShed},
            std::pair{f::sim::Object::OriginalTeaBush26,orig::ModelBuilding::TeaBush},
            std::pair{f::sim::Object::OriginalLacquerTree27,orig::ModelBuilding::LacquerTree},
            std::pair{f::sim::Object::OriginalMulberryTree28,orig::ModelBuilding::MulberryTree},
            std::pair{f::sim::Object::OriginalWaterWell72,orig::ModelBuilding::Well}}) {
        const auto mapped=orig::functional_model_candidate(object);
        check(mapped.candidate==model && mapped.functional==orig::EvidenceStatus::AuthoredFallback &&
            mapped.mechanics_identity==orig::EvidenceStatus::AuthoredFallback &&
            mapped.presentation_identity==orig::EvidenceStatus::Unknown,
            "Typed production identity was omitted or falsely advertised as original mechanics/art parity");
    }
    f::Root root;const auto path=root.data/"Model/EmperorBuildingModels.txt";
    f::write_model(root.data);auto old=prepared(root.data,false);
    for (const auto building:extra) {
        check(!old->supports_building(building),"Historical factory acquired active capabilities");
        refuses([&]{(void)old->building_value(building,orig::BuildingField::Cost);});
    }
    refuses([&]{(void)prepared(root.data,true);});
    oe_production_fixture::write_model(root.data);
    auto active=prepared(root.data,true);old=prepared(root.data,false);equivalent_legacy(*old,*active);
    check(old->provenance().sha256==active->provenance().sha256,"Explicit active factory changed source identity");
    check(active->building_value(extra[0],orig::BuildingField::Cost)==40 &&
        active->building_value(extra[0],orig::BuildingField::Employees)==18 &&
        active->building_value(extra[1],orig::BuildingField::Cost)==4 &&
        active->building_value(extra[1],orig::BuildingField::Employees)==0 &&
        active->building_value(extra[2],orig::BuildingField::Cost)==4 &&
        active->building_value(extra[3],orig::BuildingField::Cost)==4 &&
        active->building_value(extra[4],orig::BuildingField::Cost)==50 &&
        active->building_value(extra[4],orig::BuildingField::Employees)==8 &&
        active->building_value(extra[5],orig::BuildingField::Cost)==40 &&
        active->building_value(extra[5],orig::BuildingField::Employees)==4 &&
        active->building_value(extra[6],orig::BuildingField::Cost)==45 &&
        active->building_value(extra[6],orig::BuildingField::Employees)==4 &&
        active->building_value(extra[7],orig::BuildingField::Cost)==45 &&
        active->building_value(extra[7],orig::BuildingField::Employees)==4 &&
        active->building_value(extra[8],orig::BuildingField::Cost)==80 &&
        active->building_value(extra[8],orig::BuildingField::Employees)==12 &&
        active->building_value(extra[9],orig::BuildingField::Cost)==50 &&
        active->building_value(extra[9],orig::BuildingField::Employees)==8 &&
        active->building_value(extra[10],orig::BuildingField::Cost)==60 &&
        active->building_value(extra[10],orig::BuildingField::Employees)==9 &&
        active->building_value(extra[11],orig::BuildingField::Cost)==70 &&
        active->building_value(extra[11],orig::BuildingField::Employees)==10,
        "Authored named production values were not prepared");
    for(const auto [kind,cost,workers]:std::array{
        std::tuple{orig::ModelBuilding::LoggingShed,70,14},
        std::tuple{orig::ModelBuilding::BronzeSmelter,80,19},
        std::tuple{orig::ModelBuilding::BronzewareMaker,60,12},
        std::tuple{orig::ModelBuilding::LacquerwareMaker,65,11},
        std::tuple{orig::ModelBuilding::PaperMaker,70,10},
        std::tuple{orig::ModelBuilding::Weaver,60,11},
        std::tuple{orig::ModelBuilding::TeaCuringShed,40,12},
        std::tuple{orig::ModelBuilding::LacquerRefinery,40,12},
        std::tuple{orig::ModelBuilding::SilkwormShed,40,12},
        std::tuple{orig::ModelBuilding::TeaBush,6,0},
        std::tuple{orig::ModelBuilding::LacquerTree,5,0},
        std::tuple{orig::ModelBuilding::MulberryTree,5,0}})
        check(active->building_value(kind,orig::BuildingField::Cost)==cost&&
              active->building_value(kind,orig::BuildingField::Employees)==workers,
              "Named industry Cost/Employees not prepared");
    check(!active->supports_building(static_cast<orig::ModelBuilding>(4096)),"Undefined production model ID admitted");
    for (const auto candidate:{orig::ModelBuilding::FishingQuay,orig::ModelBuilding::HuntersTent}) {
        check(!active->supports_building(candidate),"Harvest observations granted production model capability");
        refuses([&]{(void)active->building_value(candidate,orig::BuildingField::Cost);});
    }
    refuses([&]{(void)active->building_value(extra[0],static_cast<orig::BuildingField>(13));});
    const auto pristine=f::read_text(path);
    for (const auto building:extra) {
        auto malformed=pristine;const auto label=std::string(orig::model_label(building));
        const auto at=malformed.find(label);check(at!=std::string::npos,"Named authored production row missing");
        malformed.replace(at,label.size(),"Wrong producer");f::write_text(path,malformed);
        // Additional edited rows must not add dependencies to old sessions.
        equivalent_legacy(*old,*prepared(root.data,false));
        refuses([&]{(void)prepared(root.data,true);});
        auto missing=pristine;
        const auto begin=missing.rfind('\n',at),end=missing.find('\n',at);
        check(begin!=std::string::npos && end!=std::string::npos,"Explicit production row boundaries missing");
        missing.erase(begin+1,end-begin);f::write_text(path,missing);
        equivalent_legacy(*old,*prepared(root.data,false));
        refuses([&]{(void)prepared(root.data,true);});
    }
    auto overflow=pristine;const std::string from="192,Hemp Farm,{,40,";
    const auto at=overflow.find(from);check(at!=std::string::npos,"Overflow control missing");
    overflow.replace(at,from.size(),"192,Hemp Farm,{,"+std::to_string(INT32_MAX)+',');
    f::write_text(path,overflow);check(bool(prepared(root.data,false)),"Old factory inspected new overflow row");
    refuses([&]{(void)prepared(root.data,true);});
    f::write_text(path,pristine);const auto identity=active->provenance().sha256;
    std::filesystem::remove(path);
    for (int i=0;i<1000;++i) check(active->supports_building(extra[0]) &&
        active->building_value(extra[0],orig::BuildingField::Employees)==18 && active->provenance().sha256==identity,
        "Prepared active queries re-read source or lost immutable values");
    refuses([&]{(void)prepared(root.data,true);});
}
int real(const std::filesystem::path& data) {
    const auto path=data/"Model/EmperorBuildingModels.txt";
    if (!std::filesystem::exists(path)) return 77;
    const auto before=f::read_text(path);const auto loaded=orig::load_model_data(data);check(bool(loaded),"Real model parse");
    for (std::size_t d=0;d<orig::difficulty_count;++d) {
        const auto difficulty=static_cast<orig::Difficulty>(d);
        const auto old=prepared(data,false,difficulty),active=prepared(data,true,difficulty);
        equivalent_legacy(*old,*active);
        check(old->provenance().sha256==active->provenance().sha256,"Real factory source identities differ");
        check(active->provenance().sha256=="7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6",
            "Real active model source fingerprint changed");
        for (const auto building:extra) for (std::size_t field=0;field<orig::building_field_count;++field) {
            const auto column=static_cast<orig::BuildingField>(field);
            check(active->supports_building(building) &&
                active->building_value(building,column)==orig::building_value(*loaded.model,building,column,difficulty),
                "Active prepared value differs from checked raw model/difficulty arithmetic");
        }
        const auto permissions=f::permissions();
        auto world=f::sim::World::create_original_production_development(permissions,active);
        const auto farm_cost=orig::building_value(*loaded.model,extra[0],orig::BuildingField::Cost,difficulty);
        const auto field_cost=orig::building_value(*loaded.model,extra[1],orig::BuildingField::Cost,difficulty);
        check(world.construction_cost(f::sim::CommandType::PlaceOriginalHempFarm)==farm_cost &&
            world.construction_cost(f::sim::CommandType::PlaceOriginalHempField)==field_cost,
            "Real difficulty-adjusted model prices did not reach paid World commands");
        f::buy(world,{f::sim::CommandType::PlaceOriginalHempFarm,{0,9}});
        f::sim::Command field{f::sim::CommandType::PlaceOriginalHempField,{2,9}};
        field.building_id=*world.building_owner_at({0,9});f::buy(world,field);
        check(world.construction_spent_total()==static_cast<std::uint64_t>(farm_cost+field_cost) &&
            world.treasury()==12000-farm_cost-field_cost,"Paid Farm/Field did not charge the selected difficulty");
        std::int64_t food_paid=0;
        for (const bool millet:{false,true}) {
            const f::sim::Cell origin{millet ? 12:6,9};
            const auto crop=millet ? orig::ModelBuilding::MilletField:orig::ModelBuilding::WheatField;
            f::buy(world,{f::sim::CommandType::PlaceOriginalFoodFarm,origin});
            const auto id=*world.building_owner_at(origin);
            check(world.workforce_required(id)==orig::building_value(*loaded.model,orig::ModelBuilding::Farmhouse,
                orig::BuildingField::Employees,difficulty),"Real Food Farm staffing did not use prepared difficulty");
            f::sim::Command plant{millet ? f::sim::CommandType::PlaceOriginalMilletField:f::sim::CommandType::PlaceOriginalWheatField,{origin.x+3,origin.y}};
            plant.building_id=id;f::buy(world,plant);
            food_paid+=orig::building_value(*loaded.model,orig::ModelBuilding::Farmhouse,orig::BuildingField::Cost,difficulty)+
                orig::building_value(*loaded.model,crop,orig::BuildingField::Cost,difficulty);
        }
        check(world.construction_spent_total()==static_cast<std::uint64_t>(farm_cost+field_cost+food_paid) &&
            world.treasury()==12000-farm_cost-field_cost-food_paid,"Food Farm/Field prices did not use exact selected difficulty");
        std::int64_t medicine_paid=0;
        for (const auto [command,kind,cell]:std::array{
            std::tuple{f::sim::CommandType::PlaceHealthPost,orig::ModelBuilding::HerbalistsStall,f::sim::Cell{18,9}},
            std::tuple{f::sim::CommandType::PlaceServicePost,orig::ModelBuilding::AcupuncturistsClinic,f::sim::Cell{21,9}},
            std::tuple{f::sim::CommandType::PlaceOriginalAncestorShrine,orig::ModelBuilding::AncestralShrine,f::sim::Cell{18,13}},
            std::tuple{f::sim::CommandType::PlaceOriginalDaoistShrine,orig::ModelBuilding::DaoistShrine,f::sim::Cell{0,15}},
            std::tuple{f::sim::CommandType::PlaceOriginalBuddhistShrine,orig::ModelBuilding::BuddhistShrine,f::sim::Cell{3,15}},
            std::tuple{f::sim::CommandType::PlaceOriginalConfucianAcademy,orig::ModelBuilding::ConfucianAcademy,f::sim::Cell{6,15}},
            std::tuple{f::sim::CommandType::PlaceOriginalMusicSchool,orig::ModelBuilding::MusicSchool,f::sim::Cell{9,15}},
            std::tuple{f::sim::CommandType::PlaceOriginalAcrobatSchool,orig::ModelBuilding::AcrobatSchool,f::sim::Cell{12,15}},
            std::tuple{f::sim::CommandType::PlaceOriginalDramaSchool,orig::ModelBuilding::DramaSchool,f::sim::Cell{15,15}}}) {
            const auto cost=orig::building_value(*loaded.model,kind,orig::BuildingField::Cost,difficulty);
            check(world.construction_cost(command)==cost,"Service/religion/culture command ignored prepared model price");
            f::buy(world,{command,cell});medicine_paid+=cost;
            check(world.workforce_required(*world.building_owner_at(cell))==orig::building_value(*loaded.model,kind,
                orig::BuildingField::Employees,difficulty),"Service/religion/culture source ignored prepared Employees");
        }
        for (const auto [kind,side]:std::array{
            std::pair{f::sim::Object::OriginalDaoistShrine215,2},
            std::pair{f::sim::Object::OriginalBuddhistShrine217,2},
            std::pair{f::sim::Object::OriginalConfucianAcademy219,3},
            std::pair{f::sim::Object::OriginalMusicSchool211,2},
            std::pair{f::sim::Object::OriginalAcrobatSchool212,3},
            std::pair{f::sim::Object::OriginalDramaSchool213,3}}) {
            const auto footprint=f::sim::building_footprint(world.profile(),world.rule_version(),kind);
            check(footprint.width==side && footprint.height==side,"Religion/culture source lost its separately evidenced paid footprint");
        }
        check(world.construction_spent_total()==static_cast<std::uint64_t>(farm_cost+field_cost+food_paid+medicine_paid) &&
            world.treasury()==12000-farm_cost-field_cost-food_paid-medicine_paid,"Actual service/religion/culture model prices were not paid");
        const auto saved=world.snapshot();
        check(f::sim::World::restore_original_production_development(saved,permissions,active).snapshot()==saved,
            "Real difficulty-adjusted Farm/Field finance did not restore exactly");
        for(const auto [command,kind]:std::array{
            std::pair{f::sim::CommandType::PlaceOriginalLoggingShed,orig::ModelBuilding::LoggingShed},
            std::pair{f::sim::CommandType::PlaceOriginalBronzeSmelter,orig::ModelBuilding::BronzeSmelter},
            std::pair{f::sim::CommandType::PlaceOriginalBronzewareMaker,orig::ModelBuilding::BronzewareMaker},
            std::pair{f::sim::CommandType::PlaceOriginalLacquerwareMaker,orig::ModelBuilding::LacquerwareMaker},
            std::pair{f::sim::CommandType::PlaceOriginalPaperMaker,orig::ModelBuilding::PaperMaker},
            std::pair{f::sim::CommandType::PlaceOriginalWeaver,orig::ModelBuilding::Weaver},
            std::pair{f::sim::CommandType::PlaceOriginalTeaCuringShed,orig::ModelBuilding::TeaCuringShed},
            std::pair{f::sim::CommandType::PlaceOriginalLacquerRefinery,orig::ModelBuilding::LacquerRefinery},
            std::pair{f::sim::CommandType::PlaceOriginalSilkwormShed,orig::ModelBuilding::SilkwormShed}}) {
            auto industry=f::sim::World::create_original_production_development(permissions,active);
            const auto cost=active->building_value(kind,orig::BuildingField::Cost);
            check(industry.construction_cost(command)==cost,"Industry command ignored selected difficulty price");
            f::buy(industry,{command,{4,9}});const auto id=*industry.building_owner_at({4,9});
            check(industry.workforce_required(id)==active->building_value(kind,orig::BuildingField::Employees)&&
                  industry.construction_spent_total()==static_cast<std::uint64_t>(cost)&&industry.treasury()==12000-cost,
                  "Actual industry purchase ignored model Cost/Employees");
            if(kind==orig::ModelBuilding::TeaCuringShed||kind==orig::ModelBuilding::LacquerRefinery||kind==orig::ModelBuilding::SilkwormShed){
                const auto crop=kind==orig::ModelBuilding::TeaCuringShed?orig::ModelBuilding::TeaBush:
                    kind==orig::ModelBuilding::LacquerRefinery?orig::ModelBuilding::LacquerTree:orig::ModelBuilding::MulberryTree;
                const auto place=crop==orig::ModelBuilding::TeaBush?f::sim::CommandType::PlaceOriginalTeaBush:
                    crop==orig::ModelBuilding::LacquerTree?f::sim::CommandType::PlaceOriginalLacquerTree:f::sim::CommandType::PlaceOriginalMulberryTree;
                f::sim::Command plant{place,{6,9}};plant.building_id=id;f::buy(industry,plant);
                const auto plant_cost=active->building_value(crop,orig::BuildingField::Cost);
                check(industry.construction_spent_total()==static_cast<std::uint64_t>(cost+plant_cost)&&
                      industry.workforce_required(*industry.building_owner_at({6,9}))==0,"Actual plant price/zero workforce differs");
            }
            check(f::sim::World::restore_original_production_development(industry.snapshot(),permissions,active).snapshot()==industry.snapshot(),
                  "Real industry difficulty-adjusted paid bodies did not restore");
        }
        auto bad=saved;++bad.construction_spent_total;--bad.treasury;
        refuses([&]{(void)f::sim::World::restore_original_production_development(bad,permissions,active);});
    }
    check(f::read_text(path)==before,"Real production model was modified");return 0;
}
}
int main(int argc,char** argv) {
    try {
        if (argc==3 && std::string_view(argv[1])=="--real") {const auto result=real(argv[2]);if (result)return result;}
        else {check(argc==1,"Unknown production model test arguments");synthetic();}
        std::cout<<"Explicit active model production preparation/old seven-row compatibility PASS\n";return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
