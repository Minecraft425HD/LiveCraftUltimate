#include <chrono>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>

#include "game/components/ai_wander.h"
#include "game/components/position.h"
#include "game/systems/ai_wander_system.h"
#include "lcu/core/log.h"
#include "lcu/core/types.h"
#include "lcu/ecs/registry.h"
#include "lcu/network/connection.h"
#include "lcu/network/udp_socket.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"
#include "lcu/world/world.h"
#include "lcu/world/worldgen.h"

namespace {

struct ServerConfig {
    std::string world = "world";
    lcu::u16 port = 25565;
};

// Minimal CLI parsing for `VoxelServer --world <name> --port <n>`. Full
// server config file support lands with a dedicated config system, not
// this phase's job.
ServerConfig parse_args(int argc, char** argv) {
    ServerConfig config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--world" && i + 1 < argc) {
            config.world = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<lcu::u16>(std::strtoul(argv[++i], nullptr, 10));
        }
    }
    return config;
}

std::optional<lcu::u64> max_ticks_from_env() {
    const char* value = std::getenv("LCU_MAX_TICKS");
    if (!value) {
        return std::nullopt;
    }
    return static_cast<lcu::u64>(std::strtoull(value, nullptr, 10));
}

constexpr lcu::u32 kWorldSeed = 1337;
constexpr lcu::i32 kLoadRadiusXZ = 1;
constexpr lcu::i32 kMinChunkY = 0;
constexpr lcu::i32 kMaxChunkY = 3;
constexpr int kAiEntityCount = 3;
constexpr lcu::u32 kAiRngSeed = 20260909;
constexpr int kTicksPerSecond = 20;

// Application-level messages riding on top of engine/network's channel
// transport - engine/network itself is deliberately game-agnostic (it
// has no idea what a "world seed" is), so this small hand-rolled framing
// (a one-byte type tag, then fixed big-endian fields - the same manual
// approach packet_header.cpp uses, not a generic serialization
// framework nothing else needs yet) lives here in server/ instead.
enum class MessageType : lcu::u8 { Welcome = 0, Heartbeat = 1 };

std::vector<lcu::u8> encode_welcome(lcu::u32 world_seed, lcu::u8 tick_rate) {
    return {
        static_cast<lcu::u8>(MessageType::Welcome),
        static_cast<lcu::u8>((world_seed >> 24) & 0xFF),
        static_cast<lcu::u8>((world_seed >> 16) & 0xFF),
        static_cast<lcu::u8>((world_seed >> 8) & 0xFF),
        static_cast<lcu::u8>(world_seed & 0xFF),
        tick_rate,
    };
}

std::vector<lcu::u8> encode_heartbeat(lcu::u64 tick, lcu::u16 entity_count) {
    const auto tick32 = static_cast<lcu::u32>(tick);
    return {
        static_cast<lcu::u8>(MessageType::Heartbeat),
        static_cast<lcu::u8>((tick32 >> 24) & 0xFF),
        static_cast<lcu::u8>((tick32 >> 16) & 0xFF),
        static_cast<lcu::u8>((tick32 >> 8) & 0xFF),
        static_cast<lcu::u8>(tick32 & 0xFF),
        static_cast<lcu::u8>((entity_count >> 8) & 0xFF),
        static_cast<lcu::u8>(entity_count & 0xFF),
    };
}

}  // namespace

int main(int argc, char** argv) {
    const ServerConfig config = parse_args(argc, argv);

    LCU_LOG_INFO("VoxelServer starting: world=\"{}\" port={}", config.world, config.port);
    LCU_LOG_INFO("Headless dedicated server: no SDL, no bgfx, no GPU (see ARCHITECTURE.md)");

    // --- Real world simulation (replacing the Phase 0 sleep-only placeholder) ---
    lcu::voxel::BlockRegistry block_registry;
    lcu::voxel::BlockDefinition stone_def;
    stone_def.namespaced_id = "game:stone";
    stone_def.display_name = "Stone";
    stone_def.is_transparent = false;
    stone_def.has_collision = true;
    const lcu::voxel::BlockId stone_id = block_registry.register_block(stone_def);

    lcu::world::World world(kWorldSeed, [&](lcu::voxel::Chunk& chunk, lcu::voxel::ChunkCoord coord) {
        lcu::world::worldgen::generate_terrain_chunk(chunk, coord, kWorldSeed, stone_id);
    });
    for (lcu::i32 cx = -kLoadRadiusXZ; cx <= kLoadRadiusXZ; ++cx) {
        for (lcu::i32 cz = -kLoadRadiusXZ; cz <= kLoadRadiusXZ; ++cz) {
            for (lcu::i32 cy = kMinChunkY; cy <= kMaxChunkY; ++cy) {
                world.load_chunk({cx, cy, cz});
            }
        }
    }
    LCU_LOG_INFO("Loaded {} chunks (seed={})", world.loaded_chunk_count(), kWorldSeed);

    // Same wandering AI simulation VoxelClient runs, now driven
    // server-side - a dedicated server needs to simulate entities
    // whether or not any client is even connected to watch them (brief
    // section 64's "server-authoritative state").
    lcu::ecs::Registry entity_registry;
    std::mt19937 ai_rng(kAiRngSeed);
    const lcu::i32 spawn_ground_y = lcu::world::worldgen::terrain_height(kWorldSeed, 0, 0) + 1;
    for (int i = 0; i < kAiEntityCount; ++i) {
        const lcu::f32 angle = static_cast<lcu::f32>(i) * (6.28318f / static_cast<lcu::f32>(kAiEntityCount));
        const lcu::math::Vec3 spawn_pos{4.0f * std::cos(angle), static_cast<lcu::f32>(spawn_ground_y),
                                         4.0f * std::sin(angle)};
        const lcu::ecs::EntityId entity = entity_registry.create_entity();
        entity_registry.add_component<game::components::Position>(entity, {spawn_pos});
        entity_registry.add_component<game::components::AIWander>(entity, {spawn_pos, 1.5f, 0.0f});
    }
    game::systems::AIWanderConfig ai_wander_config;
    LCU_LOG_INFO("Spawned {} wandering AI entities", entity_registry.entity_count());

    // --- Real network transport (replacing "no networking at all") ---
    lcu::network::UdpSocket socket;
    if (!socket.bind(config.port)) {
        LCU_LOG_ERROR("Failed to bind UDP port {}, exiting", config.port);
        return 1;
    }
    LCU_LOG_INFO("Listening for connections on UDP port {}", config.port);

    // UDP has no built-in notion of "a connection" - the server treats
    // any address it's received at least one datagram from as a known
    // client, keyed by that address (the same technique
    // tests/network/loopback_integration_test.cpp uses to "learn" a
    // peer's address). No handshake/auth beyond that exists yet - see
    // DECISIONS.md; nothing gatekeeps who can send a packet and be
    // treated as connected, which is fine for this vertical slice with
    // no untrusted network exposure, not for a real public server.
    std::unordered_map<lcu::network::Address, lcu::network::Connection> connections;

    constexpr auto kTickDuration = std::chrono::milliseconds(1000 / kTicksPerSecond);
    constexpr lcu::f32 kTickDt = 1.0f / static_cast<lcu::f32>(kTicksPerSecond);

    const std::optional<lcu::u64> max_ticks = max_ticks_from_env();
    lcu::u64 tick = 0;

    while (true) {
        ++tick;

        lcu::network::Address from;
        while (auto packet = socket.try_receive(from)) {
            auto [it, inserted] = connections.try_emplace(from);
            if (inserted) {
                LCU_LOG_INFO("New client connection from {}", from.to_string());
            }
            const auto messages = it->second.on_packet_received(*packet);
            for (const auto& message : messages) {
                LCU_LOG_DEBUG("Received a {}-byte message on channel {} from {}", message.payload.size(),
                              static_cast<int>(message.channel), from.to_string());
            }
            if (inserted) {
                // The real handshake: greet a newly-seen connection with
                // the world seed and tick rate over the reliable channel.
                it->second.send(lcu::network::Channel::ReliableOrdered,
                                 encode_welcome(kWorldSeed, static_cast<lcu::u8>(kTicksPerSecond)));
                LCU_LOG_INFO("Sent Welcome (seed={}, tick_rate={}) to {}", kWorldSeed, kTicksPerSecond,
                             from.to_string());
            }
        }

        game::systems::update_ai_wander(entity_registry, ai_wander_config, ai_rng, kTickDt);

        // A per-tick heartbeat to every known client on the unreliable
        // channel - real, live proof the unreliable path works in an
        // actual running server loop, not just in isolated tests.
        for (auto& [addr, connection] : connections) {
            connection.send(lcu::network::Channel::UnreliableSequenced,
                             encode_heartbeat(tick, static_cast<lcu::u16>(entity_registry.entity_count())));
        }

        for (auto& [addr, connection] : connections) {
            connection.update(kTickDt);
            for (auto& packet : connection.take_outgoing_packets()) {
                socket.send_to(addr, packet);
            }
        }

        if (max_ticks && tick >= *max_ticks) {
            LCU_LOG_INFO("LCU_MAX_TICKS reached ({} ticks), shutting down", tick);
            break;
        }
        std::this_thread::sleep_for(kTickDuration);
    }

    LCU_LOG_INFO("VoxelServer shut down cleanly after {} ticks, {} client connection(s) seen", tick,
                 connections.size());
    return 0;
}
