cmake_minimum_required (VERSION 3.23)

project (aZeroEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(PROJECT_DIRECTORY "${CMAKE_SOURCE_DIR}" CACHE STRING "Project directory path")
set(ENGINE_SHADER_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders" CACHE STRING "")

include(FetchContent)

# Vulkan
FetchContent_Declare(Vulkan_Headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG "vulkan-sdk-1.4.350.0"
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Vulkan_Headers)
#

# Volk
FetchContent_Declare(Volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG "1.4.350"
    GIT_SHALLOW TRUE
)
set(VULKAN_HEADERS_INSTALL_DIR "${vulkan_headers_SOURCE_DIR}" CACHE PATH "Where to get the Vulkan headers" FORCE)
FetchContent_MakeAvailable(Volk)
#

# VMA
FetchContent_Declare(Vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG "v3.4.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Vma)
#

# Spir-v Reflection
FetchContent_Declare(SpirvReflection
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
    GIT_TAG "vulkan-sdk-1.4.350.0"
    GIT_SHALLOW TRUE
)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SpirvReflection)
#

# GLM
FetchContent_Declare(GLM
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG "1.0.3"
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(GLM)
#

# KTX 
FetchContent_Declare(
    ktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG "v4.4.2"
    #SOURCE_SUBDIR "lib"
    GIT_SHALLOW TRUE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)

# https://github.com/KhronosGroup/KTX-Software/blob/main/lib/include/ktx.h 
# https://github.com/KhronosGroup/KTX-Software/blob/main/examples/vkload.cpp 
FetchContent_MakeAvailable(ktx)
#

# FastGltf
FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        "v0.9.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fastgltf)
#

# Meshoptimizer
FetchContent_Declare(
    meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
    GIT_TAG        "v1.2"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(meshoptimizer)
#

# Slang
FetchContent_Declare(
    slang
    GIT_REPOSITORY https://github.com/shader-slang/slang.git
    #GIT_TAG "vulkan-sdk-1.4.350.0"
    GIT_TAG "v2026.13"
    GIT_SHALLOW TRUE
)

set(SLANG_ENABLE_SLANGC ON CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGD OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGI OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_OPTIX OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_CUDA OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_DXIL OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_COVERAGE OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_TIME_TRACE OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANG_GLSLANG OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANG_PROXY OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_REPLAYER OFF CACHE BOOL "" FORCE)
set(SLANG_EXCLUDE_DAWN ON CACHE BOOL "" FORCE)
set(SLANG_LIB_TYPE STATIC CACHE STRING "" FORCE)

set(SLANG_ENABLE_MINIZ ON CACHE BOOL "" FORCE)
set(SLANG_USE_SYSTEM_MINIZ OFF CACHE BOOL "" FORCE)
set(SLANG_STATIC ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(slang)

# Jolt Physics
FetchContent_Declare(
    joltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        "master"
    SOURCE_SUBDIR "Build"
)
#set(USE_ASSERTS OFF)
set(JPH_BUILD_SHARED_LIBS OFF)
##set(JPH_CPP_EXCEPTIONS_ENABLED OFF)
#set(JPH_FLOATING_POINT_EXCEPTIONS_ENABLED ON)
#set(JPH_CROSS_PLATFORM_DETERMINISTIC OFF)
#set(GENERATE_DEBUG_SYMBOLS ON)
#set(JPH_DOUBLE_PRECISION OFF)
#set(JPH_OBJECT_LAYER_BITS 16)
#set(JPH_USE_DX12 ON)
#option(CPP_RTTI_ENABLED "Enable C++ RTTI" OFF)
FetchContent_MakeAvailable(joltPhysics)
#

# Flecs
FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG master
)
 
FetchContent_MakeAvailable(flecs)
#

# aZero Audio
FetchContent_Declare(
    aZeroAudio
    GIT_REPOSITORY https://github.com/NoahSch1999/aZero-Audio.git
    GIT_TAG        "main"
)
FetchContent_MakeAvailable(aZeroAudio)
#

# aZero Sparse Set
FetchContent_Declare(
    aZero-Sparse-Set
    GIT_REPOSITORY https://github.com/NoahSch1999/aZero-Sparse-Set.git
    GIT_TAG        "master"
)
FetchContent_MakeAvailable(aZero-Sparse-Set)
#

# aZero Linear Allocator
FetchContent_Declare(
   aZero-Linear-Allocator
   GIT_REPOSITORY https://github.com/NoahSch1999/aZero-LinearAllocator.git
   GIT_TAG        "master"
)
FetchContent_MakeAvailable(aZero-Linear-Allocator)

file(GLOB ENGINE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.hpp"
)

add_library(${PROJECT_NAME}
    "src/Engine.cpp"
    "src/misc/STBIHack.cpp"
    "src/physics/PhysicsEngine.cpp"
    "src/physics/PhysicsWorld.cpp"
    "src/render_api/CommandPool.cpp"
    "src/render_api/VolkImpl.cpp"
)

target_sources(${PROJECT_NAME} PUBLIC ${ENGINE_HEADERS})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_DEBUG)
    target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g)   # Disable optimization, enable debug info
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${PROJECT_NAME} PRIVATE -O2)      # Full optimization
endif()

# PRE-PROCESSOR SETTINGS
target_compile_definitions(${PROJECT_NAME} PUBLIC 
    PROJECT_DIRECTORY="${PROJECT_DIRECTORY}"
    ENGINE_SHADER_DIRECTORY="${ENGINE_SHADER_DIRECTORY}"
    VK_NO_PROTOTYPES
    VK_USE_PLATFORM_WIN32_KHR
    NOMINMAX
    _USE_MATH_DEFINES
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(${PROJECT_NAME}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC ${CMAKE_BINARY_DIR}/_deps/aZero-sparse-set-src/src/include
    PUBLIC ${CMAKE_BINARY_DIR}/_deps/aZero-linear-allocator-src/include
    PUBLIC ${joltPhysics_SOURCE_DIR}/..
    PUBLIC ${ktx_SOURCE_DIR}/include
)

target_link_directories(${PROJECT_NAME}
    PUBLIC ${ktx_SOURCE_DIR}/include
)

# Linking
target_link_libraries(${PROJECT_NAME} PUBLIC
    aZeroAudio
    meshoptimizer
    Jolt
    flecs::flecs_static
    ktx
    fastgltf
    Vulkan::Headers
    volk::volk
    GPUOpen::VulkanMemoryAllocator
    glm::glm
    slang
    spirv-reflect-static
)
#