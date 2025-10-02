#pragma once

#include "sfml_renderer.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace hexaworld {

// ============================================================================
// HEX GRID CLASS - Manages hexagonal grid with proper neighbor relationships
// ============================================================================

class HexGrid {
public:
    float hex_size;
    std::map<std::pair<int, int>, std::pair<float, float>> hexagons;  // (q,r) -> (x,y)

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
              float offset_x = 0.0f, float offset_y = 0.0f) const;

private:
    // Get neighbor coordinates for a given direction
    std::pair<int, int> get_neighbor_coords(int q, int r, int direction) const;

    // Neighbor directions for flat-top hexagons (axial coordinates)
    const std::vector<std::pair<int, int>> directions = {
        {0, -1},  // top
        {1, -1},  // upper-right
        {1, 0},   // lower-right
        {0, 1},   // bottom
        {-1, 1},  // lower-left
        {-1, 0}   // upper-left
    };
};

// ============================================================================
// END OF HEX GRID CLASS
// ============================================================================

} // namespace hexaworld