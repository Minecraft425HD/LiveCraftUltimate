// Real micro-benchmarks (Phase 11, brief section 96/98: measure before
// optimizing, don't guess). Each benchmark exercises the actual engine
// code path a real game frame/tick would run, not a synthetic stand-in -
// same worldgen, meshing, lighting, physics, serialization, network and
// entity-sim functions the client/server use. Numbers are specific to
// this sandbox's CPU (see BUILD_STATUS.md for the reproduce command and
// the actual measured results) - the point of this suite is to have real
// numbers to profile against once something needs optimizing, not to
// hit any particular target.

#include <cstdio>
#include <filesystem>
#include <random>
#include <string>

#include <benchmark/benchmark.h>

#include "game/components/ai_wander.h"
#include "game/components/position.h"
#include "game/systems/ai_wander_system.h"
#include "lcu/ecs/registry.h"
#include "lcu/lighting/light_storage.h"
#include "lcu/lighting/propagation.h"
#include "lcu/lighting/world_light.h"
#include "lcu/network/connection.h"
#include "lcu/physics/collision.h"
#include "lcu/physics/raycast.h"
#include "lcu/serialization/chunk_serializer.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"
#include "lcu/voxel/chunk_coord.h"
#include "lcu/voxel/greedy_mesher.h"
#include "lcu/world/world.h"
#include "lcu/world/worldgen.h"

namespace {

using lcu::voxel::BlockDefinition;
using lcu::voxel::BlockId;
using lcu::voxel::BlockRegistry;
using lcu::voxel::Chunk;
using lcu::voxel::ChunkCoord;
using lcu::world::World;

BlockRegistry make_registry_with_stone(BlockId& out_stone_id) {
    BlockRegistry registry;
    BlockDefinition stone;
    stone.namespaced_id = "bench:stone";
    stone.display_name = "Stone";
    stone.is_transparent = false;
    stone.has_collision = true;
    out_stone_id = registry.register_block(stone);
    return registry;
}

bool is_solid_predicate(BlockId id, const BlockRegistry& registry) { return registry.definition_of(id).has_collision; }

}  // namespace

// --- Voxel access (engine/voxel::ChunkStorage) ------------------------------

static void BM_ChunkStorage_SetBlock(benchmark::State& state) {
    Chunk chunk;
    for (auto _ : state) {
        for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
            for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
                for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
                    chunk.set_block(x, y, z, 1);
                }
            }
        }
    }
    state.SetItemsProcessed(state.iterations() * Chunk::kEdgeLength * Chunk::kEdgeLength * Chunk::kEdgeLength);
}
BENCHMARK(BM_ChunkStorage_SetBlock);

static void BM_ChunkStorage_BlockAt(benchmark::State& state) {
    Chunk chunk;
    chunk.set_block(8, 8, 8, 1);
    lcu::u64 sum = 0;
    for (auto _ : state) {
        for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
            for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
                for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
                    sum += chunk.block_at(x, y, z);
                }
            }
        }
    }
    benchmark::DoNotOptimize(sum);
    state.SetItemsProcessed(state.iterations() * Chunk::kEdgeLength * Chunk::kEdgeLength * Chunk::kEdgeLength);
}
BENCHMARK(BM_ChunkStorage_BlockAt);

// --- Chunk generation (engine/world::worldgen) ------------------------------

static void BM_Worldgen_GenerateTerrainChunk(benchmark::State& state) {
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    Chunk chunk;
    for (auto _ : state) {
        lcu::world::worldgen::generate_terrain_chunk(chunk, ChunkCoord{0, 0, 0}, 1337, stone_id, stone_id, stone_id);
    }
}
BENCHMARK(BM_Worldgen_GenerateTerrainChunk);

// --- Meshing (engine/voxel::mesh_chunk_greedy) ------------------------------

static void BM_GreedyMesher_SolidChunk(benchmark::State& state) {
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    Chunk chunk;  // fully solid - worst case for face count before merging
    for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
        for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
            for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
                chunk.set_block(x, y, z, stone_id);
            }
        }
    }
    for (auto _ : state) {
        auto mesh = lcu::voxel::mesh_chunk_greedy(chunk, registry);
        benchmark::DoNotOptimize(mesh);
    }
}
BENCHMARK(BM_GreedyMesher_SolidChunk);

static void BM_GreedyMesher_CheckerboardChunk(benchmark::State& state) {
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    Chunk chunk;  // no two neighbors share a block type - no merging possible
    for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
        for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
            for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
                if ((x + y + z) % 2 == 0) {
                    chunk.set_block(x, y, z, stone_id);
                }
            }
        }
    }
    for (auto _ : state) {
        auto mesh = lcu::voxel::mesh_chunk_greedy(chunk, registry);
        benchmark::DoNotOptimize(mesh);
    }
}
BENCHMARK(BM_GreedyMesher_CheckerboardChunk);

// --- Lighting (engine/lighting) ---------------------------------------------

static void BM_Lighting_ComputeBlockLight(benchmark::State& state) {
    BlockRegistry registry;
    BlockDefinition torch;
    torch.namespaced_id = "bench:torch";
    torch.display_name = "Torch";
    torch.is_transparent = true;
    torch.has_collision = false;
    torch.light_emission = 15;
    const BlockId torch_id = registry.register_block(torch);

    Chunk chunk;
    chunk.set_block(8, 8, 8, torch_id);
    lcu::lighting::Light light;
    for (auto _ : state) {
        lcu::lighting::compute_block_light(chunk, registry, light);
    }
}
BENCHMARK(BM_Lighting_ComputeBlockLight);

static void BM_Lighting_ComputeSkyLight(benchmark::State& state) {
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    Chunk chunk;
    for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
        for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
            chunk.set_block(x, 0, z, stone_id);  // floor only, rest open to sky
        }
    }
    lcu::lighting::Light light;
    for (auto _ : state) {
        lcu::lighting::compute_sky_light(chunk, registry, light);
    }
}
BENCHMARK(BM_Lighting_ComputeSkyLight);

// The three benchmarks below (Phase 34) are the brief's own explicit
// perf budgets, unlike the rest of this file's "numbers to profile
// against, not a target" numbers: BM_Lighting_ComputeChunkWithNeighbors
// < 2ms, BM_Lighting_PlaceTorchAtChunkEdge / BM_Lighting_
// UnplaceTorchAtChunkEdge < 0.5ms each - real, measured, and confirmed
// under budget on this sandbox's CPU before this phase's commit (see
// BUILD_STATUS.md for the actual numbers and the reproduce command);
// "optimize before continuing" if a future change pushes any of them
// over.

static void BM_Lighting_ComputeChunkWithNeighbors(benchmark::State& state) {
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    // A partial roof in the chunk above (not fully open, not fully
    // solid) so the chunk below's own sky light genuinely depends on
    // real cross-chunk seeding (Phase 30) - a representative case, not
    // the trivial "nothing above" one.
    World world(1, [&](Chunk& chunk, ChunkCoord coord) {
        if (coord.y == 1) {
            for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
                for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
                    if ((x + z) % 3 != 0) {
                        chunk.set_block(x, 0, z, stone_id);
                    }
                }
            }
        }
    });
    world.load_chunk({0, 1, 0});
    world.load_chunk({0, 0, 0});
    const Chunk* above = world.chunk_at({0, 1, 0});
    const Chunk* below = world.chunk_at({0, 0, 0});

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    // The neighbor above is loaded and lit once, before timing starts -
    // this measures the real per-chunk-load cost given an already-lit
    // neighbor (client/main.cpp's actual load-loop shape - see
    // compute_initial_sky_light's top-down ordering requirement), not
    // the neighbor's own light cost too.
    lcu::lighting::compute_block_light(*above, registry, world_light.chunk_light({0, 1, 0}));
    lcu::lighting::compute_sky_light_cross_chunk(*above, registry, world_light, {0, 1, 0});

    for (auto _ : state) {
        lcu::lighting::compute_block_light(*below, registry, world_light.chunk_light({0, 0, 0}));
        lcu::lighting::compute_sky_light_cross_chunk(*below, registry, world_light, {0, 0, 0});
    }
}
BENCHMARK(BM_Lighting_ComputeChunkWithNeighbors);

static void BM_Lighting_PlaceTorchAtChunkEdge(benchmark::State& state) {
    BlockRegistry registry;
    BlockDefinition torch;
    torch.namespaced_id = "bench:torch";
    torch.display_name = "Torch";
    torch.is_transparent = false;
    torch.light_emission = 14;
    const BlockId torch_id = registry.register_block(torch);

    // Worst-case reach for a single placement: right at the +X chunk
    // boundary, with the neighbor chunk actually loaded so the BFS has
    // somewhere real to spread into (see engine/lighting/propagation.h
    // "flood_block_light_cross_chunk").
    World world(1, [](Chunk&, ChunkCoord) {});  // both chunks all-air
    world.load_chunk({0, 0, 0});
    world.load_chunk({1, 0, 0});
    Chunk* origin = world.chunk_at_mutable({0, 0, 0});
    origin->set_block(Chunk::kEdgeLength - 1, 8, 8, torch_id);

    for (auto _ : state) {
        // A fresh WorldLight per iteration - constructing an empty
        // 2-entry unordered_map is negligible next to the BFS itself,
        // and guarantees no residual light from a prior iteration
        // silently makes a later iteration cheaper (propagate only
        // writes when the new level is strictly brighter than what's
        // already there).
        lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
        world_light.chunk_light({0, 0, 0}).set_block_light(Chunk::kEdgeLength - 1, 8, 8, 14);
        lcu::lighting::propagate_added_block_light_cross_chunk(world, registry, world_light, {0, 0, 0},
                                                                 Chunk::kEdgeLength - 1, 8, 8);
        benchmark::DoNotOptimize(world_light);
    }
}
BENCHMARK(BM_Lighting_PlaceTorchAtChunkEdge);

static void BM_Lighting_UnplaceTorchAtChunkEdge(benchmark::State& state) {
    BlockRegistry registry;
    BlockDefinition torch;
    torch.namespaced_id = "bench:torch";
    torch.display_name = "Torch";
    torch.is_transparent = false;
    torch.light_emission = 14;
    const BlockId torch_id = registry.register_block(torch);

    World world(1, [](Chunk&, ChunkCoord) {});
    world.load_chunk({0, 0, 0});
    world.load_chunk({1, 0, 0});
    Chunk* origin = world.chunk_at_mutable({0, 0, 0});
    origin->set_block(Chunk::kEdgeLength - 1, 8, 8, torch_id);

    for (auto _ : state) {
        // Priming (the place half) is excluded from the timed region -
        // this benchmark measures unpropagate_block_light_cross_chunk
        // alone, not place+unplace combined.
        state.PauseTiming();
        lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
        world_light.chunk_light({0, 0, 0}).set_block_light(Chunk::kEdgeLength - 1, 8, 8, 14);
        lcu::lighting::propagate_added_block_light_cross_chunk(world, registry, world_light, {0, 0, 0},
                                                                 Chunk::kEdgeLength - 1, 8, 8);
        state.ResumeTiming();

        lcu::lighting::unpropagate_block_light_cross_chunk(world, registry, world_light, {0, 0, 0},
                                                            Chunk::kEdgeLength - 1, 8, 8, 14);
        benchmark::DoNotOptimize(world_light);
    }
}
BENCHMARK(BM_Lighting_UnplaceTorchAtChunkEdge);

// --- Physics (engine/physics) -----------------------------------------------

static void BM_Physics_Raycast(benchmark::State& state) {
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    World world(1, [&](Chunk& chunk, ChunkCoord) { chunk.set_block(8, 8, 8, stone_id); });
    world.load_chunk({0, 0, 0});
    const auto is_solid = [&](BlockId id) { return is_solid_predicate(id, registry); };

    for (auto _ : state) {
        auto hit = lcu::physics::raycast(world, {0.5f, 8.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, 20.0f, is_solid);
        benchmark::DoNotOptimize(hit);
    }
}
BENCHMARK(BM_Physics_Raycast);

static void BM_Physics_MoveAndCollide(benchmark::State& state) {
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    World world(1, [&](Chunk& chunk, ChunkCoord) {
        for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
            for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
                chunk.set_block(x, 0, z, stone_id);
            }
        }
    });
    world.load_chunk({0, 0, 0});
    const auto is_solid = [&](BlockId id) { return is_solid_predicate(id, registry); };
    const lcu::physics::AABB aabb{{4.0f, 1.0f, 4.0f}, {4.6f, 2.8f, 4.6f}};

    for (auto _ : state) {
        auto result = lcu::physics::move_and_collide(world, aabb, {0.1f, -0.1f, 0.05f}, is_solid);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Physics_MoveAndCollide);

// --- Serialization + compression (engine/serialization, zstd inside) -------

std::string bench_temp_path(const std::string& name) {
    return (std::filesystem::temp_directory_path() / ("lcu_benchmark_" + name)).string();
}

static void BM_Serialization_SaveChunk(benchmark::State& state) {
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    Chunk chunk;
    lcu::world::worldgen::generate_terrain_chunk(chunk, ChunkCoord{0, 0, 0}, 1337, stone_id, stone_id, stone_id);
    const std::string path = bench_temp_path("save_chunk.chunk");

    for (auto _ : state) {
        bool ok = lcu::serialization::save_chunk_to_file(chunk, path);
        benchmark::DoNotOptimize(ok);
    }
    std::remove(path.c_str());
}
BENCHMARK(BM_Serialization_SaveChunk);

static void BM_Serialization_LoadChunk(benchmark::State& state) {
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    Chunk chunk;
    lcu::world::worldgen::generate_terrain_chunk(chunk, ChunkCoord{0, 0, 0}, 1337, stone_id, stone_id, stone_id);
    const std::string path = bench_temp_path("load_chunk.chunk");
    lcu::serialization::save_chunk_to_file(chunk, path);

    Chunk loaded;
    for (auto _ : state) {
        auto result = lcu::serialization::load_chunk_from_file(path, loaded);
        benchmark::DoNotOptimize(result);
    }
    std::remove(path.c_str());
}
BENCHMARK(BM_Serialization_LoadChunk);

// --- Network (engine/network::Connection) -----------------------------------

static void BM_Network_ReliableOrderedRoundTrip(benchmark::State& state) {
    using lcu::network::Channel;
    using lcu::network::Connection;

    const std::vector<lcu::u8> payload(64, 0x42);  // representative small game message
    for (auto _ : state) {
        Connection sender;
        Connection receiver;
        sender.send(Channel::ReliableOrdered, payload);
        for (const auto& packet : sender.take_outgoing_packets()) {
            auto received = receiver.on_packet_received(packet);
            benchmark::DoNotOptimize(received);
        }
    }
}
BENCHMARK(BM_Network_ReliableOrderedRoundTrip);

// --- Entity simulation (game::systems::update_ai_wander) --------------------

static void BM_EntitySim_UpdateAiWander(benchmark::State& state) {
    using game::components::AIWander;
    using game::components::Position;
    using lcu::ecs::Registry;

    const auto entity_count = static_cast<lcu::usize>(state.range(0));
    Registry registry;
    std::mt19937 setup_rng(42);
    std::uniform_real_distribution<lcu::f32> coord(-50.0f, 50.0f);
    for (lcu::usize i = 0; i < entity_count; ++i) {
        const auto entity = registry.create_entity();
        registry.add_component<Position>(entity, Position{{coord(setup_rng), 0.0f, coord(setup_rng)}});
        registry.add_component<AIWander>(entity, AIWander{{coord(setup_rng), 0.0f, coord(setup_rng)}, 1.5f, 0.0f});
    }

    std::mt19937 rng(1234);
    const game::systems::AIWanderConfig config;
    for (auto _ : state) {
        game::systems::update_ai_wander(registry, config, rng, 0.05f);
    }
    state.SetItemsProcessed(state.iterations() * entity_count);
}
BENCHMARK(BM_EntitySim_UpdateAiWander)->Arg(10)->Arg(100)->Arg(1000);

BENCHMARK_MAIN();
