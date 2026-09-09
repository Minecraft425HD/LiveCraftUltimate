# Architecture

## Layering

```
GAME (game/)
  |
VOXEL ENGINE (engine/)
  |
RENDERING ABSTRACTION (engine/rendering)
  |
BGFX
  |
PLATFORM GRAPHICS BACKEND (D3D12/Vulkan/Metal/OpenGL ES, chosen by bgfx per-platform)
```

The concrete graphics backend is never visible above `engine/rendering`.
No `#ifdef _WIN32` / `#ifdef __APPLE__` / `#ifdef __ANDROID__` is permitted
in `game/`, `client/` (outside `client/input` platform glue) or `server/`.
Platform-specific code lives in `engine/platform`.

## Directory map

- `engine/core` — fundamental types, result/error handling, logging, assertions.
- `engine/memory` — arenas, pools, handle-based allocation.
- `engine/math` — vector/matrix/quaternion primitives (no external math dependency).
- `engine/platform` — SDL3-backed window, input device, timer, filesystem abstraction.
- `engine/jobs` — job system (worker pool, priorities, dependencies, cancellation).
- `engine/ecs` — entity/component storage.
- `engine/rendering` — render abstraction layer on top of bgfx.
- `engine/voxel` — chunk storage, block state encoding, meshing.
- `engine/world` — world streaming, chunk lifecycle, coordinate spaces.
- `engine/physics` — AABB/voxel collision, raycasting (voxel DDA).
- `engine/audio` — audio abstraction (SDL3 audio backend).
- `engine/network` — transport layer (reliable/unreliable channels).
- `engine/serialization` — save format, versioned (de)serialization.
- `engine/assets` — asset request/load/cache/release pipeline.
- `engine/scripting` — Lua VM host and sandboxing.
- `engine/modding` — registries, mod loader, mod manifest parsing.
- `engine/ui` — UI primitives usable by desktop, gamepad, touch.
- `engine/debug` — debug overlay, profiling hooks.
- `game/` — data-driven gameplay content: blocks, items, entities, recipes,
  biomes, worldgen, structures, ECS systems/components built on the engine.
- `client/` — SDL3 window, input mapping, client-side prediction/interpolation,
  in-game UI. Depends on `engine` (including rendering/bgfx) and `game`.
- `server/` — dedicated server: network, simulation, world persistence,
  administration. **Never** depends on SDL window, bgfx, or any GPU API.
- `tools/` — offline tools (mod scaffolding, world tools, asset pipeline,
  benchmarks, server tools, debug tools).
- `third_party/` — reproducible fetch + build of external dependencies.
- `mods/` — example/data mods.
- `examples/server` — example dedicated server configuration and mod.
- `tests/` — unit and integration tests (GoogleTest).

## Build targets

- `VoxelEngine` — static library, `engine/` only. No SDL/bgfx symbols leak
  into its public headers outside `engine/platform` and `engine/rendering`.
- `VoxelGame` — static library, `game/`. Depends on `VoxelEngine`.
- `VoxelClient` — executable. Depends on `VoxelEngine` (full, incl.
  rendering+platform), `VoxelGame`. Links SDL3 + bgfx.
- `VoxelServer` — executable. Depends on `VoxelEngine` built with
  `LCU_HEADLESS=ON` (no platform/rendering), `VoxelGame`. No SDL3, no bgfx.
- `VoxelTools` — executables under `tools/`.
- `VoxelTests` — GoogleTest binary, `tests/`.

## Core principle: data -> system -> job -> result

Engine systems operate on contiguous data via jobs producing results, not
through virtual dispatch in hot loops. Data-oriented design is preferred;
`shared_ptr`, global singletons, and unnecessary heap churn are avoided in
hot paths (chunk generation, meshing, physics, networking serialization).

## Chunk lifecycle

```
UNLOADED -> REQUESTED -> GENERATING -> GENERATED -> LIGHTING -> MESHING
  -> GPU UPLOAD -> READY -> VISIBLE
VISIBLE -> UNLOAD -> GPU RELEASE -> RAM RELEASE -> DISK CACHE (optional)
```

## Coordinate spaces

To avoid floating point precision loss in large worlds, positions are split
into `ChunkCoord` (integer) + local offset within the chunk. World-space
float positions are only reconstructed relative to a nearby chunk origin,
never accumulated as a single unbounded double across the whole world.

## Networking channels

`RELIABLE_ORDERED`, `RELIABLE_UNORDERED`, `UNRELIABLE`,
`UNRELIABLE_SEQUENCED`. See `NETWORKING.md` (to be written in Phase 7) for
wire format details once the transport is implemented.

This file is updated whenever a structural decision changes the above; see
`DECISIONS.md` for the reasoning behind each choice.
