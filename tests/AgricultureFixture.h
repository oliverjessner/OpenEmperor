#pragma once

// Authored, paid scenario inputs. No original map/model bytes are embedded.
#include "OriginalFinanceFixture.h"
#include "maps/EmperorMap.h"
#include "maps/LandscapeProvenance.h"

#include <zlib.h>

namespace oe_agriculture_fixture {
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace maps=openemperor::maps;
namespace fs=std::filesystem;
using f::check;
using Bytes=std::vector<std::uint8_t>;

inline void u32(Bytes& bytes,std::size_t at,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)bytes.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));
}
inline Bytes encoded_map(const std::vector<std::uint32_t>& terrain,const std::vector<std::uint8_t>& fertility) {
    check(terrain.size()==sim::OriginalTerrainFacts::cell_count && fertility.size()==terrain.size(),
          "Authored Agriculture map requires complete independent terrain/fertility planes");
    Bytes raw(static_cast<std::size_t>(maps::landscape_height_offset)+terrain.size());
    const std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),raw.begin());u32(raw,84,226);
    for(std::size_t i=0;i<terrain.size();++i)
        u32(raw,static_cast<std::size_t>(maps::terrain_logical_offset)+4*i,terrain[i]);
    // The existing standalone source profile serializes this separate lane.
    std::copy(fertility.begin(),fertility.end(),raw.begin()+937255);
    Bytes bytes(4);u32(bytes,0,0xfedcbaaaU);
    for(std::size_t at=0;at<raw.size();at+=32768) {
        const auto size=std::min<std::size_t>(32768,raw.size()-at);
        uLongf length=compressBound(static_cast<uLong>(size));Bytes block(length);
        check(compress2(block.data(),&length,raw.data()+at,static_cast<uLong>(size),6)==Z_OK,
              "Authored Agriculture map compression failed");
        const auto header=bytes.size();bytes.resize(header+12);u32(bytes,header,0x12345678U);
        u32(bytes,header+4,static_cast<std::uint32_t>(length));u32(bytes,header+8,static_cast<std::uint32_t>(size));
        bytes.insert(bytes.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(length));
    }
    return bytes;
}
inline std::string authored_model() {
    auto text=oe_finance_fixture::authored_model();
    const auto at=text.find("HOUSE MODS\n");check(at!=std::string::npos,"Authored Agriculture model section missing");
    text.insert(at,
        "197,Rice Paddy,{,7,0,0,0,0,0,0,0,0,0,0,0,0,},,,\n"
        "198,Cabbage Field,{,5,0,0,0,0,0,0,0,0,0,0,0,0,},,,\n"
        "199,Soybean Field,{,5,0,0,0,0,0,0,0,0,0,0,0,0,},,,\n");
    return text;
}
struct Fixture {
    f::Root root;
    std::vector<std::uint32_t> raw=std::vector<std::uint32_t>(sim::OriginalTerrainFacts::cell_count);
    std::vector<std::uint8_t> fertility=std::vector<std::uint8_t>(sim::OriginalTerrainFacts::cell_count,100);
    std::shared_ptr<const sim::MapPermissions> policy;
    std::shared_ptr<const orig::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain;
    Fixture() {
        std::vector<sim::MapCellPermission> cells(raw.size(),
            {true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});
        policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),std::vector<sim::FixedGatePassage>{});
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",authored_model());
        model=save::prepare_original_agriculture_rules_model(root.data);
        write_map();terrain=save::prepare_original_agriculture_terrain_facts(root.data,f::map_relative);
    }
    void write_map() const {
        const auto bytes=encoded_map(raw,fertility);
        f::write_text(root.data/f::map_relative,std::string(bytes.begin(),bytes.end()));
    }
    std::vector<std::uint8_t> mask() const {return policy->building_mask();}
    sim::World world() const {return sim::World::create_original_agriculture(policy,model,terrain);}
    save::SaveDocument document(const sim::World& world) const {
        return save::make_document(root.data,f::map_relative,mask(),world);
    }
    sim::World restore(const save::SaveDocument& document) const {
        return save::restore_original_agriculture(document,root.data,mask(),policy,model,terrain);
    }
};
struct Scene {
    sim::BuildingId cabbage_farm{},soy_farm{},cabbage_field{},soy_field{},market{},water{},ancestor{};
    std::vector<sim::BuildingId> homes,watches;
};
inline sim::BuildingId purchase(sim::World& world,sim::CommandType type,sim::Cell cell) {
    f::buy(world,{type,cell});return *world.building_owner_at(cell);
}
inline Scene build(sim::World& world) {
    Scene result;
    for(int x=20;x<=51;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
    for(int x=20;x<44;x+=3)result.homes.push_back(purchase(world,sim::CommandType::PlaceHousehold,{x,22}));
    result.cabbage_farm=purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    result.soy_farm=purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{27,25});
    sim::Command field{sim::CommandType::PlaceOriginalCabbageField,{23,25}};field.building_id=result.cabbage_farm;
    f::buy(world,field);result.cabbage_field=*world.building_owner_at(field.cell);
    field={sim::CommandType::PlaceOriginalSoybeanField,{30,25}};field.building_id=result.soy_farm;
    f::buy(world,field);result.soy_field=*world.building_owner_at(field.cell);
    result.market=purchase(world,sim::CommandType::PlaceMarket,{33,25});
    result.water=purchase(world,sim::CommandType::PlaceOriginalWaterWell,{37,25});
    result.ancestor=purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{41,25});
    result.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,{45,25}));
    result.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,{48,25}));
    check(!world.total_population() && !world.taxes_collected_total(),"Paid Agriculture build granted residents or income");
    return result;
}
inline const sim::AgricultureStock& stock(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.original_agriculture_state()->stocks)if(row.building_id==id)return row;
    throw std::runtime_error("Paid Agriculture owner lost its sole typed Food stock");
}
inline const sim::AgricultureField& field(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.original_agriculture_state()->fields)if(row.field_id==id)return row;
    throw std::runtime_error("Paid Agriculture field lost its typed production identity");
}
template<class Predicate> inline void until(sim::World& world,Predicate predicate,const char* reason,int limit=30000) {
    for(int tick=0;tick<limit && !predicate();++tick)world.tick();
    check(predicate(),reason);
}
} // namespace oe_agriculture_fixture
