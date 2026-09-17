#include "assets/Sg3PayloadLayout.h"

#include <limits>
#include <stdexcept>

namespace openemperor::assets {
namespace {

std::uint64_t add(std::uint64_t left, std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        throw std::overflow_error("SG3 payload offset arithmetic overflow");
    }
    return left + right;
}

std::uint64_t multiply(std::uint64_t left, std::uint64_t right) {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw std::overflow_error("SG3 payload offset arithmetic overflow");
    }
    return left * right;
}

} // namespace

const char* alpha_policy_name(AlphaPolicy policy) {
    switch (policy) {
    case AlphaPolicy::None: return "none";
    case AlphaPolicy::InternalV214SpriteContiguous: return "internal_v214_type256_contiguous";
    case AlphaPolicy::Unverified: return "unverified";
    case AlphaPolicy::IgnoredDiagnostic: return "ignored_diagnostic";
    case AlphaPolicy::DiagnosticSpec: return "diagnostic_spec";
    case AlphaPolicy::DiagnosticContiguous: return "diagnostic_contiguous";
    case AlphaPolicy::DiagnosticLegacy: return "diagnostic_legacy";
    }
    return "unknown";
}

Sg3PayloadLayout sg3_payload_layout(std::uint32_t version, const Sg3Image& image,
                                    bool ignore_alpha,
                                    std::optional<AlphaAddressing> diagnostic_addressing) {
    Sg3PayloadLayout layout;
    layout.color = {image.data_offset, image.data_length};
    if (diagnostic_addressing && ignore_alpha) {
        throw std::invalid_argument("--ignore-alpha conflicts with --alpha-addressing");
    }
    if (image.alpha_length == 0) {
        if (diagnostic_addressing) {
            throw std::invalid_argument("diagnostic alpha addressing requires an alpha-bearing image");
        }
        return layout;
    }
    if (ignore_alpha) {
        layout.alpha_policy = AlphaPolicy::IgnoredDiagnostic;
        layout.alpha_profile_supported = false;
        return layout;
    }
    if (diagnostic_addressing) {
        if (*diagnostic_addressing == AlphaAddressing::Spec) {
            layout.alpha_policy = AlphaPolicy::DiagnosticSpec;
            layout.alpha = PayloadRange{image.alpha_offset, image.alpha_length};
        } else if (*diagnostic_addressing == AlphaAddressing::Contiguous) {
            layout.alpha_policy = AlphaPolicy::DiagnosticContiguous;
            layout.alpha = PayloadRange{add(layout.color.offset, layout.color.length),
                                        image.alpha_length};
        } else {
            layout.alpha_policy = AlphaPolicy::DiagnosticLegacy;
            if (layout.color.offset < image.external_flag) {
                throw std::invalid_argument("diagnostic legacy color offset is negative");
            }
            layout.color.offset -= image.external_flag;
            layout.alpha = PayloadRange{add(layout.color.offset, layout.color.length),
                                        image.alpha_length};
        }
        layout.alpha_profile_supported = false;
        return layout;
    }
    const std::uint64_t data_offset = image.data_offset;
    const std::uint64_t data_length = image.data_length;
    const std::uint64_t expected_raw = add(data_offset, multiply(data_length, 2U));
    if (version == 214 && image.image_type == 256 && image.external_flag == 0 &&
        expected_raw == image.alpha_offset) {
        layout.alpha_policy = AlphaPolicy::InternalV214SpriteContiguous;
        layout.alpha = PayloadRange{add(data_offset, data_length), image.alpha_length};
    } else {
        layout.alpha_policy = AlphaPolicy::Unverified;
        layout.alpha_profile_supported = false;
    }
    return layout;
}

} // namespace openemperor::assets
