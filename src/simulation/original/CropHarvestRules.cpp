#include "simulation/original/CropHarvestRules.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace openemperor::original {
namespace {

// Exact nonnegative value significand*2^exponent. The supported instruction
// chain needs at most56 product bits: base<=200, binary32 operands<=24 bits,
// and a signed-int32-safe reload before the final global product.
struct Binary {
    std::uint64_t significand = 0;
    int exponent = 0;
};

enum class Rounding : std::uint8_t { NearestEven, Down, Up, TowardZero };

Binary normalized(Binary value) {
    if (!value.significand) return {};
    const auto zeros = std::countr_zero(value.significand);
    value.significand >>= zeros;
    value.exponent += zeros;
    return value;
}

Binary from_binary32(std::uint32_t bits) {
    const auto exponent = (bits >> 23U) & 0xffU;
    const auto fraction = bits & 0x7fffffU;
    if (exponent == 0xffU || ((bits >> 31U) != 0 && (exponent != 0 || fraction != 0)))
        throw std::invalid_argument("crop yield factor must be finite and nonnegative");
    if (exponent == 0) return normalized({fraction, -149});
    return normalized({0x800000U | fraction, static_cast<int>(exponent) - 150});
}

Binary rounded(Binary value, unsigned precision, Rounding mode) {
    const auto width = static_cast<unsigned>(std::bit_width(value.significand));
    if (width <= precision) return normalized(value);
    const auto shift = width - precision;
    const auto remainder = value.significand & ((std::uint64_t{1} << shift) - 1U);
    auto retained = value.significand >> shift;
    const auto half = std::uint64_t{1} << (shift - 1U);
    if ((mode == Rounding::Up && remainder != 0) ||
        (mode == Rounding::NearestEven &&
         (remainder > half || (remainder == half && (retained & 1U) != 0))))
        ++retained;
    return normalized({retained, value.exponent + static_cast<int>(shift)});
}

Binary multiplied(Binary left, Binary right, unsigned precision, Rounding mode) {
    if (right.significand != 0 &&
        left.significand > std::numeric_limits<std::uint64_t>::max() / right.significand)
        throw std::overflow_error("crop yield exact product exceeds supported arithmetic");
    return rounded({left.significand * right.significand, left.exponent + right.exponent},
                   precision, mode);
}

Binary added(Binary left, Binary right, unsigned precision, Rounding mode) {
    if (!left.significand) return rounded(right, precision, mode);
    if (!right.significand) return rounded(left, precision, mode);
    const auto exponent = std::min(left.exponent, right.exponent);
    const auto align = [exponent](Binary value) {
        const auto shift = static_cast<unsigned>(value.exponent - exponent);
        if (shift >= 64 || value.significand > (std::numeric_limits<std::uint64_t>::max() >> shift))
            throw std::overflow_error("crop fertility alignment exceeds supported arithmetic");
        return value.significand << shift;
    };
    const auto a = align(left), b = align(right);
    if (a > std::numeric_limits<std::uint64_t>::max() - b)
        throw std::overflow_error("crop fertility sum exceeds supported arithmetic");
    return rounded({a + b, exponent}, precision, mode);
}

std::uint32_t stored_binary32(Binary value, Rounding mode) {
    value = rounded(value, 24, mode);
    if (!value.significand) return 0;
    const auto width = static_cast<unsigned>(std::bit_width(value.significand));
    const auto exponent = value.exponent + static_cast<int>(width) - 1;
    // These are only the byte*0.01 and optional+0.35 stores, so their nonzero
    // values are normal binary32 numbers. Keep an explicit checked boundary.
    if (exponent < -126 || exponent > 127)
        throw std::overflow_error("crop fertility store exceeds supported normal binary32");
    const auto significand = value.significand << (24U - width);
    return (static_cast<std::uint32_t>(exponent + 127) << 23U) |
           (static_cast<std::uint32_t>(significand) & 0x7fffffU);
}

std::uint32_t truncated_signed32(Binary value) {
    std::uint64_t integer = 0;
    if (value.significand != 0) {
        if (value.exponent >= 0) {
            if (value.exponent > 30 ||
                value.significand > (std::uint64_t{0x7fffffffU} >> value.exponent))
                throw std::overflow_error("crop yield conversion exceeds supported signed-int32 domain");
            integer = value.significand << value.exponent;
        } else {
            const auto shift = static_cast<unsigned>(-value.exponent);
            integer = shift >= 64 ? 0 : value.significand >> shift;
        }
    }
    if (integer > 0x7fffffffU)
        throw std::overflow_error("crop yield conversion exceeds supported signed-int32 domain");
    return static_cast<std::uint32_t>(integer);
}

} // namespace

CropHarvestYield calculate_crop_harvest_yield(
    ModelBuilding field, std::uint16_t quantity, const CropHarvestContext& context) {
    (void)crop_rule(field);
    if ((context.x87_control_word & 0x003fU) != 0x003fU)
        throw std::invalid_argument("crop yield requires explicitly masked native FP exceptions");
    const auto precision_bits = (context.x87_control_word >> 8U) & 3U;
    if (precision_bits == 1)
        throw std::invalid_argument("reserved native x87 crop precision is unsupported");
    const unsigned precision = precision_bits == 0 ? 24U : precision_bits == 2 ? 53U : 64U;
    const auto rounding = static_cast<Rounding>((context.x87_control_word >> 10U) & 3U);
    const auto base = raw_base_yield(quantity, crop_doubled_base(field,
        context.event_enabled, context.event_state, context.event_crop_selector));
    const auto region = from_binary32(context.region_factor_binary32);
    auto fertility = multiplied({context.runtime_fertility, 0}, from_binary32(0x3c23d70aU),
                               precision, rounding);
    auto fertility_bits = stored_binary32(fertility, rounding);
    if (context.irrigated) {
        fertility = added(from_binary32(fertility_bits), from_binary32(0x3eb33333U),
                          precision, rounding);
        fertility_bits = stored_binary32(fertility, rounding);
    }
    const auto first = truncated_signed32(multiplied(
        multiplied({base, 0}, region, precision, rounding),
        from_binary32(fertility_bits), precision, rounding));
    auto final = first;
    if (context.raw_date_operand > -300)
        final = truncated_signed32(multiplied({first, 0},
            from_binary32(context.global_factor_binary32), precision, rounding));
    return {base, fertility_bits, first, final, static_cast<std::uint16_t>(final & 0xffffU)};
}

CropHarvestFieldState apply_crop_tending_callback(
    ModelBuilding field, CropHarvestFieldState state) {
    (void)crop_rule(field);
    if (state.quantity > 10000)
        throw std::out_of_range("crop tending callback requires quantity0..10000");
    state.work_word = complete_crop_tending(state.work_word);
    state.body_counter = 0;
    state.assigned_actor_word = 0;
    return state;
}

CropHarvestCallback apply_crop_harvest_callback(
    ModelBuilding field, std::uint8_t slot, CropHarvestFieldState state,
    std::uint16_t actual_actor_yield_word, const CropHarvestContext& context) {
    (void)crop_rule(field);
    if (slot > 11 || state.quantity > 10000)
        throw std::out_of_range("crop harvest callback requires slot0..11 and quantity0..10000");
    CropHarvestCallback result{state, actual_actor_yield_word, std::nullopt, false, false};
    result.field.work_word = static_cast<std::uint16_t>(state.work_word | 0x4000U);
    result.field.body_counter = 0;
    if ((state.work_word & 0x8000U) == 0) return result;

    result.yield = calculate_crop_harvest_yield(field, state.quantity, context);
    const auto retained = result.yield->retained_yield_word;
    const auto sum = static_cast<std::uint32_t>(actual_actor_yield_word) + retained;
    result.numeric_word_narrowed = result.yield->numeric_yield > 0xffffU;
    result.actor_word_wrapped = sum > 0xffffU;
    result.actor_yield_word = static_cast<std::uint16_t>(sum & 0xffffU);
    result.field.work_word = static_cast<std::uint16_t>(result.field.work_word & 0x7fffU);
    result.field.history[slot] = retained;
    result.field.quantity = 0;
    result.field.assigned_actor_word = 0;
    return result;
}

} // namespace openemperor::original
