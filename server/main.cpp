#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "game/components/ai_wander.h"
#include "game/components/position.h"
#include "game/systems/ai_wander_system.h"
#include "game/systems/replication_protocol.h"
#include "lcu/core/log.h"
#include "lcu/core/types.h"
#include "lcu/ecs/registry.h"
#include "lcu/items/item_registry.h"
#include "lcu/network/connection.h"
#include "lcu/network/udp_socket.h"
#include "lcu/physics/collision.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"
#include "lcu/world/world.h"
#include "lcu/world/worldgen.h"

#if defined(LCU_ENABLE_SCRIPTING)
#include "lcu/modding/event_bus.h"
#include "lcu/modding/mod_loader.h"
#include "lcu/modding/registry_bindings.h"
#include "lcu/scripting/lua_state.h"
#endif

namespace {

namespace protocol = game::systems::protocol;

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

// A client-reported PlayerInput's `dt` is trusted for the movement math
// (brief section 64 doesn't require full movement validation/anti-cheat
// yet - see DECISIONS.md), but clamped to a sane ceiling so a bogus or
// malicious huge dt can't move a player an absurd distance in one input.
constexpr lcu::f32 kMaxAcceptedInputDt = 0.25f;

// How often (in ticks) each connection gets a PlayerCorrection - not
// every single tick, since the correction only matters when there's
// something to reconcile against, and this keeps the broadcast volume
// down (still real "authoritative periodic sync", just not maximal
// frequency - see DECISIONS.md).
constexpr lcu::u64 kCorrectionIntervalTicks = 4;

// Interest management (brief section 22/64): a client's EntityState
// broadcast only includes AI entities within this many blocks of that
// client's own (server-known) player position - real filtering logic,
// even though every entity in this vertical slice's small world
// currently falls within it (see DECISIONS.md).
constexpr lcu::f32 kInterestRadius = 24.0f;

struct ClientState {
    lcu::network::Connection connection;
    lcu::physics::PlayerPhysicsState player;
    lcu::u32 last_acknowledged_sequence = 0;
};

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

    // Not otherwise used by the server yet (no inventories, no item drops -
    // see NETWORKING.md "what's deferred": block edits aren't replicated).
    // It exists here purely so mods share one namespaced id space across
    // client and server (see the LCU_ENABLE_SCRIPTING block below) - a mod
    // that calls register_item must not fail to load on the server just
    // because nothing server-side reads the result yet.
    lcu::items::ItemRegistry item_registry;

#if defined(LCU_ENABLE_SCRIPTING)
    // Mods run here too (Phase 9) so a mod's registered blocks/items exist
    // in the server's authoritative registries, not just the client's -
    // ids otherwise silently drift between the two (registries aren't
    // synced over the wire yet, so both sides still have to load the same
    // mods locally to agree). EventBus is constructed and exposed even
    // though the server never calls emit_block_broken() itself (block
    // edits aren't replicated) - a mod script is shared between client and
    // server, so lcu.subscribe(...) has to exist on both or a mod that
    // calls it unconditionally fails to load on whichever host lacks it.
    lcu::scripting::LuaState mod_lua;
    lcu::modding::EventBus mod_event_bus(mod_lua);
    mod_event_bus.expose_to_lua();
    lcu::modding::bind_block_registry(mod_lua, block_registry);
    lcu::modding::bind_item_registry(mod_lua, item_registry);
    lcu::modding::ModLoader mod_loader(mod_lua);
    mod_loader.load_all("mods");
#endif

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

    const auto is_solid = [&](lcu::voxel::BlockId id) { return block_registry.definition_of(id).has_collision; };
    const lcu::physics::PlayerPhysicsConfig physics_config;
    const lcu::i32 spawn_ground_y = lcu::world::worldgen::terrain_height(kWorldSeed, 0, 0) + 1;

    auto make_spawn_aabb = [&](lcu::math::Vec3 feet) {
        constexpr lcu::f32 kHalfWidth = 0.3f;
        constexpr lcu::f32 kHeight = 1.8f;
        return lcu::physics::AABB{{feet.x - kHalfWidth, feet.y, feet.z - kHalfWidth},
                                   {feet.x + kHalfWidth, feet.y + kHeight, feet.z + kHalfWidth}};
    };

    // Same wandering AI simulation VoxelClient runs, now driven
    // server-side - a dedicated server needs to simulate entities
    // whether or not any client is even connected to watch them (brief
    // section 64's "server-authoritative state").
    lcu::ecs::Registry entity_registry;
    std::mt19937 ai_rng(kAiRngSeed);
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
    std::unordered_map<lcu::network::Address, ClientState> clients;

    constexpr auto kTickDuration = std::chrono::milliseconds(1000 / kTicksPerSecond);
    constexpr lcu::f32 kTickDt = 1.0f / static_cast<lcu::f32>(kTicksPerSecond);

    const std::optional<lcu::u64> max_ticks = max_ticks_from_env();
    lcu::u64 tick = 0;

    while (true) {
        ++tick;

        lcu::network::Address from;
        while (auto packet = socket.try_receive(from)) {
            auto [it, inserted] = clients.try_emplace(from);
            ClientState& client = it->second;
            if (inserted) {
                client.player.aabb = make_spawn_aabb({0.0f, static_cast<lcu::f32>(spawn_ground_y), 0.0f});
                LCU_LOG_INFO("New client connection from {}", from.to_string());
            }

            const auto messages = client.connection.on_packet_received(*packet);
            for (const auto& message : messages) {
                const auto input = protocol::decode_player_input(message.payload);
                if (!input) {
                    continue;  // not a PlayerInput (or a malformed one) - nothing else expected client->server yet.
                }
                const lcu::f32 dt = std::min(input->dt, kMaxAcceptedInputDt);
                lcu::physics::apply_gravity(client.player, physics_config, dt);
                lcu::physics::integrate_player(world, client.player, input->horizontal_delta, physics_config, dt,
                                                is_solid);
                client.last_acknowledged_sequence = input->sequence;
            }

            if (inserted) {
                // The real handshake: greet a newly-seen connection with
                // the world seed and tick rate over the reliable channel.
                client.connection.send(lcu::network::Channel::ReliableOrdered,
                                        protocol::encode_welcome({kWorldSeed, static_cast<lcu::u8>(kTicksPerSecond)}));
                LCU_LOG_INFO("Sent Welcome (seed={}, tick_rate={}) to {}", kWorldSeed, kTicksPerSecond,
                             from.to_string());
            }
        }

        game::systems::update_ai_wander(entity_registry, ai_wander_config, ai_rng, kTickDt);

        // Snapshot every AI entity's current position once per tick -
        // shared across all clients' (interest-filtered) EntityState
        // messages below rather than re-walking the registry per client.
        std::vector<protocol::EntitySnapshot> all_entities;
        for (const lcu::ecs::EntityId& entity : entity_registry.pool_for<game::components::AIWander>().dense_entities()) {
            const auto* pos = entity_registry.get_component<game::components::Position>(entity);
            if (pos != nullptr) {
                all_entities.push_back({entity.index, pos->value});
            }
        }

        for (auto& [addr, client] : clients) {
            // A per-tick heartbeat - real, live proof the unreliable
            // path works in an actual running server loop, not just in
            // isolated tests.
            client.connection.send(
                lcu::network::Channel::UnreliableSequenced,
                protocol::encode_heartbeat(
                    {static_cast<lcu::u32>(tick), static_cast<lcu::u16>(entity_registry.entity_count())}));

            // Interest management: only entities within kInterestRadius
            // of this client's own (server-known) player position.
            const lcu::math::Vec3 client_pos = client.player.aabb.center();
            std::vector<protocol::EntitySnapshot> visible_entities;
            for (const auto& snapshot : all_entities) {
                if (lcu::math::length(snapshot.position - client_pos) <= kInterestRadius) {
                    visible_entities.push_back(snapshot);
                }
            }
            client.connection.send(lcu::network::Channel::UnreliableSequenced,
                                    protocol::encode_entity_state(visible_entities));

            if (tick % kCorrectionIntervalTicks == 0) {
                client.connection.send(
                    lcu::network::Channel::UnreliableSequenced,
                    protocol::encode_player_correction({client.last_acknowledged_sequence, client.player.aabb.min}));
            }
        }

        for (auto& [addr, client] : clients) {
            client.connection.update(kTickDt);
            for (auto& packet : client.connection.take_outgoing_packets()) {
                socket.send_to(addr, packet);
            }
        }

        if (max_ticks && tick >= *max_ticks) {
            LCU_LOG_INFO("LCU_MAX_TICKS reached ({} ticks), shutting down", tick);
            break;
        }
        std::this_thread::sleep_for(kTickDuration);
    }

    LCU_LOG_INFO("VoxelServer shut down cleanly after {} ticks, {} client connection(s) seen", tick, clients.size());
    return 0;
}
