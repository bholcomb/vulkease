cmake_minimum_required(VERSION 3.15)
project(VulkEaseExamples LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

# Find VulkEase library (assuming it's installed or built separately)
find_library(VULKEASE_LIB vulkease2 PATHS ${CMAKE_SOURCE_DIR}/lib)
if(NOT VULKEASE_LIB)
    message(FATAL_ERROR "VulkEase library not found. Please build vulkease2 first.")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${GLFW_INCLUDE_DIRS}
)

# Common compile definitions and flags
add_compile_definitions(
    $<$<CONFIG:Debug>:DEBUG>
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
    $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XLIB_KHR>
    $<$<PLATFORM_ID:Darwin>:VK_USE_PLATFORM_MACOS_MVK>
)

# Common libraries
set(COMMON_LIBS 
    ${VULKEASE_LIB}
    ${GLFW_LIBRARIES}
    $<$<PLATFORM_ID:Windows>:user32>
    $<$<PLATFORM_ID:Linux>:X11>
    m  # Math library
)

# Example 1: Basic Triangle
add_executable(01_triangle 01_triangle.c)
target_link_libraries(01_triangle ${COMMON_LIBS})

# Example 2: Textured Quad
add_executable(02_textured_quad 02_textured_quad.c)
target_link_libraries(02_textured_quad ${COMMON_LIBS})

# Example 3: Compute Particles
add_executable(03_compute_particles 03_compute_particles.c)
target_link_libraries(03_compute_particles ${COMMON_LIBS})

# Example 4: Deferred Rendering
add_executable(04_deferred_rendering 04_deferred_rendering.c)
target_link_libraries(04_deferred_rendering ${COMMON_LIBS})

# Create output directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/textures)

# Copy assets to build directory
file(GLOB SHADER_FILES "${CMAKE_SOURCE_DIR}/shaders/*.spv")
file(GLOB TEXTURE_FILES "${CMAKE_SOURCE_DIR}/textures/*")

if(SHADER_FILES)
    file(COPY ${SHADER_FILES} DESTINATION ${CMAKE_BINARY_DIR}/shaders)
endif()

if(TEXTURE_FILES)
    file(COPY ${TEXTURE_FILES} DESTINATION ${CMAKE_BINARY_DIR}/textures)
endif()

# Shader compilation helper (requires glslc or glslangValidator)
find_program(GLSLC glslc)
find_program(GLSLANG glslangValidator)

if(GLSLC)
    set(GLSL_COMPILER ${GLSLC})
    set(GLSL_ARGS -fshader-stage=)
elseif(GLSLANG)
    set(GLSL_COMPILER ${GLSLANG})
    set(GLSL_ARGS -V -S)
else()
    message(WARNING "No GLSL compiler found. Please install glslc (from Vulkan SDK) or glslangValidator")
endif()

# Function to compile GLSL shaders
function(compile_shaders)
    if(NOT GLSL_COMPILER)
        return()
    endif()
    
    file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/shaders/*.vert"
        "${CMAKE_SOURCE_DIR}/shaders/*.frag"
        "${CMAKE_SOURCE_DIR}/shaders/*.comp"
        "${CMAKE_SOURCE_DIR}/shaders/*.geom"
        "${CMAKE_SOURCE_DIR}/shaders/*.tesc"
        "${CMAKE_SOURCE_DIR}/shaders/*.tese"
    )
    
    foreach(GLSL ${GLSL_SOURCE_FILES})
        get_filename_component(FILE_NAME ${GLSL} NAME)
        set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")
        
        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_COMPILER} ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
            COMMENT "Compiling ${FILE_NAME}"
        )
        
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach(GLSL)
    
    add_custom_target(
        shaders 
        DEPENDS ${SPIRV_BINARY_FILES}
        COMMENT "Compiling shaders"
    )
    
    # Make examples depend on shaders
    add_dependencies(01_triangle shaders)
    add_dependencies(02_textured_quad shaders)
    add_dependencies(03_compute_particles shaders)
    add_dependencies(04_deferred_rendering shaders)
endfunction()

compile_shaders()

# Install targets
install(TARGETS 01_triangle 02_textured_quad 03_compute_particles 04_deferred_rendering
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_BINARY_DIR}/shaders/
    DESTINATION share/vulkease-examples/shaders
    FILES_MATCHING PATTERN "*.spv"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/textures/
    DESTINATION share/vulkease-examples/textures
    FILES_MATCHING PATTERN "*"
)

# Package configuration
set(CPACK_PACKAGE_NAME "vulkease-examples")
set(CPACK_PACKAGE_VERSION "2.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VulkEase 2.0 Example Applications")
set(CPACK_PACKAGE_VENDOR "VulkEase")

include(CPack)

# Print build information
message(STATUS "VulkEase Examples Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  VulkEase library: ${VULKEASE_LIB}")
message(STATUS "  GLFW libraries: ${GLFW_LIBRARIES}")
message(STATUS "  GLSL compiler: ${GLSL_COMPILER}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  01_triangle - Basic triangle rendering")
message(STATUS "  02_textured_quad - Indexed rendering with textures") 
message(STATUS "  03_compute_particles - Compute shader particle system")
message(STATUS "  04_deferred_rendering - Multi-pass deferred rendering")
message(STATUS "")
message(STATUS "Build with: make -j$(nproc)")
message(STATUS "Run example: ./01_triangle")