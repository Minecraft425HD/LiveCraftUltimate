#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/voxel/chunk.h"
#include "lcu/voxel/chunk_coord.h"
#include "lcu/world/chunk_lifecycle.h"

namespace lcu::world {

// Fills `chunk`'s blocks for the given coordinate. World doesn't know
// how to generate terrain itself (see lcu::world::worldgen) - it's
// handed a generator function so it stays decoupled from any specific
// worldgen algorithm; unit tests can pass a trivial generator without
// depending on the real terrain code.
using ChunkGenerator = std::function<void(voxel::Chunk&, voxel::ChunkCoord)>;

// Owns a sparse set of loaded chunks, keyed by ChunkCoord - only chunks
// actually requested/generated are held here, never the whole world
// (brief section 22: "nie die komplette Welt in den RAM laden"). Drives
// each chunk through Unloaded -> Requested -> Generating -> Generated
// synchronously today. Dispatching generation through
// engine/jobs::JobSystem so it doesn't block the caller is wiring a
// client adds once it actually streams multiple chunks per frame - not
// needed yet with a single hardcoded placeholder chunk (see
// PROJECT_STATE.md); World's job here is the state machine and storage,
// not scheduling.
//
// Known simplification: update_streaming loads/unloads a full 3D cube
// by Chebyshev distance, not the horizontal-disc-plus-bounded-vertical-
// column shape most voxel games actually want. There's no camera/player
// yet to define "horizontal" meaningfully against (see DECISIONS.md) -
// revisit once Phase 4 gives this a real caller.
class World : public NonCopyable {
   public:
    World(u32 seed, ChunkGenerator generator);

    u32 seed() const { return seed_; }

    ChunkLifecycleState state_of(voxel::ChunkCoord coord) const;

    // Ensures an entry exists for `coord` (Unloaded -> Requested); no-op
    // if a chunk is already loaded (or requested) there.
    void request_chunk(voxel::ChunkCoord coord);

    // Runs the generator for a Requested chunk, transitioning it to
    // Generated. Asserts if `coord` isn't currently Requested - callers
    // drive the state machine in order, not out of it.
    void generate_chunk(voxel::ChunkCoord coord);

    // Convenience: request_chunk then generate_chunk, but only if the
    // chunk isn't already loaded (idempotent - safe to call every frame
    // for the same coordinate without regenerating it).
    void load_chunk(voxel::ChunkCoord coord);

    // Adopts `chunk` as already-Generated content for `coord`, without
    // ever calling `generator_` (Phase 71, brief section 71.3's own
    // "Chunk-Generierung ueber JobSystem verteilen") - the real
    // counterpart to load_chunk for a caller that already produced the
    // chunk's content itself (typically off the main thread, since
    // `generator_` isn't required to be thread-safe and this class
    // itself has no internal locking - see the class's own doc comment
    // on chunks_ being a plain, unsynchronized unordered_map). No-op if
    // `coord` is already loaded (idempotent, same as load_chunk) so a
    // caller racing against e.g. a concurrent stream_chunks_around call
    // for the same coordinate can't clobber it with stale data.
    void adopt_generated_chunk(voxel::ChunkCoord coord, voxel::Chunk chunk);

    // Returns nullptr if no chunk is loaded (state < Generated) at
    // `coord`.
    const voxel::Chunk* chunk_at(voxel::ChunkCoord coord) const;
    voxel::Chunk* chunk_at_mutable(voxel::ChunkCoord coord);

    // Drops a loaded chunk back to Unloaded. "Unloaded" isn't a table
    // entry with that state - it's the absence of an entry at all.
    void unload_chunk(voxel::ChunkCoord coord);

    // Streaming (brief section 22): loads every unloaded chunk within
    // `load_radius` (Chebyshev distance, in chunks) of `center`, and
    // unloads every currently-loaded chunk farther than `unload_radius`.
    // `unload_radius` must be >= `load_radius`; the gap between them is
    // hysteresis, so a chunk just outside load_radius stays loaded
    // instead of being immediately reloaded on the next call (avoids
    // thrashing at the boundary as `center` moves back and forth by one
    // chunk). Priority ordering within the load set (view direction,
    // movement direction - brief section 22) isn't implemented yet;
    // distance is the only signal so far.
    void update_streaming(voxel::ChunkCoord center, u32 load_radius, u32 unload_radius);

    usize loaded_chunk_count() const { return chunks_.size(); }

    // Every currently-loaded (state >= Generated) chunk's coordinate, in
    // unspecified order. Real consumer: VoxelServer enumerates this to
    // send a newly-connecting client a full chunk snapshot (see
    // NETWORKING.md "Chunk network streaming") - not needed before that,
    // since nothing else needs to walk every loaded chunk at once.
    std::vector<voxel::ChunkCoord> loaded_chunk_coords() const;

   private:
    struct ChunkEntry {
        voxel::Chunk chunk;
        ChunkLifecycleState state = ChunkLifecycleState::Requested;
    };

    u32 seed_;
    ChunkGenerator generator_;
    std::unordered_map<voxel::ChunkCoord, ChunkEntry> chunks_;
};

}  // namespace lcu::world
