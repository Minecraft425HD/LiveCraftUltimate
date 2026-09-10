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
    BlockAction = 5,       // client -> server, on break/place: a requested block edit. ReliableOrdered.
    BlockChange = 6,       // server -> client, broadcast to all: an applied, authoritative block edit. ReliableOrdered.
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

// Which edit a BlockAction is requesting - matches the client's existing
// Interact (break)/PlaceBlock actions (see client/main.cpp).
enum class BlockActionType : lcu::u8 {
    Break = 0,
    Place = 1,
};

// Client's requested edit - a request, not a fact: the server validates it
// (target chunk loaded, break targets a non-air block, place targets an
// air block, block_id is registered, and the position is within
// kMaxBlockActionRange of that client's own server-known player position -
// see server/main.cpp) before ever touching its own World. A rejected
// request produces no BlockChange and is otherwise silently dropped - the
// requester's world simply doesn't change, same outcome as any other
// no-op edit attempt.
struct BlockAction {
    BlockActionType action = BlockActionType::Break;
    lcu::i64 x = 0;
    lcu::i64 y = 0;
    lcu::i64 z = 0;
    lcu::u16 block_id = 0;  // meaningful for Place only; ignored (but still sent) for Break.
};
std::vector<lcu::u8> encode_block_action(const BlockAction& message);
std::optional<BlockAction> decode_block_action(const std::vector<lcu::u8>& payload);

// Server's authoritative result of an applied edit, broadcast to every
// connected client (including whichever one requested it - this client
// never mutates its own World speculatively, see DECISIONS.md "block
// edits are not client-predicted"). `block_id` is the resulting block at
// (x, y, z) - kAirBlockId for a successful break, the placed id for a
// successful place.
struct BlockChange {
    lcu::i64 x = 0;
    lcu::i64 y = 0;
    lcu::i64 z = 0;
    lcu::u16 block_id = 0;
};
std::vector<lcu::u8> encode_block_change(const BlockChange& message);
std::optional<BlockChange> decode_block_change(const std::vector<lcu::u8>& payload);

}  // namespace game::systems::protocol
