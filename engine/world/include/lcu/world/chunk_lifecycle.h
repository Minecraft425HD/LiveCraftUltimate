#pragma once

#include "lcu/core/types.h"

namespace lcu::world {

// Matches the chunk lifecycle in ARCHITECTURE.md:
//   Unloaded -> Requested -> Generating -> Generated -> Lighting ->
//   Meshing -> GpuUpload -> Ready -> Visible
// and back down through Unloaded on unload. Not every state has a real
// system driving it yet (Lighting: Phase 6, GpuUpload/Ready/Visible:
// tracked by the client's render-side bookkeeping once more than one
// chunk exists to stream, not by World itself) - World only drives it as
// far as Generated today (see world.h). Intermediate states are declared
// now so callers that build on World don't invent a competing enum.
enum class ChunkLifecycleState : u8 {
    Unloaded,
    Requested,
    Generating,
    Generated,
    Lighting,
    Meshing,
    GpuUpload,
    Ready,
    Visible,
};

}  // namespace lcu::world
