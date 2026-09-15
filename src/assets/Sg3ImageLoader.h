#pragma once

#include "assets/Sg3Archive.h"
#include "assets/Sg3RgbaDecoder.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace openemperor::assets {

class Sg3LoadError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Sg3ImageRequest {
    std::filesystem::path archive_path;
    std::uint32_t image_index = 0;
};

enum class Sg3BitmapStatus {
    Resolved,
    UnresolvedName,
    UnsafeRelativeName,
    InvalidGroup,
    UnknownExternalFlag,
};

struct Sg3BitmapLocation {
    std::filesystem::path path;
    std::string ref;
    Sg3BitmapStatus status = Sg3BitmapStatus::UnresolvedName;
};

struct LoadedSg3Image {
    RgbaImage rgba;
    Sg3BitmapLocation bitmap;
};

// Only the declared SG3 metadata table is read. The .555 payload is not read.
Sg3Archive read_sg3_archive(const std::filesystem::path& archive_path);

const char* sg3_bitmap_status_name(Sg3BitmapStatus status);
Sg3BitmapLocation resolve_sg3_group_bitmap(const std::filesystem::path& archive_path,
                                            const Sg3Group& group, std::size_t group_index);
Sg3BitmapLocation resolve_sg3_image_bitmap(const std::filesystem::path& archive_path,
                                            const Sg3Archive& archive, const Sg3Image& image);

// Reads exactly one documented uncompressed regular image from its .555 file.
LoadedSg3Image load_sg3_image_with_source(const Sg3ImageRequest& request);
RgbaImage load_sg3_image(const Sg3ImageRequest& request);

} // namespace openemperor::assets
