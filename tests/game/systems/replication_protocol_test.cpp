#include "game/systems/replication_protocol.h"

#include <gtest/gtest.h>

using namespace game::systems::protocol;

TEST(ReplicationProtocol, PeekTypeReadsTheFirstByte) {
    const auto bytes = encode_welcome({1337, 20});
    EXPECT_EQ(peek_type(bytes), MessageType::Welcome);
    EXPECT_EQ(peek_type({}), std::nullopt);
}

TEST(ReplicationProtocol, WelcomeRoundTrips) {
    const Welcome sent{1337, 20};
    const auto bytes = encode_welcome(sent);
    const auto decoded = decode_welcome(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->world_seed, 1337u);
    EXPECT_EQ(decoded->tick_rate, 20u);
}

TEST(ReplicationProtocol, HeartbeatRoundTrips) {
    const Heartbeat sent{123456u, 7u};
    const auto bytes = encode_heartbeat(sent);
    const auto decoded = decode_heartbeat(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->tick, 123456u);
    EXPECT_EQ(decoded->entity_count, 7u);
}

TEST(ReplicationProtocol, EntityStateRoundTripsMultipleEntities) {
    const std::vector<EntitySnapshot> sent = {
        {1, {1.5f, 2.5f, 3.5f}},
        {2, {-1.0f, 0.0f, 100.25f}},
        {5, {0.0f, 0.0f, 0.0f}},
    };
    const auto bytes = encode_entity_state(sent);
    const auto decoded = decode_entity_state(bytes);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->size(), 3u);
    EXPECT_EQ((*decoded)[0].entity_index, 1u);
    EXPECT_FLOAT_EQ((*decoded)[0].position.x, 1.5f);
    EXPECT_EQ((*decoded)[1].entity_index, 2u);
    EXPECT_FLOAT_EQ((*decoded)[1].position.z, 100.25f);
    EXPECT_EQ((*decoded)[2].entity_index, 5u);
}

TEST(ReplicationProtocol, EntityStateRoundTripsEmptyList) {
    const auto bytes = encode_entity_state({});
    const auto decoded = decode_entity_state(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->empty());
}

TEST(ReplicationProtocol, PlayerInputRoundTrips) {
    const PlayerInput sent{42, {1.0f, 0.0f, -2.5f}, 0.05f};
    const auto bytes = encode_player_input(sent);
    const auto decoded = decode_player_input(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, 42u);
    EXPECT_FLOAT_EQ(decoded->horizontal_delta.x, 1.0f);
    EXPECT_FLOAT_EQ(decoded->horizontal_delta.z, -2.5f);
    EXPECT_FLOAT_EQ(decoded->dt, 0.05f);
}

TEST(ReplicationProtocol, PlayerCorrectionRoundTrips) {
    const PlayerCorrection sent{99, {10.0f, 20.0f, 30.0f}};
    const auto bytes = encode_player_correction(sent);
    const auto decoded = decode_player_correction(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->acknowledged_sequence, 99u);
    EXPECT_FLOAT_EQ(decoded->position.y, 20.0f);
}

TEST(ReplicationProtocol, DecodeRejectsWrongMessageType) {
    const auto welcome_bytes = encode_welcome({1, 1});
    EXPECT_EQ(decode_heartbeat(welcome_bytes), std::nullopt);
    EXPECT_EQ(decode_player_input(welcome_bytes), std::nullopt);
}

TEST(ReplicationProtocol, DecodeRejectsTruncatedPayload) {
    auto bytes = encode_player_correction({1, {1.0f, 2.0f, 3.0f}});
    bytes.resize(bytes.size() - 2);  // chop off the last couple of bytes
    EXPECT_EQ(decode_player_correction(bytes), std::nullopt);
}

TEST(ReplicationProtocol, DecodeRejectsEmptyPayload) {
    EXPECT_EQ(decode_welcome({}), std::nullopt);
    EXPECT_EQ(decode_entity_state({}), std::nullopt);
}

TEST(ReplicationProtocol, NegativeFloatValuesRoundTripExactly) {
    const PlayerInput sent{1, {-123.456f, 0.0f, -0.001f}, -1.0f};
    const auto decoded = decode_player_input(encode_player_input(sent));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_FLOAT_EQ(decoded->horizontal_delta.x, -123.456f);
    EXPECT_FLOAT_EQ(decoded->horizontal_delta.z, -0.001f);
    EXPECT_FLOAT_EQ(decoded->dt, -1.0f);
}

TEST(ReplicationProtocol, BlockActionBreakRoundTrips) {
    const BlockAction sent{BlockActionType::Break, 10, -5, 200, 0};
    const auto bytes = encode_block_action(sent);
    const auto decoded = decode_block_action(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->action, BlockActionType::Break);
    EXPECT_EQ(decoded->x, 10);
    EXPECT_EQ(decoded->y, -5);
    EXPECT_EQ(decoded->z, 200);
}

TEST(ReplicationProtocol, BlockActionPlaceRoundTripsWithBlockId) {
    const BlockAction sent{BlockActionType::Place, -1000000, 64, 1000000, 42};
    const auto bytes = encode_block_action(sent);
    const auto decoded = decode_block_action(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->action, BlockActionType::Place);
    EXPECT_EQ(decoded->x, -1000000);
    EXPECT_EQ(decoded->y, 64);
    EXPECT_EQ(decoded->z, 1000000);
    EXPECT_EQ(decoded->block_id, 42u);
}

TEST(ReplicationProtocol, BlockActionRejectsInvalidActionByte) {
    auto bytes = encode_block_action({BlockActionType::Break, 1, 2, 3, 0});
    bytes[1] = 0xFF;  // neither Break(0) nor Place(1)
    EXPECT_EQ(decode_block_action(bytes), std::nullopt);
}

TEST(ReplicationProtocol, BlockChangeRoundTrips) {
    const BlockChange sent{-42, 0, 999999999, 7};
    const auto bytes = encode_block_change(sent);
    const auto decoded = decode_block_change(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->x, -42);
    EXPECT_EQ(decoded->y, 0);
    EXPECT_EQ(decoded->z, 999999999);
    EXPECT_EQ(decoded->block_id, 7u);
}

TEST(ReplicationProtocol, BlockChangeToAirRoundTrips) {
    // block_id 0 == kAirBlockId - the "this was a break" case.
    const BlockChange sent{5, 5, 5, 0};
    const auto decoded = decode_block_change(encode_block_change(sent));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->block_id, 0u);
}

TEST(ReplicationProtocol, BlockActionAndBlockChangeRejectTruncatedPayload) {
    auto action_bytes = encode_block_action({BlockActionType::Place, 1, 2, 3, 4});
    action_bytes.resize(action_bytes.size() - 1);
    EXPECT_EQ(decode_block_action(action_bytes), std::nullopt);

    auto change_bytes = encode_block_change({1, 2, 3, 4});
    change_bytes.resize(change_bytes.size() - 1);
    EXPECT_EQ(decode_block_change(change_bytes), std::nullopt);
}

TEST(ReplicationProtocol, BlockActionAndBlockChangeRejectWrongMessageType) {
    const auto welcome_bytes = encode_welcome({1, 1});
    EXPECT_EQ(decode_block_action(welcome_bytes), std::nullopt);
    EXPECT_EQ(decode_block_change(welcome_bytes), std::nullopt);
}

TEST(ReplicationProtocol, PeekTypeDistinguishesBlockActionAndBlockChange) {
    EXPECT_EQ(peek_type(encode_block_action({BlockActionType::Break, 0, 0, 0, 0})), MessageType::BlockAction);
    EXPECT_EQ(peek_type(encode_block_change({0, 0, 0, 0})), MessageType::BlockChange);
}

TEST(ReplicationProtocol, ChunkDataRoundTrips) {
    const ChunkData sent{-3, 0, 7, {1, 2, 3, 4, 5, 250, 251, 252}};
    const auto bytes = encode_chunk_data(sent);
    const auto decoded = decode_chunk_data(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->chunk_x, -3);
    EXPECT_EQ(decoded->chunk_y, 0);
    EXPECT_EQ(decoded->chunk_z, 7);
    EXPECT_EQ(decoded->compressed_bytes, sent.compressed_bytes);
}

TEST(ReplicationProtocol, ChunkDataRoundTripsWithEmptyCompressedBytes) {
    const ChunkData sent{1, -2, 3, {}};
    const auto decoded = decode_chunk_data(encode_chunk_data(sent));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->chunk_x, 1);
    EXPECT_EQ(decoded->chunk_y, -2);
    EXPECT_EQ(decoded->chunk_z, 3);
    EXPECT_TRUE(decoded->compressed_bytes.empty());
}

TEST(ReplicationProtocol, ChunkDataRejectsTruncatedPayload) {
    auto bytes = encode_chunk_data({1, 2, 3, {9, 9}});
    bytes.resize(12);  // shorter than the 13-byte type+coords header
    EXPECT_EQ(decode_chunk_data(bytes), std::nullopt);
}

TEST(ReplicationProtocol, ChunkDataRejectsWrongMessageType) {
    EXPECT_EQ(decode_chunk_data(encode_welcome({1, 1})), std::nullopt);
}

TEST(ReplicationProtocol, ChunkDataFragmentRoundTrips) {
    const std::vector<lcu::u8> fragment_bytes = {0, 1, 0, 0, 0, 2, 10, 20};
    const auto wire_bytes = encode_chunk_data_fragment(fragment_bytes);
    EXPECT_EQ(peek_type(wire_bytes), MessageType::ChunkDataFragment);
    const auto decoded = decode_chunk_data_fragment(wire_bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, fragment_bytes);
}

TEST(ReplicationProtocol, ChunkDataFragmentRejectsWrongMessageType) {
    EXPECT_EQ(decode_chunk_data_fragment(encode_welcome({1, 1})), std::nullopt);
}
