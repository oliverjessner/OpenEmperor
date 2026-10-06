#pragma once

#include "simulation/World.h"
#include "platform/AtomicReplace.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace openemperor::persistence {

struct SaveDocument {
    std::filesystem::path map_relative;
    std::string map_sha256;
    std::string buildable_sha256;
    simulation::WorldSnapshot world;
    bool migrated_from_schema1=false; // Diagnostic only; never serialized.
    std::uint32_t source_schema_version=0; // Diagnostic only; set when reading.
    // Schema 19 / City-v16 rule 3 only. The save contains no transit links.
    std::uint32_t map_permissions_policy_version=0;
    std::string map_permissions_sha256;
    // Prepared immutable input for manual/autosave validation; never serialized.
    std::shared_ptr<const simulation::MapPermissions> prepared_permissions;
};

// Fault injection is used only by tests; production always uses None.
using WriteFault=platform::AtomicWriteFault;

SaveDocument make_document(const std::filesystem::path& data_root,
                           const std::filesystem::path& map_relative,
                           const std::vector<std::uint8_t>& buildable,
                           const simulation::World& world);
SaveDocument read_save(const std::filesystem::path& path);
SaveDocument upgrade_city_v11_v2_to_v3(const SaveDocument& document);
SaveDocument upgrade_city_v11_v3_to_v4(const SaveDocument& document);
simulation::World restore_save(const SaveDocument& document,
                               const std::filesystem::path& data_root,
                               std::vector<std::uint8_t> buildable);
simulation::World restore_save(const SaveDocument& document,
                               const std::filesystem::path& data_root,
                               std::vector<std::uint8_t> buildable,
                               std::shared_ptr<const simulation::MapPermissions> permissions);
// Explicit domain/version/map-hash framing over canonical permission/topology
// bytes. Does not modify the original-map or legacy-buildability fingerprints.
std::string map_permissions_fingerprint(const simulation::MapPermissions& permissions,
                                        const std::string& map_sha256);
void write_save(const std::filesystem::path& path,const SaveDocument& document,
                const std::filesystem::path& data_root,
                const std::vector<std::uint8_t>& buildable,WriteFault fault=WriteFault::None);
void validate_save_target(const std::filesystem::path& path,const std::filesystem::path& data_root);

} // namespace openemperor::persistence
