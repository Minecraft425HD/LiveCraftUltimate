#include "lcu/network/fragmentation.h"

#include <algorithm>
#include <cstring>

namespace lcu::network {

namespace {
constexpr usize kFragmentHeaderSize = 6;  // message_id(2) + fragment_index(2) + fragment_count(2)

void write_u16_be(std::vector<u8>& out, u16 value) {
    out.push_back(static_cast<u8>((value >> 8) & 0xFF));
    out.push_back(static_cast<u8>(value & 0xFF));
}

u16 read_u16_be(const u8* data) { return static_cast<u16>((static_cast<u16>(data[0]) << 8) | data[1]); }
}  // namespace

std::vector<std::vector<u8>> fragment_payload(const std::vector<u8>& payload, u16 message_id,
                                               usize max_fragment_data_size) {
    const usize data_size = std::max<usize>(max_fragment_data_size, 1);
    const usize fragment_count = std::max<usize>((payload.size() + data_size - 1) / data_size, 1);

    std::vector<std::vector<u8>> fragments;
    fragments.reserve(fragment_count);
    for (usize i = 0; i < fragment_count; ++i) {
        const usize begin = i * data_size;
        const usize end = std::min(begin + data_size, payload.size());

        std::vector<u8> fragment;
        fragment.reserve(kFragmentHeaderSize + (end - begin));
        write_u16_be(fragment, message_id);
        write_u16_be(fragment, static_cast<u16>(i));
        write_u16_be(fragment, static_cast<u16>(fragment_count));
        fragment.insert(fragment.end(), payload.begin() + static_cast<std::ptrdiff_t>(begin),
                         payload.begin() + static_cast<std::ptrdiff_t>(end));
        fragments.push_back(std::move(fragment));
    }
    return fragments;
}

std::optional<std::vector<u8>> FragmentReassembler::add_fragment(const std::vector<u8>& fragment) {
    if (fragment.size() < kFragmentHeaderSize) {
        return std::nullopt;  // malformed - too short to even hold the header, dropped
    }

    const u16 message_id = read_u16_be(fragment.data());
    const u16 fragment_index = read_u16_be(fragment.data() + 2);
    const u16 fragment_count = read_u16_be(fragment.data() + 4);
    if (fragment_count == 0 || fragment_index >= fragment_count) {
        return std::nullopt;  // malformed - can't be a real fragment_payload() output
    }

    InProgress& entry = in_progress_[message_id];
    if (entry.parts.empty()) {
        entry.parts.resize(fragment_count);
    }
    if (entry.parts.size() != fragment_count) {
        return std::nullopt;  // fragment_count disagrees with an already-in-progress message_id - drop
    }
    if (entry.parts[fragment_index].has_value()) {
        return std::nullopt;  // duplicate delivery of a fragment already received - nothing new
    }

    entry.parts[fragment_index] =
        std::vector<u8>(fragment.begin() + static_cast<std::ptrdiff_t>(kFragmentHeaderSize), fragment.end());
    ++entry.received_count;

    if (entry.received_count < fragment_count) {
        return std::nullopt;
    }

    std::vector<u8> reassembled;
    for (const auto& part : entry.parts) {
        reassembled.insert(reassembled.end(), part->begin(), part->end());
    }
    in_progress_.erase(message_id);
    return reassembled;
}

}  // namespace lcu::network
