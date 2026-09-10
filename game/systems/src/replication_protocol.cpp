#include "game/systems/replication_protocol.h"

#include <cstring>

namespace game::systems::protocol {

namespace {

using lcu::f32;
using lcu::i64;
using lcu::u16;
using lcu::u32;
using lcu::u64;
using lcu::u8;
using lcu::usize;

void write_u32_be(std::vector<u8>& out, u32 value) {
    out.push_back(static_cast<u8>((value >> 24) & 0xFF));
    out.push_back(static_cast<u8>((value >> 16) & 0xFF));
    out.push_back(static_cast<u8>((value >> 8) & 0xFF));
    out.push_back(static_cast<u8>(value & 0xFF));
}

void write_i64_be(std::vector<u8>& out, i64 value) {
    u64 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<u8>((bits >> shift) & 0xFF));
    }
}

i64 read_i64_be(const u8* data) {
    u64 bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits = (bits << 8) | data[i];
    }
    i64 value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void write_u16_be(std::vector<u8>& out, u16 value) {
    out.push_back(static_cast<u8>((value >> 8) & 0xFF));
    out.push_back(static_cast<u8>(value & 0xFF));
}

void write_f32_be(std::vector<u8>& out, f32 value) {
    u32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32_be(out, bits);
}

u32 read_u32_be(const u8* data) {
    return (static_cast<u32>(data[0]) << 24) | (static_cast<u32>(data[1]) << 16) |
           (static_cast<u32>(data[2]) << 8) | static_cast<u32>(data[3]);
}

u16 read_u16_be(const u8* data) { return static_cast<u16>((static_cast<u16>(data[0]) << 8) | data[1]); }

f32 read_f32_be(const u8* data) {
    const u32 bits = read_u32_be(data);
    f32 value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void write_vec3(std::vector<u8>& out, const lcu::math::Vec3& v) {
    write_f32_be(out, v.x);
    write_f32_be(out, v.y);
    write_f32_be(out, v.z);
}

lcu::math::Vec3 read_vec3(const u8* data) {
    return {read_f32_be(data), read_f32_be(data + 4), read_f32_be(data + 8)};
}

bool has_type(const std::vector<u8>& payload, MessageType type) {
    return !payload.empty() && payload[0] == static_cast<u8>(type);
}

}  // namespace

std::optional<MessageType> peek_type(const std::vector<u8>& payload) {
    if (payload.empty()) {
        return std::nullopt;
    }
    return static_cast<MessageType>(payload[0]);
}

std::vector<u8> encode_welcome(const Welcome& message) {
    std::vector<u8> out;
    out.push_back(static_cast<u8>(MessageType::Welcome));
    write_u32_be(out, message.world_seed);
    out.push_back(message.tick_rate);
    return out;
}

std::optional<Welcome> decode_welcome(const std::vector<u8>& payload) {
    if (!has_type(payload, MessageType::Welcome) || payload.size() < 6) {
        return std::nullopt;
    }
    Welcome message;
    message.world_seed = read_u32_be(payload.data() + 1);
    message.tick_rate = payload[5];
    return message;
}

std::vector<u8> encode_heartbeat(const Heartbeat& message) {
    std::vector<u8> out;
    out.push_back(static_cast<u8>(MessageType::Heartbeat));
    write_u32_be(out, message.tick);
    write_u16_be(out, message.entity_count);
    return out;
}

std::optional<Heartbeat> decode_heartbeat(const std::vector<u8>& payload) {
    if (!has_type(payload, MessageType::Heartbeat) || payload.size() < 7) {
        return std::nullopt;
    }
    Heartbeat message;
    message.tick = read_u32_be(payload.data() + 1);
    message.entity_count = read_u16_be(payload.data() + 5);
    return message;
}

std::vector<u8> encode_entity_state(const std::vector<EntitySnapshot>& entities) {
    std::vector<u8> out;
    out.push_back(static_cast<u8>(MessageType::EntityState));
    // Capped at 255 entries (a one-byte count field) - well beyond what
    // fits in one UDP datagram anyway (kMaxDatagramSize / 16 bytes per
    // entry is far fewer than 255), so this cap is never the actual
    // limiting factor in practice.
    out.push_back(static_cast<u8>(entities.size() > 255 ? 255 : entities.size()));
    for (usize i = 0; i < entities.size() && i < 255; ++i) {
        write_u32_be(out, entities[i].entity_index);
        write_vec3(out, entities[i].position);
    }
    return out;
}

std::optional<std::vector<EntitySnapshot>> decode_entity_state(const std::vector<u8>& payload) {
    if (!has_type(payload, MessageType::EntityState) || payload.size() < 2) {
        return std::nullopt;
    }
    const usize count = payload[1];
    constexpr usize kEntryBytes = 4 + 12;  // entity_index (u32) + Vec3
    if (payload.size() < 2 + count * kEntryBytes) {
        return std::nullopt;
    }
    std::vector<EntitySnapshot> entities;
    entities.reserve(count);
    const u8* cursor = payload.data() + 2;
    for (usize i = 0; i < count; ++i) {
        EntitySnapshot snapshot;
        snapshot.entity_index = read_u32_be(cursor);
        snapshot.position = read_vec3(cursor + 4);
        entities.push_back(snapshot);
        cursor += kEntryBytes;
    }
    return entities;
}

std::vector<u8> encode_player_input(const PlayerInput& message) {
    std::vector<u8> out;
    out.push_back(static_cast<u8>(MessageType::PlayerInput));
    write_u32_be(out, message.sequence);
    write_vec3(out, message.horizontal_delta);
    write_f32_be(out, message.dt);
    return out;
}

std::optional<PlayerInput> decode_player_input(const std::vector<u8>& payload) {
    if (!has_type(payload, MessageType::PlayerInput) || payload.size() < 21) {
        return std::nullopt;
    }
    PlayerInput message;
    message.sequence = read_u32_be(payload.data() + 1);
    message.horizontal_delta = read_vec3(payload.data() + 5);
    message.dt = read_f32_be(payload.data() + 17);
    return message;
}

std::vector<u8> encode_player_correction(const PlayerCorrection& message) {
    std::vector<u8> out;
    out.push_back(static_cast<u8>(MessageType::PlayerCorrection));
    write_u32_be(out, message.acknowledged_sequence);
    write_vec3(out, message.position);
    return out;
}

std::optional<PlayerCorrection> decode_player_correction(const std::vector<u8>& payload) {
    if (!has_type(payload, MessageType::PlayerCorrection) || payload.size() < 17) {
        return std::nullopt;
    }
    PlayerCorrection message;
    message.acknowledged_sequence = read_u32_be(payload.data() + 1);
    message.position = read_vec3(payload.data() + 5);
    return message;
}

std::vector<u8> encode_block_action(const BlockAction& message) {
    std::vector<u8> out;
    out.push_back(static_cast<u8>(MessageType::BlockAction));
    out.push_back(static_cast<u8>(message.action));
    write_i64_be(out, message.x);
    write_i64_be(out, message.y);
    write_i64_be(out, message.z);
    write_u16_be(out, message.block_id);
    return out;
}

std::optional<BlockAction> decode_block_action(const std::vector<u8>& payload) {
    // type(1) + action(1) + x,y,z(8 each) + block_id(2) = 28 bytes.
    if (!has_type(payload, MessageType::BlockAction) || payload.size() < 28) {
        return std::nullopt;
    }
    if (payload[1] != static_cast<u8>(BlockActionType::Break) &&
        payload[1] != static_cast<u8>(BlockActionType::Place)) {
        return std::nullopt;
    }
    BlockAction message;
    message.action = static_cast<BlockActionType>(payload[1]);
    message.x = read_i64_be(payload.data() + 2);
    message.y = read_i64_be(payload.data() + 10);
    message.z = read_i64_be(payload.data() + 18);
    message.block_id = read_u16_be(payload.data() + 26);
    return message;
}

std::vector<u8> encode_block_change(const BlockChange& message) {
    std::vector<u8> out;
    out.push_back(static_cast<u8>(MessageType::BlockChange));
    write_i64_be(out, message.x);
    write_i64_be(out, message.y);
    write_i64_be(out, message.z);
    write_u16_be(out, message.block_id);
    return out;
}

std::optional<BlockChange> decode_block_change(const std::vector<u8>& payload) {
    // type(1) + x,y,z(8 each) + block_id(2) = 27 bytes.
    if (!has_type(payload, MessageType::BlockChange) || payload.size() < 27) {
        return std::nullopt;
    }
    BlockChange message;
    message.x = read_i64_be(payload.data() + 1);
    message.y = read_i64_be(payload.data() + 9);
    message.z = read_i64_be(payload.data() + 17);
    message.block_id = read_u16_be(payload.data() + 25);
    return message;
}

}  // namespace game::systems::protocol
