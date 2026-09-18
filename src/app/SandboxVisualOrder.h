#pragma once
#include <cstdint>
#include <tuple>

namespace openemperor {
enum class SandboxVisualKind : std::uint8_t { Road, Building, Walker };
struct SandboxVisualKey {
    double depth=0; // Projected ground y, independent of image height.
    double ground_x=0;
    SandboxVisualKind kind=SandboxVisualKind::Road;
    unsigned stable_id=0;
};
inline bool operator<(const SandboxVisualKey& a,const SandboxVisualKey& b) {
    return std::tie(a.depth,a.ground_x,a.kind,a.stable_id)<
           std::tie(b.depth,b.ground_x,b.kind,b.stable_id);
}
}
