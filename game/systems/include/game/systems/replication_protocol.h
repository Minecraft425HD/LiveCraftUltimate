#pragma once

#include <optional>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/math/vec3.h"

namespace game::systems::protocol {

// The application-level messages VoxelClient and VoxelServer exchange
// on top of engine/network's channel transport (see NETWORKING.md).
// engine/network itself has no opinion on what a payload means - this
// is that opinion, shared by both executables so they can't silently
// drift out of sync with each other the way two independent copies of
// the same encode/decode logic eventually would.
enum class MessageType : lcu::u8 {
    Welcome = 0,           // server -> client, once, on connect. ReliableOrdered.
    Heartbeat = 1,         // server -> client, every tick. UnreliableSequenced.
    EntityState = 2,       // server -> client, every tick: AI entity positions. UnreliableSequenced.
    PlayerInput = 3,       // client -> server, every tick: local player's movement input. UnreliableSequenced.
    PlayerCorrection = 4,  // server -> client, periodically: authoritative player position. UnreliableSequenced.
};

// Reads just the type byte, for a caller that needs to dispatch before
// fully decoding. std::nullopt if `payload` is empty.
std::optional<MessageType> peek_type(const std::vector<lcu::u8>& payload);

struct Welcome {
    lcu::u32 world_seed = 0;
    lcu::u8 tick_rate = 0;
};
std::vector<lcu::u8> encode_welcome(const Welcome& message);
std::optional<Welcome> decode_welcome(const std::vector<lcu::u8>& payload);

struct Heartbeat {
    lcu::u32 tick = 0;
    lcu::u16 entity_count = 0;
};
std::vector<lcu::u8> encode_heartbeat(const Heartbeat& message);
std::optional<Heartbeat> decode_heartbeat(const std::vector<lcu::u8>& payload);

struct EntitySnapshot {
    lcu::u32 entity_index = 0;
    lcu::math::Vec3 position;
};
// Known limitation: only the entity's slot index is sent, not its
// generation (see lcu::ecs::EntityId) - a session that never recycles
// entity slots (nothing currently destroys AI entities) can't observe
// the difference, but a client naively keying its own local state by
// entity_index alone could misattribute state to the wrong entity after
// one is destroyed and its slot reused. Not an issue yet - see
// DECISIONS.md.
std::vector<lcu::u8> encode_entity_state(const std::vector<EntitySnapshot>& entities);
std::optional<std::vector<EntitySnapshot>> decode_entity_state(const std::vector<lcu::u8>& payload);

struct PlayerInput {
    lcu::u32 sequence = 0;
    lcu::math::Vec3 horizontal_delta;
    lcu::f32 dt = 0.0f;
};
std::vector<lcu::u8> encode_player_input(const PlayerInput& message);
std::optional<PlayerInput> decode_player_input(const std::vector<lcu::u8>& payload);

struct PlayerCorrection {
    lcu::u32 acknowledged_sequence = 0;
    lcu::math::Vec3 position;
};
std::vector<lcu::u8> encode_player_correction(const PlayerCorrection& message);
std::optional<PlayerCorrection> decode_player_correction(const std::vector<lcu::u8>& payload);

}  // namespace game::systems::protocol
