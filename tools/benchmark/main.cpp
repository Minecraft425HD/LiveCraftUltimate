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
        lcu::world::worldgen::generate_terrain_chunk(chunk, ChunkCoord{0, 0, 0}, 1337, stone_id);
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
    lcu::world::worldgen::generate_terrain_chunk(chunk, ChunkCoord{0, 0, 0}, 1337, stone_id);
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
    lcu::world::worldgen::generate_terrain_chunk(chunk, ChunkCoord{0, 0, 0}, 1337, stone_id);
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
