cmake_minimum_required(VERSION 3.20)
project(NeonWM VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
include(ExternalProject)

# ============================================================================
# Fetch and build Wayland if not found
# ============================================================================
find_package(PkgConfig)

pkg_check_modules(WAYLAND wayland-server)
if(NOT WAYLAND_FOUND)
    message(STATUS "Wayland not found, downloading...")
    FetchContent_Declare(
        wayland
        GIT_REPOSITORY https://gitlab.freedesktop.org/wayland/wayland.git
        GIT_TAG 1.22.0
    )
    FetchContent_MakeAvailable(wayland)
    set(WAYLAND_INCLUDE_DIRS ${wayland_SOURCE_DIR}/src)
    set(WAYLAND_LIBRARIES wayland-server)
else()
    message(STATUS "Found Wayland: ${WAYLAND_INCLUDE_DIRS}")
endif()

# ============================================================================
# Fetch and build Wayland-protocols
# ============================================================================
pkg_check_modules(WAYLAND_PROTOCOLS wayland-protocols)
if(NOT WAYLAND_PROTOCOLS_FOUND)
    message(STATUS "Wayland-protocols not found, downloading...")
    FetchContent_Declare(
        wayland-protocols
        GIT_REPOSITORY https://gitlab.freedesktop.org/wayland/wayland-protocols.git
        GIT_TAG 1.32
    )
    FetchContent_MakeAvailable(wayland-protocols)
    set(WAYLAND_PROTOCOLS_DIR ${wayland-protocols_SOURCE_DIR})
else()
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=pkgdatadir wayland-protocols
        OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

# ============================================================================
# Fetch wlroots (lightweight wayland compositor library)
# ============================================================================
pkg_check_modules(WLROOTS wlroots)
if(NOT WLROOTS_FOUND)
    message(STATUS "wlroots not found, downloading...")
    ExternalProject_Add(
        wlroots_external
        GIT_REPOSITORY https://gitlab.freedesktop.org/wlroots/wlroots.git
        GIT_TAG 0.17.0
        PREFIX ${CMAKE_BINARY_DIR}/wlroots
        CONFIGURE_COMMAND meson setup <BINARY_DIR> <SOURCE_DIR> --prefix=<INSTALL_DIR>
        BUILD_COMMAND ninja -C <BINARY_DIR>
        INSTALL_COMMAND ninja -C <BINARY_DIR> install
    )
    set(WLROOTS_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/wlroots/include)
    set(WLROOTS_LIBRARIES ${CMAKE_BINARY_DIR}/wlroots/lib/libwlroots.so)
else()
    message(STATUS "Found wlroots: ${WLROOTS_INCLUDE_DIRS}")
endif()

# ============================================================================
# XKBCommon for keyboard handling
# ============================================================================
pkg_check_modules(XKBCOMMON xkbcommon)
if(NOT XKBCOMMON_FOUND)
    message(STATUS "xkbcommon not found, downloading...")
    FetchContent_Declare(
        xkbcommon
        GIT_REPOSITORY https://github.com/xkbcommon/libxkbcommon.git
        GIT_TAG xkbcommon-1.6.0
    )
    FetchContent_MakeAvailable(xkbcommon)
    set(XKBCOMMON_INCLUDE_DIRS ${xkbcommon_SOURCE_DIR}/include)
    set(XKBCOMMON_LIBRARIES xkbcommon)
else()
    message(STATUS "Found xkbcommon: ${XKBCOMMON_INCLUDE_DIRS}")
endif()

# ============================================================================
# OpenGL ES and EGL
# ============================================================================
find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)
if(NOT OPENGL_FOUND)
    message(FATAL_ERROR "OpenGL not found! Install mesa or GPU drivers.")
endif()

pkg_check_modules(GLESV3 glesv2)
if(NOT GLESV3_FOUND)
    message(WARNING "GLESv3 not found via pkg-config, using system default")
    set(GLESV3_LIBRARIES GLESv2)
    set(GLESV3_INCLUDE_DIRS "")
endif()

pkg_check_modules(EGL egl)
if(NOT EGL_FOUND)
    message(WARNING "EGL not found via pkg-config, using system default")
    set(EGL_LIBRARIES EGL)
    set(EGL_INCLUDE_DIRS "")
endif()

# ============================================================================
# Pixman (for region/rect handling)
# ============================================================================
pkg_check_modules(PIXMAN pixman-1)
if(NOT PIXMAN_FOUND)
    message(STATUS "Pixman not found, downloading...")
    FetchContent_Declare(
        pixman
        GIT_REPOSITORY https://gitlab.freedesktop.org/pixman/pixman.git
        GIT_TAG pixman-0.42.2
    )
    FetchContent_MakeAvailable(pixman)
    set(PIXMAN_INCLUDE_DIRS ${pixman_SOURCE_DIR}/pixman)
    set(PIXMAN_LIBRARIES pixman)
else()
    message(STATUS "Found pixman: ${PIXMAN_INCLUDE_DIRS}")
endif()

# ============================================================================
# Build NeonWM
# ============================================================================
add_executable(neonwm
    src/main.cpp
)

target_include_directories(neonwm PRIVATE
    ${WAYLAND_INCLUDE_DIRS}
    ${WLROOTS_INCLUDE_DIRS}
    ${XKBCOMMON_INCLUDE_DIRS}
    ${GLESV3_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(neonwm PRIVATE
    ${WAYLAND_LIBRARIES}
    ${WLROOTS_LIBRARIES}
    ${XKBCOMMON_LIBRARIES}
    ${GLESV3_LIBRARIES}
    ${EGL_LIBRARIES}
    ${PIXMAN_LIBRARIES}
    OpenGL::OpenGL
    OpenGL::EGL
    m
    pthread
)

# Add wlroots dependency if building it
if(NOT WLROOTS_FOUND)
    add_dependencies(neonwm wlroots_external)
endif()

target_compile_options(neonwm PRIVATE
    -Wall
    -Wextra
    -O3
    -march=native
)

# ============================================================================
# Install targets
# ============================================================================
install(TARGETS neonwm
    RUNTIME DESTINATION bin
)

# Install session file
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/neonwm.desktop
    DESTINATION share/wayland-sessions
)

# ============================================================================
# Generate Wayland protocol bindings
# ============================================================================
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(PROTOCOLS
    ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml
    ${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
)

foreach(PROTOCOL ${PROTOCOLS})
    get_filename_component(PROTOCOL_NAME ${PROTOCOL} NAME_WE)
    
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/${PROTOCOL_NAME}-protocol.h
        COMMAND ${WAYLAND_SCANNER} server-header ${PROTOCOL} ${CMAKE_BINARY_DIR}/${PROTOCOL_NAME}-protocol.h
        DEPENDS ${PROTOCOL}
    )
    
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/${PROTOCOL_NAME}-protocol.c
        COMMAND ${WAYLAND_SCANNER} private-code ${PROTOCOL} ${CMAKE_BINARY_DIR}/${PROTOCOL_NAME}-protocol.c
        DEPENDS ${PROTOCOL}
    )
    
    list(APPEND PROTOCOL_SOURCES
        ${CMAKE_BINARY_DIR}/${PROTOCOL_NAME}-protocol.h
        ${CMAKE_BINARY_DIR}/${PROTOCOL_NAME}-protocol.c
    )
endforeach()

target_sources(neonwm PRIVATE ${PROTOCOL_SOURCES})
target_include_directories(neonwm PRIVATE ${CMAKE_BINARY_DIR})

# ============================================================================
# Print configuration summary
# ============================================================================
message(STATUS "")
message(STATUS "╔═══════════════════════════════════════════════════╗")
message(STATUS "║          NeonWM Configuration Summary             ║")
message(STATUS "╚═══════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  Wayland:         ${WAYLAND_LIBRARIES}")
message(STATUS "  wlroots:         ${WLROOTS_LIBRARIES}")
message(STATUS "  xkbcommon:       ${XKBCOMMON_LIBRARIES}")
message(STATUS "  OpenGL ES:       ${GLESV3_LIBRARIES}")
message(STATUS "  EGL:             ${EGL_LIBRARIES}")
message(STATUS "")
message(STATUS "Configure complete! Run 'make' to build NeonWM")
message(STATUS "")
