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
- First-person movement, AABB collision, block break/place, simple
  wandering AI, and a day/night cycle.
- A real inventory/crafting system: physically-simulated dropped item
  entities, a real drag/drop inventory screen and a crafting table with
  its own 3x3 grid, a rebindable-input pause/options/controls menu, and
  a real HUD (hotbar, health/hunger bars, crosshair, block-break
  progress).
- Real player vitals: fall damage, natural regen, starvation, hunger
  drain, eating, and death/respawn.
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

## Controls

Every binding below is the real default from `KeyBindings::
reset_to_defaults()`, and every one is rebindable in-game via Esc →
Steuerung (Controls) — select a row and press Enter/click to capture a
new key, Reset restores these defaults.

| Action | Default | Notes |
|---|---|---|
| Move | `W` `A` `S` `D` | |
| Jump | `Space` | Falling more than 3 blocks deals real fall damage on landing. |
| Sprint | `Left Ctrl` | Doubles hunger drain while held and moving; doesn't itself move you faster yet (see `PROJECT_STATE.md` Known Limitations). |
| Crouch | `Left Shift` | Shift-click modifier in the inventory/crafting-table screens only (moves a whole stack at once) — no movement/sneak effect. |
| Look | Mouse (captured) / `↑` `↓` `←` `→` | Click into the window to capture the mouse for real mouse-look; the arrow keys always work as a fallback and can be used together with the mouse. |
| Break / Attack | `Left Mouse` (hold) | Real per-block hold-to-break timing (hardness-based). |
| Place / Use / Eat | `Right Mouse` | Places the block in your selected hotbar slot, opens a crafting table you're looking at, or eats a held food item — whichever applies. |
| Pick Block | `Middle Mouse` | Selects the hotbar slot already holding the block you're looking at (never grants a new item). |
| Open Inventory | `E` | Doesn't pause the world — only your own movement/mining/placing/eating lock while it's open. |
| Quick-craft | `C` | Auto-assembles one of each distinct held item into a query grid; use the inventory/crafting-table screen's 2x2/3x3 grid for anything needing more than one of an ingredient. |
| Select hotbar slot 1-9 | `1`-`9` | |
| Cycle hotbar | `R` / mouse wheel down | Mouse wheel up is the reverse (`CycleHotbarPrev`, no keyboard default). |
| Swap offhand | `F` | Bound but not wired to anything yet — no offhand slot exists. |
| Pause / back | `Esc` or `Tab` | Opens the pause menu; also closes the inventory/crafting-table screen or cancels a rebind capture. |
| Confirm (menus) | `Enter` | |
| Toggle HUD | `F1` | |
| Screenshot | `F2` | Writes a PNG next to the executable (bgfx builds only). |
| Toggle debug overlay | `F3` | |
| Toggle perspective | `F5` | First-person/third-person camera only — doesn't change hit-detection. |
| Fullscreen | `F11` | |

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
