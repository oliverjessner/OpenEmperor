#include "assets/WalkerVisualProfile.h"
#include "FireInspectorWalkerFixture.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

// These profile checks use independently authored Omega pixels, not retained
// original graphics. Live CourierRole/pose/render checks have separate targets.
namespace {
namespace assets=openemperor::assets;
namespace fs=std::filesystem;
using Fixture=openemperor::testing::inspector::Fixture;
using Json=nlohmann::json;
using Role=assets::WalkerVisualRole;
using openemperor::testing::inspector::check;
using openemperor::testing::inspector::write;
using openemperor::testing::inspector::u32;
template<class F> void rejects(F action,const char* reason) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(reason);
}
bool same_role(const assets::WalkerRoleVisual& a,const assets::WalkerRoleVisual& b) {
    if (a.ticks_per_frame!=b.ticks_per_frame || a.clip_id!=b.clip_id || a.evidence!=b.evidence ||
        a.clips!=b.clips || a.idle_frame!=b.idle_frame || a.frames.size()!=b.frames.size()) return false;
    for (std::size_t i=0;i<a.frames.size();++i) {
        const auto& x=a.frames[i];const auto& y=b.frames[i];
        if (x.alias!=y.alias || x.id!=y.id || x.foot_x!=y.foot_x || x.foot_y!=y.foot_y ||
            x.image_index!=y.image_index || x.flip_x!=y.flip_x) return false;
    }
    return true;
}
bool same_profile(const assets::WalkerVisualProfile& a,const assets::WalkerVisualProfile& b) {
    if (a.schema_version!=b.schema_version || a.unique_images.size()!=b.unique_images.size()) return false;
    for (std::size_t i=0;i<a.roles.size();++i) {
        if (a.roles[i].has_value()!=b.roles[i].has_value()) return false;
        if (a.roles[i] && !same_role(*a.roles[i],*b.roles[i])) return false;
    }
    for (std::size_t i=0;i<a.unique_images.size();++i) {
        const auto& x=a.unique_images[i];const auto& y=b.unique_images[i];
        if (x.width!=y.width || x.height!=y.height || x.pixels!=y.pixels) return false;
    }
    return true;
}
Json market(const std::string& archive="DATA/market.sg3") {
    const auto base=Fixture::inspector()["roles"]["fire_inspector"];
    Json result={{"schema_version",4},{"mode","curated_walker_preview"},{"roles",Json::object()}};
    for (const auto name:{"supplier","distributor"}) {
        auto role=base;
        role["clip_id"]=std::string("authored-")+name+"-walk";
        role["evidence"]="Independent four-direction silhouettes; presentation family only.";
        for (auto& frame:role["frames"]) {
            frame["archive"]=archive;
            if (std::string_view(name)=="distributor") {
                frame["foot_anchor"][0]=frame["foot_anchor"][0].get<double>()+1;
                frame["foot_anchor"][1]=frame["foot_anchor"][1].get<double>()+2;
            }
        }
        result["roles"][name]=std::move(role);
    }
    return result;
}
assets::WalkerVisualProfile core_with_inspector(Fixture& fixture) {
    Fixture::save(fixture.core,Fixture::legacy());
    Fixture::save(fixture.supplement,Fixture::inspector());
    auto profile=assets::load_walker_visual_profile(fixture.data,fixture.core);
    assets::append_fire_inspector_visual_profile(fixture.data,fixture.supplement,profile);
    return profile;
}
void schema_checks(Fixture& fixture,const fs::path& manifest) {
    static_assert(assets::walker_role_index(Role::Clay)==0);
    static_assert(assets::walker_role_index(Role::Pottery)==1);
    static_assert(assets::walker_role_index(Role::Household)==2);
    static_assert(assets::walker_role_index(Role::FireInspector)==3);
    static_assert(assets::walker_role_index(Role::Supplier)==4);
    static_assert(assets::walker_role_index(Role::Distributor)==5);
    static_assert(assets::walker_core_role_count==3 && assets::walker_schema3_role_count==4 &&
                  assets::walker_schema4_role_count==6 && assets::walker_visual_role_count==7);
    check(std::string_view(assets::walker_role_name(Role::Supplier))=="supplier" &&
          std::string_view(assets::walker_role_name(Role::Distributor))=="distributor",
          "market family names disagree with appended enum values");
    auto legacy=assets::load_walker_visual_profile(fixture.data,fixture.core);
    const auto one_role=Fixture::legacy()["roles"]["clay"];
    auto schema1=one_role;schema1["schema_version"]=1;schema1["mode"]="curated_walker_preview";
    schema1["role"]="clay";Fixture::save(manifest,schema1);
    const auto one=assets::load_walker_visual_profile(fixture.data,manifest);
    check(one.schema_version==1 && same_role(*one.roles[0],*legacy.roles[0]),"schema1 core changed");
    for (std::size_t i=1;i<one.roles.size();++i) check(!one.roles[i],"schema1 gained another role");
    for (const auto version:{2,3}) for (const auto name:{"supplier","distributor"}) {
        auto invalid=Fixture::legacy();invalid["schema_version"]=version;
        invalid["roles"][name]=market()["roles"][name];Fixture::save(manifest,invalid);
        rejects([&]{assets::load_walker_visual_profile(fixture.data,manifest);},
                "historical schema accepted a market family");
    }
    auto old=Fixture::legacy();old["schema_version"]=3;
    old["roles"]["fire_inspector"]=Fixture::inspector()["roles"]["fire_inspector"];
    Fixture::save(manifest,old);
    const auto four=assets::load_walker_visual_profile(fixture.data,manifest);
    check(four.schema_version==3 && four.unique_images.size()==8 && !four.roles[4] && !four.roles[5],
          "schema3 gained new roles or changed physical dedupe");
    for (std::size_t i=0;i<3;++i) check(same_role(*four.roles[i],*legacy.roles[i]),"schema3 changed core");
    auto full=market("DATA/walker.sg3");
    for (const auto& [name,value]:old["roles"].items()) full["roles"][name]=value;
    Fixture::save(manifest,full);
    const auto six=assets::load_walker_visual_profile(fixture.data,manifest);
    check(six.schema_version==4 && six.unique_images.size()==8,"schema4 failed full-profile global dedupe");
    for (std::size_t i=0;i<assets::walker_schema4_role_count;++i)
        check(six.roles[i].has_value(),"schema4 dropped a declared family");
    check(!six.find(Role::Service),"schema4 gained a Service role");
    full["schema_version"]=3;Fixture::save(manifest,full);
    rejects([&]{assets::load_walker_visual_profile(fixture.data,manifest);},"schema3 four-role limit changed");
    auto partial=market();partial["roles"].erase("distributor");
    partial["roles"]["supplier"]["clips"].erase("neg_y");Fixture::save(manifest,partial);
    const auto custom=assets::load_walker_visual_profile(fixture.data,manifest);
    check(custom.roles[4] && custom.roles[4]->clips[3].empty() && !custom.roles[5] &&
          !custom.roles[0] && !custom.roles[3],"explicit custom gained hidden roles/directions");
    auto unknown=market();unknown["roles"]["service"]=one_role;Fixture::save(manifest,unknown);
    rejects([&]{assets::load_walker_visual_profile(fixture.data,manifest);},"schema4 accepted unknown service role");
    unknown=market();unknown["schema_version"]=6;Fixture::save(manifest,unknown);
    rejects([&]{assets::load_walker_visual_profile(fixture.data,manifest);},"future schema accepted");
}
void flip_checks(Fixture& fixture,const fs::path& manifest) {
    // Omission and explicit false keep the native frame. An explicit flipped
    // alias shares its physical source and preserves already-reflected feet.
    auto document=market();
    for (const auto name:{"supplier","distributor"}) {
        auto& frames=document["roles"][name]["frames"];
        frames[0]["flip_x"]=false;
        auto reflected=frames[1];reflected["alias"]="reflected-gait";
        reflected["flip_x"]=true;reflected["foot_anchor"]={2.25,9.5};
        frames.push_back(reflected);
    }
    Fixture::save(manifest,document);
    const auto prepared=assets::load_walker_visual_profile(fixture.data,manifest);
    for (const auto role:{Role::Supplier,Role::Distributor}) {
        const auto& frames=prepared.find(role)->frames;
        check(!frames[0].flip_x && !frames[1].flip_x && frames.back().flip_x,
              "omitted/false/true flip_x did not retain exact explicit meaning");
        check(frames[1].image_index==frames.back().image_index &&
              frames.back().foot_x==2.25 && frames.back().foot_y==9.5,
              "display flip copied native pixels or transformed the explicit foot a second time");
    }
    check(prepared.unique_images.size()==8 && assets::walker_rgba_bytes(prepared)==6016,
          "display transform doubled physical images/bytes");
    auto core=core_with_inspector(fixture);const auto old=core;
    assets::append_market_visual_profile(fixture.data,manifest,core);
    check(core.roles[4]->frames.back().flip_x && core.roles[5]->frames.back().flip_x &&
          core.unique_images.size()==16,"optional append lost explicit transform or dedupe");
    for (std::size_t i=0;i<4;++i) check(same_role(*core.roles[i],*old.roles[i]),
                                      "display flip changed an old role");
    const auto invalid=[&](Json value,const char* reason) {
        Fixture::save(manifest,value);
        rejects([&]{assets::load_walker_visual_profile(fixture.data,manifest);},reason);
        auto unchanged=old;
        rejects([&]{assets::append_market_visual_profile(fixture.data,manifest,unchanged);},reason);
        check(same_profile(unchanged,old),"invalid transform changed retained core/Inspector");
    };
    for (const auto& value:std::array<Json,5>{0,1,"true",nullptr,Json::array({true})}) {
        auto bad=market();bad["roles"]["distributor"]["frames"][7]["flip_x"]=value;
        invalid(bad,"nonboolean flip_x activated");
    }
    for (const auto name:{"flipX","flip_y","horizontal_flip","rotation","rotate","scale"}) {
        auto bad=market();bad["roles"]["distributor"]["frames"][7][name]=true;
        invalid(bad,"unknown schema4 transform field silently ignored");
    }
    for (const auto role:{"clay","pottery","household","fire_inspector"}) {
        auto bad=Fixture::legacy();bad["schema_version"]=4;
        if (std::string_view(role)=="fire_inspector")
            bad["roles"][role]=Fixture::inspector()["roles"]["fire_inspector"];
        bad["roles"][role]["frames"][0]["flip_x"]=false;Fixture::save(manifest,bad);
        rejects([&]{assets::load_walker_visual_profile(fixture.data,manifest);},
                "schema4 transform field was accepted on a historical visual role");
    }
    // Historical schemas ignored unknown fields. Preserve that behavior and
    // native output instead of retroactively introducing a new transform.
    for (const auto version:{1,2,3}) {
        auto legacy=Fixture::legacy();legacy["schema_version"]=version;
        if (version==1) {
            legacy=Fixture::legacy()["roles"]["clay"];legacy["schema_version"]=1;
            legacy["mode"]="curated_walker_preview";legacy["role"]="clay";
            legacy["frames"][0]["flip_x"]=true;legacy["frames"][0]["flip_y"]=true;
        } else {
            legacy["roles"]["clay"]["frames"][0]["flip_x"]=true;
            legacy["roles"]["clay"]["frames"][0]["flip_y"]=true;
            if (version==3) {
                legacy["roles"]["fire_inspector"]=Fixture::inspector()["roles"]["fire_inspector"];
                legacy["roles"]["fire_inspector"]["frames"][0]["flip_x"]=true;
            }
        }
        Fixture::save(manifest,legacy);const auto native=assets::load_walker_visual_profile(fixture.data,manifest);
        for (const auto& role:native.roles) if (role) for (const auto& frame:role->frames)
            check(!frame.flip_x,"historical schema silently acquired display mirroring");
    }
}
void append_checks(Fixture& fixture,const fs::path& manifest) {
    auto profile=core_with_inspector(fixture);
    const auto before=profile;
    std::vector<const std::uint8_t*> retained;
    for (const auto& image:profile.unique_images) retained.push_back(image.pixels.data());
    // Canonically identical in-root archive names still share one physical set.
    fs::create_symlink(fixture.data/"DATA/market.sg3",fixture.data/"DATA/alias.sg3");
    auto aliased=market();
    for (auto& frame:aliased["roles"]["supplier"]["frames"]) frame["archive"]="DATA/alias.sg3";
    Fixture::save(manifest,aliased);
    assets::append_market_visual_profile(fixture.data,manifest,profile);
    check(profile.schema_version==4 && profile.unique_images.size()==16 &&
          profile.roles[4]->frames.size()==8 && profile.roles[5]->frames.size()==8,
          "market append did not prepare both families atomically");
    for (std::size_t i=0;i<4;++i) check(same_role(*profile.roles[i],*before.roles[i]),
                                      "market append changed historical core/Inspector role");
    for (std::size_t i=0;i<retained.size();++i) check(profile.unique_images[i].pixels.data()==retained[i],
                                                    "append copied retained decoded pixels");
    for (std::size_t i=0;i<8;++i) check(profile.roles[4]->frames[i].image_index==profile.roles[5]->frames[i].image_index &&
        profile.roles[4]->frames[i].foot_x+1==profile.roles[5]->frames[i].foot_x &&
        profile.roles[4]->frames[i].foot_y+2==profile.roles[5]->frames[i].foot_y,
        "shared supplier/distributor pixels lost independent foot anchors");
    check(assets::walker_rgba_bytes(profile)==12032,"merged physical byte accounting changed");
    const auto complete=profile;
    rejects([&]{assets::append_market_visual_profile(fixture.data,manifest,profile);},"duplicate market append accepted");
    check(same_profile(profile,complete),"duplicate append changed active profile");
    // Reverse append order must retain schema4 and exact market image indexes.
    auto reverse=assets::load_walker_visual_profile(fixture.data,fixture.core);
    Fixture::save(manifest,market());assets::append_market_visual_profile(fixture.data,manifest,reverse);
    const auto market_before=reverse;
    auto bad_inspector=Fixture::inspector();
    bad_inspector["roles"]["fire_inspector"]["frames"][7]["image_index"]=17;
    Fixture::save(fixture.supplement,bad_inspector);
    rejects([&]{assets::append_fire_inspector_visual_profile(fixture.data,fixture.supplement,reverse);},
            "bad later FI frame committed over existing market families");
    check(same_profile(reverse,market_before),"optional FI failure displaced market roles/images/schema4");
    Fixture::save(fixture.supplement,Fixture::inspector());
    assets::append_fire_inspector_visual_profile(fixture.data,fixture.supplement,reverse);
    check(reverse.schema_version==4 && reverse.unique_images.size()==16 &&
          same_role(*reverse.roles[4],*market_before.roles[4]) &&
          same_role(*reverse.roles[5],*market_before.roles[5]),"FI appended after market downgraded/displaced it");
    // Already shared core/FI assets do not consume a second asset/RGBA budget.
    auto shared=before;Fixture::save(manifest,market("DATA/walker.sg3"));
    assets::append_market_visual_profile(fixture.data,manifest,shared);
    check(shared.unique_images.size()==8 && assets::walker_rgba_bytes(shared)==6016,
          "market append duplicated core/Inspector physical images");
}
void rejection_checks(Fixture& fixture,const fs::path& manifest) {
    auto core=core_with_inspector(fixture);const auto before=core;
    const auto fail=[&](Json value,const char* reason) {
        Fixture::save(manifest,value);
        rejects([&]{assets::append_market_visual_profile(fixture.data,manifest,core);},reason);
        check(same_profile(core,before),"failed market supplement changed core/FI images/roles/schema");
    };
    auto invalid=market();invalid["schema_version"]=3;fail(invalid,"schema3 supplement accepted as market");
    invalid=market();invalid["roles"].erase("distributor");fail(invalid,"one-family automatic supplement activated");
    invalid=market();invalid["roles"]["clay"]=Fixture::legacy()["roles"]["clay"];
    fail(invalid,"market supplement replaced core");
    for (const auto name:{"supplier","distributor"}) {
        invalid=market();invalid["roles"][name]["clips"].erase("neg_y");fail(invalid,"missing automatic direction activated");
        invalid=market();invalid["roles"][name]["clips"]["neg_x"]={"d1-0"};fail(invalid,"single-frame animation activated");
        invalid=market();invalid["roles"][name]["clips"]["pos_y"]={"d2-0","d2-0"};fail(invalid,"identical animation activated");
        invalid=market();invalid["roles"][name].erase("clip_id");fail(invalid,"unidentified clip activated");
        invalid=market();invalid["roles"][name]["clip_id"]=std::string(65,'x');fail(invalid,"overlong clip identifier activated");
        invalid=market();invalid["roles"][name]["ticks_per_frame"]=std::numeric_limits<std::uint64_t>::max();
        fail(invalid,"overflowing cadence activated");
    }
    invalid=market();invalid["roles"]["distributor"]["frames"][7]["image_index"]=17;
    fail(invalid,"bad later distributor frame committed the supplier");
    invalid=market();invalid["roles"]["distributor"]["frames"][7]["archive"]="../escape.sg3";
    fail(invalid,"late traversal committed first family");
    invalid=market();invalid["roles"]["supplier"]["frames"][0]["foot_anchor"]={0,1e100};
    fail(invalid,"nonfinite anchor activated");
    invalid=market();invalid["roles"]["supplier"]["frames"][1]["alias"]="d0-0";
    fail(invalid,"duplicate alias activated");
    const auto archive=fixture.data/"DATA/market.sg3";
    auto bytes=fixture.archive;u32(bytes,40680+15*72+16,1);write(archive,bytes);
    fail(market(),"unsupported mirror activated");write(archive,fixture.archive);
    // Distinct physical records with identical decoded pixels are not an
    // animated direction, even when the alias/record sequence changes.
    bytes=fixture.archive;
    std::copy_n(bytes.begin()+40680+1*72,72,bytes.begin()+40680+3*72);write(archive,bytes);
    fail(market(),"distinct records with identical pixels activated");write(archive,fixture.archive);
    bytes=fixture.archive;bytes[40680+15*72+52]=1;write(archive,bytes);
    fail(market(),"automatic market read unfingerprinted external dependency");write(archive,fixture.archive);
    const auto bitmap=fixture.data/"DATA/market.555",outside=fixture.root/"outside.555";
    bytes=fixture.archive;auto transparent=fixture.bitmap;
    u32(bytes,40680+15*72,static_cast<std::uint32_t>(transparent.size()));
    u32(bytes,40680+15*72+4,36);
    for (int row=0;row<18;++row) transparent.insert(transparent.end(),{255,12});
    write(archive,bytes);write(bitmap,transparent);
    fail(market(),"all-transparent later frame activated");
    write(archive,fixture.archive);write(bitmap,fixture.bitmap);
    fs::rename(bitmap,outside);fail(market(),"missing market bitmap activated");fs::rename(outside,bitmap);
    fs::rename(bitmap,outside);fs::create_symlink(outside,bitmap);
    fail(market(),"bitmap escape activated");fs::remove(bitmap);fs::rename(outside,bitmap);
    // Duplicate keys must reject before either family can be staged.
    {
        std::ofstream out(manifest);
        out<<"{\"schema_version\":4,\"mode\":\"curated_walker_preview\",\"roles\":{\"supplier\":{} ,\"supplier\":{}}}";
    }
    rejects([&]{assets::append_market_visual_profile(fixture.data,manifest,core);},"duplicate family key accepted");
    check(same_profile(core,before),"duplicate key changed active profile");
}
void budget_checks(Fixture& fixture,const fs::path& manifest) {
    Fixture::save(manifest,market());
    const auto base=core_with_inspector(fixture);
    auto exact=base;
    for (std::size_t i=0;i<229;++i) {
        auto frame=exact.roles[0]->frames[0];frame.alias="old-"+std::to_string(i);
        exact.roles[0]->frames.push_back(std::move(frame));
    }
    auto overflow=exact;
    overflow.roles[0]->frames.push_back(overflow.roles[0]->frames[0]);
    const auto unchanged=overflow;
    rejects([&]{assets::append_market_visual_profile(fixture.data,manifest,overflow);},
            "core/FI/market aliases bypassed global256 limit");
    check(same_profile(overflow,unchanged),"alias budget failure changed existing profile");
    assets::append_market_visual_profile(fixture.data,manifest,exact);
    std::size_t aliases=0;for (const auto& role:exact.roles) if (role) aliases+=role->frames.size();
    check(aliases==256,"exact global alias limit rejected");
    auto assets_exact=base;
    while (assets_exact.unique_images.size()<248) assets_exact.unique_images.push_back({1,1,{0,0,0,255}});
    auto assets_overflow=assets_exact;assets_overflow.unique_images.push_back({1,1,{0,0,0,255}});
    const auto old_asset_count=assets_overflow.unique_images.size();
    rejects([&]{assets::append_market_visual_profile(fixture.data,manifest,assets_overflow);},
            "unique asset budget bypassed by second family");
    check(assets_overflow.unique_images.size()==old_asset_count && !assets_overflow.roles[4] &&
          !assets_overflow.roles[5] && assets_overflow.schema_version==3,"asset failure committed a partial family");
    assets::append_market_visual_profile(fixture.data,manifest,assets_exact);
    check(assets_exact.unique_images.size()==256,"exact unique asset bound rejected");
    auto rgba=assets::load_walker_visual_profile(fixture.data,fixture.core);
    auto& image=rgba.unique_images[0];image.width=4096;image.height=4096;
    image.pixels.assign(static_cast<std::size_t>(assets::walker_max_rgba_bytes),255);
    const auto* pixels=image.pixels.data();
    check(assets::walker_rgba_bytes(rgba)==assets::walker_max_rgba_bytes,"exact RGBA limit rejected");
    rejects([&]{assets::append_market_visual_profile(fixture.data,manifest,rgba);},"market bypassed global RGBA budget");
    check(rgba.schema_version==2 && rgba.unique_images.size()==1 && rgba.unique_images[0].pixels.data()==pixels &&
          !rgba.roles[4] && !rgba.roles[5] && assets::walker_rgba_bytes(rgba)==assets::walker_max_rgba_bytes,
          "RGBA budget failure replaced or copied core");
}
}
int main() {
    try {
        Fixture fixture;
        write(fixture.data/"DATA/market.sg3",fixture.archive);
        write(fixture.data/"DATA/market.555",fixture.bitmap);
        const auto manifest=fixture.root/"market.json";
        schema_checks(fixture,manifest);flip_checks(fixture,manifest);append_checks(fixture,manifest);
        rejection_checks(fixture,manifest);budget_checks(fixture,manifest);
        std::cout<<"PASS schema1-3 preserved; schema4 families; complete/partial clips; canonical physical dedupe; "
            "independent anchors; both append orders; atomic fallback; global256 alias/asset and64MiB bounds\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
