#pragma once

// Opt-in QA only. One ordinary bounded Recovery history verifies selected
// event checkpoints; this helper supplies no World commands or gameplay state.
#include "persistence/RecoveryStore.h"
#include "simulation/World.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace oe_checkpoint_recovery {
namespace fs=std::filesystem;
namespace save=openemperor::persistence;
namespace sim=openemperor::simulation;
using Json=nlohmann::json;
inline constexpr const char* contract="qa-event-checkpoint-recovery-v1";
inline constexpr const char* publication_ledger_contract="qa-event-recovery-publication-ledger-v1";
inline void require(bool accepted,const char* reason) {
    if(!accepted)throw std::runtime_error(reason);
}
inline std::string bytes(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);
    require(bool(input),"Event checkpoint payload is unavailable");
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
class Proof {
    save::RecoveryStore store_;
    std::string history_;
    Json events_=Json::array();
    fs::path ledger_path_;
    std::function<std::string(std::string_view)> publication_digest_;
    Json publications_=Json::array();
public:
    Proof(const fs::path& output,const fs::path& data,
          std::function<std::string(std::string_view)> publication_digest={}):
        store_(output/"checkpoint-recovery",data,save::recovery_storage_budget_bytes,
               save::RecoveryAccess::OriginalProduction),
        ledger_path_(output/"checkpoint-recovery"/"publication-ledger.json"),
        publication_digest_(std::move(publication_digest)) {}
    template<class Restore>
    void capture(const std::string& label,const save::SaveDocument& document,
                 const std::vector<std::uint8_t>& mask,const sim::World& world,
                 const fs::path& manual,Restore restore) {
        require(events_.size()<32,"Selected event Recovery proof exceeds32 checkpoints");
        if(history_.empty())history_=store_.create_history(document,mask);
        const auto entry=store_.write_checkpoint(history_,document,mask);
        require(entry.loadable()&&document.source_schema_version!=0&&
                entry.schema==document.source_schema_version&&entry.rule_version==world.rule_version()&&
                entry.tick==world.ticks(),"Event Recovery publication lost exact identity or clock");
        auto fresh=restore(save::read_save(entry.path));
        require(fresh.snapshot()==world.snapshot(),"Fresh event Recovery World differs");
        require(bytes(entry.path)==bytes(manual),"Event Recovery payload differs from canonical manual save");
        const Json event={{"label",label},{"tick",world.ticks()},{"schema",entry.schema},
            {"sequence",entry.sequence},{"filename",entry.filename},
            {"canonical_payload_exact",true},{"fresh_restore_exact",true}};
        if(publication_digest_) {
            // Optional QA evidence only: preserve the ordinary metadata as it
            // actually existed before later rotation removes an older entry.
            // No payload or RecoveryStore format/retention is changed.
            const auto metadata_path=entry.path.parent_path()/"history.json";
            require(fs::is_regular_file(metadata_path)&&!fs::is_symlink(metadata_path)&&
                    fs::file_size(metadata_path)<=1024U*1024U,"Publication metadata is unavailable or unbounded");
            const auto metadata=Json::parse(bytes(metadata_path));
            const auto& rows=metadata.at("checkpoints");
            const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value) {
                return value.at("sequence")==entry.sequence;
            });
            require(metadata.at("history_id")==history_&&metadata.at("next_sequence")==entry.sequence+1&&
                    row!=rows.end()&&row->at("filename")==entry.filename&&row->at("tick")==world.ticks()&&
                    row->at("schema")==entry.schema&&row->at("rule_version")==world.rule_version(),
                    "Published Recovery entry differs from its actual metadata");
            publications_.push_back({{"event",event},{"canonical_payload_sha256",publication_digest_(bytes(manual))},
                {"history_metadata",metadata}});
            const auto ledger=Json{{"contract",publication_ledger_contract},{"publications",publications_}}.dump(2)+"\n";
            require(ledger.size()<=1024U*1024U,"Bounded publication ledger exceeds its QA allowance");
            std::ofstream output(ledger_path_,std::ios::binary|std::ios::trunc);
            require(bool(output),"Cannot write QA publication ledger");
            output<<ledger;output.close();require(bool(output),"Cannot complete QA publication ledger");
        }
        events_.push_back(event);
    }
    Json report()const {
        return {{"contract",contract},{"history_count",history_.empty()?0:1},
            {"verified_events",events_},{"checkpoint_limit",32},
            {"retention","Ordinary bounded Recovery rotation may evict earlier payloads; each event was compared at publication"}};
    }
};
} // namespace oe_checkpoint_recovery
