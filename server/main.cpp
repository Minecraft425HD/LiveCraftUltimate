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
#include "lcu/items/inventory.h"
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
    // Server-authoritative item storage (brief section 20 / Phase 15) -
    // 9 slots, matching VoxelClient's own `player_inventory`. Starts
    // empty; populated only by validated `BlockAction`s (see
    // handle_block_action below), never by the client directly.
    lcu::items::Inventory inventory{9};
    // Per-movement chunk streaming (Phase 16): the chunk column this
    // client's loaded-chunk range was last computed around - set at
    // connect time to the spawn column, re-checked every tick against
    // the client's current position.
    lcu::voxel::ChunkCoord last_streamed_center{};
};

}  // namespace

int main(int argc, char** argv) {
    const ServerConfig config = parse_args(argc, argv);

    LCU_LOG_INFO("VoxelServer starting: world=\"{}\" port={}", config.world, config.port);
    LCU_LOG_INFO("Headless dedicated server: no SDL, no bgfx, no GPU (see ARCHITECTURE.md)");

    // --- Real world simulation (replacing the Phase 0 sleep-only placeholder) ---
    // Registered in the exact same order as VoxelClient's own block
    // registration (stone, then grass, then dirt) - BlockId assignment
    // is sequential, and every wire message carrying a raw block_id
    // (BlockAction/BlockChange/ChunkData) relies on both sides agreeing
    // on what each id means, the same simplification already
    // documented for item ids (see DECISIONS.md "server-side inventory
    // (Phase 15)").
    lcu::voxel::BlockRegistry block_registry;
    lcu::voxel::BlockDefinition stone_def;
    stone_def.namespaced_id = "game:stone";
    stone_def.display_name = "Stone";
    stone_def.is_transparent = false;
    stone_def.has_collision = true;
    const lcu::voxel::BlockId stone_id = block_registry.register_block(stone_def);

    lcu::voxel::BlockDefinition grass_def;
    grass_def.namespaced_id = "game:grass";
    grass_def.display_name = "Grass";
    grass_def.is_transparent = false;
    grass_def.has_collision = true;
    const lcu::voxel::BlockId grass_id = block_registry.register_block(grass_def);

    lcu::voxel::BlockDefinition dirt_def;
    dirt_def.namespaced_id = "game:dirt";
    dirt_def.display_name = "Dirt";
    dirt_def.is_transparent = false;
    dirt_def.has_collision = true;
    const lcu::voxel::BlockId dirt_id = block_registry.register_block(dirt_def);

    // Mirrors VoxelClient's own registration exactly (brief section 20:
    // server-side inventory, Phase 15) - both sides independently
    // register the same one item in the same order, so their ItemIds
    // coincide by construction, the same simplification block/item ids
    // already carry for mod content (see DECISIONS.md "server-side
    // inventory (Phase 15)").
    lcu::items::ItemRegistry item_registry;
    lcu::items::ItemDefinition stone_item_def;
    stone_item_def.namespaced_id = "game:stone";
    stone_item_def.display_name = "Stone";
    stone_item_def.max_stack_size = 64;
    const lcu::items::ItemId stone_item_id = item_registry.register_item(stone_item_def);

    // Registered (in the same order as VoxelClient) purely to keep the
    // two sides' ItemId spaces aligned, same as game:stone's own
    // comment above - the server doesn't track either in a per-client
    // Inventory yet (Phase 17's grass/dirt item pickup is entirely
    // client-authoritative/optimistic for now, see DECISIONS.md), so
    // neither id is bound to a variable here.
    lcu::items::ItemDefinition grass_item_def;
    grass_item_def.namespaced_id = "game:grass";
    grass_item_def.display_name = "Grass";
    grass_item_def.max_stack_size = 64;
    item_registry.register_item(grass_item_def);

    lcu::items::ItemDefinition dirt_item_def;
    dirt_item_def.namespaced_id = "game:dirt";
    dirt_item_def.display_name = "Dirt";
    dirt_item_def.max_stack_size = 64;
    item_registry.register_item(dirt_item_def);

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
        lcu::world::worldgen::generate_terrain_chunk(chunk, coord, kWorldSeed, grass_id, dirt_id, stone_id);
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

    // Per-movement chunk streaming (Phase 16, see DECISIONS.md): which
    // chunk column a world-space position falls in, for deciding when a
    // client has moved far enough to need its loaded-chunk range
    // re-checked.
    const auto chunk_coord_of_position = [](const lcu::math::Vec3& pos) {
        const lcu::voxel::BlockWorldCoord block{static_cast<lcu::i64>(std::floor(pos.x)),
                                                 static_cast<lcu::i64>(std::floor(pos.y)),
                                                 static_cast<lcu::i64>(std::floor(pos.z))};
        return lcu::voxel::world_to_chunk_and_local(block, lcu::voxel::Chunk::kEdgeLength).chunk;
    };

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

    // Sends `client`'s current authoritative game:stone count - called
    // after every BlockAction that could have affected it, accepted or
    // rejected, so the client's own optimistic local guess (see
    // DECISIONS.md) gets corrected the instant it diverges from what
    // the server actually did.
    auto send_inventory_update = [&](ClientState& client) {
        client.connection.send(
            lcu::network::Channel::ReliableOrdered,
            protocol::encode_inventory_update({stone_item_id, client.inventory.count_item(stone_item_id)}));
    };

    // Server-authoritative block editing (brief section 8/19/20): validates
    // and applies a client's requested break/place against this server's
    // own World, then broadcasts the result to every connected client
    // (including the requester - no client mutates its own World
    // speculatively for a block edit, see DECISIONS.md). Rejects a
    // request whose target chunk isn't loaded, whose target block isn't
    // actually breakable/placeable given its current state, whose
    // requested block_id isn't a registered block, whose target is too
    // far from the requesting client's own known position, or (for a
    // game:stone place specifically) whose requester doesn't actually
    // hold one server-side - logged, no reply sent beyond the inventory
    // correction above (the requester's world simply doesn't change).
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
            send_inventory_update(client);
            return;
        }

        const auto split = lcu::voxel::world_to_chunk_and_local(target, lcu::voxel::Chunk::kEdgeLength);
        lcu::voxel::Chunk* chunk = world.chunk_at_mutable(split.chunk);
        if (chunk == nullptr) {
            LCU_LOG_WARN("Rejected BlockAction from {}: chunk ({},{},{}) isn't loaded", from.to_string(),
                         split.chunk.x, split.chunk.y, split.chunk.z);
            send_inventory_update(client);
            return;
        }

        const lcu::voxel::BlockId current = chunk->block_at(split.local.x, split.local.y, split.local.z);
        lcu::voxel::BlockId new_id = current;
        bool valid = false;
        if (action.action == protocol::BlockActionType::Break) {
            valid = current != lcu::voxel::kAirBlockId;
            new_id = lcu::voxel::kAirBlockId;
        } else {
            // Placing game:stone specifically requires the client to
            // actually hold one, server-side (brief section 20 applied
            // to item accounting, not just block edits - see
            // DECISIONS.md "server-side inventory (Phase 15)"). Any
            // other registered block_id (e.g. mod content) has no
            // item-backing infrastructure yet, so it isn't gated here.
            const bool has_required_item =
                action.block_id != stone_id || client.inventory.count_item(stone_item_id) > 0;
            valid = current == lcu::voxel::kAirBlockId && action.block_id < block_registry.count() &&
                    has_required_item;
            new_id = action.block_id;
        }
        if (!valid) {
            LCU_LOG_WARN("Rejected BlockAction from {}: target ({},{},{}) current_block={} requested_block={}",
                         from.to_string(), target.x, target.y, target.z, current, action.block_id);
            send_inventory_update(client);
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
        if (action.action == protocol::BlockActionType::Break && current == stone_id) {
            client.inventory.add_item(item_registry, {stone_item_id, 1});
        } else if (action.action == protocol::BlockActionType::Place && new_id == stone_id) {
            client.inventory.remove_item(stone_item_id, 1);
        }
        send_inventory_update(client);

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
                client.last_streamed_center = chunk_coord_of_position(client.player.aabb.center());
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

        // Per-movement chunk streaming (brief section 19/22, Phase 16 -
        // closes Phase 14's "connect-time-only" gap, see DECISIONS.md):
        // grow - never shrink, this `World` is shared across every
        // connected client, so unloading anything a *different* client
        // still needs would break that client; see DECISIONS.md
        // "server-side chunk streaming never unloads" - the server's
        // loaded-chunk set to follow any client that has crossed into a
        // new chunk column since last checked, then broadcast every
        // newly-loaded chunk to every connected client (not just the one
        // that triggered it - anyone already connected is equally
        // missing a chunk that didn't exist yet a moment ago).
        std::vector<lcu::voxel::ChunkCoord> newly_loaded_chunks;
        for (auto& [addr, client] : clients) {
            const lcu::voxel::ChunkCoord current_center = chunk_coord_of_position(client.player.aabb.center());
            if (current_center == client.last_streamed_center) {
                continue;
            }
            client.last_streamed_center = current_center;
            for (lcu::i32 cx = current_center.x - load_settings.radius_xz; cx <= current_center.x + load_settings.radius_xz;
                 ++cx) {
                for (lcu::i32 cz = current_center.z - load_settings.radius_xz;
                     cz <= current_center.z + load_settings.radius_xz; ++cz) {
                    for (lcu::i32 cy = load_settings.min_chunk_y; cy <= load_settings.max_chunk_y; ++cy) {
                        const lcu::voxel::ChunkCoord coord{cx, cy, cz};
                        if (world.state_of(coord) != lcu::world::ChunkLifecycleState::Unloaded) {
                            continue;
                        }
                        world.load_chunk(coord);
                        newly_loaded_chunks.push_back(coord);
                    }
                }
            }
        }
        if (!newly_loaded_chunks.empty()) {
            LCU_LOG_INFO("Streamed {} newly-loaded chunk(s) into range (total {} loaded)", newly_loaded_chunks.size(),
                         world.loaded_chunk_count());
            for (const lcu::voxel::ChunkCoord& coord : newly_loaded_chunks) {
                const lcu::voxel::Chunk* streamed_chunk = world.chunk_at(coord);
                if (streamed_chunk == nullptr) {
                    continue;
                }
                const protocol::ChunkData chunk_message{coord.x, coord.y, coord.z,
                                                          lcu::serialization::serialize_chunk_to_bytes(*streamed_chunk)};
                const auto encoded = protocol::encode_chunk_data(chunk_message);
                for (auto& [broadcast_addr, broadcast_client] : clients) {
                    const auto fragments = lcu::network::fragment_payload(
                        encoded, broadcast_client.next_chunk_message_id++, lcu::network::kMaxFragmentDataSize);
                    for (const auto& fragment : fragments) {
                        broadcast_client.connection.send(lcu::network::Channel::ReliableOrdered,
                                                          protocol::encode_chunk_data_fragment(fragment));
                    }
                }
            }
        }

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
