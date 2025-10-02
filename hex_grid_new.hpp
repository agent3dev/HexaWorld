#pragma once

#include "sfml_renderer.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <random>

namespace hexaworld {

// Terrain types
enum TerrainType {
    SOIL,
    WATER,
    ROCK
};

// Terrain tile class
struct TerrainTile {
    int q, r;
    TerrainType type;
    float nutrients; // 0.0 to 1.0, affects plant growth likelihood and quality
    TerrainTile(int q, int r, TerrainType type, float nutrients) : q(q), r(r), type(type), nutrients(nutrients) {}
};

// Plant stages
enum PlantStage { SEED, SPROUT, PLANT };

// Plant class
struct Plant {
    int q, r;
    PlantStage stage;
    float growth_time;
    float nutrients; // cached from tile
    Plant(int q, int r, PlantStage stage, float nutrients) : q(q), r(r), stage(stage), growth_time(0.0f), nutrients(nutrients) {}
};

// ============================================================================
// HEX GRID CLASS - Manages hexagonal grid with proper neighbor relationships
// ============================================================================

class HexGrid {
public:
    float hex_size;
    std::map<std::pair<int, int>, std::pair<float, float>> hexagons;  // (q,r) -> (x,y)
    std::map<std::pair<int, int>, TerrainTile> terrainTiles;  // (q,r) -> tile
    static const std::vector<std::pair<int, int>> directions;

    HexGrid(float size) : hex_size(size) {}

    // Convert axial coordinates to pixel position
    std::pair<float, float> axial_to_pixel(int q, int r) const;

    // Add hexagon at axial coordinates (q, r)
    void add_hexagon(int q, int r);

    // Check if hexagon exists at coordinates
    bool has_hexagon(int q, int r) const;

    // Create neighbors of existing hexagons
    void create_neighbors();

    // Expand grid by one layer
    void expand_grid(int layers = 1);

    // Draw all hexagons
    void draw(SFMLRenderer& renderer, uint8_t r, uint8_t g, uint8_t b,
              uint8_t outline_r, uint8_t outline_g, uint8_t outline_b,
              float offset_x, float offset_y, int screen_width, int screen_height) const;

    // Get neighbor coordinates for a given direction
    std::pair<int, int> get_neighbor_coords(int q, int r, int direction) const;

private:
};

// ============================================================================
// HEX OBJECT - Movable object on the grid
// ============================================================================

struct HexObject {
    int q, r;
    HexObject(int q, int r) : q(q), r(r) {}
    void move(int direction) {
        auto [dq, dr] = HexGrid::directions[direction % 6];
        q += dq;
        r += dr;
    }
};

// ============================================================================
// END OF HEX GRID CLASS
// ============================================================================

} // namespace hexaworld