# LiveCraftUltimate

A cross-platform (Windows/Linux/macOS/Android/iOS) voxel sandbox game
engine and game, in C++20, built around one rendering abstraction
(bgfx) so no platform-specific graphics code leaks above
`engine/rendering`. Client/server are architecturally separate from
the start: `VoxelServer` never links SDL or any GPU API, and every
gameplay-affecting decision (block edits, movement, crafting) is
server-authoritative.

## Status

This repository has been developed and verified almost entirely inside
a headless Linux sandbox with **no GPU and no display**. Every claim in
this project's docs is backed by something that was actually run here
— a real build, a real `ctest` pass, a real headless client/server run
— never assumed. Where that's not possible (does a window actually
appear? does chunk lighting look right on a real GPU?), the docs say so
explicitly: **NOT VERIFIED — ENVIRONMENT LIMITATION**, rather than
silently claiming more than was checked. See `PROJECT_STATE.md`
("Reality Audit") for the full, current picture, and `BUILD_STATUS.md`
for a target-by-target verified/unverified table.

As of the most recent phase, the game has:

- Chunked voxel world storage, greedy meshing, and a bgfx-backed
  renderer with real per-voxel lighting (sky light + block light,
  propagated across chunk boundaries) and smooth per-vertex shading.
- A deterministic, seed-based worldgen pipeline: continental/mountain
  terrain shape, sea level with real water, three climate biomes
  (Snowy/Plains/Desert), cave carving, two ore types (coal/iron), and
  single-column tree/cactus vegetation.
- First-person movement, AABB collision, block break/place, an
  inventory/item/crafting system, simple wandering AI, and a day/night
  cycle.
- A real client/server network protocol (reliable + unreliable
  channels over UDP) with server-authoritative block edits, chunk
  streaming, interest-scoped unloading, and client-side
  prediction/interpolation.
- Lua-based modding: a real mod loader, an event bus, and
  block/item-registry bindings mods can call into (see
  `mods/example_mod`).
- Mobile touch input and quality-tier chunk-loading profiles, plus a
  benchmark suite (`tools/benchmark`) against real engine hot paths.

See `CHANGELOG.md` for the full, phase-by-phase history of how this was
built, and `DECISIONS.md` for the reasoning behind the non-obvious
choices along the way.

## Quick start

```sh
cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Development
cmake --build build/dev -j$(nproc)
ctest --test-dir build/dev --output-on-failure

# Run headlessly (no display/GPU required):
SDL_VIDEODRIVER=dummy LCU_MAX_FRAMES=5 ./build/dev/bin/VoxelClient
LCU_MAX_TICKS=5 ./build/dev/bin/VoxelServer --world MyWorld --port 25565
```

See `BUILDING.md` for prerequisites, build options, shader compilation,
ThreadSanitizer testing, and a macOS-specific build/run guide.

## Documentation map

| File | What it covers |
|---|---|
| `PROJECT_STATE.md` | Current phase, an honest "Reality Audit" of what's actually verified, known bugs/limitations. Read this first. |
| `BUILDING.md` | Prerequisites, build options, headless running, macOS build guide. |
| `BUILD_STATUS.md` | Target-by-target tested/untested matrix with real verification evidence. |
| `ARCHITECTURE.md` | Layering, directory map, build targets. |
| `NETWORKING.md` | Client/server protocol, channels, authority model. |
| `CHANGELOG.md` | Newest-first, per-phase change history. |
| `DECISIONS.md` | Chronological record of non-obvious technical decisions and their alternatives. |
| `ROADMAP.md` | Longer-term direction. |
| `TASK_QUEUE.md` | Per-phase task checklists. |

## License

No license file is present in this repository yet.
