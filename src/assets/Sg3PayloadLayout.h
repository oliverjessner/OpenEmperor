#pragma once

#include "assets/Sg3Archive.h"

#include <cstdint>
#include <optional>

namespace openemperor::assets {

enum class AlphaAddressing { Spec, Contiguous, Legacy };
enum class AlphaPolicy {
    None,
    InternalV214SpriteContiguous,
    Unverified,
    IgnoredDiagnostic,
    DiagnosticSpec,
    DiagnosticContiguous,
    DiagnosticLegacy,
};

const char* alpha_policy_name(AlphaPolicy policy);

struct PayloadRange {
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
};

struct Sg3PayloadLayout {
    PayloadRange color;
    std::optional<PayloadRange> alpha;
    AlphaPolicy alpha_policy = AlphaPolicy::None;
    bool alpha_profile_supported = true;
};

// Pure metadata calculation. Does not alter raw fields or inspect a source file.
// Throws on arithmetic overflow or a negative diagnostic LEGACY start.
Sg3PayloadLayout sg3_payload_layout(std::uint32_t version, const Sg3Image& image,
    bool ignore_alpha = false,
    std::optional<AlphaAddressing> diagnostic_addressing = std::nullopt);

} // namespace openemperor::assets
