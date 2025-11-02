cmake_minimum_required(VERSION 3.20)
project(NeonWM VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use system dependencies preferentially
find_package(PkgConfig REQUIRED)

# ============================================================================
# Check for required system packages
# ============================================================================
message(STATUS "Checking for dependencies...")

# Wayland
pkg_check_modules(WAYLAND REQUIRED wayland-server>=1.20)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client>=1.20)

# Wayland protocols
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.25)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

# wlroots
pkg_check_modules(WLROOTS REQUIRED wlroots>=0.16.0)

# xkbcommon
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon>=1.0.0)

# Pixman
pkg_check_modules(PIXMAN REQUIRED pixman-1>=0.42.0)

# OpenGL ES and EGL
pkg_check_modules(GLESV2 REQUIRED glesv2)
pkg_check_modules(EGL REQUIRED egl)

# Input (libinput)
pkg_check_modules(LIBINPUT REQUIRED libinput>=1.14)

# ============================================================================
# Generate Wayland protocol bindings
# ============================================================================
message(STATUS "Generating Wayland protocol bindings...")

set(PROTOCOL_FILES
    "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
    "${WAYLAND_PROTOCOLS_DIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
)

set(PROTOCOL_SOURCES "")
foreach(PROTOCOL_XML ${PROTOCOL_FILES})
    get_filename_component(PROTOCOL_NAME ${PROTOCOL_XML} NAME_WE)
    
    set(PROTOCOL_HEADER "${CMAKE_BINARY_DIR}/protocols/${PROTOCOL_NAME}-protocol.h")
    set(PROTOCOL_CODE "${CMAKE_BINARY_DIR}/protocols/${PROTOCOL_NAME}-protocol.c")
    
    add_custom_command(
        OUTPUT ${PROTOCOL_HEADER}
        COMMAND mkdir -p ${CMAKE_BINARY_DIR}/protocols
        COMMAND ${WAYLAND_SCANNER} server-header ${PROTOCOL_XML} ${PROTOCOL_HEADER}
        DEPENDS ${PROTOCOL_XML}
        VERBATIM
    )
    
    add_custom_command(
        OUTPUT ${PROTOCOL_CODE}
        COMMAND ${WAYLAND_SCANNER} private-code ${PROTOCOL_XML} ${PROTOCOL_CODE}
        DEPENDS ${PROTOCOL_XML} ${PROTOCOL_HEADER}
        VERBATIM
    )
    
    list(APPEND PROTOCOL_SOURCES ${PROTOCOL_HEADER} ${PROTOCOL_CODE})
endforeach()

# ============================================================================
# Source files
# ============================================================================
set(NEONWM_SOURCES
    src/main.cpp
)

# ============================================================================
# Create the executable
# ============================================================================
add_executable(neonwm ${NEONWM_SOURCES} ${PROTOCOL_SOURCES})

target_include_directories(neonwm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/protocols
    ${WAYLAND_INCLUDE_DIRS}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${WLROOTS_INCLUDE_DIRS}
    ${XKBCOMMON_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
    ${GLESV2_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${LIBINPUT_INCLUDE_DIRS}
)

target_link_libraries(neonwm PRIVATE
    ${WAYLAND_LIBRARIES}
    ${WAYLAND_CLIENT_LIBRARIES}
    ${WLROOTS_LIBRARIES}
    ${XKBCOMMON_LIBRARIES}
    ${PIXMAN_LIBRARIES}
    ${GLESV2_LIBRARIES}
    ${EGL_LIBRARIES}
    ${LIBINPUT_LIBRARIES}
    m
    pthread
)

target_compile_options(neonwm PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O3
    -march=native
)

# Add definitions
target_compile_definitions(neonwm PRIVATE
    WLR_USE_UNSTABLE
)

# ============================================================================
# Install targets
# ============================================================================
install(TARGETS neonwm
    RUNTIME DESTINATION bin
)

# Create desktop entry
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/neonwm.desktop.in
    ${CMAKE_BINARY_DIR}/neonwm.desktop
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/neonwm.desktop
    DESTINATION share/wayland-sessions
)

# ============================================================================
# Create src directory structure if it doesn't exist
# ============================================================================
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ============================================================================
# Print configuration summary
# ============================================================================
message(STATUS "")
message(STATUS "╔═══════════════════════════════════════════════════╗")
message(STATUS "║          NeonWM Configuration Summary             ║")
message(STATUS "╚═══════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "Build type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:         ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies Found:")
message(STATUS "  Wayland:            ${WAYLAND_VERSION}")
message(STATUS "  wlroots:            ${WLROOTS_VERSION}")
message(STATUS "  xkbcommon:          ${XKBCOMMON_VERSION}")
message(STATUS "  pixman:             ${PIXMAN_VERSION}")
message(STATUS "  OpenGL ES:          ${GLESV2_VERSION}")
message(STATUS "  EGL:                ${EGL_VERSION}")
message(STATUS "")
message(STATUS "Wayland protocols:    ${WAYLAND_PROTOCOLS_DIR}")
message(STATUS "Wayland scanner:      ${WAYLAND_SCANNER}")
message(STATUS "")
message(STATUS "Configure complete! Run 'make' or 'ninja' to build")
message(STATUS "")
