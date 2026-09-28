#pragma once

#include "persistence/SandboxSave.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::persistence {

inline constexpr std::size_t recovery_checkpoint_limit=5;
inline constexpr std::uintmax_t recovery_storage_budget_bytes=256ULL*1024ULL*1024ULL;

struct RecoveryParent {
    std::string history_id;
    std::uint64_t sequence=0;
};

struct RecoveryEntry {
    std::string history_id;
    std::optional<RecoveryParent> parent;
    std::uint64_t sequence=0;
    bool start_point=false;
    std::filesystem::path path;
    std::string filename;
    std::string map;
    std::string profile;
    std::string created_local;
    std::uint64_t tick=0;
    std::uint32_t rule_version=0;
    std::uint32_t schema=0;
    int population=0;
    std::int64_t treasury=0;
    std::string error;
    bool loadable() const { return error.empty(); }
};

struct RecoveryHistory {
    std::string history_id;
    std::optional<RecoveryParent> parent;
    std::vector<RecoveryEntry> entries;
    std::string error;
};

struct RecoveryCatalog {
    std::vector<RecoveryHistory> histories;
    bool truncated=false;
    std::uintmax_t storage_bytes=0;
};

struct RecoveryWriteFaults {
    WriteFault save=WriteFault::None;
    platform::AtomicWriteFault metadata=platform::AtomicWriteFault::None;
};

class RecoveryStore {
public:
    RecoveryStore(std::filesystem::path app_root,std::filesystem::path data_root,
                  std::uintmax_t budget=recovery_storage_budget_bytes);
    std::string create_history(const SaveDocument& document,
                               const std::vector<std::uint8_t>& buildable,
                               std::optional<RecoveryParent> parent=std::nullopt,
                               RecoveryWriteFaults faults={});
    RecoveryEntry write_checkpoint(const std::string& history_id,
                                   const SaveDocument& document,
                                   const std::vector<std::uint8_t>& buildable,
                                   RecoveryWriteFaults faults={});
    RecoveryCatalog catalog() const;
    void delete_history(const std::string& history_id);
    const std::filesystem::path& root() const { return recovery_root_; }
private:
    std::filesystem::path app_root_,data_root_,recovery_root_;
    std::uintmax_t budget_;
};

} // namespace openemperor::persistence
