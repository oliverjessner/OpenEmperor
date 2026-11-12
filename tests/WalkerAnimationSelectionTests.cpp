#include "app/WalkerAnimationSelection.h"
#include "WalkerAnimationSelectionFixture.h"

#include <iostream>
#include <iterator>

namespace {
namespace fixture=openemperor::testing::walker_animation;
namespace assets=openemperor::assets;
using Role=assets::WalkerVisualRole;
template<class F> void rejects(F operation,const char* reason) {
    try {operation();} catch (const std::exception&) {return;}
    throw std::runtime_error(reason);
}
fixture::Bytes read(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void source_qualification() {
    fixture::Fixture source;
    const auto valid=assets::load_walker_visual_profile(source.data,source.manifest);
    openemperor::validate_builtin_walker_animations(source.data,valid);
    fixture::check(valid.unique_images.size()==240,"authored source table must exercise physical deduplication");
    const auto reject_profile=[&](auto mutation,const char* message) {
        auto changed=valid;mutation(changed);
        rejects([&]{openemperor::validate_builtin_walker_animations(source.data,changed);},message);
    };
    reject_profile([](auto& p){p.schema_version=6;},"historical schema cannot qualify as the new source contract");
    reject_profile([](auto& p){p.roles[assets::walker_role_index(Role::VenueMusic)].reset();},
        "missing venue must reject the complete contract");
    reject_profile([](auto& p){
        auto& music=*p.roles[assets::walker_role_index(Role::Music)];
        music=*p.find(Role::Clay);music.ticks_per_frame=2;
    },"original pixels from the wrong profession cannot qualify a Musician");
    reject_profile([](auto& p){auto& clip=p.roles[8]->clips[0];std::swap(clip[0],clip[1]);},
        "two valid native frames in reversed order must reject");
    reject_profile([](auto& p){p.roles[8]->clips[0].pop_back();},"shortened gait must reject");
    reject_profile([](auto& p){p.roles[8]->clip_flip_x[1]=false;},"wrong opposite direction must reject");
    reject_profile([](auto& p){p.roles[8]->frames[0].foot_x+=0.5;},"unqualified optical foot correction must reject");
    reject_profile([](auto& p){p.roles[8]->idle_flip_x=false;},"changed static idle orientation must reject");
    reject_profile([](auto& p){p.roles[8]->ticks_per_frame=3;},"source speed metadata cannot invent a new cadence");
    reject_profile([](auto& p){p.roles[8]->frames[1]=p.roles[8]->frames[0];},"duplicate native phase identity must reject");
    reject_profile([](auto& p){p.roles[8]->frames[0].id.archive_relative_path="DATA/other.sg3";},
        "a different archive cannot acquire the source registration");
    reject_profile([](auto& p){p.roles[8]->frames.push_back(p.roles[8]->frames[0]);},
        "unused hidden frame aliases must not qualify");
    reject_profile([](auto& p){p.roles[8]->frames[0].image_index=p.unique_images.size();},
        "invalid prepared image reference must reject before publication");
    // Equivalent supported frame-plus-clip reflections remain valid; this is
    // a semantic source contract, not a prescribed JSON spelling.
    auto equivalent=valid;auto& supplier=*equivalent.roles[assets::walker_role_index(Role::Supplier)];
    for (auto& frame:supplier.frames) {
        frame.foot_x=equivalent.unique_images[frame.image_index].width-frame.foot_x;frame.flip_x=true;
    }
    for (auto&& flip:supplier.clip_flip_x) flip=!flip;
    supplier.idle_flip_x=!supplier.idle_flip_x;
    openemperor::validate_builtin_walker_animations(source.data,equivalent);

    const auto archive=source.data/"DATA/SprMain.sg3";
    const auto reject_metadata=[&](auto mutation,const char* message) {
        auto bytes=read(archive);mutation(bytes);fixture::write(archive,bytes);
        rejects([&]{openemperor::validate_builtin_walker_animations(source.data,valid);},message);
        source.rebuild();
    };
    constexpr std::uint32_t music_index=102,music_base=1+96*music_index;
    reject_metadata([](auto& bytes){fixture::u16(bytes,80+2*music_index,193);},
        "changed source group binding cannot reuse a previously decoded Music profile");
    reject_metadata([](auto& bytes){fixture::u16(bytes,80+2*(music_index+1),music_base+48);},
        "a walking sequence cannot cross its actual source group boundary");
    reject_metadata([](auto& bytes){fixture::u32(bytes,40680+72ULL*(music_base+6)+16,0);},
        "an unverified mirror relation cannot qualify the opposite direction");
    reject_metadata([](auto& bytes){fixture::u16(bytes,40680+72ULL*(music_base+6)+50,1);},
        "an unsupported reference kind cannot inherit sprite mirror semantics");
    reject_metadata([](auto& bytes){fixture::u16(bytes,40680+72ULL*music_base+32,4);},
        "another original stride cannot silently become the twelve-phase walk");
    reject_metadata([](auto& bytes){bytes[680]='X';},"unknown embedded registration must reject");

    // The generic custom format still accepts coherent authored choices. Only
    // a built-in source-bound claim applies the narrower qualification above.
    auto custom=source.document();custom["roles"]["music"]=custom["roles"]["clay"];
    custom["roles"]["music"]["ticks_per_frame"]=2;source.save(custom);
    const auto preview=assets::load_walker_visual_profile(source.data,source.manifest);
    rejects([&]{openemperor::validate_builtin_walker_animations(source.data,preview);},
        "a valid custom preview cannot inherit a false original profession label");
    custom=source.document();custom["roles"]["music"]["frames"][0]["image_index"]=music_base+6;
    source.save(custom);
    rejects([&]{(void)assets::load_walker_visual_profile(source.data,source.manifest);},
        "native SG3 mirror references must remain outside the decoder path");
}
}

int main() {
    try {source_qualification();return 0;}
    catch (const std::exception& error) {std::cerr<<"WalkerAnimationSelectionTests: "<<error.what()<<'\n';return 1;}
}
