#pragma once

#include "lcu/core/types.h"

namespace game::components {

// Real, immutable-after-spawn NPC skin choice (Phase 62, brief section
// 59's own "jeder NPC behaelt seinen einmal beim Spawn zugewiesenen
// Skin"): `skin_preset_index` is fixed once at spawn (client/main.cpp
// cycles through lcu::assets::SkinPreset's own builtin values) and
// never revisited afterward - not even if the player later changes
// their own skin or uploads a new one, matching the brief's own
// wording literally. A plain index rather than a `SkinPreset` value
// directly to keep this header free of an engine/assets dependency
// (the same "gameplay layer stays generic" separation ai_wander.h's
// own doc comment already describes).
struct NpcAppearance {
    lcu::u32 skin_preset_index = 0;
};

}  // namespace game::components
