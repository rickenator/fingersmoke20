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
    fluid/GPUFluidSolver.cpp
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
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)

# List of shader files to compile
set(SHADER_FILES
    ${SHADER_DIR}/fluid_addforce.comp
    ${SHADER_DIR}/fluid_diffuse.comp
    ${SHADER_DIR}/fluid_advect.comp
    ${SHADER_DIR}/fluid_project.comp
)

if(GLSLC)
    # Create a custom target to compile all shaders
    set(SPIRV_OUTPUTS)
    foreach(SHADER_FILE ${SHADER_FILES})
        get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
        set(SPIRV_FILE ${SHADER_DIR}/${SHADER_NAME}.spv)

        add_custom_command(
            OUTPUT ${SPIRV_FILE}
            COMMAND ${GLSLC} ${SHADER_FILE} -o ${SPIRV_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling GLSL compute shader ${SHADER_NAME} to SPIR-V"
        )

        list(APPEND SPIRV_OUTPUTS ${SPIRV_FILE})
    endforeach()

    add_custom_target(fluid_shaders DEPENDS ${SPIRV_OUTPUTS})
    add_dependencies(fluidsim fluid_shaders)
endif()
