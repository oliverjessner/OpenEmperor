#include "persistence/RecoveryStore.h"

#include "platform/AtomicReplace.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <limits>
#include <random>
#include <regex>
#include <set>
#include <stdexcept>

namespace openemperor::persistence {
namespace fs=std::filesystem;
namespace {
using json=nlohmann::json;
constexpr std::uint32_t metadata_version=1;
constexpr std::uintmax_t max_metadata_bytes=1024U*1024U;
constexpr std::size_t max_histories=128,max_directory_entries=4096;

void require(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
bool below(const fs::path& path,const fs::path& root) {
    auto p=path.begin(),r=root.begin();
    for (;r!=root.end();++r,++p) if (p==path.end() || *p!=*r) return false;
    return true;
}
bool valid_id(const std::string& value) {
    static const std::regex pattern("history-[0-9]+-[0-9a-f]{8,32}");
    return value.size()<=80 && std::regex_match(value,pattern);
}
bool valid_filename(const std::string& value) {
    if (value.empty() || value.size()>128 || fs::path(value).filename()!=fs::path(value)) return false;
    if (value=="." || value=="..") return false;
    return value=="start.json" ||
        (value.starts_with("checkpoint-") && fs::path(value).extension()==".json");
}
std::string local_time() {
    const auto now=std::chrono::system_clock::now();
    const auto value=std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm,&value);
#else
    localtime_r(&value,&tm);
#endif
    char buffer[32]{};
    if (std::strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",&tm)==0)
        return "unknown";
    return buffer;
}
std::string new_history_id() {
    std::random_device device;
    const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
    constexpr char digits[]="0123456789abcdef";
    std::string random(16,'0');
    for (auto& c:random) c=digits[device()&15U];
    return "history-"+std::to_string(stamp)+"-"+random;
}
int population(const SaveDocument& document) {
    int result=0;
    if (simulation::original_production_enabled(document.world.profile,document.world.rule_version)) {
        require(document.world.original_economy.has_value(),"original recovery population authority is missing");
        for (const auto& house:document.world.original_economy->houses) {
            const auto occupants=house.house.occupants;
            require(occupants>=0 && occupants<=std::numeric_limits<int>::max()-result,
                    "original recovery population overflow");
            result+=occupants;
        }
        return result;
    }
    for (const auto& building:document.world.buildings)
        if (building.placed && building.kind==simulation::Object::Household) {
            if (building.population>std::numeric_limits<int>::max()-result)
                throw std::runtime_error("recovery population overflow");
            result+=building.population;
        }
    return result;
}
json parent_json(const std::optional<RecoveryParent>& parent) {
    if (!parent) return nullptr;
    return {{"history_id",parent->history_id},{"checkpoint_sequence",parent->sequence}};
}
std::optional<RecoveryParent> parse_parent(const json& value) {
    if (value.is_null()) return std::nullopt;
    require(value.is_object() && value.contains("history_id") && value.at("history_id").is_string() &&
            value.contains("checkpoint_sequence") && value.at("checkpoint_sequence").is_number_unsigned(),
            "invalid recovery parent metadata");
    RecoveryParent result{value.at("history_id").get<std::string>(),
                          value.at("checkpoint_sequence").get<std::uint64_t>()};
    require(valid_id(result.history_id),"invalid recovery parent history ID");
    return result;
}
json entry_json(const RecoveryEntry& entry) {
    return {{"sequence",entry.sequence},{"kind",entry.start_point?"start":"autosave"},
        {"filename",entry.filename},{"tick",entry.tick},{"map",entry.map},
        {"profile",entry.profile},{"rule_version",entry.rule_version},{"schema",entry.schema},
        {"population",entry.population},{"treasury",entry.treasury},
        {"created_local",entry.created_local}};
}
RecoveryEntry parse_entry(const json& value,const fs::path& dir,const std::string& history,
                          const std::optional<RecoveryParent>& parent) {
    require(value.is_object(),"invalid recovery checkpoint metadata");
    RecoveryEntry result; result.history_id=history; result.parent=parent;
    auto unsigned_field=[&](const char* name) {
        require(value.contains(name) && value.at(name).is_number_unsigned(),
                std::string("invalid recovery field: ")+name);
        return value.at(name).get<std::uint64_t>();
    };
    auto string_field=[&](const char* name,std::size_t limit) {
        require(value.contains(name) && value.at(name).is_string(),
                std::string("invalid recovery field: ")+name);
        auto text=value.at(name).get<std::string>();
        require(text.size()<=limit,std::string("recovery field too long: ")+name);
        return text;
    };
    result.sequence=unsigned_field("sequence");
    const auto kind=string_field("kind",16);
    require(kind=="start" || kind=="autosave","invalid recovery checkpoint kind");
    result.start_point=kind=="start";
    result.filename=string_field("filename",128);
    require(valid_filename(result.filename),"unsafe recovery checkpoint filename");
    result.path=dir/result.filename;
    result.tick=unsigned_field("tick");
    result.map=string_field("map",4096); result.profile=string_field("profile",128);
    const auto rule=unsigned_field("rule_version"),schema=unsigned_field("schema");
    require(rule<=std::numeric_limits<std::uint32_t>::max() &&
            schema<=std::numeric_limits<std::uint32_t>::max(),"recovery version value too large");
    result.rule_version=static_cast<std::uint32_t>(rule);
    result.schema=static_cast<std::uint32_t>(schema);
    require(value.contains("population") && value.at("population").is_number_integer() &&
            value.contains("treasury") && value.at("treasury").is_number_integer(),
            "invalid recovery summary values");
    if (result.profile==simulation::city_v16_profile_name && simulation::original_production_enabled(simulation::RulesProfile::CityV16,result.rule_version)) {
        const auto& people=value.at("population");const auto& funds=value.at("treasury");
        require(!people.is_number_unsigned() || people.get<std::uint64_t>()<=static_cast<std::uint64_t>(std::numeric_limits<int>::max()),
                "original recovery population exceeds int");
        require(people.get<std::int64_t>()>=0 && people.get<std::int64_t>()<=std::numeric_limits<int>::max(),
                "invalid original recovery population");
        require(!funds.is_number_unsigned() || funds.get<std::uint64_t>()<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),
                "original recovery treasury exceeds signed range");
    }
    result.population=value.at("population").get<int>();
    result.treasury=value.at("treasury").get<std::int64_t>();
    result.created_local=string_field("created_local",64);
    return result;
}
struct Metadata {
    std::string history;
    std::optional<RecoveryParent> parent;
    std::uint64_t next_sequence=1;
    std::vector<RecoveryEntry> entries;
};
json metadata_json(const Metadata& metadata);
Metadata parse_metadata(const fs::path& file) {
    std::error_code ec;
    require(!fs::is_symlink(file,ec) && !ec && fs::is_regular_file(file,ec) && !ec,
            "recovery metadata is missing or unsafe");
    const auto size=fs::file_size(file,ec);
    require(!ec && size>0 && size<=max_metadata_bytes,"recovery metadata size is invalid");
    std::ifstream input(file,std::ios::binary);
    require(input.good(),"recovery metadata cannot be read");
    std::vector<std::set<std::string>> keys;bool duplicate_key=false;
    const auto value=json::parse(input,[&](int depth,json::parse_event_t event,json& parsed) {
        if (event==json::parse_event_t::object_start || event==json::parse_event_t::array_start)
            require(depth<=32,"recovery metadata nesting exceeds bounded depth");
        if (event==json::parse_event_t::object_start) keys.emplace_back();
        else if (event==json::parse_event_t::key) {
            require(!keys.empty(),"invalid recovery JSON object nesting");
            if (!keys.back().insert(parsed.get<std::string>()).second) duplicate_key=true;
        } else if (event==json::parse_event_t::object_end) {
            require(!keys.empty(),"invalid recovery JSON object nesting");keys.pop_back();
        }
        return true;
    });
    require(value.is_object() && value.contains("version") && value.at("version").is_number_unsigned() &&
            value.at("version").get<std::uint32_t>()==metadata_version,
            "unknown recovery metadata version");
    require(value.contains("history_id") && value.at("history_id").is_string(),"missing recovery history ID");
    Metadata result; result.history=value.at("history_id").get<std::string>();
    require(valid_id(result.history),"invalid recovery history ID");
    result.parent=value.contains("parent") ? parse_parent(value.at("parent")):std::nullopt;
    require(value.contains("next_sequence") && value.at("next_sequence").is_number_unsigned(),
            "invalid recovery sequence");
    result.next_sequence=value.at("next_sequence").get<std::uint64_t>();
    require(result.next_sequence>0,"invalid recovery sequence");
    require(value.contains("checkpoints") && value.at("checkpoints").is_array() &&
            value.at("checkpoints").size()<=recovery_checkpoint_limit+1,
            "invalid recovery checkpoint list");
    for (const auto& item:value.at("checkpoints"))
        result.entries.push_back(parse_entry(item,file.parent_path(),result.history,result.parent));
    require(std::count_if(result.entries.begin(),result.entries.end(),
            [](const auto& e){return e.start_point;})==1,"recovery history must contain one start point");
    std::vector<std::uint64_t> sequences;
    std::vector<std::string> filenames;
    for (const auto& entry:result.entries) {
        require(entry.start_point ? entry.sequence==0 && entry.filename=="start.json":
                entry.sequence>0 && entry.sequence<result.next_sequence,
                "invalid recovery checkpoint sequence");
        require(std::find(sequences.begin(),sequences.end(),entry.sequence)==sequences.end() &&
                std::find(filenames.begin(),filenames.end(),entry.filename)==filenames.end(),
                "duplicate recovery checkpoint metadata");
        sequences.push_back(entry.sequence);filenames.push_back(entry.filename);
    }
    if (std::any_of(result.entries.begin(),result.entries.end(),[](const auto& entry) {
        return entry.profile==simulation::city_v16_profile_name&&simulation::original_production_enabled(simulation::RulesProfile::CityV16,entry.rule_version);
    })) {
        require(!duplicate_key,"original recovery metadata contains a duplicate key");
        require(value==metadata_json(result),"original recovery metadata contains missing, additional or noncanonical fields");
    }
    return result;
}
json metadata_json(const Metadata& metadata) {
    json entries=json::array();
    for (const auto& entry:metadata.entries) entries.push_back(entry_json(entry));
    return {{"version",metadata_version},{"history_id",metadata.history},
        {"parent",parent_json(metadata.parent)},{"next_sequence",metadata.next_sequence},
        {"checkpoints",std::move(entries)}};
}
void ensure_directory_safe(const fs::path& app_root,const fs::path& data_root,const fs::path& recovery) {
    std::error_code ec;
    const auto data=fs::canonical(data_root,ec);
    require(!ec,"original data root cannot be resolved");
    const auto app=fs::weakly_canonical(app_root,ec);
    require(!ec && !below(app,data),"recovery root must be outside original data");
    fs::create_directories(recovery);
    require(!fs::is_symlink(recovery) && fs::is_directory(recovery),"recovery root is unsafe");
    const auto resolved=fs::canonical(recovery,ec);
    require(!ec && below(resolved,app) && !below(resolved,data),"recovery root escapes app storage");
}
std::uintmax_t storage_usage(const fs::path& root) {
    std::error_code ec;
    if (!fs::exists(root,ec)) return 0;
    std::uintmax_t total=0; std::size_t count=0;
    for (fs::recursive_directory_iterator it(root,fs::directory_options::none,ec),end;
         it!=end && !ec;it.increment(ec)) {
        require(++count<=max_directory_entries,"recovery storage entry limit exceeded");
        const auto status=it->symlink_status(ec);
        require(!ec && !fs::is_symlink(status),"symlink in recovery storage rejected");
        if (fs::is_regular_file(status)) {
            const auto size=it->file_size(ec);
            require(!ec && size<=std::numeric_limits<std::uintmax_t>::max()-total,
                    "recovery storage size overflow");
            total+=size;
        }
    }
    require(!ec,"recovery storage cannot be scanned");
    return total;
}
RecoveryEntry make_entry(const std::string& history,const std::optional<RecoveryParent>& parent,
                         std::uint64_t sequence,bool start,const std::string& filename,
                         const fs::path& path,const SaveDocument& saved) {
    RecoveryEntry e; e.history_id=history;e.parent=parent;e.sequence=sequence;e.start_point=start;
    e.filename=filename;e.path=path;e.map=saved.map_relative.generic_string();
    e.profile=simulation::rules_profile_name(saved.world.profile);e.created_local=local_time();
    e.tick=saved.world.ticks;e.rule_version=saved.world.rule_version;
    e.schema=saved.source_schema_version;e.population=population(saved);e.treasury=saved.world.treasury;
    return e;
}
void validate_policy_readback(const SaveDocument& saved,const SaveDocument& source) {
    if (simulation::original_production_enabled(source.world.profile,source.world.rule_version)) {
        require(saved.source_schema_version==(simulation::original_deadline_food_dispatch_enabled(source.world.profile,source.world.rule_version) ? 34U:
                    simulation::original_deadline_service_dispatch_enabled(source.world.profile,source.world.rule_version) ? 33U:
                    simulation::original_deadline_fire_dispatch_enabled(source.world.profile,source.world.rule_version) ? 32U:
                    simulation::original_inspector_coordination_enabled(source.world.profile,source.world.rule_version) ? 31U:
                    simulation::original_cardinal_water_enabled(source.world.profile,source.world.rule_version) ? 30U:
                    simulation::original_market_logistics_enabled(source.world.profile,source.world.rule_version) ? 29U:
                    simulation::original_sustainable_economy_enabled(source.world.profile,source.world.rule_version) ? 28U:
                    simulation::original_fire_protection_enabled(source.world.profile,source.world.rule_version) ? 27U:26U) && saved.world==source.world &&
                saved.map_relative==source.map_relative && saved.map_sha256==source.map_sha256 &&
                saved.buildable_sha256==source.buildable_sha256 &&
                saved.map_permissions_policy_version==source.map_permissions_policy_version &&
                saved.map_permissions_sha256==source.map_permissions_sha256 && !saved.migrated_from_schema1,
                "original recovery read-back differs from schema26 world or source policy");
        return;
    }
    if (source.world.profile!=simulation::RulesProfile::CityV16 ||
        (source.world.rule_version<3 || source.world.rule_version>6)) return;
    const auto expected_schema=source.world.rule_version==3 ? 19U:source.world.rule_version==4 ? 20U:source.world.rule_version==5 ? 21U:22U;
    require(saved.source_schema_version==expected_schema && saved.world==source.world &&
            saved.map_relative==source.map_relative && saved.map_sha256==source.map_sha256 &&
            saved.buildable_sha256==source.buildable_sha256 &&
            saved.map_permissions_policy_version==source.map_permissions_policy_version &&
            saved.map_permissions_sha256==source.map_permissions_sha256,
            source.world.rule_version==3 ? "recovery read-back differs from schema19 world or map policy":
                source.world.rule_version==4 ? "recovery read-back differs from schema20 world or map policy":
                "recovery read-back differs from schema21 world or map policy");
}
bool validate_recovery_access(const SaveDocument& document,RecoveryAccess access) {
    if (!simulation::original_economy_enabled(document.world.profile,document.world.rule_version)) return false;
    require(access==RecoveryAccess::OriginalProduction &&
            simulation::original_production_enabled(document.world.profile,document.world.rule_version),
            "schema23 development saves are unavailable in normal recovery histories");
    require((document.source_schema_version==0 || document.source_schema_version==
                (simulation::original_deadline_food_dispatch_enabled(document.world.profile,document.world.rule_version) ? 34U:
                 simulation::original_deadline_service_dispatch_enabled(document.world.profile,document.world.rule_version) ? 33U:
                 simulation::original_deadline_fire_dispatch_enabled(document.world.profile,document.world.rule_version) ? 32U:
                 simulation::original_inspector_coordination_enabled(document.world.profile,document.world.rule_version) ? 31U:
                 simulation::original_cardinal_water_enabled(document.world.profile,document.world.rule_version) ? 30U:
                 simulation::original_market_logistics_enabled(document.world.profile,document.world.rule_version) ? 29U:
                 simulation::original_sustainable_economy_enabled(document.world.profile,document.world.rule_version) ? 28U:
                 simulation::original_fire_protection_enabled(document.world.profile,document.world.rule_version) ? 27U:26U)) &&
            document.world.original_production && document.world.original_production->development_revision==
                simulation::kOriginalProductionDevelopmentRevision &&
            document.world.original_economy && document.world.original_economy->model_source,
            "original recovery requires its exact Rule10/11/12/13/14/15/16/17/18 schema and revision27 model source");
    return true;
}
using OriginalSourceCheck=std::optional<original::ModelFingerprintResult>;
void validate_original_source(const SaveDocument& document,const fs::path& root,OriginalSourceCheck& checked) {
    if (!checked) checked=original::inspect_model_source(root);
    require(checked->fingerprint.has_value(),"original recovery model source is unavailable");
    const auto& source=*document.world.original_economy->model_source;
    require(source.format_version==original::model_schema_version &&
            static_cast<std::size_t>(source.difficulty)<original::difficulty_count &&
            source.sha256==checked->fingerprint->sha256 && source.byte_count==checked->fingerprint->byte_count,
            "original recovery model source identity differs from the current installation");
}
SaveDocument checked_entry(const RecoveryEntry& entry,RecoveryAccess access,const fs::path& root,
                           OriginalSourceCheck& model) {
    std::error_code ec;
    require(!fs::is_symlink(entry.path,ec) && !ec,"unsafe recovery checkpoint symlink");
    auto doc=read_save(entry.path);
    if (validate_recovery_access(doc,access)) validate_original_source(doc,root,model);
    require(doc.world.ticks==entry.tick && doc.map_relative.generic_string()==entry.map &&
            simulation::rules_profile_name(doc.world.profile)==entry.profile &&
            doc.world.rule_version==entry.rule_version && doc.source_schema_version==entry.schema &&
            population(doc)==entry.population && doc.world.treasury==entry.treasury,
            "recovery metadata does not match checkpoint");
    return doc;
}
void validate_entry_document(RecoveryEntry& entry,RecoveryAccess access,const fs::path& root,OriginalSourceCheck& model) {
    try {(void)checked_entry(entry,access,root,model);}
    catch (const std::exception& error) {entry.error=error.what();}
}
bool has_original_entry(const Metadata& metadata) {
    return std::any_of(metadata.entries.begin(),metadata.entries.end(),[](const auto& entry) {
        return entry.profile==simulation::city_v16_profile_name &&
            simulation::original_economy_enabled(simulation::RulesProfile::CityV16,entry.rule_version);
    });
}
void validate_original_sequence(const Metadata& metadata) {
    std::vector<std::uint64_t> periodic;
    for (const auto& entry:metadata.entries) if (!entry.start_point) periodic.push_back(entry.sequence);
    std::sort(periodic.begin(),periodic.end());
    require(metadata.next_sequence==(periodic.empty()?1:periodic.back()+1),
            "original recovery next sequence does not follow its checkpoints");
    require(periodic.size()==std::min<std::uint64_t>(metadata.next_sequence-1,recovery_checkpoint_limit),
            "original recovery checkpoint sequence is incomplete");
    for (std::size_t i=0;i<periodic.size();++i)
        require(periodic[i]==metadata.next_sequence-static_cast<std::uint64_t>(periodic.size())+i,
                "original recovery checkpoint sequence contains a gap");
}
void same_original_identity(const SaveDocument& a,const SaveDocument& b) {
    require(simulation::original_production_enabled(a.world.profile,a.world.rule_version) &&
            simulation::original_production_enabled(b.world.profile,b.world.rule_version) &&
            a.world.rule_version==b.world.rule_version &&
            a.world.original_production->development_revision==b.world.original_production->development_revision &&
            a.map_relative==b.map_relative && a.map_sha256==b.map_sha256 && a.buildable_sha256==b.buildable_sha256 &&
            a.map_permissions_policy_version==b.map_permissions_policy_version &&
            a.map_permissions_sha256==b.map_permissions_sha256 &&
            a.world.original_economy->model_source==b.world.original_economy->model_source,
            "original recovery history source, map, model, difficulty or version differs");
}
struct OriginalHistoryState {
    std::optional<SaveDocument> first;
    std::uint64_t last_tick=0;
};
OriginalHistoryState inspect_original_entries(std::vector<RecoveryEntry>& entries,RecoveryAccess access,
                                             const fs::path& root,OriginalSourceCheck& model) {
    std::vector<std::size_t> order(entries.size());
    for (std::size_t i=0;i<order.size();++i) order[i]=i;
    std::sort(order.begin(),order.end(),[&](auto a,auto b) {return entries[a].sequence<entries[b].sequence;});
    OriginalHistoryState state;
    for (const auto index:order) {
        auto& entry=entries[index];
        try {
            auto document=checked_entry(entry,access,root,model);
            require(validate_recovery_access(document,access),"legacy checkpoint is inside an original recovery history");
            if (state.first) {
                same_original_identity(*state.first,document);
                require(document.world.ticks>=state.last_tick,"original recovery checkpoint moves simulation time backwards");
            }
            state.last_tick=document.world.ticks;
            if (!state.first) state.first=std::move(document);
        } catch (const std::exception& error) {entry.error=error.what();}
    }
    return state;
}
void validate_original_history(const Metadata& metadata,const SaveDocument& document,RecoveryAccess access,
                               const fs::path& root) {
    require(validate_recovery_access(document,access),"legacy documents cannot append to an original recovery history");
    validate_original_sequence(metadata);
    require(metadata.next_sequence<std::numeric_limits<std::uint64_t>::max(),"original recovery sequence is exhausted");
    OriginalSourceCheck model;
    auto entries=metadata.entries;
    const auto state=inspect_original_entries(entries,access,root,model);
    for (const auto& entry:entries) require(entry.loadable(),entry.error);
    require(state.first.has_value(),"original recovery history has no valid checkpoint");
    same_original_identity(*state.first,document);
    require(document.world.ticks>=state.last_tick,"original recovery checkpoint moves simulation time backwards");
}
}

RecoveryStore::RecoveryStore(fs::path app_root,fs::path data_root,std::uintmax_t budget,RecoveryAccess access)
    : app_root_(std::move(app_root)),data_root_(std::move(data_root)),
      recovery_root_(app_root_/"recovery"),budget_(budget),access_(access) {
    require(budget_>0,"recovery storage budget must be positive");
    ensure_directory_safe(app_root_,data_root_,recovery_root_);
    std::error_code ec;
    app_root_=fs::canonical(app_root_,ec); require(!ec,"app storage root cannot be resolved");
    data_root_=fs::canonical(data_root_,ec); require(!ec,"original data root cannot be resolved");
    recovery_root_=fs::canonical(recovery_root_,ec); require(!ec,"recovery root cannot be resolved");
}

std::string RecoveryStore::create_history(const SaveDocument& document,
                                           const std::vector<std::uint8_t>& buildable,
                                           std::optional<RecoveryParent> parent,
                                           RecoveryWriteFaults faults) {
    const bool original=validate_recovery_access(document,access_);
    if (parent) require(valid_id(parent->history_id),"invalid recovery parent history ID");
    if (original && parent) {
        const auto dir=recovery_root_/parent->history_id;
        require(!fs::is_symlink(dir) && fs::is_directory(dir),"original recovery parent history is missing or unsafe");
        auto metadata=parse_metadata(dir/"history.json");
        require(metadata.history==parent->history_id,"original recovery parent history ID differs");
        validate_original_sequence(metadata);
        OriginalSourceCheck model;
        (void)inspect_original_entries(metadata.entries,access_,data_root_,model);
        const auto selected=std::find_if(metadata.entries.begin(),metadata.entries.end(),[&](const auto& entry) {
            return entry.sequence==parent->sequence;
        });
        require(selected!=metadata.entries.end(),"original recovery parent checkpoint is missing");
        require(selected->loadable(),"original recovery parent checkpoint is unavailable: "+selected->error);
        const auto source=checked_entry(*selected,access_,data_root_,model);
        same_original_identity(source,document);
        require(source.world==document.world,"original recovery branch differs from its selected checkpoint");
    }
    require(storage_usage(recovery_root_)<budget_,"recovery storage budget is full");
    std::string history;
    fs::path dir;
    for (int attempt=0;attempt<64;++attempt) {
        history=new_history_id(); dir=recovery_root_/history;
        std::error_code ec;
        if (fs::create_directory(dir,ec)) break;
        require(!ec,"cannot create recovery history directory");
        history.clear();
    }
    require(!history.empty(),"cannot allocate unique recovery history ID");
    const auto save=dir/"start.json";
    try {
        write_save(save,document,data_root_,buildable,faults.save);
        auto verified=read_save(save);
        validate_policy_readback(verified,document);
        auto entry=make_entry(history,parent,0,true,"start.json",save,verified);
        Metadata metadata{history,parent,1,{entry}};
        const auto metadata_bytes=metadata_json(metadata).dump(2);
        const auto usage=storage_usage(recovery_root_);
        require(usage<=budget_ && metadata_bytes.size()<=budget_-usage,
                "recovery storage budget is full");
        const auto metadata_path=dir/"history.json";
        platform::atomic_replace(metadata_path,metadata_bytes,[&] {
            require(!fs::is_symlink(dir) && !fs::is_symlink(metadata_path),"unsafe recovery metadata path");
        },faults.metadata);
        require(storage_usage(recovery_root_)<=budget_,"recovery storage budget is full");
        return history;
    } catch (...) {
        // Only files created by this failed, unpublished history are eligible for cleanup.
        std::error_code ec; fs::remove(save,ec); fs::remove(dir/"history.json",ec); fs::remove(dir,ec);
        throw;
    }
}

RecoveryEntry RecoveryStore::write_checkpoint(const std::string& history,
                                               const SaveDocument& document,
                                               const std::vector<std::uint8_t>& buildable,
                                               RecoveryWriteFaults faults) {
    const bool original=validate_recovery_access(document,access_);
    require(valid_id(history),"invalid recovery history ID");
    ensure_directory_safe(app_root_,data_root_,recovery_root_);
    const auto dir=recovery_root_/history;
    require(!fs::is_symlink(dir) && fs::is_directory(dir),"recovery history directory is unsafe");
    auto metadata=parse_metadata(dir/"history.json");
    require(metadata.history==history,"recovery history directory and metadata disagree");
    if (original || has_original_entry(metadata)) validate_original_history(metadata,document,access_,data_root_);
    require(storage_usage(recovery_root_)<budget_,"recovery storage budget is full");
    const auto sequence=metadata.next_sequence;
    std::random_device random;
    std::string filename;fs::path save;
    for (int attempt=0;attempt<64;++attempt) {
        filename="checkpoint-"+std::to_string(sequence)+"-"+std::to_string(random())+".json";
        save=dir/filename;
        if (!fs::exists(save)) break;
        filename.clear();
    }
    require(!filename.empty(),"cannot allocate unique recovery checkpoint filename");
    try {
        write_save(save,document,data_root_,buildable,faults.save);
        auto verified=read_save(save);
        validate_policy_readback(verified,document);
        auto entry=make_entry(history,metadata.parent,sequence,false,filename,save,verified);
        require(storage_usage(recovery_root_)<=budget_,"recovery storage budget is full");
        metadata.next_sequence=sequence+1;
        metadata.entries.push_back(entry);
        std::vector<RecoveryEntry> periodic;
        for (const auto& value:metadata.entries) if (!value.start_point) periodic.push_back(value);
        std::sort(periodic.begin(),periodic.end(),[](const auto& a,const auto& b){return a.sequence>b.sequence;});
        std::vector<RecoveryEntry> removed;
        if (periodic.size()>recovery_checkpoint_limit)
            removed.assign(periodic.begin()+static_cast<std::ptrdiff_t>(recovery_checkpoint_limit),periodic.end());
        metadata.entries.erase(std::remove_if(metadata.entries.begin(),metadata.entries.end(),[&](const auto& value) {
            return std::any_of(removed.begin(),removed.end(),[&](const auto& old){return old.filename==value.filename;});
        }),metadata.entries.end());
        const auto metadata_path=dir/"history.json";
        const auto metadata_bytes=metadata_json(metadata).dump(2);
        std::error_code size_error;
        const auto old_metadata_size=fs::file_size(metadata_path,size_error);
        require(!size_error,"recovery metadata size cannot be read");
        const auto usage=storage_usage(recovery_root_);
        require(usage>=old_metadata_size,"recovery storage size accounting failed");
        const auto without_old_metadata=usage-old_metadata_size;
        require(without_old_metadata<=budget_ && metadata_bytes.size()<=
                budget_-without_old_metadata,"recovery storage budget is full");
        platform::atomic_replace(metadata_path,metadata_bytes,[&] {
            require(!fs::is_symlink(dir) && !fs::is_symlink(dir/"history.json"),"unsafe recovery metadata path");
        },faults.metadata);
        // The new, verified checkpoint is now referenced before old referenced files are removed.
        for (const auto& old:removed) {
            require(valid_filename(old.filename),"unsafe rotated recovery filename");
            std::error_code ec; fs::remove(dir/old.filename,ec);
        }
        return entry;
    } catch (...) {
        // A failure before metadata publication leaves all prior recovery points intact.
        std::error_code ec; fs::remove(save,ec);
        throw;
    }
}

RecoveryCatalog RecoveryStore::catalog() const {
    ensure_directory_safe(app_root_,data_root_,recovery_root_);
    RecoveryCatalog result;
    try { result.storage_bytes=storage_usage(recovery_root_); }
    catch (const std::exception&) { result.truncated=true; }
    std::error_code ec; std::vector<fs::path> dirs; std::size_t scanned=0;
    for (fs::directory_iterator it(recovery_root_,fs::directory_options::none,ec),end;
         it!=end && !ec;it.increment(ec)) {
        if (++scanned>max_directory_entries) { result.truncated=true; break; }
        if (fs::is_symlink(it->symlink_status())) {
            RecoveryHistory unsafe;unsafe.history_id=it->path().filename().string();
            unsafe.error="symlink in recovery storage rejected";
            if (result.histories.size()<max_histories) result.histories.push_back(std::move(unsafe));
            else result.truncated=true;
            continue;
        }
        if (!it->is_directory()) continue;
        dirs.push_back(it->path());
    }
    require(!ec,"recovery histories cannot be enumerated");
    std::sort(dirs.begin(),dirs.end(),std::greater<>());
    const auto available=max_histories-result.histories.size();
    if (dirs.size()>available) { dirs.resize(available);result.truncated=true; }
    OriginalSourceCheck original_model;
    for (const auto& dir:dirs) {
        RecoveryHistory history; history.history_id=dir.filename().string();
        try {
            require(valid_id(history.history_id),"invalid recovery history directory name");
            auto metadata=parse_metadata(dir/"history.json");
            require(metadata.history==history.history_id,"recovery history directory and metadata disagree");
            const bool original=has_original_entry(metadata);
            if (original) validate_original_sequence(metadata);
            history.parent=metadata.parent; history.entries=std::move(metadata.entries);
            if (original) (void)inspect_original_entries(history.entries,access_,data_root_,original_model);
            else for (auto& entry:history.entries) validate_entry_document(entry,access_,data_root_,original_model);
            std::sort(history.entries.begin(),history.entries.end(),[](const auto& a,const auto& b) {
                if (a.start_point!=b.start_point) return !a.start_point;
                return a.sequence>b.sequence;
            });
        } catch (const std::exception& error) { history.error=error.what(); }
        result.histories.push_back(std::move(history));
    }
    return result;
}

void RecoveryStore::delete_history(const std::string& history) {
    require(valid_id(history),"invalid recovery history ID");
    const auto dir=recovery_root_/history;
    require(!fs::is_symlink(dir) && fs::is_directory(dir),"recovery history directory is unsafe");
    bool metadata_valid=false;
    try {
        const auto metadata=parse_metadata(dir/"history.json");
        require(metadata.history==history,"recovery history directory and metadata disagree");
        metadata_valid=true;
    } catch (const std::exception&) {}
    // Enumerate only this already validated directory and accept only names generated by
    // RecoveryStore. This also finds safe orphan files left before metadata publication.
    std::vector<fs::path> files;
    std::error_code scan_error;std::size_t scanned=0;
    for (fs::directory_iterator it(dir,fs::directory_options::none,scan_error),end;
         it!=end && !scan_error;it.increment(scan_error)) {
        require(++scanned<=max_directory_entries,"recovery history has too many files");
        const auto name=it->path().filename().string();
        require(!fs::is_symlink(it->symlink_status()),"recovery history contains a symlink");
        if (it->is_regular_file() && (name=="history.json" || valid_filename(name)))
            files.push_back(it->path());
    }
    require(!scan_error,"cannot enumerate recovery history");
    require(metadata_valid || !files.empty(),"corrupt recovery history contains no managed files");
    for (const auto& path:files) {
        require(path.parent_path()==dir && !fs::is_symlink(path),"unsafe recovery deletion path");
        std::error_code ec;
        if (fs::exists(path,ec)) require(fs::remove(path,ec) && !ec,"cannot delete recovery file");
    }
    std::error_code ec;
    // Refuse recursive removal: unknown/orphan files deliberately keep the directory non-empty.
    (void)fs::remove(dir,ec);
}

} // namespace openemperor::persistence
