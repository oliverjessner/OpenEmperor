#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace openemperor::maps {

class ContainerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct BlockInfo {
    std::uint64_t physical_header_offset = 0;
    std::uint64_t physical_payload_offset = 0;
    std::uint64_t offset_within_part = 0; // Physical offset from the part's first byte.
    std::uint64_t logical_offset = 0;     // Offset in the uncompressed part stream.
    std::uint32_t unknown_word = 0;
    std::uint32_t compressed_size = 0;
    std::uint32_t uncompressed_size = 0;
};

struct PartInfo {
    std::uint64_t physical_start = 0;
    std::uint64_t physical_end = 0; // Exclusive.
    std::uint64_t uncompressed_size = 0;
    std::vector<BlockInfo> blocks;
};

class EmperorContainer {
public:
    static EmperorContainer open(const std::filesystem::path& path);
    bool multipart() const { return multipart_; }
    std::uint64_t physical_size() const { return file_.size(); }
    const std::vector<PartInfo>& parts() const { return parts_; }
    const std::vector<std::uint32_t>& ignored_part_table_words() const { return ignored_table_words_; }
    std::vector<std::uint8_t> read_part(std::size_t part) const;
    std::vector<std::uint8_t> read_range(std::size_t part, std::uint64_t logical_offset,
                                         std::uint64_t length) const;
private:
    bool multipart_ = false;
    std::vector<std::uint8_t> file_;
    std::vector<PartInfo> parts_;
    std::vector<std::uint32_t> ignored_table_words_;
};

} // namespace openemperor::maps
