#include "app/MapRulesPreflight.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using Controller=openemperor::menu::MapRulesPreflight;
using Request=openemperor::maps::MapRulesRequest;
using Result=openemperor::maps::MapRulesResult;
using Status=openemperor::maps::MapRulesStatus;
using Profile=openemperor::simulation::RulesProfile;
void check(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}
Request request(std::string map="Cities/A.map",std::string root="/data/one") {
    return {root,map,Profile::CityV16,3,1};
}
std::string content_hash(const std::string& name) {
    if (name.empty()) return {};
    const char digit=name=="input-one" ? 'a':name=="changed-content" ? 'b':
        name=="stale-blocked" ? 'c':'d';
    return std::string(64,digit);
}
Result result(const Request& selected,Status status=Status::MapRulesChecked,
              std::string hash="input-one") {
    Result value;
    value.request=selected; value.status=status;
    value.reason=status==Status::UnsupportedForRules ? "Original occupancy is unsupported":"Map rules checked";
    value.detail="Authored controlled result"; value.input_sha256=content_hash(hash);
    value.physical_bytes=100;
    return value;
}

// No sleeps or parser timing control these tests. Every checker call blocks
// until the test explicitly supplies that call's completion.
class ControlledChecks {
public:
    Result run(const Request& selected) {
        std::unique_lock lock(mutex_);
        const auto index=started_.size();
        started_.push_back(selected); outcomes_.emplace_back();
        worker_ids_.push_back(std::this_thread::get_id());
        changed_.notify_all();
        check(changed_.wait_for(lock,std::chrono::seconds(10),[&]{return outcomes_.at(index).has_value();}),
              "controlled checker was not explicitly completed");
        return *outcomes_.at(index);
    }
    void wait_started(std::size_t count) {
        std::unique_lock lock(mutex_);
        check(changed_.wait_for(lock,std::chrono::seconds(10),[&]{return started_.size()>=count;}),
              "controlled worker did not begin requested check");
    }
    void release(std::size_t index,Status status=Status::MapRulesChecked,
                 std::string hash="input-one") {
        std::lock_guard lock(mutex_);
        outcomes_.at(index)=result(started_.at(index),status,std::move(hash));
        changed_.notify_all();
    }
    Request started(std::size_t index) const {
        std::lock_guard lock(mutex_); return started_.at(index);
    }
    std::size_t size() const {
        std::lock_guard lock(mutex_); return started_.size();
    }
    std::thread::id worker_id(std::size_t index) const {
        std::lock_guard lock(mutex_); return worker_ids_.at(index);
    }
private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<Request> started_;
    std::vector<std::optional<Result>> outcomes_;
    std::vector<std::thread::id> worker_ids_;
};
void accept(Controller& controller) {
    // Only the thread-to-main-thread handoff is awaited here; the checker
    // completion has already been explicitly released by the test.
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (!controller.poll()) {
        check(std::chrono::steady_clock::now()<deadline,"worker completion was not adopted");
        std::this_thread::yield();
    }
}
void idle(Controller& controller) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (controller.active_check_count()!=0 || controller.pending_check_count()!=0) {
        check(std::chrono::steady_clock::now()<deadline,"worker did not become idle");
        std::this_thread::yield();
    }
}
void stale_replacement(const Request& first,const Request& replacement) {
    auto controls=std::make_shared<ControlledChecks>();
    Controller controller([controls](const auto& selected){return controls->run(selected);});
    const auto initial_generation=controller.select(first);
    controls->wait_started(1);
    const auto replacement_generation=controller.select(replacement);
    check(replacement_generation>initial_generation,"selection generation did not change");
    check(controller.result().status==Status::Checking && controller.result().request==replacement &&
          !controller.can_start(),"replacement was authorized before check");
    controls->release(0,Status::UnsupportedForRules,"stale-blocked");
    controls->wait_started(2);
    check(!controller.poll() && !controller.can_start() && controller.cache_size()==0 &&
          controller.result().request==replacement && controller.result().status==Status::Checking,
          "stale unsupported result changed current selection or cache");
    check(controls->started(1)==replacement,"worker checked wrong replacement identity");
    controls->release(1);
    accept(controller);
    check(controller.can_start() && controller.result().request==replacement,
          "current replacement result did not enable start");
}
void selection_and_lifecycle() {
    auto first=request();
    stale_replacement(first,request("Cities/B.map"));
    auto older=first; older.profile=Profile::CityV10; older.rule_version=1; older.policy_version=0;
    stale_replacement(first,older);
    auto rule2=first; rule2.rule_version=2; rule2.policy_version=0;
    stale_replacement(first,rule2);
    stale_replacement(first,request("Cities/A.map","/data/two"));

    auto controls=std::make_shared<ControlledChecks>();
    Controller controller([controls](const auto& selected){return controls->run(selected);});
    check(!controller.has_selection() && controller.result().status==Status::Unchecked &&
          !controller.can_start() && controller.active_check_count()==0,
          "unchecked controller did work or allowed start");
    controller.select(first); controls->wait_started(1);
    check(controls->worker_id(0)!=std::this_thread::get_id(),"checker ran on menu thread");
    controller.invalidate();
    check(!controller.has_selection() && controller.result().status==Status::Unchecked &&
          controller.pending_check_count()==0,"leaving setup retained selected request");
    controls->release(0); idle(controller);
    check(!controller.poll() && !controller.can_start() && controller.cache_size()==0,
          "late result after leaving setup was adopted");
    const auto old_generation=controller.select(first); controls->wait_started(2);
    controller.invalidate();
    const auto reopened_generation=controller.select(first);
    check(reopened_generation>old_generation,"reopened identical selection reused generation");
    controls->release(1,Status::UnsupportedForRules,"old-session");
    controls->wait_started(3);
    check(!controller.poll() && !controller.can_start(),
          "old identical selection result crossed setup lifetime boundary");
    controls->release(2); accept(controller);
    check(controller.can_start(),"reopened setup did not accept fresh result");
}
void bounded_queue() {
    auto controls=std::make_shared<ControlledChecks>();
    Controller controller([controls](const auto& selected){return controls->run(selected);});
    controller.select(request()); controls->wait_started(1);
    for (int index=0;index<100;++index) {
        controller.select(request("Cities/"+std::to_string(index)+".map"));
        check(controller.active_check_count()==1 && controller.pending_check_count()==1,
              "rapid selection grew active or pending queue");
    }
    controls->release(0); controls->wait_started(2);
    check(controls->size()==2 && controls->started(1).map_relative=="Cities/99.map",
          "worker parsed intermediate superseded selections");
    controls->release(1); accept(controller);
    check(controller.can_start() && controller.cache_size()==1,
          "latest pending result or bounded cache was lost");
}
void content_bound_cache() {
    auto controls=std::make_shared<ControlledChecks>();
    Controller controller([controls](const auto& selected){return controls->run(selected);});
    const auto first=request();
    controller.select(first); controls->wait_started(1);
    controls->release(0); accept(controller);
    check(controller.cache_size()==1 && controller.remembered_result(first)->input_sha256==content_hash("input-one"),
          "checked content was not remembered");
    controller.recheck(); controls->wait_started(2);
    check(!controller.can_start() && controller.result().status==Status::Checking,
          "remembered content authorized an explicit recheck");
    controls->release(1,Status::UnsupportedForRules,"changed-content"); accept(controller);
    check(!controller.can_start() && controller.result().input_sha256==content_hash("changed-content") &&
          controller.cache_size()==2,"changed source content reused old approval");
    // The same name under another canonical root has no shared cache identity.
    const auto another_root=request("Cities/A.map","/data/two");
    check(!controller.remembered_result(another_root),"same map name crossed data roots");
    controller.select(another_root); controls->wait_started(3);
    controls->release(2); accept(controller);
    check(controller.can_start() && controller.remembered_result(first)->status==Status::UnsupportedForRules &&
          controller.remembered_result(another_root)->status==Status::MapRulesChecked,
          "different canonical roots conflated compatibility");
    auto other_profile=another_root; other_profile.profile=Profile::CityV10;
    other_profile.rule_version=1; other_profile.policy_version=0;
    check(!controller.remembered_result(other_profile),"cache crossed rule/profile identity");
    controller.select(other_profile); controls->wait_started(4);
    controls->release(3,Status::InputError,""); accept(controller);
    check(controller.cache_size()==3 && !controller.remembered_result(other_profile),
          "retryable input error became cached unsupported state");
    for (std::size_t index=4;index<20;++index) {
        controller.select(request("Cities/cache-"+std::to_string(index)+".map"));
        controls->wait_started(index+1); controls->release(index); accept(controller);
        check(controller.cache_size()<=Controller::cache_capacity,"session cache grew without bound");
    }
    check(controller.cache_size()==Controller::cache_capacity && !controller.remembered_result(first),
          "oldest cache entries were not evicted");
    const auto before=controls->size();
    for (int index=0;index<1000;++index) {
        controller.poll(); (void)controller.result(); (void)controller.can_start();
        (void)controller.remembered_result(first);
    }
    check(controls->size()==before,"poll or display getter rechecked source files");
}
void exception_and_shutdown() {
    {
        Controller controller([](const auto&)->Result{throw std::runtime_error("authored checker failure");});
        controller.select(request()); accept(controller);
        check(controller.result().status==Status::InputError && !controller.can_start() &&
              controller.result().detail=="authored checker failure" && controller.cache_size()==0,
              "checker exception escaped or was classified as unsupported");
    }
    {
        Controller controller([](const auto& selected){
            auto value=result(selected); value.request.map_relative="Cities/wrong.map"; return value;
        });
        controller.select(request()); accept(controller);
        check(controller.result().status==Status::InputError && !controller.can_start(),
              "mismatched service selection was adopted as ready");
    }
    {
        Controller controller([](const auto& selected){return result(selected,Status::MapRulesChecked,"");});
        controller.select(request()); accept(controller);
        check(controller.result().status==Status::InputError && !controller.can_start(),
              "positive check without content identity authorized start");
    }
    {
        Controller controller([](const auto& selected){
            auto value=result(selected); value.reason=std::string(5000,'r');
            value.detail=std::string(5000,'d'); return value;
        });
        controller.select(request()); accept(controller);
        check(controller.result().reason.size()<=2048 && controller.result().detail.size()<=2048,
              "completed result text was unbounded");
    }
    auto controls=std::make_shared<ControlledChecks>();
    auto controller=std::make_unique<Controller>([controls](const auto& selected){return controls->run(selected);});
    controller->select(request()); controls->wait_started(1);
    controller->select(request("Cities/B.map"));
    controller->invalidate();
    // Destroying the controller owns and joins the still-active call. No
    // callback references the controller/menu object after destruction.
    std::mutex mutex;
    std::condition_variable changed;
    bool shutdown_started=false,shutdown_finished=false;
    std::thread destroyer([&]{
        { std::lock_guard lock(mutex); shutdown_started=true; changed.notify_one(); }
        controller.reset();
        { std::lock_guard lock(mutex); shutdown_finished=true; changed.notify_one(); }
    });
    {
        std::unique_lock lock(mutex);
        check(changed.wait_for(lock,std::chrono::seconds(10),[&]{return shutdown_started;}),
              "shutdown test did not start");
        check(!shutdown_finished,"shutdown did not join blocked active checker");
    }
    controls->release(0); destroyer.join();
    check(shutdown_finished && controls->size()==1,
          "shutdown ran pending work or did not complete joined lifetime");
    Controller empty;
    empty.shutdown(); empty.shutdown();
    check(!empty.can_start() && !empty.poll(),"idempotent shutdown retained result");
}
}

int main() {
    try {
        selection_and_lifecycle(); bounded_queue(); content_bound_cache(); exception_and_shutdown();
        std::cout<<"map rules selected worker, content cache, generations and shutdown checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n'; return 1;
    }
}
