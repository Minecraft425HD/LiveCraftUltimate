// Same real isolation reasoning as stb_image_impl.cpp - this project's
// own test fixtures (tests/assets/skin_catalog_test.cpp) need to write
// a real PNG file to feed back into SkinCatalog::add_from_file(), and
// stb_image_write.h's own generated code trips warnings this project's
// -Wall/-Wextra/-Wpedantic/-Werror would otherwise reject, so its
// implementation lives in this one dedicated, unstrict-flags target
// (see third_party/CMakeLists.txt's own StbImageWriteImpl target).
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
