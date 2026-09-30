cmake_minimum_required(VERSION 3.20)
project(RecompiledGame LANGUAGES C CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.28.5
)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

if(APPLE)
    set(_sdl_gl_src "${sdl2_SOURCE_DIR}/src/video/cocoa/SDL_cocoaopengl.m")
    if(EXISTS "${_sdl_gl_src}")
        file(READ "${_sdl_gl_src}" _sdl_gl_txt)
        string(REPLACE "attr[i++] = NSOpenGLPFAAllowOfflineRenderers;"
                       "/* NWii: breaks pixel format on dual-GPU Macs */"
                       _sdl_gl_patched "${_sdl_gl_txt}")
        if(NOT _sdl_gl_txt STREQUAL _sdl_gl_patched)
            file(WRITE "${_sdl_gl_src}" "${_sdl_gl_patched}")
        endif()
    endif()
endif()

add_subdirectory(nWiiRuntime)

add_executable(RecompiledGame nWiiRuntime/src/core/main.cpp output.cpp)
target_include_directories(RecompiledGame PRIVATE nWiiRuntime/src nWiiRuntime/include)
target_link_libraries(RecompiledGame PRIVATE nwiiruntime)
