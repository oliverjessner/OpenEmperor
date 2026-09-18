#pragma once

#include "simulation/World.h"
#include "platform/AtomicReplace.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace openemperor::persistence {

struct SaveDocument {
    std::filesystem::path map_relative;
    std::string map_sha256;
    std::string buildable_sha256;
    simulation::WorldSnapshot world;
    bool migrated_from_schema1=false; // Diagnostic only; never serialized.
};

// Fault injection is used only by tests; production always uses None.
using WriteFault=platform::AtomicWriteFault;

SaveDocument make_document(const std::filesystem::path& data_root,
                           const std::filesystem::path& map_relative,
                           const std::vector<std::uint8_t>& buildable,
                           const simulation::World& world);
SaveDocument read_save(const std::filesystem::path& path);
simulation::World restore_save(const SaveDocument& document,
                               const std::filesystem::path& data_root,
                               std::vector<std::uint8_t> buildable);
void write_save(const std::filesystem::path& path,const SaveDocument& document,
                const std::filesystem::path& data_root,
                const std::vector<std::uint8_t>& buildable,WriteFault fault=WriteFault::None);
void validate_save_target(const std::filesystem::path& path,const std::filesystem::path& data_root);

} // namespace openemperor::persistence
