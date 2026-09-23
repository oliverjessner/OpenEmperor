#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>

void audit_sg3_red_pixels(const std::filesystem::path& path,
                          std::uint32_t image_index,
                          std::ostream& output);
