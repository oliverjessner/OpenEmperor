#include "assets/IrrigationVisualProfile.h"
#include "core/PerformanceDiagnostics.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <sstream>

namespace {
namespace a = openemperor::assets;
namespace fs = std::filesystem;
using J = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;
void check(bool yes, const char *text) {
  if (!yes)
    throw std::runtime_error(text);
}
void u16(Bytes &b, std::size_t at, unsigned v) {
  b.at(at) = static_cast<std::uint8_t>(v);
  b.at(at + 1) = static_cast<std::uint8_t>(v >> 8U);
}
void u32(Bytes &b, std::size_t at, unsigned v) {
  u16(b, at, v);
  u16(b, at + 2, v >> 16U);
}
std::string hash(const Bytes &bytes) {
  std::array<unsigned char, 32> d{};
  unsigned n = 0;
  check(EVP_Digest(bytes.data(), bytes.size(), d.data(), &n, EVP_sha256(),
                   nullptr) == 1 &&
            n == 32,
        "SHA256");
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (auto v : d)
    out << std::setw(2) << unsigned(v);
  return out.str();
}
void write(const fs::path &p, const Bytes &b) {
  fs::create_directories(p.parent_path());
  std::ofstream o(p, std::ios::binary);
  o.write(reinterpret_cast<const char *>(b.data()),
          static_cast<std::streamsize>(b.size()));
  check(bool(o), "Write fixture");
}
void write(const fs::path &p, const J &j) {
  const auto text = j.dump();
  write(p, Bytes(text.begin(), text.end()));
}
struct Fixture {
  fs::path root =
      fs::temp_directory_path() /
      ("oe-irrigation-art-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::path data = root / "data", manifest = root / "irrigation.json";
  Bytes sg3 = Bytes(40680 + 241 * 64), pixels;
  J metadata;
  Fixture() {
    u32(sg3, 0, static_cast<unsigned>(sg3.size()));
    u32(sg3, 4, 213);
    u32(sg3, 12, 241);
    u32(sg3, 16, 240);
    u32(sg3, 20, 2);
    const std::string system = "Zeus_system.bmp";
    std::copy(system.begin(), system.end(), sg3.begin() + 680);
    u32(sg3, 804, 200);
    u32(sg3, 808, 1);
    u32(sg3, 812, 200);
    for (unsigned i = 0; i < 183; ++i)
      u16(sg3, 80 + 2 * i, 201);
    const auto image = [&](unsigned physical, unsigned side, bool pump) {
      const unsigned base = 3200 * side * side, width = 80 * side - 2,
                     height = 40 * side, at = 40680 + physical * 64,
                     offset = static_cast<unsigned>(pixels.size());
      pixels.resize(pixels.size() + base);
      for (unsigned pixel = 0; pixel < base / 2; ++pixel)
        u16(pixels, offset + pixel * 2, 0x03e0 + (physical - 201));
      u32(sg3, at, offset);
      u32(sg3, at + 4, base);
      u32(sg3, at + 8, base);
      u16(sg3, at + 20, width);
      u16(sg3, at + 22, height);
      u16(sg3, at + 50, 30);
      sg3[at + 55] = static_cast<std::uint8_t>(side);
      if (pump) {
        u16(sg3, at + 30, 15);
        sg3[at + 58] = 4;
      }
      return J{{"image_index", physical},
               {"width", width},
               {"height", height},
               {"base_bytes", base},
               {"ground_anchor", {40 * side - 1, height - 20}}};
    };
    J ditches = J::array(), pumps = J::array();
    for (unsigned i = 0; i < 36; ++i) {
      auto row = image(201 + i, 1, false);
      row["offset"] = i;
      ditches.push_back(row);
    }
    constexpr std::array<unsigned, 4> groups{0x439, 0x4b6, 0x4b7, 0x436};
    for (unsigned i = 0; i < 4; ++i) {
      u16(sg3, 80 + 2 * (groups[i] % 512 - 1), 237 + i);
      auto row = image(237 + i, 2, true);
      row["orientation"] = i;
      row["group_key"] = groups[i];
      pumps.push_back(row);
    }
    u16(sg3, 80 + 2 * (0x435 % 512 - 1), 201);
    write(data / "DATA/China_General.sg3", sg3);
    write(data / "DATA/China_General.555", pixels);
    metadata = {
        {"schema_version", 1},
        {"mode", "irrigation_static"},
        {"selection", a::irrigation_visual_selection_contract},
        {"anchor_policy", "type30_front_cell"},
        {"evidence",
         "Independent authored synthetic pixels; no Original data fixture"},
        {"files",
         {{{"path", "DATA/China_General.sg3"}, {"sha256", hash(sg3)}},
          {{"path", "DATA/China_General.555"}, {"sha256", hash(pixels)}}}},
        {"ditches", ditches},
        {"pumps", pumps}};
  }
  ~Fixture() {
    std::error_code ec;
    fs::remove_all(root, ec);
  }
};
void synthetic() {
  Fixture f;
  write(f.manifest, f.metadata);
  a::BuildingVisualProfile p;
  a::append_irrigation_visual_profile(f.data, f.manifest, p);
  check(p.irrigation_prepared && p.unique_images.size() == 40 &&
            !p.original_building_prepared && !p.plantation_prepared,
        "Irrigation supplement changed historical roles");
  for (unsigned v = 0; v < 18; ++v) {
    check(a::find_irrigation_ditch_visual(p, static_cast<std::uint8_t>(v), true)
                  ->id.image_index == 201 + v,
          "Wet bank identity");
    check(
        a::find_irrigation_ditch_visual(p, static_cast<std::uint8_t>(v), false)
                ->id.image_index == 219 + v,
        "Dry bank identity");
  }
  for (unsigned o = 0; o < 4; ++o)
    check(a::find_irrigation_pump_visual(p, static_cast<std::uint8_t>(o))
                  ->id.image_index == 237 + o,
          "Pump direction identity");
  check(!a::find_irrigation_ditch_visual(p, 18, true) &&
            !a::find_irrigation_pump_visual(p, 4),
        "Invalid selector admitted");
  openemperor::performance::set_enabled(true);
  openemperor::performance::reset();
  for (unsigned repeat = 0; repeat < 100; ++repeat)
    for (unsigned v = 0; v < 18; ++v)
      check(a::find_irrigation_ditch_visual(p, static_cast<std::uint8_t>(v),
                                            repeat % 2),
            "Prepared selector missing");
  for (std::size_t i = 0;
       i < static_cast<std::size_t>(openemperor::performance::Counter::Count);
       ++i)
    check(!openemperor::performance::counter(
              static_cast<openemperor::performance::Counter>(i)),
          "Prepared selector did resource work");
  openemperor::performance::set_enabled(false);
  const auto reject = [&](J metadata,
                          std::uint64_t budget = a::building_max_rgba_bytes) {
    write(f.manifest, metadata);
    a::BuildingVisualProfile candidate;
    candidate.unique_images.push_back(p.unique_images.front());
    const auto before = hash(candidate.unique_images.front().pixels);
    bool failed = false;
    try {
      a::append_irrigation_visual_profile(f.data, f.manifest, candidate,
                                          budget);
    } catch (const std::exception &) {
      failed = true;
    }
    check(failed && !candidate.irrigation_prepared &&
              candidate.unique_images.size() == 1 &&
              hash(candidate.unique_images.front().pixels) == before,
          "Rejected supplement published partial state");
  };
  auto bad = f.metadata;
  bad["ditches"].erase(7);
  reject(bad);
  bad = f.metadata;
  bad["pumps"][1]["orientation"] = 0;
  reject(bad);
  bad = f.metadata;
  bad["ditches"][0]["image_index"] = 202;
  reject(bad);
  bad = f.metadata;
  bad["pumps"][0]["group_key"] = 0x435;
  reject(bad);
  bad = f.metadata;
  bad["files"][1]["sha256"] = std::string(64, '0');
  reject(bad);
  bad = f.metadata;
  bad["files"][0]["path"] = "../General.sg3";
  reject(bad);
  bad = f.metadata;
  bad["ditches"][0]["ground_anchor"] = {39, 21};
  reject(bad);
  reject(f.metadata, 1);
  write(f.manifest, f.metadata);
  a::BuildingVisualProfile dedup;
  dedup.unique_images.push_back(p.unique_images[0]);
  auto existing = *a::find_irrigation_ditch_visual(p, 0, true);
  existing.image_index = 0;
  dedup.entries[0] = existing;
  a::append_irrigation_visual_profile(f.data, f.manifest, dedup);
  check(dedup.unique_images.size() == 40 && dedup.entries[0]->image_index == 0,
        "Physical image prefix was duplicated");
}
void real(const fs::path &data, const fs::path &metadata) {
  a::BuildingVisualProfile p;
  a::append_irrigation_visual_profile(data, metadata, p);
  check(p.irrigation_prepared && p.unique_images.size() == 40,
        "Real complete atlas unavailable");
  std::uint64_t bytes = 0;
  for (const auto &im : p.unique_images)
    bytes += im.pixels.size();
  check(bytes == 762008, "Real static atlas geometry changed");
  for (unsigned v = 0; v < 18; ++v) {
    check(a::find_irrigation_ditch_visual(p, static_cast<std::uint8_t>(v), true)
                  ->id.image_index == 672 + v,
          "Real wet selector");
    check(
        a::find_irrigation_ditch_visual(p, static_cast<std::uint8_t>(v), false)
                ->id.image_index == 690 + v,
        "Real dry selector");
  }
  constexpr std::array<unsigned, 4> bases{761, 745, 729, 777};
  for (unsigned o = 0; o < 4; ++o)
    check(a::find_irrigation_pump_visual(p, static_cast<std::uint8_t>(o))
                  ->id.image_index == bases[o],
          "Real Pump orientation");
}
} // namespace
int main(int argc, char **argv) try {
  check(argc == 1 || argc == 3, "Expected optional DATA_ROOT METADATA");
  synthetic();
  if (argc == 3)
    real(fs::canonical(argv[1]), fs::canonical(argv[2]));
  std::cout
      << "Irrigation static source identities, separate wet/dry banks, four "
         "directions, pure selection, atomic rejection and dedup PASS\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
