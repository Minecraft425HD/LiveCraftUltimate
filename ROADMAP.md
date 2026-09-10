# Roadmap

Phases as defined by the project brief. A phase is not "done" until its
code builds, its tests pass, and it is documented — see `DONE` bar in
`PROJECT_STATE.md`.

- **Phase 0** — Repository + build system + persistent state docs.
- **Phase 1** — SDL3 + bgfx + window + game loop + input.
- **Phase 2** — Voxel storage + chunk + meshing + rendering.
- **Phase 3** — World generation + streaming + save.
- **Phase 4** — Player + physics + interaction.
- **Phase 5** — Items + inventory + crafting.
- **Phase 6** — Entities + AI + lighting + day/night.
- **Phase 7** — Networking + dedicated server.
- **Phase 8** — Replication + prediction + interpolation.
- **Phase 9** — Modding + registries + Lua + events.
- **Phase 10** — Mobile + touch + Android + iOS.
- **Phase 11** — Optimization + profiling.
- **Phase 12** — UI + audio + content + polish.

## Vertical slice targets (brief section 80)

Slice 1 (single player, local):
`window -> renderer -> voxel chunk -> world -> player -> camera -> raycast
-> break block -> place block -> save -> load`

Slice 2 (multiplayer):
`server -> client -> second player -> player sync -> block sync`

Slice 3 (modding):
`mod -> custom block -> custom item -> script -> custom recipe`

## Platform rollout order

Linux (primary dev target, this sandbox) -> Windows -> macOS -> Android
-> iOS/iPadOS. Desktop-and-server-first, mobile once the input
abstraction and quality-profile system exist (Phase 10 per brief, not
earlier — building mobile projects against a moving engine API wastes
work).

See `PROJECT_STATE.md` for what phase we are actually in right now, and
`TASK_QUEUE.md` for the current task breakdown.
