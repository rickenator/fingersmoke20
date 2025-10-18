# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.4.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
project("fingersmoke20")

find_package(Vulkan REQUIRED)

add_library(${CMAKE_PROJECT_NAME} SHARED fs20.cpp)

find_library(log-lib log)

find_program(GLSLANG_VALIDATOR NAMES glslangValidator)

if(GLSLANG_VALIDATOR)
    set(SHADER_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compute_shader.glsl"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vertex_shader.glsl"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fragment_shader.glsl"
    )

    set(SHADER_OUTPUTS)
    foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
        set(SPIRV_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv")
        add_custom_command(
                OUTPUT ${SPIRV_OUTPUT}
                COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SOURCE} -o ${SPIRV_OUTPUT}
                DEPENDS ${SHADER_SOURCE}
                COMMENT "Compiling ${SHADER_SOURCE}"
        )
        list(APPEND SHADER_OUTPUTS ${SPIRV_OUTPUT})
    endforeach()

    add_custom_target(CompileAllShaders ALL DEPENDS ${SHADER_OUTPUTS})
else()
    message(WARNING "glslangValidator not found; shader compilation will be skipped.")
endif()

if(TARGET CompileAllShaders)
    add_dependencies(${CMAKE_PROJECT_NAME} CompileAllShaders)
endif()

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
