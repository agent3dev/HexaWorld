cmake_minimum_required(VERSION 3.16)
project(HexaWorld)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall -Wextra -fsanitize=address")

# Find SFML
find_package(SFML 3.0 REQUIRED COMPONENTS Graphics Window System)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Source files
set(SOURCES
    hexaworld_main.cpp
    hex_grid_new.cpp
    sfml_renderer.cpp
)

# Create executable
add_executable(hexaworld ${SOURCES})

# Link SFML libraries
target_link_libraries(hexaworld
    sfml-graphics
    sfml-window
    sfml-system
)

# Copy assets if any (none for now)
# configure_file(...)

# Installation (optional)
install(TARGETS hexaworld DESTINATION bin)