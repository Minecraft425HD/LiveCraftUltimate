// Real, single, dedicated translation unit for stb_image's own
// implementation (Phase 62, engine/assets::SkinCatalog's real PNG
// skin-file decode) - kept as its own target that does NOT get this
// project's own -Wall/-Wextra/-Wpedantic/-Werror (see
// third_party/CMakeLists.txt's own StbImageImpl target) since
// stb_image.h's generated code trips several of those warnings that
// have nothing to do with this project's own code quality. Isolating
// the implementation here keeps the real zero-warning discipline
// everywhere else intact without disabling those warnings project-wide
// or silencing them with per-file pragmas around third-party source.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
