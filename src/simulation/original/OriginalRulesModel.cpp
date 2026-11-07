#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace openemperor::original {
namespace {

constexpr std::array<ModelBuilding,7> buildings{
    ModelBuilding::ClayPit,ModelBuilding::Kiln,ModelBuilding::Warehouse,
    ModelBuilding::CommonMarketSquare,ModelBuilding::Well,ModelBuilding::Farmhouse,
    ModelBuilding::HerbalistsStall};
constexpr std::array<ModelBuilding,24> production_buildings{
    ModelBuilding::HempFarm,ModelBuilding::HempField,ModelBuilding::WheatField,ModelBuilding::MilletField,
    ModelBuilding::AcupuncturistsClinic,ModelBuilding::AncestralShrine,
    ModelBuilding::DaoistShrine,ModelBuilding::BuddhistShrine,ModelBuilding::ConfucianAcademy,
    ModelBuilding::MusicSchool,ModelBuilding::AcrobatSchool,ModelBuilding::DramaSchool,
    ModelBuilding::LoggingShed, ModelBuilding::BronzeSmelter, ModelBuilding::BronzewareMaker, ModelBuilding::LacquerwareMaker, ModelBuilding::PaperMaker, ModelBuilding::Weaver, ModelBuilding::TeaCuringShed, ModelBuilding::LacquerRefinery, ModelBuilding::SilkwormShed, ModelBuilding::TeaBush, ModelBuilding::LacquerTree, ModelBuilding::MulberryTree};

ModelProvenance checked_provenance(const ModelData& model,Difficulty difficulty) {
    if (static_cast<std::size_t>(difficulty)>=difficulty_count)
        throw std::out_of_range("invalid original rules model difficulty");
    const auto& source=model.provenance();
    if (source.format_version!=model_schema_version || source.source_kind!=ModelSourceKind::File ||
        source.source_path.empty() || source.sha256.size()!=64 ||
        !std::all_of(source.sha256.begin(),source.sha256.end(),[](char c) {
            return (c>='0' && c<='9') || (c>='a' && c<='f');
        }) || source.byte_count==0 || source.byte_count>max_model_bytes)
        throw std::invalid_argument("original rules model requires valid loaded file provenance");
    return source;
}

std::array<BuildingRow,buildings.size()> prepare_buildings(const ModelData& model,Difficulty difficulty) {
    std::array<BuildingRow,buildings.size()> rows{};
    for (std::size_t row=0;row<buildings.size();++row) {
        if (!model.has_building(model_id(buildings[row])))
            throw std::invalid_argument("original rules model is missing named building: "+
                std::string(model_label(buildings[row])));
        for (std::size_t field=0;field<building_field_count;++field)
            rows[row][field]=original::building_value(model,buildings[row],
                static_cast<BuildingField>(field),difficulty);
    }
    return rows;
}

std::size_t building_index(ModelBuilding building) {
    const auto found=std::find(buildings.begin(),buildings.end(),building);
    if (found==buildings.end()) throw std::out_of_range("invalid prepared original building ID");
    return static_cast<std::size_t>(found-buildings.begin());
}

std::array<BuildingRow,production_buildings.size()> prepare_production_buildings(
        const ModelData& model,Difficulty difficulty,bool enabled) {
    std::array<BuildingRow,production_buildings.size()> rows{};
    if (!enabled) return rows;
    for (std::size_t row=0;row<production_buildings.size();++row) {
        const auto building=production_buildings[row];
        if (!model.has_building(model_id(building)))
            throw std::invalid_argument("active original model is missing named building: "+
                std::string(model_label(building)));
        for (std::size_t field=0;field<building_field_count;++field)
            rows[row][field]=original::building_value(model,building,
                static_cast<BuildingField>(field),difficulty);
    }
    return rows;
}

std::optional<BuildingRow> prepare_culture_building(
        const ModelData& model,Difficulty difficulty,bool enabled) {
    if (!enabled) return std::nullopt;
    constexpr auto building=ModelBuilding::TheatrePavilion;
    if (!model.has_building(model_id(building)))
        throw std::invalid_argument("culture model is missing named building: "+
            std::string(model_label(building)));
    BuildingRow row{};
    for (std::size_t field=0;field<building_field_count;++field)
        row[field]=original::building_value(model,building,
            static_cast<BuildingField>(field),difficulty);
    return row;
}

} // namespace

OriginalRulesModel::OriginalRulesModel(const ModelData& model,Difficulty difficulty,bool production,bool culture)
    : provenance_(checked_provenance(model,difficulty)),difficulty_(difficulty),
      house_rules_(model,difficulty),building_rows_(prepare_buildings(model,difficulty)),
      production_prepared_(production),
      production_rows_(prepare_production_buildings(model,difficulty,production)),
      culture_row_(prepare_culture_building(model,difficulty,culture)) {}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,false));
}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare_production(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,true));
}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare_culture(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,true,true));
}

bool OriginalRulesModel::supports_building(ModelBuilding building) const noexcept {
    if (building==ModelBuilding::TheatrePavilion) return culture_row_.has_value();
    if (std::find(buildings.begin(),buildings.end(),building)!=buildings.end()) return true;
    return production_prepared_ &&
        std::find(production_buildings.begin(),production_buildings.end(),building)!=production_buildings.end();
}

std::int32_t OriginalRulesModel::building_value(ModelBuilding building,BuildingField field) const {
    const auto column=static_cast<std::size_t>(field);
    if (column>=building_field_count) throw std::out_of_range("invalid prepared original building field");
    if (building==ModelBuilding::TheatrePavilion) {
        if (!culture_row_) throw std::out_of_range("culture model was not explicitly prepared");
        return culture_row_->at(column);
    }
    const auto extra=std::find(production_buildings.begin(),production_buildings.end(),building);
    if (extra!=production_buildings.end()) {
        if (!production_prepared_) throw std::out_of_range("active production model was not explicitly prepared");
        return production_rows_.at(static_cast<std::size_t>(extra-production_buildings.begin())).at(column);
    }
    return building_rows_.at(building_index(building)).at(column);
}

} // namespace openemperor::original
