# HexaWorld - Hexagonal Grid Visualization

A standalone C++ application that demonstrates hexagonal grid generation and rendering using SFML.

## Features

- **Hexagonal Grid Generation**: Creates expandable hexagonal grids using axial coordinates
- **SFML Rendering**: Real-time visualization of hexagonal grids
- **Configurable Layers**: Expand the grid by adding concentric layers of hexagons
- **Clean Architecture**: Standalone implementation without game dependencies

## Mathematical Background

The application uses **axial coordinates** (q, r) for hexagonal grids, where:
- Each hexagon has 6 neighbors
- Flat-top orientation with proper spacing
- Coordinate conversion to pixel positions using trigonometry

## Building

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler
- SFML 3.0 or higher

### Build Steps

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
make
```

## Running

```bash
# From the build directory
./hexaworld
```

### Controls

- **ESC**: Exit the application

## Grid Structure

The hexagonal grid starts with a center hexagon and expands outward:

- **Layer 0**: 1 hexagon (center)
- **Layer 1**: 6 hexagons (ring around center)
- **Layer 2**: 12 hexagons (second ring)
- **Layer 3**: 18 hexagons (third ring)
- **Total for 3 layers**: 37 hexagons

## Architecture

### Files

- `hex_grid_new.hpp/cpp`: HexGrid class with axial coordinate system
- `sfml_renderer.hpp/cpp`: SFML-based rendering utilities
- `hexaworld_main.cpp`: Main application entry point
- `CMakeLists.txt`: Build configuration

### Classes

- **HexGrid**: Manages hexagonal grid with automatic neighbor generation
- **SFMLRenderer**: Handles window management and drawing primitives

## Technical Details

- **Coordinate System**: Axial coordinates (q, r) for efficient hexagonal operations
- **Rendering**: Flat-top hexagons with proper vertex calculation
- **Performance**: Efficient grid expansion without duplicates
- **Memory**: Minimal memory footprint using std::map for coordinate storage

## Future Enhancements

- Interactive grid expansion (keyboard controls)
- Different grid patterns and shapes
- Color schemes and visual effects
- Performance optimizations for larger grids