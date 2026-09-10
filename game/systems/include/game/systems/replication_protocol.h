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
    ChunkData = 7,          // logical only - a full chunk snapshot, too large for one datagram. Never sent
                             // directly; always split into ChunkDataFragment pieces (see fragmentation.h)
                             // and reassembled before decode_chunk_data ever sees it.
    ChunkDataFragment = 8,  // server -> client, on connect: one fragment of a fragmented ChunkData message.
                             // ReliableOrdered.
    InventoryUpdate = 9,    // server -> one client, after a BlockAction that affects inventory (accepted or
                             // rejected): that client's authoritative count for one item. ReliableOrdered.
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

// A full chunk snapshot: coordinates plus the same zstd-compressed bytes
// lcu::serialization::serialize_chunk_to_bytes produces. Sent by the
// server to a newly-connecting client for every chunk it has loaded
// (VoxelServer), so the client's world matches the server's authoritative
// one instead of trusting its own independently-generated (if
// deterministic-and-usually-identical) local terrain - see
// NETWORKING.md "Chunk network streaming" and DECISIONS.md. A compressed
// chunk is typically a few KB, so encode_chunk_data's output is fragmented
// via lcu::network::fragment_payload before ever going out on the wire -
// see encode_chunk_data_fragment below.
struct ChunkData {
    lcu::i32 chunk_x = 0;
    lcu::i32 chunk_y = 0;
    lcu::i32 chunk_z = 0;
    std::vector<lcu::u8> compressed_bytes;
};
std::vector<lcu::u8> encode_chunk_data(const ChunkData& message);
std::optional<ChunkData> decode_chunk_data(const std::vector<lcu::u8>& payload);

// Wraps one already-fragmented piece of an encode_chunk_data payload (as
// produced by lcu::network::fragment_payload) with the ChunkDataFragment
// type byte, ready to send over Connection::send like any other message.
// decode_chunk_data_fragment strips that byte back off, handing the raw
// fragment bytes to lcu::network::FragmentReassembler::add_fragment;
// once every fragment for a message has arrived, decode_chunk_data runs
// on the reassembled result.
std::vector<lcu::u8> encode_chunk_data_fragment(const std::vector<lcu::u8>& fragment_bytes);
std::optional<std::vector<lcu::u8>> decode_chunk_data_fragment(const std::vector<lcu::u8>& payload);

// The server's authoritative count of one item in the requesting
// client's server-side inventory (brief section 20 "never trust client
// data" applied to item accounting, not just block edits) - sent after
// every `BlockAction` that could have affected it, accepted or
// rejected, so a client's own optimistic local guess (see
// DECISIONS.md "item pickup/consumption stays client-authoritative")
// gets corrected the moment it diverges from what the server actually
// did, not just when it happens to agree. `item_id` is meaningful only
// insofar as both sides register the same items in the same order (see
// DECISIONS.md "server-side inventory (Phase 15)") - not a general
// cross-process item-id sync mechanism.
struct InventoryUpdate {
    lcu::u16 item_id = 0;
    lcu::u32 count = 0;
};
std::vector<lcu::u8> encode_inventory_update(const InventoryUpdate& message);
std::optional<InventoryUpdate> decode_inventory_update(const std::vector<lcu::u8>& payload);

}  // namespace game::systems::protocol
