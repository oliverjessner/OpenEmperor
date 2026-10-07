#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Independently authored SG3/Omega images, shared by the focused and production
// Inspector tests. No original graphics or map data are retained here.
namespace openemperor::testing::inspector {
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
inline void check(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
inline void u16(Bytes& bytes,std::size_t at,std::uint16_t value) {
    bytes.at(at)=static_cast<std::uint8_t>(value);bytes.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
inline void u32(Bytes& bytes,std::size_t at,std::uint32_t value) {
    u16(bytes,at,static_cast<std::uint16_t>(value));u16(bytes,at+2,static_cast<std::uint16_t>(value>>16U));
}
inline void write(const fs::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(bool(out),"synthetic file write");
}
struct Fixture {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-inspector-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",core=root/"core.json",supplement=root/"inspector.json";
    Bytes archive,bitmap{0,0,0,0};
    Fixture() : archive(40680+18*72,0) {
        fs::create_directories(data/"DATA");
        u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
        u32(archive,12,18);u32(archive,16,18);u32(archive,20,1);
        const std::string group="authored-inspector.bmp";
        std::copy(group.begin(),group.end(),archive.begin()+680);u32(archive,680+124,18);
        constexpr std::array<std::uint16_t,4> colors{0x001f,0x03e0,0x7fe0,0x03ff};
        for (std::size_t direction=0;direction<4;++direction) for (int phase=0;phase<2;++phase) {
            const int width=10+2*phase,height=16+2*phase,fx=width/2,fy=height-1;
            std::vector<std::uint16_t> raster(static_cast<std::size_t>(width*height),0);
            const auto fill=[&](int x,int y,int w,int h,std::uint16_t color) {
                for (int dy=0;dy<h;++dy) for (int dx=0;dx<w;++dx)
                    raster.at(static_cast<std::size_t>((y+dy)*width+x+dx))=color;
            };
            fill(fx-2,fy-12,5,9,colors[direction]); // Stable torso/head.
            fill(phase ? fx+3:fx-4,fy-10,2,3,colors[direction]); // Actual gait silhouette.
            fill(fx-1,fy-3,3,3,0x7fff); // Common foot-contact reference.
            fill(fx+3,fy-5,2,2,0x7c00); // Verified alpha-free Omega shadow marker.
            Bytes payload;
            for (int y=0;y<height;++y) for (int x=0;x<width;) {
                const auto color=raster[static_cast<std::size_t>(y*width+x)];
                if (!color) {
                    int count=1;
                    while (x+count<width && !raster[static_cast<std::size_t>(y*width+x+count)]) ++count;
                    payload.push_back(255);payload.push_back(static_cast<std::uint8_t>(count));x+=count;
                } else {
                    int count=1;
                    while (x+count<width && raster[static_cast<std::size_t>(y*width+x+count)]) ++count;
                    payload.push_back(static_cast<std::uint8_t>(count));
                    for (int dx=0;dx<count;++dx) {
                        const auto value=raster[static_cast<std::size_t>(y*width+x+dx)];
                        payload.push_back(static_cast<std::uint8_t>(value));
                        payload.push_back(static_cast<std::uint8_t>(value>>8U));
                    }
                    x+=count;
                }
            }
            const auto record=1+direction*4+static_cast<std::size_t>(phase)*2;
            const auto at=40680+record*72;
            u32(archive,at,static_cast<std::uint32_t>(bitmap.size()));
            u32(archive,at+4,static_cast<std::uint32_t>(payload.size()));
            u16(archive,at+20,static_cast<std::uint16_t>(width));
            u16(archive,at+22,static_cast<std::uint16_t>(height));u16(archive,at+50,256);
            archive[at+59]=1;
            bitmap.insert(bitmap.end(),payload.begin(),payload.end());
        }
        write(data/"DATA/walker.sg3",archive);write(data/"DATA/walker.555",bitmap);
        save(core,legacy());save(supplement,inspector());
    }
    ~Fixture() { std::error_code ignored;fs::remove_all(root,ignored); }
    static void save(const fs::path& path,const Json& value) {
        std::ofstream out(path);out<<value.dump(2);check(bool(out),"synthetic JSON write");
    }
    static Json frame(std::size_t direction,int phase) {
        return {{"alias","d"+std::to_string(direction)+"-"+std::to_string(phase)},
            {"archive","DATA/walker.sg3"},{"image_index",1+direction*4+static_cast<std::size_t>(phase)*2},
            {"foot_anchor",{5+phase,15+2*phase}}};
    }
    static Json legacy() {
        Json role={{"ticks_per_frame",2},{"evidence","Independently authored legacy core."},
            {"frames",Json::array({frame(0,0)})},{"clips",{{"pos_x",{"d0-0"}}}},
            {"idle","d0-0"}};
        return {{"schema_version",2},{"mode","curated_walker_preview"},
            {"roles",{{"clay",role},{"pottery",role},{"household",role}}}};
    }
    static Json inspector() {
        Json role={{"ticks_per_frame",2},{"clip_id","authored-four-direction-inspector"},
            {"evidence","Independent variable-canvas silhouettes and foot references."},
            {"frames",Json::array()},{"clips",Json::object()},{"idle","d0-0"}};
        constexpr std::array<const char*,4> names{"pos_x","neg_x","pos_y","neg_y"};
        for (std::size_t direction=0;direction<4;++direction) {
            role["frames"].push_back(frame(direction,0));role["frames"].push_back(frame(direction,1));
            role["clips"][names[direction]]=Json::array({"d"+std::to_string(direction)+"-0",
                                                        "d"+std::to_string(direction)+"-1"});
        }
        return {{"schema_version",3},{"mode","curated_walker_preview"},
            {"roles",{{"fire_inspector",role}}}};
    }
};
}
