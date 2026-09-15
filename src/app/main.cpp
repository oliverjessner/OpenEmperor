#include "app/Application.h"
#include "assets/RgbaPngReader.h"

#include <SDL3/SDL_main.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

namespace {

void print_usage(const char* executable) {
    std::cerr << "Usage: " << executable << " [--data <directory>] [--preview <exported.png>]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    namespace fs = std::filesystem;
    bool data_supplied = false;
    bool preview_supplied = false;
    fs::path data_directory;
    fs::path preview_path;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (index + 1 >= argc) {
            print_usage(argv[0]);
            return 2;
        }
        if (argument == "--data" && !data_supplied) {
            data_directory = argv[++index];
            data_supplied = true;
        } else if (argument == "--preview" && !preview_supplied) {
            preview_path = argv[++index];
            preview_supplied = true;
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    if (data_supplied) {
        std::error_code error;
        const fs::path absolute_path = fs::absolute(data_directory, error);
        if (error || !fs::is_directory(absolute_path, error) || error) {
            std::cerr << "Data directory does not exist or cannot be accessed: " << data_directory.string() << '\n';
            return 2;
        }
        std::cout << "Data directory: " << absolute_path.lexically_normal().string() << '\n';
    }

    std::optional<openemperor::assets::RgbaImage> preview;
    if (preview_supplied) {
        try {
            preview = openemperor::assets::read_exported_png(preview_path);
        } catch (const std::exception& error_message) {
            std::cerr << "PNG preview failed: " << error_message.what() << '\n';
            return 1;
        }
        std::cout << "Preview image: " << fs::absolute(preview_path).lexically_normal().string()
                  << " (" << preview->width << 'x' << preview->height << ")\n";
    }

    openemperor::Application application{std::move(preview)};
    if (!application.initialize()) {
        return 1;
    }
    const int result = application.run();
    application.shutdown();
    return result;
}
