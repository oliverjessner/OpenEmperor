#include "assets/GreatWallDependencyPaths.h"
#include "assets/Sg3ImageLoader.h"
#include "maps/StoredGraphicsPlan.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
namespace fs=std::filesystem;
namespace assets=openemperor::assets;
namespace maps=openemperor::maps;
using Bytes=std::vector<std::uint8_t>;
void check(bool value,const char* message) {if (!value) throw std::runtime_error(message);}
template<class F> void rejects(F&& f,const char* message) {
    bool rejected=false;
    try {f();} catch (const std::exception&) {rejected=true;}
    check(rejected,message);
}
struct Temp {
    fs::path path=fs::temp_directory_path()/
        ("openemperor-greatwall-paths-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() {fs::create_directories(path/"DATA");}
    ~Temp() {std::error_code error;fs::remove_all(path,error);}
};
struct CurrentPath {
    fs::path original=fs::current_path();
    explicit CurrentPath(const fs::path& path) {fs::current_path(path);}
    ~CurrentPath() {std::error_code error;fs::current_path(original,error);}
};
void u16(Bytes& b,std::size_t at,std::uint16_t value) {
    b.at(at)=std::uint8_t(value);b.at(at+1)=std::uint8_t(value>>8U);
}
void u32(Bytes& b,std::size_t at,std::uint32_t value) {
    u16(b,at,std::uint16_t(value));u16(b,at+2,std::uint16_t(value>>16U));
}
void write(const fs::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    check(bool(out),"write synthetic dependency fixture");
}
Bytes archive(bool external=false,std::string group="China_Mon_Greatwall_10.bmp") {
    constexpr unsigned capacity=203;
    Bytes b(40680+capacity*64,0);
    u32(b,0,unsigned(b.size()));u32(b,4,213);u32(b,12,capacity);u32(b,16,201);u32(b,20,2);
    u16(b,80,201);
    const std::string system="Zeus_system.bmp";
    std::copy(system.begin(),system.end(),b.begin()+680);
    u32(b,680+124,200);u32(b,680+128,1);u32(b,680+132,200);
    std::copy(group.begin(),group.end(),b.begin()+880);
    u32(b,880+124,1);u32(b,880+128,201);u32(b,880+132,201);
    const auto at=40680+201*64;
    u32(b,at,4);u32(b,at+4,3200);u32(b,at+8,3200);
    u16(b,at+20,78);u16(b,at+22,40);u16(b,at+50,30);
    b[at+52]=std::uint8_t(external);b[at+55]=1;b[at+56]=1;
    return b;
}
Bytes bitmap() {
    Bytes b(3204,0);
    for (std::size_t at=4;at<b.size();at+=2) u16(b,at,0x03e0);
    return b;
}
void check_ready(const maps::StoredArchiveRegistration& r,const fs::path& relative) {
    check(!r.archive_missing && r.optional_error.empty() && r.layout && r.catalog,
        "supported Great Wall registration resolves complete sources");
    check(r.relative_path==relative && r.catalog->records[201].id.archive_relative_path==relative,
        "registration and AssetId expose actual canonical relative archive spelling");
    check(r.catalog->records[201].payload_in_bounds,"catalog and loader share resolved bitmap source");
}
}

int main() {try {
    const std::vector<std::string> names{"China_Mon_Greatwall_10.sg3","China_Mon_GreatWall_10.sg3"};
    check(assets::select_great_wall_dependency_filename(names[0],names)==names[0],
        "exact spelling takes precedence over alternative case match");
    rejects([&]{(void)assets::select_great_wall_dependency_filename("CHINA_MON_GREATWALL_10.SG3",names);},
        "ambiguous case matches fail without relying on volume case sensitivity");
    check(!assets::select_great_wall_dependency_filename("China_Mon_Greatwall_9.sg3",names),
        "different phase is not a filename substitute");
    for (const auto invalid:{"China_Mon_Greatwall_0.sg3","China_Mon_Greatwall_11.sg3",
                              "China_Mon_Greatwall_01.sg3","China_General.sg3",
                              "China_Mon_Greatwall_10.bmp","../China_Mon_Greatwall_10.sg3"})
        check(!assets::known_great_wall_dependency_filename(invalid),"only bounded dependency names accepted");

    Temp exact;
    const fs::path requested="DATA/China_Mon_Greatwall_10.sg3";
    write(exact.path/requested,archive());
    write(exact.path/"DATA/China_Mon_Greatwall_10.555",bitmap());
    const auto exact_registration=maps::load_regenerated_great_wall_registration(exact.path,8,requested);
    check_ready(exact_registration,requested);
    const auto exact_image=assets::load_sg3_image_with_source({exact.path/requested,201});
    check(exact_image.rgba.width==78 && exact_image.rgba.height==40,"exact dependency decodes normally");
    {
        const CurrentPath current{exact.path};
        const auto relative_bitmap=assets::resolve_sg3_image_bitmap(requested,
            *exact_registration.metadata,exact_registration.metadata->images[201]);
        check(relative_bitmap.status==assets::Sg3BitmapStatus::Resolved &&
            relative_bitmap.path==fs::canonical("DATA/China_Mon_Greatwall_10.555"),
            "public bitmap resolver accepts relative DATA archive paths");
    }

    Temp alternate;
    const fs::path actual="DATA/China_Mon_GreatWall_10.sg3";
    write(alternate.path/actual,archive());
    // Actual directory-entry spellings drive the match even on a host that
    // would report exists(requested) for a differently spelled filename.
    write(alternate.path/"DATA/CHINA_MON_GREATWALL_10.555",bitmap());
    const auto normalized=maps::load_regenerated_great_wall_registration(alternate.path,8,requested);
    check_ready(normalized,actual);
    const auto alternate_image=assets::load_sg3_image_with_source({alternate.path/normalized.relative_path,201});
    check(alternate_image.bitmap.path==fs::canonical(alternate.path/"DATA/CHINA_MON_GREATWALL_10.555") &&
        alternate_image.rgba.pixels==exact_image.rgba.pixels,"alternate SG3 and bitmap spelling use unchanged decoder");
    const auto slot9=maps::load_regenerated_great_wall_registration(alternate.path,9,requested);
    check_ready(slot9,actual);
    check(slot9.layout->slot==9 && normalized.layout->slot==8 &&
        slot9.catalog->records[201].id==normalized.catalog->records[201].id,
        "slot registrations remain distinct while physical asset identity is shared");

    Temp absent;
    const auto missing=maps::load_regenerated_great_wall_registration(absent.path,8,requested);
    check(missing.archive_missing && !missing.catalog && missing.relative_path==requested,
        "missing archive retains requested identity and explicit fallback");
    write(absent.path/actual,archive());
    const auto missing_bitmap=maps::load_regenerated_great_wall_registration(absent.path,8,requested);
    check(!missing_bitmap.optional_error.empty() && !missing_bitmap.catalog,
        "missing referenced bitmap rejects whole optional registration");
    const auto unsupported=maps::load_regenerated_great_wall_registration(absent.path,8,"DATA/China_Mon_Greatwall_11.sg3");
    check(!unsupported.optional_error.empty(),"unobserved phase does not expand lookup");
    rejects([&]{(void)assets::resolve_great_wall_dependency_path(absent.path,"OTHER/China_Mon_Greatwall_10.sg3");},
        "known names outside DATA are not searched");
    rejects([&]{(void)assets::resolve_great_wall_dependency_path(absent.path,"DATA/nested/China_Mon_Greatwall_10.sg3");},
        "known names do not start a nested directory search");

    Temp external;
    const fs::path ruined="DATA/China_Mon_GreatWall_Ruined.sg3";
    write(external.path/ruined,archive(true,"China_Mon_Greatwall_Ruins.bmp"));
    write(external.path/"DATA/China_Mon_GreatWall_Ruins.555",bitmap());
    const auto external_registration=maps::load_regenerated_great_wall_registration(external.path,8,
        "DATA/China_Mon_Greatwall_Ruined.sg3");
    check_ready(external_registration,ruined);
    const auto external_image=assets::load_sg3_image_with_source({external.path/ruined,201});
    check(external_image.rgba.pixels==exact_image.rgba.pixels,
        "referenced known external bitmap uses same bounded filename resolution");
    write(external.path/"DATA/ordinary.sg3",archive(true,"CHINA_MON_GREATWALL_RUINS.bmp"));
    const auto ordinary=assets::read_sg3_archive(external.path/"DATA/ordinary.sg3");
    const auto ordinary_bitmap=assets::resolve_sg3_image_bitmap(external.path/"DATA/ordinary.sg3",ordinary,ordinary.images[201]);
    check(ordinary_bitmap.path.filename()=="CHINA_MON_GREATWALL_RUINS.555",
        "ordinary archives retain their exact bitmap-name semantics");

    Temp outside;
    write(outside.path/"DATA/foreign.sg3",archive());
    write(outside.path/"DATA/foreign.555",bitmap());
    Temp escaped;
    fs::create_symlink(outside.path/"DATA/foreign.sg3",escaped.path/actual);
    const auto escaped_archive=maps::load_regenerated_great_wall_registration(escaped.path,8,requested);
    check(!escaped_archive.optional_error.empty() && !escaped_archive.catalog,
        "alternate archive symlink cannot escape data root");
    fs::remove(escaped.path/actual);write(escaped.path/actual,archive());
    fs::create_symlink(outside.path/"DATA/foreign.555",escaped.path/"DATA/China_Mon_GreatWall_10.555");
    const auto escaped_bitmap=maps::load_regenerated_great_wall_registration(escaped.path,8,requested);
    check(!escaped_bitmap.optional_error.empty() && !escaped_bitmap.catalog,
        "referenced bitmap symlink cannot escape data root");
    rejects([&]{(void)assets::load_sg3_image_with_source({escaped.path/actual,201});},
        "shared loader rejects escaped Great Wall bitmap before payload read");
    Temp directory;
    fs::remove(directory.path/"DATA");
    fs::create_directory_symlink(outside.path/"DATA",directory.path/"DATA");
    rejects([&]{(void)assets::resolve_great_wall_dependency_path(directory.path,requested);},
        "DATA directory symlink cannot expand lookup outside root");
    std::cout<<"Great Wall dependency path tests passed\n";
    return 0;
} catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
