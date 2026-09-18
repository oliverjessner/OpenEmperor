#include "app/MenuDialog.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace openemperor::menu {
namespace {
std::mutex mutex;
std::unordered_map<std::uintptr_t,DialogAdapter::Callback> callbacks;
std::atomic<std::uintptr_t> next_token{1};
void SDLCALL completed(void* userdata,const char* const* files,int) {
    const auto token=reinterpret_cast<std::uintptr_t>(userdata);
    DialogAdapter::Callback callback;
    {
        std::lock_guard lock(mutex);
        const auto it=callbacks.find(token);
        if (it==callbacks.end()) return;
        callback=std::move(it->second); callbacks.erase(it);
    }
    try {
        DialogResult value;
        if (!files) value={DialogResult::Kind::Error,"Native dialog failed"};
        else if (!files[0]) value={DialogResult::Kind::Cancelled,{}};
        else value={DialogResult::Kind::Selected,std::string(files[0])};
        callback(std::move(value));
    } catch (...) { /* Never unwind through SDL's C callback. */ }
}
std::uintptr_t register_callback(DialogAdapter::Callback callback) {
    const auto id=next_token.fetch_add(1);
    if (id==0) throw std::runtime_error("dialog token exhausted");
    std::lock_guard lock(mutex); callbacks.emplace(id,std::move(callback)); return id;
}
}
void NativeDialog::open_folder(SDL_Window* window,Callback callback) {
    { std::lock_guard lock(mutex); if (token_ && callbacks.contains(token_))
        throw std::runtime_error("dialog already active"); }
    token_=register_callback(std::move(callback));
    SDL_ShowOpenFolderDialog(completed,reinterpret_cast<void*>(token_),window,nullptr,false);
}
void NativeDialog::open_file(SDL_Window* window,Callback callback) {
    { std::lock_guard lock(mutex); if (token_ && callbacks.contains(token_))
        throw std::runtime_error("dialog already active"); }
    token_=register_callback(std::move(callback));
    static constexpr SDL_DialogFileFilter filter[]{ {"OpenEmperor sandbox JSON","json"} };
    SDL_ShowOpenFileDialog(completed,reinterpret_cast<void*>(token_),window,filter,1,nullptr,false);
}
void NativeDialog::cancel_pending() {
    if (!token_) return;
    std::lock_guard lock(mutex); callbacks.erase(token_); token_=0;
}
}
