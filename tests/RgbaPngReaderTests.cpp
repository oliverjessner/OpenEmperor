#include "assets/RgbaPngEncoder.h"
#include "assets/RgbaPngReader.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

template <typename Operation>
bool rejects(Operation operation) {
    try {
        operation();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

bool run_checks() {
    openemperor::assets::RgbaImage original;
    original.width = 128;
    original.height = 128;
    original.pixels.resize(static_cast<std::size_t>(original.width) * original.height * 4);
    for (std::size_t index = 0; index < original.pixels.size(); ++index) {
        original.pixels[index] = static_cast<std::uint8_t>((index * 29U + 7U) & 0xffU);
    }
    const std::vector<std::uint8_t> png = openemperor::assets::encode_rgba_png(original);
    const auto decoded = openemperor::assets::decode_exported_png(png);
    if (decoded.width != original.width || decoded.height != original.height ||
        decoded.pixels != original.pixels) {
        return false;
    }
    auto corrupt = png;
    corrupt.back() ^= 1U;
    if (!rejects([&] { openemperor::assets::decode_exported_png(corrupt); })) {
        return false;
    }
    auto truncated = png;
    truncated.resize(truncated.size() - 1);
    return rejects([&] { openemperor::assets::decode_exported_png(truncated); });
}

} // namespace

int main() {
    if (!run_checks()) {
        std::cerr << "PNG preview checks failed\n";
        return 1;
    }
    return 0;
}
