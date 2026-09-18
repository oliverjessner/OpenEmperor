#pragma once

#include <filesystem>
#include <functional>
#include <string_view>

namespace openemperor::platform {
enum class AtomicWriteFault { None, BeforeWrite, AfterPartialWrite, BeforeRename };

// Checked local-file replacement. The callback revalidates the target just
// before commit. No power-loss durability guarantee is implied by rename.
void atomic_replace(const std::filesystem::path& target,std::string_view bytes,
                    const std::function<void()>& before_rename,
                    AtomicWriteFault fault=AtomicWriteFault::None);
} // namespace openemperor::platform
