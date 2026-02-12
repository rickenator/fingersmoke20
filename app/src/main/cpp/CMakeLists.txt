cmake_minimum_required(VERSION 3.22.1)

project("fluidsim")

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# Add source files
set(SOURCES
    android_app_bridge.cpp
    vulkan/VulkanInstance.cpp
    vulkan/VulkanSurface.cpp
    vulkan/VulkanDevice.cpp
    vulkan/VulkanSwapchain.cpp
    vulkan/VulkanCommand.cpp
    vulkan/VulkanCore.cpp
    vulkan/VulkanContext.cpp
    vulkan/VulkanComputePipeline.cpp
    vulkan/VulkanBuffer.cpp
    vulkan/VulkanMemory.cpp
    fluid/NavierStokesSimulator.cpp
    fluid/Grid2D.cpp
    fluid/Solver.cpp
)

# Create shared library
add_library(fluidsim SHARED ${SOURCES})

# Include directories
target_include_directories(fluidsim PRIVATE
    vulkan
    fluid
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-utils
)

# Link libraries
target_link_libraries(fluidsim
    Vulkan::Vulkan
    log
)

# Compile shaders if glslc is available
find_program(GLSLC glslc)
if(GLSLC AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/fluid_compute.comp)
    set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/fluid_compute.comp)
    set(SHADER_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/fluid_compute.spv)

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${GLSLC} ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling GLSL compute shader to SPIR-V"
    )

    add_custom_target(shaders DEPENDS ${SHADER_OUTPUT})
    add_dependencies(fluidsim shaders)
endif()
