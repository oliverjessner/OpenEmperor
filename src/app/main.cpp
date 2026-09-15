#include "app/Application.h"
#include "assets/RgbaPngReader.h"
#include "assets/Sg3ImageLoader.h"

#include <SDL3/SDL_main.h>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

namespace {

void print_usage(const char* executable) {
    std::cerr << "Usage: " << executable << " [--data <directory>] [--preview <exported.png>]\n"
              << "       " << executable << " [--data <directory>] --sg3 <file.sg3> --image <index>\n";
}

} // namespace

int main(int argc, char* argv[]) {
    namespace fs = std::filesystem;
    bool data_supplied = false;
    bool preview_supplied = false;
    bool sg3_supplied = false;
    bool image_supplied = false;
    fs::path data_directory;
    fs::path preview_path;
    fs::path sg3_path;
    std::uint32_t image_index = 0;

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
        } else if (argument == "--sg3" && !sg3_supplied) {
            sg3_path = argv[++index];
            sg3_supplied = true;
        } else if (argument == "--image" && !image_supplied) {
            const std::string_view text{argv[++index]};
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), image_index);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
                std::cerr << "Image index must be a nonnegative integer\n";
                return 2;
            }
            image_supplied = true;
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }
    if ((sg3_supplied != image_supplied) || (preview_supplied && sg3_supplied)) {
        print_usage(argv[0]);
        return 2;
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
    } else if (sg3_supplied) {
        try {
            preview = openemperor::assets::load_sg3_image({sg3_path, image_index});
        } catch (const std::exception& error_message) {
            std::cerr << "SG3 image load failed: " << error_message.what() << '\n';
            return 1;
        }
        std::error_code error;
        const fs::path absolute_path = fs::absolute(sg3_path, error);
        std::cout << "SG3 image: " << (error ? sg3_path : absolute_path).lexically_normal().string()
                  << " index " << image_index << " (" << preview->width << 'x' << preview->height << ")\n";
    }

    openemperor::Application application{std::move(preview)};
    if (!application.initialize()) {
        return 1;
    }
    const int result = application.run();
    application.shutdown();
    return result;
}
