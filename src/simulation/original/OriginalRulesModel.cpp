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

constexpr std::array agriculture_buildings{ModelBuilding::RiceField,ModelBuilding::CabbageField,ModelBuilding::SoybeanField};
constexpr std::array irrigation_buildings{ModelBuilding::IrrigationDitch,ModelBuilding::IrrigationPump};
constexpr std::array food_processing_buildings{ModelBuilding::Mill,ModelBuilding::FoodShop};

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

std::optional<std::array<BuildingRow,3>> prepare_agriculture_buildings(
        const ModelData& model,Difficulty difficulty,bool enabled) {
    if (!enabled) return std::nullopt;
    std::array<BuildingRow,3> rows{};
    for (std::size_t row=0;row<agriculture_buildings.size();++row) {
        const auto building=agriculture_buildings[row];
        if (!model.has_building(model_id(building)))
            throw std::invalid_argument("Agriculture model is missing named building: "+std::string(model_label(building)));
        for (std::size_t field=0;field<building_field_count;++field)
            rows[row][field]=original::building_value(model,building,static_cast<BuildingField>(field),difficulty);
    }
    return rows;
}

std::optional<std::array<BuildingRow,2>> prepare_irrigation_buildings(
        const ModelData& model,Difficulty difficulty,bool enabled) {
    if (!enabled) return std::nullopt;
    std::array<BuildingRow,2> rows{};
    for (std::size_t row=0;row<irrigation_buildings.size();++row) {
        const auto building=irrigation_buildings[row];
        if (!model.has_building(model_id(building)))
            throw std::invalid_argument("Irrigation model is missing named building: "+std::string(model_label(building)));
        for (std::size_t field=0;field<building_field_count;++field)
            rows[row][field]=original::building_value(model,building,static_cast<BuildingField>(field),difficulty);
    }
    return rows;
}

} // namespace

OriginalRulesModel::OriginalRulesModel(const ModelData& model,Difficulty difficulty,bool production,bool culture,bool agriculture,bool irrigation,bool fishing,bool hunting,bool food_processing)
    : provenance_(checked_provenance(model,difficulty)),difficulty_(difficulty),
      house_rules_(model,difficulty),building_rows_(prepare_buildings(model,difficulty)),
      production_prepared_(production),
      production_rows_(prepare_production_buildings(model,difficulty,production)),
      culture_row_(prepare_culture_building(model,difficulty,culture)),
      agriculture_rows_(prepare_agriculture_buildings(model,difficulty,agriculture)),
      irrigation_rows_(prepare_irrigation_buildings(model,difficulty,irrigation)),
      fishing_row_([&]() -> std::optional<BuildingRow> {
          if (!fishing) return std::nullopt;
          BuildingRow row{};
          for (std::size_t i=0;i<row.size();++i)
              row[i]=original::building_value(model,ModelBuilding::FishingQuay,static_cast<BuildingField>(i),difficulty);
          return row;
      }()),
      hunting_row_([&]() -> std::optional<BuildingRow> {
          if (!hunting) return std::nullopt;
          BuildingRow row{};
          for (std::size_t i=0;i<row.size();++i)
              row[i]=original::building_value(model,ModelBuilding::HuntersTent,static_cast<BuildingField>(i),difficulty);
          return row;
      }()),
      food_processing_rows_([&]() -> std::optional<std::array<BuildingRow,2>> {
          if (!food_processing) return std::nullopt;
          std::array<BuildingRow,2> rows{};
          for (std::size_t row=0;row<food_processing_buildings.size();++row)
              for (std::size_t field=0;field<building_field_count;++field)
                  rows[row][field]=original::building_value(model,food_processing_buildings[row],
                      static_cast<BuildingField>(field),difficulty);
          return rows;
      }()) {}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,false));
}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare_production(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,true));
}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare_culture(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,true,true));
}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare_agriculture(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,true,false,true));
}
std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare_irrigation(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,true,false,true,true));
}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare_fishing(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,true,false,true,true,true));
}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare_hunting(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,true,false,true,true,true,true));
}

std::shared_ptr<const OriginalRulesModel> OriginalRulesModel::prepare_food_processing(ModelData model,Difficulty difficulty) {
    return std::shared_ptr<const OriginalRulesModel>(new OriginalRulesModel(model,difficulty,true,false,true,true,true,true,true));
}

bool OriginalRulesModel::supports_building(ModelBuilding building) const noexcept {
    if (std::find(food_processing_buildings.begin(),food_processing_buildings.end(),building)!=food_processing_buildings.end())
        return food_processing_rows_.has_value();
    if (building==ModelBuilding::HuntersTent) return hunting_row_.has_value();
    if (building==ModelBuilding::FishingQuay) return fishing_row_.has_value();
    if (std::find(irrigation_buildings.begin(),irrigation_buildings.end(),building)!=irrigation_buildings.end())
        return irrigation_rows_.has_value();
    if (std::find(agriculture_buildings.begin(),agriculture_buildings.end(),building)!=agriculture_buildings.end())
        return agriculture_rows_.has_value();
    if (building==ModelBuilding::TheatrePavilion) return culture_row_.has_value();
    if (std::find(buildings.begin(),buildings.end(),building)!=buildings.end()) return true;
    return production_prepared_ &&
        std::find(production_buildings.begin(),production_buildings.end(),building)!=production_buildings.end();
}

std::int32_t OriginalRulesModel::building_value(ModelBuilding building,BuildingField field) const {
    const auto column=static_cast<std::size_t>(field);
    if (column>=building_field_count) throw std::out_of_range("invalid prepared original building field");
    const auto processing=std::find(food_processing_buildings.begin(),food_processing_buildings.end(),building);
    if (processing!=food_processing_buildings.end()) {
        if (!food_processing_rows_) throw std::out_of_range("Food processing model was not explicitly prepared");
        return food_processing_rows_->at(static_cast<std::size_t>(processing-food_processing_buildings.begin())).at(column);
    }
    if (building==ModelBuilding::HuntersTent) {
        if (!hunting_row_) throw std::out_of_range("Hunting model was not explicitly prepared");
        return hunting_row_->at(column);
    }
    if (building==ModelBuilding::FishingQuay) {
        if (!fishing_row_) throw std::out_of_range("Fishing model was not explicitly prepared");
        return fishing_row_->at(column);
    }
    const auto irrigation=std::find(irrigation_buildings.begin(),irrigation_buildings.end(),building);
    if (irrigation!=irrigation_buildings.end()) {
        if (!irrigation_rows_) throw std::out_of_range("Irrigation model was not explicitly prepared");
        return irrigation_rows_->at(static_cast<std::size_t>(irrigation-irrigation_buildings.begin())).at(column);
    }
    const auto agriculture=std::find(agriculture_buildings.begin(),agriculture_buildings.end(),building);
    if (agriculture!=agriculture_buildings.end()) {
        if (!agriculture_rows_) throw std::out_of_range("Agriculture model was not explicitly prepared");
        return agriculture_rows_->at(static_cast<std::size_t>(agriculture-agriculture_buildings.begin())).at(column);
    }
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
