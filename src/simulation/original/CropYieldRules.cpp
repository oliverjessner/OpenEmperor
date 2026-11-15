#include "simulation/original/CropRules.h"

namespace openemperor::original {

bool crop_doubled_base(ModelBuilding field, std::uint8_t event_enabled,
                       std::uint32_t event_state, std::uint32_t crop_selector) {
    (void)crop_rule(field);
    if (!event_enabled || event_state != 3) return false;
    switch (crop_selector) {
    case 0: return field == ModelBuilding::WheatField;
    case 1: return field == ModelBuilding::SoybeanField;
    case 2:
    case 4: return field == ModelBuilding::MilletField;
    default: return false;
    }
}

} // namespace openemperor::original
