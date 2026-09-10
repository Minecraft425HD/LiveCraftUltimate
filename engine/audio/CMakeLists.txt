# Only built alongside the client - a headless VoxelServer has no
# speakers, mirrors engine/platform's LCU_BUILD_CLIENT gating (see
# ARCHITECTURE.md "Server has zero GPU/window dependency" - audio is the
# same story as GPU/window: client-only hardware).
if(LCU_BUILD_CLIENT)
    add_library(LcuAudio STATIC
        src/waveform.cpp
        src/positional.cpp
        src/audio_engine.cpp
    )
    add_library(Lcu::Audio ALIAS LcuAudio)

    target_include_directories(LcuAudio PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    # PRIVATE: only audio_engine.cpp includes <SDL3/SDL_audio.h> - see its
    # doc comment. waveform.cpp/positional.cpp are pure math/DSP with zero
    # SDL dependency (fully unit-testable without a real audio device).
    target_link_libraries(LcuAudio
        PUBLIC Lcu::Core Lcu::Math
        PRIVATE SDL3::SDL3
    )

    lcu_apply_common_options(LcuAudio)
endif()
