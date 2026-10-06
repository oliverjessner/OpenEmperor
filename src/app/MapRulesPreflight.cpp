#include "app/MapRulesPreflight.h"

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace openemperor::menu {
namespace {
bool terminal(maps::MapRulesStatus status) {
    return status==maps::MapRulesStatus::MapRulesChecked ||
           status==maps::MapRulesStatus::UnsupportedForRules ||
           status==maps::MapRulesStatus::InputError;
}
bool content_proof(const std::string& hash) {
    return hash.size()==64 && std::all_of(hash.begin(),hash.end(),[](char c){
        return (c>='0' && c<='9') || (c>='a' && c<='f');
    });
}
void bound_message(maps::MapRulesResult& result) {
    constexpr std::size_t message_limit=2048;
    if (result.reason.size()>message_limit) result.reason.resize(message_limit);
    if (result.detail.size()>message_limit) result.detail.resize(message_limit);
}
}

struct MapRulesPreflight::Worker {
    struct Request { std::uint64_t generation; maps::MapRulesRequest selection; };
    struct Completion { std::uint64_t generation; maps::MapRulesResult result; };
    explicit Worker(Checker check):checker(std::move(check)) {}
    Checker checker;
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::optional<Request> pending;
    std::optional<Completion> completed;
    std::thread thread;
    std::uint64_t desired_generation=0;
    bool active=false,stopping=false;

    void run() {
        for (;;) {
            std::unique_lock lock(mutex);
            changed.wait(lock,[&]{return stopping || pending.has_value();});
            if (stopping) return;
            auto request=std::move(*pending);
            pending.reset(); active=true;
            lock.unlock();
            maps::MapRulesResult result;
            try {
                result=checker(request.selection);
                if (result.request!=request.selection || !terminal(result.status) ||
                    (result.status==maps::MapRulesStatus::MapRulesChecked &&
                     !content_proof(result.input_sha256))) {
                    result={}; result.request=request.selection;
                    result.status=maps::MapRulesStatus::InputError;
                    result.reason="The map check did not return a valid result. Check again.";
                }
            } catch (const std::exception& error) {
                result.request=request.selection;
                result.status=maps::MapRulesStatus::InputError;
                result.reason="The map check could not finish. Check again.";
                result.detail=error.what();
            } catch (...) {
                result.request=request.selection;
                result.status=maps::MapRulesStatus::InputError;
                result.reason="The map check could not finish. Check again.";
            }
            bound_message(result);
            lock.lock(); active=false;
            // Obsolete work is neither cached nor presented as unsupported.
            if (!stopping && request.generation==desired_generation)
                completed=Completion{request.generation,std::move(result)};
        }
    }
};

MapRulesPreflight::MapRulesPreflight(Checker checker)
    :worker_(std::make_unique<Worker>(std::move(checker))) {
    if (!worker_->checker) throw std::invalid_argument("map rules checker is empty");
    cache_.reserve(cache_capacity);
}
MapRulesPreflight::~MapRulesPreflight() { shutdown(); }

std::uint64_t MapRulesPreflight::next_generation() {
    if (generation_==std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("map check generation exhausted");
    return ++generation_;
}
std::uint64_t MapRulesPreflight::select(maps::MapRulesRequest request) {
    std::lock_guard lock(worker_->mutex);
    if (worker_->stopping) throw std::logic_error("map check controller is shut down");
    const auto id=next_generation();
    selected_=std::move(request);
    result_={}; result_.request=*selected_;
    result_.status=maps::MapRulesStatus::Checking;
    worker_->desired_generation=id;
    worker_->completed.reset();
    worker_->pending=Worker::Request{id,*selected_};
    if (!worker_->thread.joinable()) {
        auto* const worker=worker_.get();
        worker_->thread=std::thread([worker]{worker->run();});
    }
    worker_->changed.notify_one();
    return id;
}
std::uint64_t MapRulesPreflight::recheck() {
    if (!selected_) return generation_;
    return select(*selected_);
}
void MapRulesPreflight::invalidate() {
    std::lock_guard lock(worker_->mutex);
    worker_->desired_generation=next_generation();
    worker_->pending.reset(); worker_->completed.reset();
    selected_.reset(); result_={};
}
bool MapRulesPreflight::poll() {
    std::optional<Worker::Completion> completed;
    {
        std::lock_guard lock(worker_->mutex);
        completed=std::move(worker_->completed);
        worker_->completed.reset();
    }
    if (!completed || !selected_ || completed->generation!=generation_ ||
        completed->result.request!=*selected_) return false;
    result_=std::move(completed->result);
    remember(result_);
    return true;
}
bool MapRulesPreflight::can_start() const {
    return selected_ && result_.request==*selected_ &&
           result_.status==maps::MapRulesStatus::MapRulesChecked && content_proof(result_.input_sha256);
}
void MapRulesPreflight::remember(const maps::MapRulesResult& result) {
    // I/O errors are retryable, not durable compatibility annotations.
    if (!content_proof(result.input_sha256) ||
        (result.status!=maps::MapRulesStatus::MapRulesChecked &&
         result.status!=maps::MapRulesStatus::UnsupportedForRules)) return;
    const auto existing=std::find_if(cache_.begin(),cache_.end(),[&](const auto& entry){
        return entry.request==result.request && entry.input_sha256==result.input_sha256;
    });
    if (existing!=cache_.end()) cache_.erase(existing);
    if (cache_.size()==cache_capacity) cache_.erase(cache_.begin());
    cache_.push_back(result);
}
std::optional<maps::MapRulesResult> MapRulesPreflight::remembered_result(
    const maps::MapRulesRequest& request) const {
    const auto found=std::find_if(cache_.rbegin(),cache_.rend(),[&](const auto& entry){
        return entry.request==request;
    });
    if (found==cache_.rend()) return std::nullopt;
    return *found;
}
std::size_t MapRulesPreflight::active_check_count() const {
    std::lock_guard lock(worker_->mutex);
    return worker_->active ? 1U:0U;
}
std::size_t MapRulesPreflight::pending_check_count() const {
    std::lock_guard lock(worker_->mutex);
    return worker_->pending ? 1U:0U;
}
void MapRulesPreflight::shutdown() {
    {
        std::lock_guard lock(worker_->mutex);
        worker_->stopping=true;
        worker_->pending.reset(); worker_->completed.reset();
        selected_.reset(); result_={};
    }
    worker_->changed.notify_one();
    if (worker_->thread.joinable()) worker_->thread.join();
}

} // namespace openemperor::menu
