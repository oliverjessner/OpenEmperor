#include "assets/Sg3OmegaDecoder.h"

#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;
const Bytes transparent{0, 0, 0, 0};
const Bytes red{255, 0, 0, 255};
const Bytes green{0, 255, 0, 255};
const Bytes blue{0, 0, 255, 255};

Bytes pixels(std::initializer_list<Bytes> colors) {
    Bytes result;
    for (const Bytes& color : colors) result.insert(result.end(), color.begin(), color.end());
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
    using openemperor::assets::decode_omega_color_rgba;
    if (decode_omega_color_rgba(Bytes{1, 0x1f, 0x00}, 1, 1).pixels != pixels({red})) return false;
    if (decode_omega_color_rgba(Bytes{2, 0x1f, 0x00, 0xe0, 0x03}, 2, 1).pixels !=
        pixels({red, green})) return false;
    if (decode_omega_color_rgba(Bytes{255, 1, 1, 0x00, 0x7c}, 3, 1).pixels !=
        pixels({transparent, blue, transparent})) return false;
    if (decode_omega_color_rgba(Bytes{255, 1, 2, 0x1f, 0x00, 0xe0, 0x03}, 2, 2).pixels !=
        pixels({transparent, red, green, transparent})) return false;
    if (decode_omega_color_rgba(Bytes{1, 0x1f, 0x00, 255, 2, 2,
                                      0xe0, 0x03, 0x00, 0x7c}, 3, 2).pixels !=
        pixels({red, transparent, transparent, green, blue, transparent})) return false;
    if (decode_omega_color_rgba(Bytes{}, 2, 2).pixels !=
        pixels({transparent, transparent, transparent, transparent})) return false;
    if (decode_omega_color_rgba(Bytes{255, 4}, 2, 2).pixels !=
        pixels({transparent, transparent, transparent, transparent})) return false;
    if (decode_omega_color_rgba(Bytes{1, 0x1f, 0xf8}, 1, 1).pixels != pixels({transparent})) return false;

    if (!rejects([&] { decode_omega_color_rgba(Bytes{255}, 1, 1); }) ||
        !rejects([&] { decode_omega_color_rgba(Bytes{2, 0x1f, 0x00}, 2, 1); }) ||
        !rejects([&] { decode_omega_color_rgba(Bytes{255, 2}, 1, 1); }) ||
        !rejects([&] { decode_omega_color_rgba(Bytes{2, 0x1f, 0x00, 0xe0, 0x03}, 1, 1); }) ||
        !rejects([&] { decode_omega_color_rgba(Bytes{1, 0x1f, 0x00, 0}, 1, 1); }) ||
        !rejects([&] { decode_omega_color_rgba(Bytes{}, 0, 1); }) ||
        !rejects([&] { decode_omega_color_rgba(Bytes{}, 1, 0); })) return false;
    return true;
}

} // namespace

int main() {
    if (!run_checks()) {
        std::cerr << "SG3 Omega decoder checks failed\n";
        return 1;
    }
    return 0;
}
