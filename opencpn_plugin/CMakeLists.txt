cmake_minimum_required(VERSION 3.10)
project(sailing_pi VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find wxWidgets ────────────────────────────────────────────────────────────
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# ── OpenCPN plugin headers ────────────────────────────────────────────────────
# Point OPENCPN_INCLUDE_DIR at the directory containing ocpn_plugin.h.
# Default: look for a system-installed OpenCPN; override on the command line:
#   cmake -DOPENCPN_INCLUDE_DIR=/path/to/opencpn/include ..
if(NOT DEFINED OPENCPN_INCLUDE_DIR)
    # Try common install locations
    find_path(OPENCPN_INCLUDE_DIR ocpn_plugin.h
        HINTS
            /usr/include/opencpn
            /usr/local/include/opencpn
            $ENV{OPENCPN_INCLUDE}
    )
endif()

if(NOT OPENCPN_INCLUDE_DIR)
    message(FATAL_ERROR
        "Could not find ocpn_plugin.h. "
        "Set -DOPENCPN_INCLUDE_DIR=/path/to/opencpn/include")
endif()

message(STATUS "OpenCPN include dir: ${OPENCPN_INCLUDE_DIR}")

# ── Build the shared library (plugin) ────────────────────────────────────────
add_library(sailing_pi SHARED
    sailing_pi.cpp
    sailing_pi.h
)

target_include_directories(sailing_pi PRIVATE
    ${OPENCPN_INCLUDE_DIR}
    ${wxWidgets_INCLUDE_DIRS}
)

target_compile_definitions(sailing_pi PRIVATE
    ${wxWidgets_DEFINITIONS}
    MAKING_PLUGIN          # required by ocpn_plugin.h
)

target_link_libraries(sailing_pi PRIVATE
    ${wxWidgets_LIBRARIES}
)

# Windows: also link Winsock
if(WIN32)
    target_link_libraries(sailing_pi PRIVATE ws2_32)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
# OpenCPN looks for plugins in:
#   Linux / macOS : ~/.opencpn/plugins/  or  /usr/lib/opencpn/
#   Windows       : %APPDATA%\opencpn\plugins\
if(UNIX AND NOT APPLE)
    install(TARGETS sailing_pi
            LIBRARY DESTINATION lib/opencpn)
elseif(APPLE)
    install(TARGETS sailing_pi
            LIBRARY DESTINATION lib/opencpn)
elseif(WIN32)
    install(TARGETS sailing_pi
            RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/plugins")
endif()
