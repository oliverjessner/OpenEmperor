#include "assets/Sg3AlphaDecoder.h"

#include <cstddef>
#include <cstdint>

namespace openemperor::assets {

AlphaSyntaxResult inspect_omega_alpha_syntax(std::span<const std::uint8_t> stream,
                                              std::uint16_t width, std::uint16_t height) {
    AlphaSyntaxResult result;
    result.image_pixel_count = static_cast<std::uint64_t>(width) * height;
    if (result.image_pixel_count == 0) {
        result.failure = "alpha destination dimensions must be nonzero";
        return result;
    }
    std::size_t input = 0;
    std::uint64_t cursor = 0;
    auto fail = [&](const char* reason) {
        result.failure = reason;
        result.bytes_consumed = input;
        result.pixels_advanced = cursor;
        result.final_pixel_cursor = cursor;
        return result;
    };
    while (input < stream.size()) {
        if (cursor == result.image_pixel_count) {
            return fail("alpha stream has trailing commands after the final pixel");
        }
        const std::uint8_t command = stream[input++];
        if (command == 255) {
            if (input == stream.size()) return fail("alpha skip command is missing its count byte");
            const std::uint8_t skip = stream[input++];
            if (skip > result.image_pixel_count - cursor) {
                return fail("alpha skip advances beyond image bounds");
            }
            cursor += skip;
            continue;
        }
        if (command > result.image_pixel_count - cursor) {
            return fail("alpha literal run advances beyond image bounds");
        }
        if (command > stream.size() - input) {
            return fail("alpha literal run is truncated");
        }
        input += command;
        cursor += command;
    }
    result.valid = true;
    result.bytes_consumed = input;
    result.pixels_advanced = cursor;
    result.final_pixel_cursor = cursor;
    return result;
}

void apply_omega_alpha_mask(std::span<const std::uint8_t> stream,
                            RgbaImage& destination) {
    if (destination.width == 0 || destination.height == 0) {
        throw Sg3DecodeError("alpha destination dimensions must be nonzero");
    }
    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(destination.width) * destination.height;
    if (pixel_count * 4U != destination.pixels.size()) {
        throw Sg3DecodeError("alpha destination pixel buffer does not match dimensions");
    }

    std::size_t input = 0;
    std::uint64_t cursor = 0;
    while (input < stream.size()) {
        if (cursor == pixel_count) {
            throw Sg3DecodeError("alpha stream has trailing commands after the final pixel");
        }
        const std::uint8_t command = stream[input++];
        if (command == 255) {
            if (input == stream.size()) {
                throw Sg3DecodeError("alpha skip command is missing its count byte");
            }
            const std::uint8_t skip = stream[input++];
            if (skip > pixel_count - cursor) {
                throw Sg3DecodeError("alpha skip advances beyond image bounds");
            }
            cursor += skip;
            continue;
        }
        const std::size_t count = command;
        if (count > pixel_count - cursor) {
            throw Sg3DecodeError("alpha literal run advances beyond image bounds");
        }
        if (count > stream.size() - input) {
            throw Sg3DecodeError("alpha literal run is truncated");
        }
        for (std::size_t index = 0; index < count; ++index) {
            const std::uint8_t value5 = stream[input++] & 0x1fU;
            const std::uint8_t alpha8 = static_cast<std::uint8_t>(
                (value5 << 3U) | (value5 >> 2U));
            destination.pixels[static_cast<std::size_t>(cursor * 4U + 3U)] = alpha8;
            ++cursor;
        }
    }
}

} // namespace openemperor::assets
