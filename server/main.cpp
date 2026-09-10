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
#include "lcu/core/quality_profile.h"
#include "lcu/core/types.h"
#include "lcu/ecs/registry.h"
#include "lcu/items/item_registry.h"
#include "lcu/network/connection.h"
#include "lcu/network/fragmentation.h"
#include "lcu/network/udp_socket.h"
#include "lcu/physics/collision.h"
#include "lcu/serialization/chunk_serializer.h"
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

// Sized by LCU_QUALITY_PROFILE (Phase 10, see lcu::core::QualityProfile) -
// defaults to Desktop, numerically identical to this vertical slice's
// original hardcoded 1/0/3 radius/min/max values.
lcu::core::ChunkLoadSettings load_settings_from_env() {
    const char* profile_name = std::getenv("LCU_QUALITY_PROFILE");
    lcu::core::QualityProfile profile = lcu::core::QualityProfile::Desktop;
    if (profile_name != nullptr) {
        profile = lcu::core::parse_quality_profile(profile_name).value_or(lcu::core::QualityProfile::Desktop);
    }
    return lcu::core::chunk_load_settings_for(profile);
}

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

// A requested block edit is rejected if its target is farther than this
// from the requesting client's own (server-known) player position -
// brief section 20's "never blindly accept client data": without this, a
// malicious client could send BlockAction for any coordinate in the
// loaded world regardless of where its player actually is. Deliberately
// looser than VoxelClient's own kInteractRange (6.0f, client/main.cpp) to
// tolerate normal client/server position drift under real latency, not
// tuned any tighter without real-world ping data to justify it.
constexpr lcu::f32 kMaxBlockActionRange = 10.0f;

struct ClientState {
    lcu::network::Connection connection;
    lcu::physics::PlayerPhysicsState player;
    lcu::u32 last_acknowledged_sequence = 0;
    // Distinguishes this client's concurrently in-flight fragmented
    // ChunkData messages from each other (see lcu::network::fragment_payload).
    // A per-client counter is enough - see fragmentation.h.
    lcu::u16 next_chunk_message_id = 0;
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

    // Not otherwise used by the server yet - there is no server-side
    // inventory/item-drop system (block edits themselves are now
    // replicated, see handle_block_action below and NETWORKING.md, but
    // items stay entirely client-local for now). Exists here purely so
    // mods share one namespaced id space across client and server (see
    // the LCU_ENABLE_SCRIPTING block below) - a mod that calls
    // register_item must not fail to load on the server just because
    // nothing server-side reads the result yet.
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
    const lcu::core::ChunkLoadSettings load_settings = load_settings_from_env();
    for (lcu::i32 cx = -load_settings.radius_xz; cx <= load_settings.radius_xz; ++cx) {
        for (lcu::i32 cz = -load_settings.radius_xz; cz <= load_settings.radius_xz; ++cz) {
            for (lcu::i32 cy = load_settings.min_chunk_y; cy <= load_settings.max_chunk_y; ++cy) {
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

    // Every successfully applied block edit, in order - replayed in full
    // to each newly connecting client right after its Welcome (see the
    // `inserted` handling below) so it catches up on edits that happened
    // before it joined, not just ones that happen from now on. Unbounded
    // for the lifetime of this process - fine for a vertical slice's
    // session lengths; a real long-running server would need to
    // periodically compact this against actual persisted chunk state
    // (once chunk save/load is wired to a server trigger - see
    // PROJECT_STATE.md "Known Limitations") rather than keep every edit
    // forever.
    std::vector<protocol::BlockChange> block_change_history;

    // Server-authoritative block editing (brief section 8/19/20): validates
    // and applies a client's requested break/place against this server's
    // own World, then broadcasts the result to every connected client
    // (including the requester - no client mutates its own World
    // speculatively for a block edit, see DECISIONS.md). Rejects a
    // request whose target chunk isn't loaded, whose target block isn't
    // actually breakable/placeable given its current state, whose
    // requested block_id isn't a registered block, or whose target is too
    // far from the requesting client's own known position - logged, no
    // reply sent (the requester's world simply doesn't change).
    auto handle_block_action = [&](const lcu::network::Address& from, ClientState& client,
                                    const protocol::BlockAction& action) {
        const lcu::voxel::BlockWorldCoord target{action.x, action.y, action.z};
        const lcu::math::Vec3 target_center{static_cast<lcu::f32>(target.x) + 0.5f,
                                             static_cast<lcu::f32>(target.y) + 0.5f,
                                             static_cast<lcu::f32>(target.z) + 0.5f};
        const lcu::f32 distance = lcu::math::length(target_center - client.player.aabb.center());
        if (distance > kMaxBlockActionRange) {
            LCU_LOG_WARN("Rejected BlockAction from {}: target ({},{},{}) is {:.1f} blocks away (max {})",
                         from.to_string(), target.x, target.y, target.z, distance, kMaxBlockActionRange);
            return;
        }

        const auto split = lcu::voxel::world_to_chunk_and_local(target, lcu::voxel::Chunk::kEdgeLength);
        lcu::voxel::Chunk* chunk = world.chunk_at_mutable(split.chunk);
        if (chunk == nullptr) {
            LCU_LOG_WARN("Rejected BlockAction from {}: chunk ({},{},{}) isn't loaded", from.to_string(),
                         split.chunk.x, split.chunk.y, split.chunk.z);
            return;
        }

        const lcu::voxel::BlockId current = chunk->block_at(split.local.x, split.local.y, split.local.z);
        lcu::voxel::BlockId new_id = current;
        bool valid = false;
        if (action.action == protocol::BlockActionType::Break) {
            valid = current != lcu::voxel::kAirBlockId;
            new_id = lcu::voxel::kAirBlockId;
        } else {
            valid = current == lcu::voxel::kAirBlockId && action.block_id < block_registry.count();
            new_id = action.block_id;
        }
        if (!valid) {
            LCU_LOG_WARN("Rejected BlockAction from {}: target ({},{},{}) current_block={} requested_block={}",
                         from.to_string(), target.x, target.y, target.z, current, action.block_id);
            return;
        }

        chunk->set_block(split.local.x, split.local.y, split.local.z, new_id);
        LCU_LOG_INFO("Applied BlockAction from {}: ({},{},{}) {} -> {}", from.to_string(), target.x, target.y,
                     target.z, current, new_id);
#if defined(LCU_ENABLE_SCRIPTING)
        if (new_id == lcu::voxel::kAirBlockId) {
            mod_event_bus.emit_block_broken(target.x, target.y, target.z, current);
        }
#endif
        const protocol::BlockChange change{target.x, target.y, target.z, new_id};
        block_change_history.push_back(change);
        const auto change_bytes = protocol::encode_block_change(change);
        for (auto& [broadcast_addr, broadcast_client] : clients) {
            broadcast_client.connection.send(lcu::network::Channel::ReliableOrdered, change_bytes);
        }
    };

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
                if (const auto input = protocol::decode_player_input(message.payload)) {
                    const lcu::f32 dt = std::min(input->dt, kMaxAcceptedInputDt);
                    lcu::physics::apply_gravity(client.player, physics_config, dt);
                    lcu::physics::integrate_player(world, client.player, input->horizontal_delta, physics_config, dt,
                                                    is_solid);
                    client.last_acknowledged_sequence = input->sequence;
                    continue;
                }
                if (const auto action = protocol::decode_block_action(message.payload)) {
                    handle_block_action(from, client, *action);
                    continue;
                }
                // Neither a PlayerInput nor a BlockAction (or a malformed
                // one) - nothing else expected client->server yet.
            }

            if (inserted) {
                // The real handshake: greet a newly-seen connection with
                // the world seed and tick rate over the reliable channel.
                client.connection.send(lcu::network::Channel::ReliableOrdered,
                                        protocol::encode_welcome({kWorldSeed, static_cast<lcu::u8>(kTicksPerSecond)}));
                LCU_LOG_INFO("Sent Welcome (seed={}, tick_rate={}) to {}", kWorldSeed, kTicksPerSecond,
                             from.to_string());

                // Catch this client up on every block edit that happened
                // before it connected - without this, a late joiner's
                // World would silently disagree with everyone else's
                // (see NETWORKING.md "no world-diff catch-up", now fixed).
                for (const auto& change : block_change_history) {
                    client.connection.send(lcu::network::Channel::ReliableOrdered,
                                            protocol::encode_block_change(change));
                }
                if (!block_change_history.empty()) {
                    LCU_LOG_INFO("Replayed {} historical block change(s) to {}", block_change_history.size(),
                                 from.to_string());
                }

                // Full authoritative world sync (brief section 19 "chunk
                // streaming"): send every chunk this server has loaded, so
                // the new client's world matches this server's actual
                // (possibly edited) state instead of trusting its own
                // independently-generated terrain to happen to agree (see
                // NETWORKING.md "Chunk network streaming"). Each chunk's
                // compressed bytes are typically a few KB - too big for one
                // UDP datagram - so they're split via fragment_payload and
                // sent as a run of ChunkDataFragment messages, reassembled
                // client-side before being applied.
                //
                // Known simplification, documented (see DECISIONS.md): this
                // is a one-shot full sync on connect, not interest-managed
                // by distance (unlike kInterestRadius for entities) and not
                // re-streamed as the client moves - both loaded worlds are
                // small enough in this vertical slice for that gap not to
                // matter yet.
                lcu::usize chunks_sent = 0;
                lcu::usize fragments_sent = 0;
                for (const lcu::voxel::ChunkCoord& coord : world.loaded_chunk_coords()) {
                    const lcu::voxel::Chunk* chunk_data = world.chunk_at(coord);
                    if (chunk_data == nullptr) {
                        continue;
                    }
                    const protocol::ChunkData chunk_message{coord.x, coord.y, coord.z,
                                                              lcu::serialization::serialize_chunk_to_bytes(*chunk_data)};
                    const auto encoded = protocol::encode_chunk_data(chunk_message);
                    const auto fragments =
                        lcu::network::fragment_payload(encoded, client.next_chunk_message_id++,
                                                        lcu::network::kMaxFragmentDataSize);
                    for (const auto& fragment : fragments) {
                        client.connection.send(lcu::network::Channel::ReliableOrdered,
                                                protocol::encode_chunk_data_fragment(fragment));
                    }
                    ++chunks_sent;
                    fragments_sent += fragments.size();
                }
                LCU_LOG_INFO("Sent {} chunk(s) ({} fragment(s)) to {}", chunks_sent, fragments_sent, from.to_string());
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
