#include "Sg3Inspect.h"

#include <array>
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    namespace fs = std::filesystem;
    if (argc != 2 && argc != 3 && argc != 6 && argc != 7 && argc != 8) {
        std::cerr << "Usage: " << argv[0] << " <file.sg3> [--summary | --image <index> (--rgba <output.rgba> | --png <output.png>) [--ignore-alpha | --alpha-addressing spec|contiguous|legacy]]\n"
                  << "       " << argv[0] << " <other-file>\n";
        return 2;
    }

    std::error_code error;
    const fs::path input{argv[1]};
    const fs::path absolute_path = fs::absolute(input, error);
    if (error || !fs::is_regular_file(absolute_path, error) || error) {
        std::cerr << "File does not exist or is not a regular file: " << input.string() << '\n';
        return 2;
    }
    const std::uintmax_t size = fs::file_size(absolute_path, error);
    if (error) {
        std::cerr << "Could not determine file size: " << error.message() << '\n';
        return 1;
    }

    std::string extension = absolute_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char letter) {
        return static_cast<char>(std::tolower(letter));
    });
    if (extension == ".sg3") {
        try {
            if (argc == 2) {
                inspect_sg3(absolute_path, std::cout);
            } else if (argc == 3) {
                if (std::string_view{argv[2]} != "--summary") {
                    std::cerr << "Usage: " << argv[0] << " <file.sg3> --summary\n";
                    return 2;
                }
                summarize_sg3(absolute_path, std::cout);
            } else {
                const std::string_view output_option{argv[4]};
                if (std::string_view{argv[2]} != "--image" ||
                    (output_option != "--rgba" && output_option != "--png")) {
                    std::cerr << "Usage: " << argv[0] << " <file.sg3> --image <index> (--rgba <output.rgba> | --png <output.png>)\n";
                    return 2;
                }
                std::uint32_t image_index = 0;
                const std::string_view index_text{argv[3]};
                const auto parsed = std::from_chars(index_text.data(),
                                                    index_text.data() + index_text.size(), image_index);
                if (parsed.ec != std::errc{} || parsed.ptr != index_text.data() + index_text.size()) {
                    std::cerr << "Image index must be a nonnegative integer\n";
                    return 2;
                }
                bool ignore_alpha = false;
                std::optional<openemperor::assets::AlphaAddressing> addressing;
                if (argc == 7) {
                    if (std::string_view{argv[6]} != "--ignore-alpha") { return 2; }
                    ignore_alpha = true;
                } else if (argc == 8) {
                    if (std::string_view{argv[6]} != "--alpha-addressing") { return 2; }
                    const std::string_view value{argv[7]};
                    if (value == "spec") addressing = openemperor::assets::AlphaAddressing::Spec;
                    else if (value == "contiguous") addressing = openemperor::assets::AlphaAddressing::Contiguous;
                    else if (value == "legacy") addressing = openemperor::assets::AlphaAddressing::Legacy;
                    else { return 2; }
                }
                decode_one_sg3_image(absolute_path, image_index, argv[5],
                                     output_option == "--png" ? ImageOutputFormat::Png : ImageOutputFormat::Rgba,
                                     std::cout, ignore_alpha, addressing);
            }
            return 0;
        } catch (const std::exception& error_message) {
            std::cerr << "SG3 inspection failed: " << error_message.what() << '\n';
            return 1;
        }
    }

    if (argc != 2) {
        std::cerr << "SG3 summary or image export requires a .sg3 input file\n";
        return 2;
    }

    std::ifstream stream{absolute_path, std::ios::binary};
    if (!stream) {
        std::cerr << "Could not open file: " << absolute_path.string() << '\n';
        return 1;
    }
    std::array<unsigned char, 32> prefix{};
    stream.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
    const std::streamsize bytes_read = stream.gcount();
    if (stream.bad()) {
        std::cerr << "Could not read file: " << absolute_path.string() << '\n';
        return 1;
    }

    std::cout << "Filename: " << absolute_path.filename().string() << '\n'
              << "Absolute path: " << absolute_path.lexically_normal().string() << '\n'
              << "File size: " << size << " bytes\n"
              << "Extension: " << (absolute_path.extension().empty() ? "(none)" : absolute_path.extension().string()) << '\n'
              << "First 32 bytes (hex):";
    for (std::streamsize index = 0; index < bytes_read; ++index) {
        std::cout << ' ' << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<unsigned int>(prefix[static_cast<std::size_t>(index)]);
    }
    std::cout << std::dec << '\n';
    return 0;
}
