#include "maps/MapRulesCheck.h"

#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"
#include "maps/LandscapeProvenance.h"
#include "maps/MapCatalog.h"
#include "maps/SandboxPlacement.h"

#include <openssl/evp.h>

#include <array>
#include <memory>
#include <span>
#include <stdexcept>

namespace openemperor::maps {
namespace {
std::string content_sha256(std::span<const std::uint8_t> bytes) {
    std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> context{EVP_MD_CTX_new(),EVP_MD_CTX_free};
    if (!context || EVP_DigestInit_ex(context.get(),EVP_sha256(),nullptr)!=1 ||
        EVP_DigestUpdate(context.get(),bytes.data(),bytes.size())!=1)
        throw std::runtime_error("map check SHA-256 initialization/calculation failed");
    std::array<unsigned char,EVP_MAX_MD_SIZE> output{};
    unsigned length=0;
    if (EVP_DigestFinal_ex(context.get(),output.data(),&length)!=1 || length!=32)
        throw std::runtime_error("map check SHA-256 finalization failed");
    constexpr char digits[]="0123456789abcdef";
    std::string result;result.reserve(64);
    for (unsigned i=0;i<length;++i) {
        result.push_back(digits[output[i]>>4U]);result.push_back(digits[output[i]&15U]);
    }
    return result;
}
void require_selection(const MapRulesRequest& request) {
    if (static_cast<unsigned>(request.profile)>static_cast<unsigned>(simulation::RulesProfile::CityV16) ||
        !simulation::rule_version_supported(request.profile,request.rule_version))
        throw std::invalid_argument("map check requires a supported explicit profile/rule version");
    if (request.policy_version!=map_rules_policy_version(request.profile,request.rule_version))
        throw std::invalid_argument("map check policy does not match the selected profile/rule version");
    if (request.canonical_data_root.empty() || request.map_relative.empty() || request.map_relative.is_absolute())
        throw std::invalid_argument("map check requires a canonical data root and a relative map path");
    for (const auto& part:request.map_relative)
        if (part=="..") throw std::invalid_argument("map check relative path escapes the data root");
    const auto canonical=std::filesystem::canonical(request.canonical_data_root);
    if (canonical!=request.canonical_data_root)
        throw std::invalid_argument("map check data root is not canonical");
}
} // namespace

std::uint32_t map_rules_policy_version(simulation::RulesProfile profile,std::uint32_t rule_version) {
    return profile==simulation::RulesProfile::CityV16 && rule_version==3 ?
        simulation::kMapPermissionsPolicyVersion:0;
}

std::string map_input_sha256(const EmperorContainer& container) {
    return content_sha256(container.physical_bytes());
}

MapRulesResult check_map_rules(const MapRulesRequest& request) {
    MapRulesResult result;result.request=request;
    try {
        require_selection(request);
        const auto path=resolve_map_path(request.canonical_data_root,request.map_relative);
        const auto container=EmperorContainer::open(path);
        result.physical_bytes=container.physical_size();
        result.input_sha256=map_input_sha256(container);
        if (container.multipart() || container.parts().size()!=1)
            throw UnsupportedMapProfile("map check requires one standalone original map part");
        const auto map=read_emperor_map(container,0); // Includes every compressed-block integrity check.
        const MapGeometry geometry{map.declared_map_size};
        if (!geometry.supported)
            throw SandboxMapRulesUnsupported("unsupported map geometry");
        if (request.policy_version!=0) {
            const auto entities=read_original_map_entities(container,0);
            const auto heights=container.read_range(0,landscape_height_offset,
                std::uint64_t(stored_grid_width)*stored_grid_height);
            validate_sandbox_original_map_rules(map,geometry,entities,heights,request.policy_version);
        }
        // Older profiles never imported the original occupancy/gate authority.
        // Their renderer-derived building mask remains a later start condition.
        result.status=MapRulesStatus::MapRulesChecked;
        result.reason="Map rules checked.";
        result.detail=request.policy_version ?
            "Original map, complete manager, occupancy, gate geometry and required heights checked. "
            "Assets, legacy building permissions, starter placement and session start are still checked when starting.":
            "Original map and supported geometry checked for these legacy rules. "
            "Assets, legacy building permissions, starter placement and session start are still checked when starting.";
    } catch (const OriginalEntityUnsupported& error) {
        result.status=MapRulesStatus::UnsupportedForRules;
        result.reason="Original object class or schema is not yet supported with these rules.";
        result.detail=error.what();
    } catch (const SandboxMapRulesUnsupported& error) {
        result.status=MapRulesStatus::UnsupportedForRules;
        result.reason="Original occupancy or gate conditions are not yet supported with these rules.";
        result.detail=error.what();
    } catch (const std::exception& error) {
        result.status=MapRulesStatus::InputError;
        result.reason="Map could not be read or validated. Check the data files and retry.";
        result.detail=error.what();
    }
    return result;
}

} // namespace openemperor::maps
