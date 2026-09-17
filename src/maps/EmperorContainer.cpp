#include "maps/EmperorContainer.h"

#include <zlib.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <span>
#include <string>

namespace openemperor::maps {
namespace {
namespace fs = std::filesystem;
constexpr std::uint64_t max_file_size = 64U * 1024U * 1024U;
constexpr std::uint64_t max_part_size = 64U * 1024U * 1024U;
constexpr std::uint64_t max_total_size = 128U * 1024U * 1024U;
constexpr std::size_t max_blocks_per_part = 4096;
constexpr std::uint32_t max_compressed_block = 65536;
constexpr std::uint32_t max_uncompressed_block = 32768;

bool within(std::uint64_t offset, std::uint64_t length, std::uint64_t limit) {
    return offset <= limit && length <= limit - offset;
}
std::uint32_t le32(std::span<const std::uint8_t> bytes, std::uint64_t at) {
    if (!within(at, 4, bytes.size())) throw ContainerError("truncated little-endian word at physical offset " + std::to_string(at));
    const auto p = static_cast<std::size_t>(at);
    return static_cast<std::uint32_t>(bytes[p]) |
        (static_cast<std::uint32_t>(bytes[p + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[p + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[p + 3]) << 24U);
}
std::vector<std::uint8_t> decode_block(std::span<const std::uint8_t> file,
                                        const BlockInfo& block, std::size_t part_index,
                                        std::size_t block_index) {
    const std::string where = "part " + std::to_string(part_index) + " block " +
        std::to_string(block_index) + " physical offset " +
        std::to_string(block.physical_header_offset) + ": ";
    if (!within(block.physical_payload_offset, block.compressed_size, file.size()))
        throw ContainerError(where + "compressed range exceeds file");
    std::vector<std::uint8_t> output(block.uncompressed_size);
    z_stream stream{};
    const auto start = static_cast<std::size_t>(block.physical_payload_offset);
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(file.data() + start));
    stream.avail_in = block.compressed_size;
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = block.uncompressed_size;
    if (inflateInit(&stream) != Z_OK) throw ContainerError(where + "zlib initialization failed");
    const int result = inflate(&stream, Z_FINISH);
    const auto consumed = stream.total_in;
    const auto produced = stream.total_out;
    inflateEnd(&stream);
    if (result != Z_STREAM_END) throw ContainerError(where + "damaged zlib stream or uncompressed length exceeded");
    if (consumed != block.compressed_size)
        throw ContainerError(where + "zlib stream did not consume the declared compressed length");
    if (produced != block.uncompressed_size)
        throw ContainerError(where + "zlib output differs from declared uncompressed length");
    return output;
}
} // namespace

EmperorContainer EmperorContainer::open(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec) throw ContainerError("container is missing or not a regular file");
    const auto size = fs::file_size(path, ec);
    if (ec || size < 16 || size > max_file_size) throw ContainerError("container size is outside 16 bytes..64 MiB");
    EmperorContainer result;
    result.file_.resize(static_cast<std::size_t>(size));
    std::ifstream input{path, std::ios::binary};
    if (!input || !input.read(reinterpret_cast<char*>(result.file_.data()), static_cast<std::streamsize>(size)))
        throw ContainerError("cannot read complete container: " + path.string());
    const std::span<const std::uint8_t> bytes{result.file_};
    if (le32(bytes, 0) != 0xfedcbaaaU) throw ContainerError("outer marker is not 0xFEDCBAAA");
    std::vector<std::uint64_t> starts{4};
    std::uint64_t data_end = size;
    if (size >= 72 && le32(bytes, size - 68) == 0xaaabcdefU) {
        if (le32(bytes, size - 64) != 0x3cU) throw ContainerError("multipart footer length is not 0x3C");
        result.multipart_ = true;
        data_end = size - 68;
        for (std::uint64_t i = 0; i < 11; ++i) {
            const std::uint32_t word = le32(bytes, size - 60 + i * 4U);
            if (word == 0 || word == starts.back() || word >= data_end) {
                if (word != 0) result.ignored_table_words_.push_back(word);
                continue;
            }
            if (word < starts.back()) throw ContainerError("multipart part starts are not increasing");
            starts.push_back(word);
        }
    }
    if (data_end <= starts.back()) throw ContainerError("empty final container part");
    starts.push_back(data_end);
    if (starts.size() - 1 > 12) throw ContainerError("container exceeds 12 parts");
    std::uint64_t total_uncompressed = 0;
    for (std::size_t part_index = 0; part_index + 1 < starts.size(); ++part_index) {
        PartInfo part;
        part.physical_start = starts[part_index];
        part.physical_end = starts[part_index + 1];
        std::uint64_t position = part.physical_start;
        while (position < part.physical_end) {
            const std::string where = "part " + std::to_string(part_index) + " block " +
                std::to_string(part.blocks.size()) + " physical offset " + std::to_string(position) + ": ";
            if (part.blocks.size() >= max_blocks_per_part) throw ContainerError(where + "block limit exceeded");
            if (!within(position, 12, part.physical_end)) throw ContainerError(where + "truncated block header");
            BlockInfo block;
            block.physical_header_offset = position;
            block.offset_within_part = position - part.physical_start;
            block.logical_offset = part.uncompressed_size;
            block.unknown_word = le32(bytes, position);
            block.compressed_size = le32(bytes, position + 4U);
            block.uncompressed_size = le32(bytes, position + 8U);
            block.physical_payload_offset = position + 12U;
            if (block.compressed_size == 0 || block.compressed_size > max_compressed_block ||
                block.uncompressed_size == 0 || block.uncompressed_size > max_uncompressed_block)
                throw ContainerError(where + "block length exceeds documented budget or is zero");
            if (!within(block.physical_payload_offset, block.compressed_size, part.physical_end))
                throw ContainerError(where + "compressed payload crosses part boundary");
            if (block.uncompressed_size > max_part_size - part.uncompressed_size ||
                block.uncompressed_size > max_total_size - total_uncompressed)
                throw ContainerError(where + "decompressed size budget exceeded");
            position = block.physical_payload_offset + block.compressed_size;
            part.uncompressed_size += block.uncompressed_size;
            total_uncompressed += block.uncompressed_size;
            part.blocks.push_back(block);
        }
        if (part.blocks.empty()) throw ContainerError("part " + std::to_string(part_index) + " has no blocks");
        result.parts_.push_back(std::move(part));
    }
    return result;
}

std::vector<std::uint8_t> EmperorContainer::read_range(std::size_t part_index,
                                                         std::uint64_t logical_offset,
                                                         std::uint64_t length) const {
    if (part_index >= parts_.size()) throw ContainerError("part index is outside container");
    const auto& part = parts_[part_index];
    if (!within(logical_offset, length, part.uncompressed_size) || length > max_part_size)
        throw ContainerError("logical read range exceeds part " + std::to_string(part_index));
    std::vector<std::uint8_t> result(static_cast<std::size_t>(length));
    const std::uint64_t end = logical_offset + length;
    for (std::size_t index = 0; index < part.blocks.size(); ++index) {
        const auto& block = part.blocks[index];
        const auto block_end = block.logical_offset + block.uncompressed_size;
        if (block_end <= logical_offset || block.logical_offset >= end) continue;
        const auto decoded = decode_block(file_, block, part_index, index);
        const auto first = std::max(logical_offset, block.logical_offset);
        const auto last = std::min(end, block_end);
        std::copy_n(decoded.begin() + static_cast<std::ptrdiff_t>(first - block.logical_offset),
                    static_cast<std::size_t>(last - first),
                    result.begin() + static_cast<std::ptrdiff_t>(first - logical_offset));
    }
    return result;
}

std::vector<std::uint8_t> EmperorContainer::read_part(std::size_t part) const {
    if (part >= parts_.size()) throw ContainerError("part index is outside container");
    return read_range(part, 0, parts_[part].uncompressed_size);
}

} // namespace openemperor::maps
