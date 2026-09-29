cmake_minimum_required(VERSION 3.21)

project(QtTune
    VERSION 0.1.0
    DESCRIPTION "Cross-platform ECU reading, logging, and tuning"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# QtTune version is the single source of truth; propagate to core + app
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include/qttune/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/qttune/version.h
    @ONLY
)

add_subdirectory(core)
add_subdirectory(app)
