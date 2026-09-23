#include "assets/Sg3ShadowComposition.h"

#include <limits>

namespace openemperor::assets {

std::size_t prepare_omega_shadow_composition(const Sg3Image& metadata,
                                             RgbaImage& image) {
    if (metadata.shadow_marker_flag == 0) return 0;
    if (classify_sg3_image_type(metadata.image_type) != Sg3ImageKind::Sprite) {
        throw Sg3DecodeError("SG3 shadow-marker flag is verified only for Omega sprites");
    }
    if (metadata.alpha_length != 0) {
        throw Sg3DecodeError("SG3 shadow-marker composition with a separate alpha stream is unverified");
    }
    if (image.width <= 0 || image.height <= 0) {
        throw Sg3DecodeError("SG3 shadow composition image dimensions are invalid");
    }
    const auto pixel_count = static_cast<std::uint64_t>(image.width) * image.height;
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4U) {
        throw Sg3DecodeError("SG3 shadow composition image dimensions overflow the pixel buffer size");
    }
    const std::size_t expected = static_cast<std::size_t>(pixel_count) * 4U;
    if (image.pixels.size() != expected) {
        throw Sg3DecodeError("SG3 shadow composition image dimensions do not match its pixels");
    }
    std::size_t changed = 0;
    for (std::size_t offset = 0; offset < image.pixels.size(); offset += 4U) {
        if (image.pixels[offset] == 255 && image.pixels[offset + 1] == 0 &&
            image.pixels[offset + 2] == 0 && image.pixels[offset + 3] == 255) {
            image.pixels[offset] = 0;
            image.pixels[offset + 3] = 128;
            ++changed;
        }
    }
    return changed;
}

} // namespace openemperor::assets
