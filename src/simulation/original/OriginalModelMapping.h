#pragma once

#include "simulation/World.h"
#include "simulation/original/ModelSchema.h"

#include <optional>
#include <stdexcept>

namespace openemperor::original {

struct FunctionalModelCandidate {
    std::optional<ModelBuilding> candidate;
    EvidenceStatus functional = EvidenceStatus::Unknown;
    EvidenceStatus mechanics_identity = EvidenceStatus::Unknown;
    EvidenceStatus presentation_identity = EvidenceStatus::Unknown;
};

// A functional analogy does not establish constructibility, original mechanics,
// footprint, price, service timing or an original source identity for an Object.
// Existing recovered Well/Herbalist image registrations are a separate fact.
[[nodiscard]] inline FunctionalModelCandidate functional_model_candidate(
    simulation::Object object) {
    using simulation::Object;
    switch (object) {
    case Object::ClaySource:
        return {ModelBuilding::ClayPit, EvidenceStatus::Inferred,
                EvidenceStatus::Unknown, EvidenceStatus::Unknown};
    case Object::Pottery:
        return {ModelBuilding::Kiln, EvidenceStatus::Inferred,
                EvidenceStatus::Unknown, EvidenceStatus::Unknown};
    case Object::Warehouse:
        return {ModelBuilding::Warehouse, EvidenceStatus::Inferred,
                EvidenceStatus::Unknown, EvidenceStatus::Unknown};
    case Object::Market:
        return {ModelBuilding::CommonMarketSquare, EvidenceStatus::Inferred,
                EvidenceStatus::Unknown, EvidenceStatus::Unknown};
    case Object::Farm:
        return {ModelBuilding::Farmhouse, EvidenceStatus::Inferred,
                EvidenceStatus::Unknown, EvidenceStatus::Unknown};
    case Object::Well:
        return {ModelBuilding::Well, EvidenceStatus::Inferred,
                EvidenceStatus::Unknown, EvidenceStatus::ReverseEngineered};
    case Object::HealthPost:
        return {ModelBuilding::HerbalistsStall, EvidenceStatus::Inferred,
                EvidenceStatus::Unknown, EvidenceStatus::ReverseEngineered};
    case Object::OriginalHempFarm:
        return {ModelBuilding::HempFarm, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalHempField:
        return {ModelBuilding::HempField, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalFoodFarm:
        return {ModelBuilding::Farmhouse, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalWheatField:
        return {ModelBuilding::WheatField, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalMilletField:
        return {ModelBuilding::MilletField, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalLoggingShed38:
        return {ModelBuilding::LoggingShed, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalBronzeSmelter39:
        return {ModelBuilding::BronzeSmelter, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalBronzewareMaker42:
        return {ModelBuilding::BronzewareMaker, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalLacquerwareMaker44:
        return {ModelBuilding::LacquerwareMaker, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalPaperMaker45:
        return {ModelBuilding::PaperMaker, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalWeaver47:
        return {ModelBuilding::Weaver, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalTeaCuringShed237:
        return {ModelBuilding::TeaCuringShed, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalLacquerRefinery238:
        return {ModelBuilding::LacquerRefinery, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalSilkwormShed239:
        return {ModelBuilding::SilkwormShed, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalTeaBush26:
        return {ModelBuilding::TeaBush, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalLacquerTree27:
        return {ModelBuilding::LacquerTree, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalMulberryTree28:
        return {ModelBuilding::MulberryTree, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalMusicSchool211:
        return {ModelBuilding::MusicSchool, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalAcrobatSchool212:
        return {ModelBuilding::AcrobatSchool, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalDramaSchool213:
        return {ModelBuilding::DramaSchool, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalTheatre75:
        return {ModelBuilding::TheatrePavilion, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalDaoistShrine215:
        return {ModelBuilding::DaoistShrine, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalBuddhistShrine217:
        return {ModelBuilding::BuddhistShrine, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalConfucianAcademy219:
        return {ModelBuilding::ConfucianAcademy, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalWaterWell72:
        return {ModelBuilding::Well, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::OriginalAncestorShrine214:
        return {ModelBuilding::AncestralShrine, EvidenceStatus::AuthoredFallback,
                EvidenceStatus::AuthoredFallback, EvidenceStatus::Unknown};
    case Object::Empty:
    case Object::Road:
    case Object::Workshop:
    case Object::Household:
    case Object::OriginalElitePlot:
    case Object::OriginalEliteHouse:
    case Object::ServicePost:
    case Object::FireWatch:
    case Object::OriginalInfluenceSource:
        return {};
    }
    throw std::out_of_range("invalid Object for original model candidate");
}

} // namespace openemperor::original
