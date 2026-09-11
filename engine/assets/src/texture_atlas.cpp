#include "lcu/assets/texture_atlas.h"

#include "lcu/core/assert.h"

namespace lcu::assets {

TileUvRange tile_uv_range(u32 tile_index) {
    LCU_ASSERT(tile_index < kMaxTiles);

    const u32 tx = tile_index % kTilesPerRow;
    const u32 ty = tile_index / kTilesPerRow;

    const f32 origin_u = static_cast<f32>(tx * kTileSize);
    const f32 origin_v = static_cast<f32>(ty * kTileSize);
    const f32 atlas_size = static_cast<f32>(kAtlasSize);
    const f32 tile_size = static_cast<f32>(kTileSize);

    TileUvRange range;
    range.u0 = (origin_u + kTileInsetTexels) / atlas_size;
    range.v0 = (origin_v + kTileInsetTexels) / atlas_size;
    range.u1 = (origin_u + tile_size - kTileInsetTexels) / atlas_size;
    range.v1 = (origin_v + tile_size - kTileInsetTexels) / atlas_size;
    return range;
}

}  // namespace lcu::assets
