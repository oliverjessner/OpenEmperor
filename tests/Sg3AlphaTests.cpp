#include "assets/Sg3AlphaDecoder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

using openemperor::assets::RgbaImage;
using Bytes = std::vector<std::uint8_t>;

RgbaImage image(std::uint16_t width, std::uint16_t height) {
    RgbaImage result;
    result.width = width;
    result.height = height;
    result.pixels.resize(static_cast<std::size_t>(width) * height * 4U);
    for (std::size_t pixel = 0; pixel < result.pixels.size() / 4U; ++pixel) {
        result.pixels[pixel * 4U] = 100;
        result.pixels[pixel * 4U + 1U] = 120;
        result.pixels[pixel * 4U + 2U] = 140;
        result.pixels[pixel * 4U + 3U] = 255;
    }
    return result;
}

template <typename Operation>
bool rejects(Operation operation) {
    try {
        operation();
        return false;
    } catch (const openemperor::assets::Sg3DecodeError&) {
        return true;
    }
}

bool run_checks() {
    using openemperor::assets::apply_omega_alpha_mask;
    const std::array<std::uint8_t, 6> five_values{0, 1, 15, 16, 30, 31};
    for (const std::uint8_t five : five_values) {
        auto result = image(1, 1);
        const Bytes stream{1, static_cast<std::uint8_t>(five | 0xe0U)};
        apply_omega_alpha_mask(stream, result);
        const std::uint8_t expected = static_cast<std::uint8_t>(
            (five << 3U) | (five >> 2U));
        if (result.pixels != Bytes{100, 120, 140, expected}) return false;
    }

    auto rows = image(3, 2);
    const Bytes row_stream{255, 2, 2, 0, 0x10, 255, 1, 1, 15};
    apply_omega_alpha_mask(row_stream, rows);
    const std::array<std::uint8_t, 6> expected_alpha{255, 255, 0, 132, 255, 123};
    for (std::size_t pixel = 0; pixel < expected_alpha.size(); ++pixel) {
        const std::size_t at = pixel * 4U;
        if (rows.pixels[at] != 100 || rows.pixels[at + 1] != 120 ||
            rows.pixels[at + 2] != 140 || rows.pixels[at + 3] != expected_alpha[pixel]) {
            return false;
        }
    }
    // A skipped RGB555-transparent pixel keeps its existing zero alpha and RGB.
    auto transparent = image(2, 1);
    transparent.pixels[0] = 0;
    transparent.pixels[1] = 0;
    transparent.pixels[2] = 0;
    transparent.pixels[3] = 0;
    apply_omega_alpha_mask(Bytes{255, 1, 1, 0x10}, transparent);
    if (transparent.pixels[0] != 0 || transparent.pixels[1] != 0 ||
        transparent.pixels[2] != 0 || transparent.pixels[3] != 0 ||
        transparent.pixels[7] != 132) return false;
    apply_omega_alpha_mask(Bytes{1, 31}, transparent);
    if (transparent.pixels[0] != 0 || transparent.pixels[1] != 0 ||
        transparent.pixels[2] != 0 || transparent.pixels[3] != 255) return false;

    auto wrong_size = image(1, 1);
    wrong_size.pixels.pop_back();
    auto zero_width = image(0, 1);
    auto zero_height = image(1, 0);
    auto one = image(1, 1);
    return rejects([&] { apply_omega_alpha_mask(Bytes{}, zero_width); }) &&
           rejects([&] { apply_omega_alpha_mask(Bytes{}, zero_height); }) &&
           rejects([&] { apply_omega_alpha_mask(Bytes{}, wrong_size); }) &&
           rejects([&] { apply_omega_alpha_mask(Bytes{255}, one); }) &&
           rejects([&] { apply_omega_alpha_mask(Bytes{2, 1}, one); }) &&
           rejects([&] { apply_omega_alpha_mask(Bytes{255, 2}, one); }) &&
           rejects([&] { apply_omega_alpha_mask(Bytes{2, 1, 2}, one); }) &&
           rejects([&] { apply_omega_alpha_mask(Bytes{255, 1, 0}, one); });
}

} // namespace

int main() {
    if (!run_checks()) {
        std::cerr << "SG3 alpha decoder checks failed\n";
        return 1;
    }
    return 0;
}
