#include "lcu/network/fragmentation.h"

#include <gtest/gtest.h>

using lcu::network::fragment_payload;
using lcu::network::FragmentReassembler;

namespace {

std::vector<lcu::u8> make_payload(lcu::usize size, lcu::u8 seed = 0) {
    std::vector<lcu::u8> payload(size);
    for (lcu::usize i = 0; i < size; ++i) {
        payload[i] = static_cast<lcu::u8>((i + seed) % 256);
    }
    return payload;
}

}  // namespace

TEST(FragmentPayload, SmallPayloadProducesExactlyOneFragment) {
    const auto payload = make_payload(10);
    const auto fragments = fragment_payload(payload, 1, 100);
    EXPECT_EQ(fragments.size(), 1u);
}

TEST(FragmentPayload, EmptyPayloadProducesExactlyOneFragment) {
    const auto fragments = fragment_payload({}, 1, 100);
    EXPECT_EQ(fragments.size(), 1u);
}

TEST(FragmentPayload, LargePayloadSplitsIntoMultipleFragments) {
    const auto payload = make_payload(2500);
    const auto fragments = fragment_payload(payload, 1, 1024);
    EXPECT_EQ(fragments.size(), 3u);  // 1024 + 1024 + 452
}

TEST(FragmentPayload, EveryFragmentStaysUnderTheDataSizeLimit) {
    const auto payload = make_payload(5000);
    const auto fragments = fragment_payload(payload, 1, 1024);
    for (const auto& fragment : fragments) {
        EXPECT_LE(fragment.size(), 6u + 1024u);  // 6-byte fragment header + data budget
    }
}

TEST(FragmentReassembler, SingleFragmentMessageReassemblesImmediately) {
    const auto payload = make_payload(50);
    const auto fragments = fragment_payload(payload, 7, 1024);
    ASSERT_EQ(fragments.size(), 1u);

    FragmentReassembler reassembler;
    const auto result = reassembler.add_fragment(fragments[0]);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}

TEST(FragmentReassembler, MultiFragmentMessageReassemblesInOrderDelivery) {
    const auto payload = make_payload(3000, 42);
    const auto fragments = fragment_payload(payload, 3, 1024);
    ASSERT_EQ(fragments.size(), 3u);

    FragmentReassembler reassembler;
    EXPECT_FALSE(reassembler.add_fragment(fragments[0]).has_value());
    EXPECT_FALSE(reassembler.add_fragment(fragments[1]).has_value());
    const auto result = reassembler.add_fragment(fragments[2]);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}

TEST(FragmentReassembler, MultiFragmentMessageReassemblesOutOfOrderDelivery) {
    const auto payload = make_payload(3000, 7);
    const auto fragments = fragment_payload(payload, 3, 1024);
    ASSERT_EQ(fragments.size(), 3u);

    FragmentReassembler reassembler;
    EXPECT_FALSE(reassembler.add_fragment(fragments[2]).has_value());
    EXPECT_FALSE(reassembler.add_fragment(fragments[0]).has_value());
    const auto result = reassembler.add_fragment(fragments[1]);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}

TEST(FragmentReassembler, InterleavedConcurrentMessagesDontCorruptEachOther) {
    const auto payload_a = make_payload(2500, 1);
    const auto payload_b = make_payload(2500, 200);
    const auto fragments_a = fragment_payload(payload_a, 1, 1024);
    const auto fragments_b = fragment_payload(payload_b, 2, 1024);
    ASSERT_EQ(fragments_a.size(), 3u);
    ASSERT_EQ(fragments_b.size(), 3u);

    FragmentReassembler reassembler;
    EXPECT_FALSE(reassembler.add_fragment(fragments_a[0]).has_value());
    EXPECT_FALSE(reassembler.add_fragment(fragments_b[0]).has_value());
    EXPECT_FALSE(reassembler.add_fragment(fragments_a[1]).has_value());
    EXPECT_FALSE(reassembler.add_fragment(fragments_b[1]).has_value());

    const auto result_a = reassembler.add_fragment(fragments_a[2]);
    ASSERT_TRUE(result_a.has_value());
    EXPECT_EQ(*result_a, payload_a);

    const auto result_b = reassembler.add_fragment(fragments_b[2]);
    ASSERT_TRUE(result_b.has_value());
    EXPECT_EQ(*result_b, payload_b);
}

TEST(FragmentReassembler, DuplicateFragmentDeliveryIsIgnored) {
    const auto payload = make_payload(3000, 5);
    const auto fragments = fragment_payload(payload, 1, 1024);
    ASSERT_EQ(fragments.size(), 3u);

    FragmentReassembler reassembler;
    EXPECT_FALSE(reassembler.add_fragment(fragments[0]).has_value());
    EXPECT_FALSE(reassembler.add_fragment(fragments[0]).has_value());  // retransmitted duplicate
    EXPECT_FALSE(reassembler.add_fragment(fragments[1]).has_value());
    const auto result = reassembler.add_fragment(fragments[2]);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}

TEST(FragmentReassembler, MalformedTooShortFragmentIsIgnoredNotFatal) {
    FragmentReassembler reassembler;
    EXPECT_FALSE(reassembler.add_fragment({1, 2, 3}).has_value());  // shorter than the 6-byte header
    EXPECT_FALSE(reassembler.add_fragment({}).has_value());
}

TEST(FragmentPayload, RoundTripExactBytesForARealisticChunkSizedPayload) {
    // A compressed 16^3 chunk is typically a few KB - representative of
    // the real payload size this layer exists for (see NETWORKING.md
    // "Chunk network streaming").
    const auto payload = make_payload(4096, 99);
    const auto fragments = fragment_payload(payload, 1);  // default kMaxFragmentDataSize

    FragmentReassembler reassembler;
    std::optional<std::vector<lcu::u8>> result;
    for (const auto& fragment : fragments) {
        result = reassembler.add_fragment(fragment);
    }
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}
