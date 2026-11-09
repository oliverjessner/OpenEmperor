#include "simulation/OriginalEconomyStarter.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace openemperor::simulation {

OriginalEconomyStarterReceipt place_original_economy_starter(World& world) {
    if (!world.original_production_enabled() || world.ticks()!=0 ||
        world.command_sequence()!=0 || !world.buildings().empty() ||
        world.treasury()!=original_production_starting_treasury || !world.map_permissions())
        throw std::invalid_argument("Paid original starter requires a fresh Rule10 candidate");

    // Keep both road rows, every footprint and their connecting edge on an
    // existing legal plane. This deliberately refuses unsuitable maps instead
    // of clearing original structures/roads or creating new permissions.
    constexpr int width=24,height=13;
    std::vector<Cell> origins;
    for (int y=0;y+height<=world.height();++y)
        for (int x=0;x+width<=world.width();++x) origins.push_back({x,y});
    const auto distance=[&](Cell c) {
        return std::abs(2*c.x+width-world.width())+std::abs(2*c.y+height-world.height());
    };
    std::stable_sort(origins.begin(),origins.end(),[&](Cell a,Cell b){return distance(a)<distance(b);});
    std::optional<Cell> origin;
    for (const auto candidate:origins) {
        const auto level=world.map_permissions()->cell_height(candidate);
        bool legal=true;
        for (int y=0;y<height && legal;++y) for (int x=0;x<width;++x) {
            const Cell c{candidate.x+x,candidate.y+y};
            if (!world.buildable(c) || !world.road_buildable(c) ||
                world.object_at(c)!=Object::Empty || world.map_permissions()->cell_height(c)!=level) {
                legal=false;break;
            }
        }
        if (legal) {origin=candidate;break;}
    }
    if (!origin) throw OriginalStarterPlacementError(
        "No legal empty 24x13 level area for the paid original starter; choose Empty or another map");

    OriginalEconomyStarterReceipt receipt;
    receipt.origin=*origin;receipt.focus={double(origin->x)+11.5,double(origin->y)+6};
    const auto before=world.construction_spent_total();
    const auto sequence=world.command_sequence();
    const auto execute=[&](Command command) {
        command.cell.x+=origin->x;command.cell.y+=origin->y;
        const auto result=world.execute(command);
        if (!result.accepted || !result.changed) throw std::runtime_error(
            "Paid original starter command refused: "+result.reason);
    };
    const auto buy=[&](CommandType type,Cell cell) {
        execute({type,cell});
        return world.building_owner_at({cell.x+origin->x,cell.y+origin->y});
    };
    for (int y:{4,9}) for (int x=0;x<24;++x) execute({CommandType::PlaceRoad,{x,y}});
    for (int y=5;y<9;++y) execute({CommandType::PlaceRoad,{20,y}});
    std::size_t house=0;
    for (int x:{0,3,6,9}) receipt.house_ids[house++]=*buy(CommandType::PlaceHousehold,{x,2});
    buy(CommandType::PlaceOriginalWaterWell,{4,5});
    buy(CommandType::PlaceOriginalAncestorShrine,{6,5});
    buy(CommandType::PlaceMarket,{9,5});
    const auto food=*buy(CommandType::PlaceOriginalFoodFarm,{0,10});
    Command field{CommandType::PlaceOriginalMilletField,{3,10}};
    field.building_id=food;execute(field);
    buy(CommandType::PlaceClaySource,{12,5});
    buy(CommandType::PlacePottery,{15,5});
    buy(CommandType::PlaceWarehouse,{18,5});
    const auto hemp=*buy(CommandType::PlaceOriginalHempFarm,{5,10});
    field={CommandType::PlaceOriginalHempField,{7,10}};field.building_id=hemp;execute(field);
    buy(CommandType::PlaceServicePost,{21,5});
    for (int x:{5,8,11,14}) for (int y=0;y<4;++y) execute({CommandType::PlaceRoad,{x,y}});
    buy(CommandType::PlaceOriginalAncestorShrine,{6,0});
    buy(CommandType::PlaceOriginalWaterWell,{9,0});
    buy(CommandType::PlaceHealthPost,{12,0});
    buy(CommandType::PlaceOriginalMusicSchool,{15,0});
    receipt.construction_spent=world.construction_spent_total()-before;
    receipt.commands_executed=world.command_sequence()-sequence;
    return receipt;
}

} // namespace openemperor::simulation
