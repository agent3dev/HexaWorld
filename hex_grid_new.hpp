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

private:
    // Get neighbor coordinates for a given direction
    std::pair<int, int> get_neighbor_coords(int q, int r, int direction) const;
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