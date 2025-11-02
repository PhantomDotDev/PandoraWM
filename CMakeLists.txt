cmake_minimum_required(VERSION 3.14)
project(NeonWM LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Dependencies via pkg-config ----
find_package(PkgConfig REQUIRED)

pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-egl)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES REQUIRED glesv2)
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)

# ---- Build target ----
file(GLOB SRC "src/*.cpp")

add_executable(neonwm ${SRC})

target_include_directories(neonwm PRIVATE
    ${WAYLAND_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLES_INCLUDE_DIRS}
    ${XKBCOMMON_INCLUDE_DIRS}
)

target_link_libraries(neonwm
    ${WAYLAND_LIBRARIES}
    ${EGL_LIBRARIES}
    ${GLES_LIBRARIES}
    ${XKBCOMMON_LIBRARIES}
)

# ---- Compiler Flags ----
target_compile_options(neonwm PRIVATE
    ${WAYLAND_CFLAGS_OTHER}
    ${EGL_CFLAGS_OTHER}
    ${GLES_CFLAGS_OTHER}
    ${XKBCOMMON_CFLAGS_OTHER}
)

# ---- Install ----
install(TARGETS neonwm)
