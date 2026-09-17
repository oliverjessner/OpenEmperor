#include "assets/AssetCatalog.h"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>

namespace {

namespace assets = openemperor::assets;

void usage() {
    std::cerr << "Usage: openemperor-assets --data <directory> [--json]"
              << " [--kind plain|sprite|isometric|unsupported] [--type <number>]"
              << " [--archive <substring>] [--group <substring>]"
              << " [--with-alpha|--without-alpha] [--min-width <n>] [--min-height <n>]\n";
}

bool parse_u16(std::string_view text, std::uint16_t& value) {
    unsigned int parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        parsed > std::numeric_limits<std::uint16_t>::max()) return false;
    value = static_cast<std::uint16_t>(parsed);
    return true;
}

void json_string(std::ostream& output, const std::string& input) {
    constexpr char digits[] = "0123456789abcdef";
    output << '"';
    for (const char letter : input) {
        const unsigned char byte = static_cast<unsigned char>(letter);
        if (byte == '"' || byte == '\\') {
            output << '\\' << static_cast<char>(byte);
        } else if (byte < 0x20 || byte >= 0x80) {
            output << "\\u00" << digits[byte >> 4U] << digits[byte & 15U];
        } else {
            output << static_cast<char>(byte);
        }
    }
    output << '"';
}

void print_json(const assets::AssetCatalog& catalog,
                const std::vector<std::size_t>& indices, std::ostream& output) {
    const assets::AssetCounts counts = assets::count_assets(catalog, indices);
    output << "{\"format\":\"OpenEmperor Asset Catalog\",\"archive_count\":"
           << catalog.archive_count << ",\"archives_with_errors\":"
           << catalog.archive_errors.size() << ",\"record_count\":" << counts.records
           << ",\"kind_counts\":{\"plain\":" << counts.plain
           << ",\"sprite\":" << counts.sprite
           << ",\"isometric\":" << counts.isometric
           << ",\"unsupported\":" << counts.unsupported
           << "},\"with_alpha\":" << counts.with_alpha
           << ",\"color_range_invalid\":" << counts.color_out_of_bounds
           << ",\"alpha_range_invalid\":" << counts.alpha_out_of_bounds
           << ",\"color_source_unavailable\":" << counts.color_source_unavailable
           << ",\"alpha_source_unavailable\":" << counts.alpha_source_unavailable
           << ",\"type_counts\":[";
    bool first = true;
    for (const auto& [type, count] : counts.type_counts) {
        if (!first) output << ',';
        first = false;
        output << "{\"type\":" << type << ",\"count\":" << count << '}';
    }
    output << "],\"archive_errors\":[";
    first = true;
    for (const auto& issue : catalog.archive_errors) {
        if (!first) output << ',';
        first = false;
        output << "{\"archive\":";
        json_string(output, issue.archive_relative_path.generic_string());
        output << ",\"message\":";
        json_string(output, issue.message);
        output << '}';
    }
    output << "],\"records\":[";
    first = true;
    for (const std::size_t index : indices) {
        const assets::AssetRecord& record = catalog.records[index];
        if (!first) output << ',';
        first = false;
        output << "{\"id\":{\"archive\":";
        json_string(output, record.id.archive_relative_path.generic_string());
        output << ",\"image_index\":" << record.id.image_index
               << "},\"sg3_version\":" << record.sg3_version
               << ",\"group_id\":" << static_cast<unsigned int>(record.group_id)
               << ",\"group_filename\":";
        json_string(output, record.group_filename);
        output << ",\"group_description\":";
        json_string(output, record.group_description);
        output << ",\"image_type\":" << record.image_type
               << ",\"image_kind\":";
        json_string(output, assets::sg3_image_kind_name(record.image_kind));
        output << ",\"width\":" << record.width
               << ",\"height\":" << record.height
               << ",\"data_offset\":" << record.data_offset
               << ",\"data_length\":" << record.data_length
               << ",\"uncompressed_length\":" << record.uncompressed_length
               << ",\"external_flag\":" << static_cast<unsigned int>(record.external_flag)
               << ",\"isometric_size_flag\":" << static_cast<unsigned int>(record.isometric_size_flag)
               << ",\"alpha_offset\":" << record.alpha_offset
               << ",\"alpha_length\":" << record.alpha_length
               << ",\"horizontal_mirror_offset\":" << record.horizontal_mirror_offset
               << ",\"color_bounds\":";
        json_string(output, assets::asset_range_status_name(record.color_bounds));
        output << ",\"alpha_bounds\":";
        json_string(output, assets::asset_range_status_name(record.alpha_bounds));
        output << ",\"alpha_metadata_present\":" << (record.alpha_length != 0 ? "true" : "false")
               << ",\"metadata_supported\":" << (record.metadata_supported ? "true" : "false")
               << ",\"payload_in_bounds\":" << (record.payload_in_bounds ? "true" : "false")
               << ",\"color_decoder_supported\":" <<
                  (record.color_decoder_supported ? "true" : "false")
               << ",\"decoder_supported\":" << (record.decoder_supported ? "true" : "false")
               << ",\"decode_attempted\":" << (record.decode_attempted ? "true" : "false")
               << ",\"decode_succeeded\":" << (record.decode_succeeded ? "true" : "false")
               << '}';
    }
    output << "]}\n";
}

void print_summary(const assets::AssetCatalog& catalog,
                   const std::vector<std::size_t>& indices, std::ostream& output) {
    const assets::AssetCounts counts = assets::count_assets(catalog, indices);
    output << "OpenEmperor Asset Catalog\n"
           << "SG3 archives: " << catalog.archive_count << '\n'
           << "Image records: " << counts.records << '\n'
           << "Plain: " << counts.plain << '\n'
           << "Sprite: " << counts.sprite << '\n'
           << "Isometric: " << counts.isometric << '\n'
           << "Unsupported: " << counts.unsupported << '\n'
           << "With alpha: " << counts.with_alpha << '\n'
           << "Color range invalid: " << counts.color_out_of_bounds << '\n'
           << "Alpha range invalid: " << counts.alpha_out_of_bounds << '\n'
           << "Color source unavailable: " << counts.color_source_unavailable << '\n'
           << "Alpha source unavailable: " << counts.alpha_source_unavailable << '\n'
           << "Archives with errors: " << catalog.archive_errors.size() << '\n';
    for (const auto& issue : catalog.archive_errors) {
        output << "  " << issue.archive_relative_path.generic_string() << ": "
               << issue.message << '\n';
    }
}

} // namespace

int main(int argc, char* argv[]) {
    std::filesystem::path data_directory;
    bool data_supplied = false;
    bool json = false;
    assets::AssetFilter filter;
    for (int index = 1; index < argc; ++index) {
        const std::string_view option{argv[index]};
        if (option == "--json" && !json) {
            json = true;
        } else if (option == "--with-alpha" && !filter.with_alpha) {
            filter.with_alpha = true;
        } else if (option == "--without-alpha" && !filter.with_alpha) {
            filter.with_alpha = false;
        } else if (index + 1 < argc) {
            const std::string_view value{argv[++index]};
            if (option == "--data" && !data_supplied) {
                data_directory = value;
                data_supplied = true;
            } else if (option == "--kind" && !filter.kind) {
                if (value == "plain") filter.kind = assets::Sg3ImageKind::Plain;
                else if (value == "sprite") filter.kind = assets::Sg3ImageKind::Sprite;
                else if (value == "isometric") filter.kind = assets::Sg3ImageKind::Isometric;
                else if (value == "unsupported") filter.kind = assets::Sg3ImageKind::Unsupported;
                else { usage(); return 2; }
            } else if (option == "--type" && !filter.image_type) {
                std::uint16_t parsed = 0;
                if (!parse_u16(value, parsed)) { usage(); return 2; }
                filter.image_type = parsed;
            } else if (option == "--archive" && !filter.archive_substring) {
                filter.archive_substring = value;
            } else if (option == "--group" && !filter.group_substring) {
                filter.group_substring = value;
            } else if (option == "--min-width" && !filter.min_width) {
                std::uint16_t parsed = 0;
                if (!parse_u16(value, parsed)) { usage(); return 2; }
                filter.min_width = parsed;
            } else if (option == "--min-height" && !filter.min_height) {
                std::uint16_t parsed = 0;
                if (!parse_u16(value, parsed)) { usage(); return 2; }
                filter.min_height = parsed;
            } else {
                usage(); return 2;
            }
        } else {
            usage(); return 2;
        }
    }
    if (!data_supplied) { usage(); return 2; }
    try {
        const assets::AssetCatalog catalog = assets::scan_asset_catalog(data_directory);
        const auto indices = assets::matching_asset_indices(catalog, filter);
        if (json) print_json(catalog, indices, std::cout);
        else print_summary(catalog, indices, std::cout);
    } catch (const std::exception& error) {
        std::cerr << "Asset catalog failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
