#include "simulation/original/CropHarvestRules.h"
#include "simulation/original/CropLifecycleRules.h"

#include <array>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace original = openemperor::original;
using original::ModelBuilding;
std::uint64_t checks = 0;

void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

template<class Exception, class Operation>
void refuses(Operation operation, std::string_view message) {
    try { operation(); }
    catch (const Exception&) { ++checks; return; }
    throw std::runtime_error(std::string(message));
}

original::CropHarvestContext context(std::uint8_t fertility = 100,
                                     std::uint16_t cw = 0x027f) {
    original::CropHarvestContext value;
    value.runtime_fertility = fertility;
    value.region_factor_binary32 = 0x3f800000U;
    value.raw_date_operand = -300;
    value.global_factor_binary32 = 0x3f933333U;
    value.x87_control_word = cw;
    return value;
}

constexpr std::array fields{
    ModelBuilding::WheatField, ModelBuilding::MilletField,
    ModelBuilding::RiceField, ModelBuilding::CabbageField, ModelBuilding::SoybeanField
};

void literal_rounding_vectors() {
    // Independent exact-rational instruction-by-instruction expectations for
    // PC24/53/64, each with RC nearest/down/up/toward-zero. No host FP oracle.
    struct Expected { std::uint32_t stored, first, final; };
    constexpr std::array<Expected, 12> dry{{
        {0x3f266666U,65,65}, {0x3f266666U,64,64},
        {0x3f266667U,65,65}, {0x3f266666U,64,64},
        {0x3f266666U,64,64}, {0x3f266666U,64,64},
        {0x3f266667U,65,65}, {0x3f266666U,64,64},
        {0x3f266666U,64,64}, {0x3f266666U,64,64},
        {0x3f266667U,65,65}, {0x3f266666U,64,64}
    }};
    constexpr std::array<Expected, 12> irrigated_global{{
        {0x3f800000U,100,115}, {0x3f7fffffU,99,113},
        {0x3f800001U,100,115}, {0x3f7fffffU,99,113},
        {0x3f800000U,100,114}, {0x3f7fffffU,99,113},
        {0x3f800001U,100,114}, {0x3f7fffffU,99,113},
        {0x3f800000U,100,114}, {0x3f7fffffU,99,113},
        {0x3f800001U,100,114}, {0x3f7fffffU,99,113}
    }};
    std::size_t index = 0;
    for (const auto pc : {0U, 2U, 3U}) {
        for (unsigned rc = 0; rc < 4; ++rc, ++index) {
            auto input = context(65, static_cast<std::uint16_t>(0x3fU | (pc << 8U) | (rc << 10U)));
            const auto expected = dry[index];
            check(original::calculate_crop_harvest_yield(ModelBuilding::WheatField,10000,input) ==
                    original::CropHarvestYield{100,expected.stored,expected.first,expected.final,
                        static_cast<std::uint16_t>(expected.final)},
                  "literal dry fertility/precision/rounding vector");
            input.irrigated = true;
            input.raw_date_operand = -299;
            const auto wet = irrigated_global[index];
            check(original::calculate_crop_harvest_yield(ModelBuilding::RiceField,10000,input) ==
                    original::CropHarvestYield{100,wet.stored,wet.first,wet.final,
                        static_cast<std::uint16_t>(wet.final)},
                  "literal irrigation store/global reload/rounding vector");
        }
    }

    auto dry_global = context(65);
    dry_global.raw_date_operand = -299;
    check(original::calculate_crop_harvest_yield(ModelBuilding::CabbageField,10000,dry_global) ==
            original::CropHarvestYield{100,0x3f266666U,64,73,73},
          "first integer truncation precedes the global multiplier");
    auto zero = context(255);
    zero.irrigated = true;
    zero.raw_date_operand = 0;
    zero.region_factor_binary32 = 0x40000000U;
    zero.event_enabled = 1; zero.event_state = 3; zero.event_crop_selector = 0;
    check(original::calculate_crop_harvest_yield(ModelBuilding::WheatField,0,zero) ==
            original::CropHarvestYield{0,0x40399999U,0,0,0},
          "zero quantity cannot produce raw yield despite all modifiers");

    auto word = context();
    word.region_factor_binary32 = 0x47800000U;
    check(original::calculate_crop_harvest_yield(ModelBuilding::CabbageField,100,word) ==
            original::CropHarvestYield{1,0x3f800000U,65536,65536,0},
          "numeric65536 is separate from source retained WORD0");
    word.x87_control_word = 0x067f;
    check(original::calculate_crop_harvest_yield(ModelBuilding::CabbageField,100,word) ==
            original::CropHarvestYield{1,0x3f7fffffU,65535,65535,65535},
          "downward fertility store remains below a WORD boundary");
    auto largest = context();
    largest.region_factor_binary32 = 0x4effffffU;
    check(original::calculate_crop_harvest_yield(ModelBuilding::SoybeanField,100,largest) ==
            original::CropHarvestYield{1,0x3f800000U,2147483520U,2147483520U,65408},
          "largest binary32 integer below signed32 boundary stays exact");
    auto tiny = context(255,0x037f);
    tiny.irrigated = true; tiny.region_factor_binary32 = 1;
    check(original::calculate_crop_harvest_yield(ModelBuilding::SoybeanField,10000,tiny) ==
            original::CropHarvestYield{100,0x40399999U,0,0,0},
          "literal subnormal region coefficient retains no integer yield");
    auto cutoff = context();
    check(original::calculate_crop_harvest_yield(ModelBuilding::MilletField,10000,cutoff) ==
            original::CropHarvestYield{100,0x3f800000U,100,100,100},
          "literal raw date-300 excludes the global stage");
    cutoff.raw_date_operand = -299;
    check(original::calculate_crop_harvest_yield(ModelBuilding::MilletField,10000,cutoff) ==
            original::CropHarvestYield{100,0x3f800000U,100,114,114},
          "literal raw date-299 includes its separately rounded global stage");
}

void exact_unit_factor_and_event_identity() {
    for (const auto field : fields) {
        for (const auto pc : {0U,2U,3U}) {
            for (unsigned rc = 0; rc < 4; ++rc) {
                auto input = context(100,static_cast<std::uint16_t>(0x3fU | (pc << 8U) | (rc << 10U)));
                for (std::uint32_t quantity = 0; quantity <= 10000; ++quantity) {
                    const auto result = original::calculate_crop_harvest_yield(
                        field,static_cast<std::uint16_t>(quantity),input);
                    const auto base = quantity / 100U;
                    const auto expected = base != 0 && (rc == 1 || rc == 3) ? base - 1U : base;
                    check(result.integer_base == base && result.first_integer_yield == expected &&
                          result.numeric_yield == expected && result.retained_yield_word == expected,
                          "unit region and literal fertility100 bounds across every quantity");
                }
            }
        }
        for (unsigned selector = 0; selector <= 4; ++selector) {
            auto input = context();
            input.event_enabled = 1; input.event_state = 3; input.event_crop_selector = selector;
            const bool qualified = (field == ModelBuilding::WheatField && selector == 0) ||
                (field == ModelBuilding::MilletField && (selector == 2 || selector == 4)) ||
                (field == ModelBuilding::SoybeanField && selector == 1);
            check(original::calculate_crop_harvest_yield(field,10000,input).numeric_yield ==
                    (qualified ? 200U : 100U),
                  "event base remains crop-qualified rather than a global doubling flag");
        }
    }
}

void independent_rational_differential() {
    // These authored inputs and checksum come from a separate Python Fraction
    // reference. It rounds the exact rational after each source instruction;
    // production instead carries bounded uint64 significands and exponents.
    std::uint32_t state = 0x026c0195U;
    const auto draw = [&state] {
        state = state * 1664525U + 1013904223U;
        return state;
    };
    std::uint64_t checksum = 14695981039346656037ULL;
    constexpr std::array<unsigned,3> pcs{0,2,3};
    constexpr std::array<std::int32_t,3> dates{-301,-300,-299};
    for (unsigned count = 0; count < 8192; ++count) {
        std::array<std::uint32_t,15> r{};
        for (auto& value : r) value = draw();
        auto input = context(static_cast<std::uint8_t>(r[2] & 255U),
            static_cast<std::uint16_t>(0x3fU | (pcs[r[10] % 3U] << 8U) | ((r[11] % 4U) << 10U)));
        input.irrigated = (r[3] & 1U) != 0;
        input.region_factor_binary32 = ((124U + r[4] % 8U) << 23U) | (r[5] & 0x7fffffU);
        input.global_factor_binary32 = ((125U + r[6] % 5U) << 23U) | (r[7] & 0x7fffffU);
        input.raw_date_operand = dates[r[9] % 3U];
        input.event_enabled = static_cast<std::uint8_t>(r[12] & 1U);
        input.event_state = 2U + (r[13] & 1U);
        input.event_crop_selector = r[14] % 5U;
        const auto before = input;
        const auto result = original::calculate_crop_harvest_yield(fields[r[0] % 5U],
            static_cast<std::uint16_t>(r[1] % 10001U),input);
        for (const auto value : std::array<std::uint32_t,5>{result.integer_base,
                 result.fertility_factor_binary32,result.first_integer_yield,result.numeric_yield,
                 result.retained_yield_word}) {
            for (unsigned shift = 0; shift < 32; shift += 8)
                checksum = (checksum ^ ((value >> shift) & 255U)) * 1099511628211ULL;
        }
        check(input == before, "resolved arithmetic does not mutate its context");
    }
    check(state == 0x9aa6e195U && checksum == 0x083918151d6daccfULL,
          "8192 independent exact-rational differential cases");
}

void actual_callback_effects_and_expiration() {
    for (const auto field : fields) {
        for (std::uint32_t raw = 0; raw <= 65535; ++raw) {
            const auto word = static_cast<std::uint16_t>(raw);
            const auto quantity = static_cast<std::uint16_t>(raw % 10001U);
            const auto actor_word = static_cast<std::uint16_t>((raw * 17U) & 0xffffU);
            const auto slot = static_cast<std::uint8_t>(raw % 12U);
            original::CropHarvestFieldState initial{quantity,word,0xf00dbaadU,
                static_cast<std::uint16_t>(65535U - raw),{}};
            for (std::size_t index = 0; index < initial.history.size(); ++index)
                initial.history[index] = static_cast<std::uint16_t>(1000U + index);
            auto expected = initial;
            expected.work_word = static_cast<std::uint16_t>(word | 0x4000U);
            expected.body_counter = 0;
            auto expected_actor = actor_word;
            std::uint32_t sum = actor_word;
            const bool marked = (word & 0x8000U) != 0;
            if (marked) {
                const auto amount = static_cast<std::uint16_t>(quantity / 100U);
                sum += amount;
                expected_actor = static_cast<std::uint16_t>(sum & 0xffffU);
                expected.work_word = static_cast<std::uint16_t>(expected.work_word & 0x7fffU);
                expected.history[slot] = amount;
                expected.quantity = 0;
                expected.assigned_actor_word = 0;
            }
            const auto input = marked ? context() : original::CropHarvestContext{};
            const auto actual = original::apply_crop_harvest_callback(field,slot,initial,actor_word,input);
            check(actual.field == expected && actual.actor_yield_word == expected_actor &&
                  actual.yield.has_value() == marked && !actual.numeric_word_narrowed &&
                  actual.actor_word_wrapped == (sum > 65535U),
                  "all work words preserve literal mark/unmarked callback effects");
            const auto repeated = original::apply_crop_harvest_callback(
                field,slot,actual.field,actual.actor_yield_word,{});
            check(repeated.field == actual.field && repeated.actor_yield_word == actual.actor_yield_word &&
                  !repeated.yield && !repeated.numeric_word_narrowed && !repeated.actor_word_wrapped,
                  "consumed mark cannot repeat a harvest or actor addition");
        }
        original::CropHarvestFieldState expired{7654,0xffff,17,23,{}};
        expired.history.fill(999);
        const auto end = original::crop_rule(field).end_slot;
        const auto reset = original::expire_crop_completed_slot(field,end,true,
            {expired.quantity,expired.work_word,true});
        expired.quantity = reset.field.quantity;
        expired.work_word = reset.field.work_word;
        expired.assigned_actor_word = 0;
        const auto after = original::apply_crop_harvest_callback(field,end,expired,123,{});
        check(reset.expired_quantity == 7654 && after.field.quantity == 0 &&
              after.field.work_word == 0x4000 && after.field.history == expired.history &&
              after.actor_yield_word == 123 && !after.yield,
              "late callback after expiration cannot invent yield from an expired mark");
    }

    original::CropHarvestFieldState field{100,0x8001,99,17,{}};
    auto input = context(); input.region_factor_binary32 = 0x47800000U;
    const auto narrowed = original::apply_crop_harvest_callback(ModelBuilding::RiceField,11,field,123,input);
    check(narrowed.numeric_word_narrowed && !narrowed.actor_word_wrapped &&
          narrowed.yield->numeric_yield == 65536 && narrowed.field.history[11] == 0 &&
          narrowed.actor_yield_word == 123 && narrowed.field.quantity == 0 &&
          narrowed.field.assigned_actor_word == 0 && narrowed.field.work_word == 0x4001,
          "numeric-to-WORD loss is explicit and is not actor carry or stock capacity");
    const auto carry = original::apply_crop_harvest_callback(ModelBuilding::RiceField,0,field,65535,context());
    check(carry.actor_yield_word == 0 && carry.actor_word_wrapped && !carry.numeric_word_narrowed &&
          carry.field.history[0] == 1,
          "actual actor WORD addition reports its separate modulo carry");
}

void actual_tending_callback() {
    for (const auto field : fields) {
        for (std::uint32_t raw = 0; raw <= 65535; ++raw) {
            original::CropHarvestFieldState initial{
                static_cast<std::uint16_t>(raw % 10001U),static_cast<std::uint16_t>(raw),
                0xffffffffU,static_cast<std::uint16_t>(raw),{}};
            for (std::size_t index = 0; index < initial.history.size(); ++index)
                initial.history[index] = static_cast<std::uint16_t>(65000U + index);
            auto expected = initial;
            expected.work_word = static_cast<std::uint16_t>(raw | 0x40U);
            expected.body_counter = 0;
            expected.assigned_actor_word = 0;
            const auto actual = original::apply_crop_tending_callback(field,initial);
            check(actual == expected,
                  "actual tending preserves every non-work field operand and all other work bits");
            check(original::apply_crop_tending_callback(field,actual) == actual,
                  "repeated tending has no quantity/history grant or extra mutation");
        }
        for (const auto quantity : {std::uint16_t{0},std::uint16_t{10000}}) {
            original::CropHarvestFieldState initial{quantity,0x8000,17,31,{}};
            initial.history.fill(65535);
            const auto actual = original::apply_crop_tending_callback(field,initial);
            check(actual.quantity == quantity && actual.history == initial.history &&
                  actual.work_word == 0x8040 && actual.body_counter == 0 &&
                  actual.assigned_actor_word == 0,
                  "tending at quantity bounds preserves existing harvest mark and history");
        }
        const original::CropHarvestFieldState invalid{10001,0x8000,99,17,{}};
        refuses<std::out_of_range>([&] {
            (void)original::apply_crop_tending_callback(field,invalid);
        }, "tending refuses unsupported quantity without mutating supplied state");
    }
    for (const auto field : {ModelBuilding::Farmhouse,ModelBuilding::HempField,ModelBuilding::TeaBush})
        refuses<std::out_of_range>([&] {
            (void)original::apply_crop_tending_callback(field,{});
        }, "non-Food type cannot inherit this tending callback by analogy");
}

void support_boundaries_and_host_independence() {
    for (const auto bad : {0x7f800000U,0x7fc00000U,0xff800000U,0xbf800000U,0x80000001U}) {
        auto input = context(); input.region_factor_binary32 = bad;
        refuses<std::invalid_argument>([&] {
            (void)original::calculate_crop_harvest_yield(ModelBuilding::WheatField,0,input);
        }, "reached nonfinite/negative region factor is refused even for zero quantity");
        input = context(); input.global_factor_binary32 = bad;
        check(original::calculate_crop_harvest_yield(ModelBuilding::WheatField,100,input).numeric_yield == 1,
              "unreached global factor does not restrict an earlier-date calculation");
        input.raw_date_operand = -299;
        refuses<std::invalid_argument>([&] {
            (void)original::calculate_crop_harvest_yield(ModelBuilding::WheatField,0,input);
        }, "reached nonfinite/negative global factor is refused");
    }
    for (const auto bits : {0U,0x80000000U,1U,0x007fffffU}) {
        auto input = context(255); input.irrigated = true; input.region_factor_binary32 = bits;
        check(original::calculate_crop_harvest_yield(ModelBuilding::SoybeanField,10000,input).numeric_yield == 0,
              "zero/signed-zero/subnormal region inputs produce no integer origin");
    }
    for (unsigned mask = 0; mask < 6; ++mask) {
        auto input = context();
        input.x87_control_word = static_cast<std::uint16_t>(input.x87_control_word & ~(1U << mask));
        refuses<std::invalid_argument>([&] {
            (void)original::calculate_crop_harvest_yield(ModelBuilding::WheatField,0,input);
        }, "unmasked native FP exception context is unsupported");
    }
    for (unsigned rc = 0; rc < 4; ++rc) {
        auto input = context(100,static_cast<std::uint16_t>(0x017fU | (rc << 10U)));
        refuses<std::invalid_argument>([&] {
            (void)original::calculate_crop_harvest_yield(ModelBuilding::WheatField,100,input);
        }, "reserved native PC01 remains unsupported");
    }
    auto overflow = context(); overflow.region_factor_binary32 = 0x4f000000U;
    refuses<std::overflow_error>([&] {
        (void)original::calculate_crop_harvest_yield(ModelBuilding::RiceField,100,overflow);
    }, "first conversion cannot silently wrap through its signed32 reload");
    overflow = context(); overflow.raw_date_operand = -299; overflow.global_factor_binary32 = 0x4f000000U;
    refuses<std::overflow_error>([&] {
        (void)original::calculate_crop_harvest_yield(ModelBuilding::RiceField,100,overflow);
    }, "second conversion cannot silently narrow outside supported signed32 domain");
    auto huge = context(); huge.region_factor_binary32 = 0x7f7fffffU;
    check(original::calculate_crop_harvest_yield(ModelBuilding::RiceField,0,huge).numeric_yield == 0,
          "large finite factor with zero quantity does not trigger unreached conversion overflow");
    refuses<std::overflow_error>([&] {
        (void)original::calculate_crop_harvest_yield(ModelBuilding::RiceField,100,huge);
    }, "huge actual converted result is refused");
    refuses<std::out_of_range>([] {
        (void)original::calculate_crop_harvest_yield(ModelBuilding::WheatField,10001,context());
    }, "unproved field quantity is refused");
    for (const auto model : {ModelBuilding::Farmhouse,ModelBuilding::HempField,ModelBuilding::TeaBush})
        refuses<std::out_of_range>([&] {
            (void)original::calculate_crop_harvest_yield(model,0,context());
        }, "non-Food model cannot use this yield callback by analogy");

    original::CropHarvestFieldState field{100,0x8000,123,17,{}};
    const auto before = field;
    refuses<std::overflow_error>([&] {
        (void)original::apply_crop_harvest_callback(ModelBuilding::RiceField,0,field,12,overflow);
    }, "unsupported arithmetic cannot publish partial work/history/actor changes");
    check(field == before, "rejected value callback leaves all supplied field bytes unchanged");
    for (const auto slot : {std::uint8_t{12},std::uint8_t{255}})
        refuses<std::out_of_range>([&] {
            (void)original::apply_crop_harvest_callback(ModelBuilding::RiceField,slot,field,12,{});
        }, "invalid history index is refused");

    const auto host_mode = std::fegetround();
    struct Restore { int mode; ~Restore() { if (mode != -1) (void)std::fesetround(mode); } } restore{host_mode};
    for (const auto mode : {FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}) {
        check(std::fesetround(mode) == 0, "test host floating-point mode is available");
        const auto result = original::calculate_crop_harvest_yield(ModelBuilding::WheatField,10000,context(65));
        check(result.numeric_yield == 64 && std::fegetround() == mode,
              "resolved source rounding is independent of and preserves host FP mode");
    }
}
} // namespace

int main() {
    try {
        literal_rounding_vectors();
        exact_unit_factor_and_event_identity();
        independent_rational_differential();
        actual_callback_effects_and_expiration();
        actual_tending_callback();
        support_boundaries_and_host_independence();
        std::cout << "Original crop harvest: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original crop harvest failed: " << error.what() << '\n';
        return 1;
    }
}
