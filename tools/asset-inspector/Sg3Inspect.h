#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>

void inspect_sg3(const std::filesystem::path& path, std::ostream& output);
void summarize_sg3(const std::filesystem::path& path, std::ostream& output);
enum class ImageOutputFormat { Rgba, Png };
void decode_one_sg3_image(const std::filesystem::path& path,
                          std::uint32_t image_index,
                          const std::filesystem::path& image_output_path,
                          ImageOutputFormat output_format,
                          std::ostream& output);
