# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.4.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
cmake_minimum_required(VERSION 3.4.1)
project("fingersmoke20")

find_package(Vulkan REQUIRED)

add_library(${CMAKE_PROJECT_NAME} SHARED fs20.cpp)

find_library(log-lib log)

# Compiles shaders to SPIR-V
set(SHADER_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compute_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fragment_shader.glsl"
)
set(SHADER_OUTPUTS
        "${CMAKE_CURRENT_BINARY_DIR}/compute_shader.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/vertex_shader.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/fragment_shader.spv"
)

foreach(shader_idx RANGE ${num_shaders})
    list(GET SHADER_SOURCES ${shader_idx} SHADER_SOURCE)
    list(GET SHADER_OUTPUTS ${shader_idx} SHADER_OUTPUT)
    add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling ${SHADER_SOURCE}"
    )
endforeach()

add_custom_target(CompileAllShaders ALL DEPENDS ${SHADER_OUTPUTS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -frtti -fexceptions")
set(CMAKE_BUILD_TYPE "Debug")
set(ANDROID_ABI "x86;x86_64;armeabi-v7a;arm64-v8a")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        Vulkan::Vulkan
        android
        log
        vulkan
)