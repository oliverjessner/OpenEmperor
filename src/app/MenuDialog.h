#pragma once
#include <SDL3/SDL.h>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace openemperor::menu {
struct DialogResult { enum class Kind { Selected, Cancelled, Error } kind; std::string path; };
class DialogAdapter {
public:
    using Callback=std::function<void(DialogResult)>;
    virtual ~DialogAdapter()=default;
    virtual void open_folder(SDL_Window* window,Callback callback)=0;
    virtual void open_file(SDL_Window* window,Callback callback)=0;
    virtual void open_visual_profile(SDL_Window* window,Callback callback) {
        open_file(window,std::move(callback));
    }
    virtual void cancel_pending()=0;
};
class NativeDialog final : public DialogAdapter {
public:
    ~NativeDialog() override { cancel_pending(); }
    void open_folder(SDL_Window* window,Callback callback) override;
    void open_file(SDL_Window* window,Callback callback) override;
    void open_visual_profile(SDL_Window* window,Callback callback) override;
    void cancel_pending() override;
private:
    std::uintptr_t token_=0;
};
}
