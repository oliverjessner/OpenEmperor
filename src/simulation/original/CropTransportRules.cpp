#include "simulation/original/CropTransportRules.h"

#include "simulation/original/CropRules.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace openemperor::original {
namespace {

void validate_word(std::uint16_t value) {
    if(value>static_cast<std::uint16_t>(std::numeric_limits<std::int16_t>::max()))
        throw std::out_of_range("raw crop transport quantity is outside0..32767");
}

bool food_resource(std::uint8_t resource) {
    for(const auto model:{ModelBuilding::WheatField,ModelBuilding::MilletField,
            ModelBuilding::RiceField,ModelBuilding::CabbageField,ModelBuilding::SoybeanField})
        if(crop_rule(model).resource_id==resource)return true;
    return false;
}

void validate_resource(std::uint8_t resource) {
    if(!food_resource(resource))throw std::out_of_range("unsupported raw Food crop resource");
}

void validate_bin(const RawFarmBin& bin) {
    validate_word(bin.quantity);validate_word(bin.capacity);
    if(bin.quantity>bin.capacity)throw std::out_of_range("raw Farm bin exceeds its resolved capacity");
    if(bin.resource)validate_resource(bin.resource);
    else if(bin.quantity)throw std::invalid_argument("empty raw Farm resource has nonzero stock");
}

} // namespace

std::optional<std::size_t> select_farm_raw_bin(
    const std::array<RawFarmBin,farm_raw_bin_count>& bins,std::uint8_t resource) {
    validate_resource(resource);
    for(const auto& bin:bins)validate_bin(bin);
    std::optional<std::size_t> empty;
    for(std::size_t index=0;index<bins.size();++index) {
        if(bins[index].resource==resource)return index;
        if(!empty && !bins[index].resource && !bins[index].quantity)empty=index;
    }
    return empty;
}

RawFarmBinCredit credit_farm_raw_bin(RawFarmBin bin,std::uint8_t resource,std::uint16_t offered) {
    validate_bin(bin);validate_resource(resource);validate_word(offered);
    if(bin.resource && bin.resource!=resource)
        throw std::invalid_argument("raw Farm bin resource does not match incoming crop");
    const auto headroom=static_cast<std::uint16_t>(bin.capacity-bin.quantity);
    const auto accepted=std::min(headroom,offered);
    bin.resource=resource;
    bin.quantity=static_cast<std::uint16_t>(bin.quantity+accepted);
    return {bin,accepted,static_cast<std::uint16_t>(offered-accepted)};
}

RawFarmBinDebit debit_farm_raw_bin(RawFarmBin bin,std::uint16_t requested,bool clear_empty_identity) {
    validate_bin(bin);validate_word(requested);
    const auto removed=std::min(bin.quantity,requested);
    bin.quantity=static_cast<std::uint16_t>(bin.quantity-removed);
    if(!bin.quantity && clear_empty_identity)bin.resource=0;
    return {bin,removed};
}

RawCartReceipt accept_farm_raw_cart_cargo(std::uint16_t cargo,std::uint16_t accepted) {
    validate_word(cargo);validate_word(accepted);
    if(accepted>cargo)throw std::out_of_range("raw Cart accepted count exceeds physical cargo");
    return {accepted,static_cast<std::uint16_t>(cargo-accepted),
        accepted==cargo ? RawCartReceiptBranch::ReturnToSource:RawCartReceiptBranch::ReselectDestination};
}

} // namespace openemperor::original
