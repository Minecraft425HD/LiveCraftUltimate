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
