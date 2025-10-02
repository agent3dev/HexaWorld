# HexaWorld - Evolutionary Ecosystem Simulation

A real-time C++ simulation of an evolutionary ecosystem on a hexagonal grid, featuring herbivores (hares), carnivores (foxes), plants, and genetic adaptation. Built with SFML for visualization.

## Features

- **Hexagonal Grid Ecosystem**: Dynamic terrain with soil, rock, and water tiles
- **Evolutionary Simulation**: Genetic algorithms for hare and fox traits (speed, efficiency, reproduction, camouflage)
- **Plant Growth System**: Seeds sprout into plants that drop new seeds
- **Predator-Prey Dynamics**: Foxes hunt hares with visibility-based detection
- **Genetic Traits**:
  - Hare: Reproduction threshold, movement aggression, weight, fear, movement efficiency, burrowing ability
  - Fox: Reproduction threshold, hunting aggression, weight, movement efficiency
- **Interactive Visualization**: Real-time population graphs, smooth animations, antialiasing
- **Terrain Interactions**: Hares burrow to hide, access rocks/water based on traits

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
- **C**: Toggle object visibility
- **G**: Log current hare genomes

## Grid Structure

The hexagonal grid starts with a center hexagon and expands outward:

- **Layer 0**: 1 hexagon (center)
- **Layer 1**: 6 hexagons (ring around center)
- **Layer 2**: 12 hexagons (second ring)
- **Layer 3**: 18 hexagons (third ring)
- **Total for 3 layers**: 37 hexagons

## Architecture

### Files

- `hex_grid_new.hpp/cpp`: HexGrid, Hare, Fox classes with simulation logic
- `sfml_renderer.hpp/cpp`: SFML-based rendering with antialiasing
- `ga.hpp`: Genetic algorithm structures for evolution
- `hexaworld_main.cpp`: Main simulation loop and initialization
- `CMakeLists.txt`: Build configuration

### Classes

- **HexGrid**: Manages terrain, plants, and hexagonal grid
- **Hare/Fox**: Animal classes with genetic traits and behaviors
- **SFMLRenderer**: Handles window, drawing, and input
- **GeneticAlgorithm**: Evolution through mutation and selection

## Technical Details

- **Coordinate System**: Axial coordinates (q, r) for efficient hexagonal operations
- **Rendering**: Flat-top hexagons with proper vertex calculation
- **Performance**: Efficient grid expansion without duplicates
- **Memory**: Minimal memory footprint using std::map for coordinate storage

## Future Enhancements

- Additional species (apex predators, omnivores)
- Dynamic environmental changes (seasons, disasters)
- Advanced AI behaviors (memory, cooperation)
- Multiplayer or networked simulations
- Data export for analysis