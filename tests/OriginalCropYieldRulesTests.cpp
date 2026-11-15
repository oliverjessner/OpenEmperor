#include "simulation/original/CropRules.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

namespace original = openemperor::original;
using original::ModelBuilding;

std::size_t checks = 0;

void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

constexpr std::array fields{
    ModelBuilding::WheatField, ModelBuilding::MilletField, ModelBuilding::RiceField,
    ModelBuilding::CabbageField, ModelBuilding::SoybeanField
};

// Rows are Food195..199 and columns the literal raw selectors0..4.
// Source dispatch bounds exclude Rice/Cabbage from every admitted case;
// nearby byte-table entries beyond those bounds are not selector evidence.
constexpr std::array<std::array<bool, 5>, 5> qualified{{
    {true, false, false, false, false},
    {false, false, true, false, true},
    {false, false, false, false, false},
    {false, false, false, false, false},
    {false, true, false, false, false}
}};

void exact_crop_and_event_operands() {
    for (std::size_t crop = 0; crop < fields.size(); ++crop) {
        for (std::uint32_t selector = 0; selector < 5; ++selector) {
            for (unsigned raw_enabled = 0; raw_enabled <= 255; ++raw_enabled) {
                const auto enabled = static_cast<std::uint8_t>(raw_enabled);
                check(original::crop_doubled_base(fields[crop], enabled, 3, selector) ==
                          (raw_enabled != 0 && qualified[crop][selector]),
                      "Doubled-base predicate changed a literal Food crop/selector/enable-byte case");
                for (const auto state : {std::uint32_t{0}, std::uint32_t{2}, std::uint32_t{4},
                                          std::numeric_limits<std::uint32_t>::max()})
                    check(!original::crop_doubled_base(fields[crop], enabled, state, selector),
                          "An unqualified raw event state admitted doubled base yield");
            }
        }
        for (const auto selector : {std::uint32_t{5}, std::uint32_t{255}, std::uint32_t{65535},
                                   std::uint32_t{0x7fffffff}, std::uint32_t{0x80000000},
                                   std::numeric_limits<std::uint32_t>::max()})
            check(!original::crop_doubled_base(fields[crop], 255, 3, selector),
                  "An out-of-range unsigned raw selector admitted doubled base yield");
    }
}

void bounded_base_arithmetic_integration() {
    for (std::size_t crop = 0; crop < fields.size(); ++crop)
        for (std::uint32_t selector = 0; selector < 5; ++selector) {
            const bool doubled = original::crop_doubled_base(fields[crop], 1, 3, selector);
            for (std::uint16_t quantity = 0; quantity <= 10000; ++quantity)
                check(original::raw_base_yield(quantity, doubled) ==
                          quantity / (qualified[crop][selector] ? 50 : 100),
                      "Crop-qualified event changed bounded original integer base arithmetic");
        }
    check(original::raw_base_yield(50, original::crop_doubled_base(ModelBuilding::SoybeanField, 1, 3, 1)) == 1 &&
              original::raw_base_yield(50, original::crop_doubled_base(ModelBuilding::RiceField, 1, 3, 1)) == 0 &&
              original::raw_base_yield(50, original::crop_doubled_base(ModelBuilding::CabbageField, 1, 3, 0)) == 0,
          "Rice or Cabbage inherited a different crop's doubled-base event");
}

void unsupported_models_stay_rejected() {
    for (const auto value : {0U, 26U, 27U, 28U, 29U, 193U, 194U, 200U, 65535U})
        for (const auto enabled : {std::uint8_t{0}, std::uint8_t{1}}) {
            bool refused = false;
            try {
                (void)original::crop_doubled_base(static_cast<ModelBuilding>(value), enabled, 3, 0);
            } catch (const std::out_of_range&) { refused = true; }
            check(refused, "An unsupported plant model reached the Food doubled-base API");
        }
}

} // namespace

int main() {
    try {
        exact_crop_and_event_operands();
        bounded_base_arithmetic_integration();
        unsupported_models_stay_rejected();
        std::cout << "Original crop yield input rules " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
