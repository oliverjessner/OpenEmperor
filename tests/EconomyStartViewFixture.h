#pragma once
// Independently authored map/SG3 fixture; no original map or pixel data.
#include "app/MenuSession.h"
#include "app/ResourceLocator.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "maps/StoredGraphicsPlan.h"
#include "maps/OriginalMapEntities.h"
#include "persistence/SandboxSave.h"
#include <SDL3/SDL.h>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace openemperor::testing::economy {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
void check(bool yes,const char* message) { if (!yes) throw std::runtime_error(message); }
void u16(Bytes& b,std::size_t at,std::uint16_t v) { b.at(at)=static_cast<std::uint8_t>(v); b.at(at+1)=static_cast<std::uint8_t>(v>>8U); }
void u32(Bytes& b,std::size_t at,std::uint32_t v) { u16(b,at,static_cast<std::uint16_t>(v)); u16(b,at+2,static_cast<std::uint16_t>(v>>16U)); }
void write(const fs::path& p,const Bytes& bytes) {
    std::ofstream out(p,std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out),"fixture write failed");
}
Bytes sg3(bool tile) {
    Bytes b(40680U+(tile?203U:202U)*64U,0);
    u32(b,0,static_cast<std::uint32_t>(b.size())); u32(b,4,213);
    u32(b,12,tile?203:202); u32(b,16,tile?202:201); u32(b,20,1);
    const std::string group="Zeus_system.bmp";
    std::copy(group.begin(),group.end(),b.begin()+680);
    u32(b,680+124,200); u32(b,680+128,1); u32(b,680+132,200);
    if (tile) { const auto at=40680U+201U*64U;
        u32(b,at+4,3200); u32(b,at+8,3200); u16(b,at+20,78); u16(b,at+22,40);
        u16(b,at+50,30); b[at+55]=1;
    }
    return b;
}
Bytes map_file(bool valid_manager=true) {
    using namespace openemperor::maps;
    Bytes raw(static_cast<std::size_t>(original_entities_logical_offset+6),0);
    // Complete empty authored manager for the rule-3 immutable map policy.
    u16(raw,static_cast<std::size_t>(original_entities_logical_offset),valid_manager ? 1:99);
    const std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),raw.begin()); u32(raw,84,84);
    for (std::size_t i=0;i<228U*228U;++i) {
        u32(raw,static_cast<std::size_t>(candidate_word_logical_offset)+4*i,0xc000);
        raw[static_cast<std::size_t>(candidate_byte_logical_offset)+i]=64;
        u32(raw,static_cast<std::size_t>(terrain_logical_offset)+4*i,0x80);
    }
    u32(raw,static_cast<std::size_t>(terrain_logical_offset)+4*(110U*228U+110U),0xc0);
    Bytes file{0xaa,0xba,0xdc,0xfe};
    for (std::size_t at=0;at<raw.size();at+=32768) {
        const auto n=std::min<std::size_t>(32768,raw.size()-at);
        uLongf capacity=compressBound(static_cast<uLong>(n)); Bytes compressed(static_cast<std::size_t>(capacity));
        check(compress2(compressed.data(),&capacity,raw.data()+at,static_cast<uLong>(n),6)==Z_OK,"zlib fixture");
        compressed.resize(static_cast<std::size_t>(capacity));
        const auto start=file.size(); file.resize(start+12);
        u32(file,start,0x12345678); u32(file,start+4,static_cast<std::uint32_t>(compressed.size()));
        u32(file,start+8,static_cast<std::uint32_t>(n));
        file.insert(file.end(),compressed.begin(),compressed.end());
    }
    return file;
}
struct Temp {
    fs::path root=fs::temp_directory_path()/ ("openemperor-economy-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() {
        fs::create_directories(root/"data/DATA"); fs::create_directories(root/"data/Cities");
        write(root/"data/DATA/China_Terrain.sg3",sg3(true));
        write(root/"data/DATA/China_Elevation.sg3",sg3(false));
        Bytes bitmap(3200); for (std::size_t i=0;i<bitmap.size();i+=2) u16(bitmap,i,0x03e0);
        write(root/"data/DATA/China_Terrain.555",bitmap);
        write(root/"data/DATA/China_Elevation.555",{});
        write(root/"data/Cities/A.map",map_file());
        write(root/"data/Cities/B.map",map_file());
        write(root/"data/Cities/Broken.map",{'b','a','d'});
    }
    ~Temp() { std::error_code ec; fs::remove_all(root,ec); }
};
}
