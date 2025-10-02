#include "hex_grid_new.hpp"
#include <cmath>

namespace hexaworld {

// ============================================================================
// HEX GRID CLASS IMPLEMENTATION
// ============================================================================

std::pair<float, float> HexGrid::axial_to_pixel(int q, int r) const {
    // Convert axial coordinates to pixel position for flat-top hexagons
    const float sqrt3 = std::sqrt(3.0f);
    float x = hex_size * (3.0f/2.0f * q);
    float y = hex_size * (sqrt3/2.0f * q + sqrt3 * r);
    return {x, y};
}

void HexGrid::add_hexagon(int q, int r) {
    if (!has_hexagon(q, r)) {
        auto [x, y] = axial_to_pixel(q, r);
        hexagons[{q, r}] = {x, y};
    }
}

bool HexGrid::has_hexagon(int q, int r) const {
    return hexagons.find({q, r}) != hexagons.end();
}

void HexGrid::create_neighbors() {
    std::vector<std::pair<int, int>> new_hexagons;

    // Find all hexagons that need neighbors created
    for (const auto& [coords, pos] : hexagons) {
        auto [q, r] = coords;

        // Check each of the 6 neighbor directions
        for (int dir = 0; dir < 6; ++dir) {
            auto [nq, nr] = get_neighbor_coords(q, r, dir);
            if (!has_hexagon(nq, nr)) {
                new_hexagons.push_back({nq, nr});
            }
        }
    }

    // Add all new hexagons
    for (const auto& [q, r] : new_hexagons) {
        add_hexagon(q, r);
    }
}

void HexGrid::expand_grid(int layers) {
    for (int i = 0; i < layers; ++i) {
        create_neighbors();
    }
}

void HexGrid::draw(SFMLRenderer& renderer, uint8_t r, uint8_t g, uint8_t b,
                  uint8_t outline_r, uint8_t outline_g, uint8_t outline_b,
                  float offset_x, float offset_y) const {
    for (const auto& [coords, pos] : hexagons) {
        auto [x, y] = pos;
        renderer.drawHexagon(x + offset_x, y + offset_y, hex_size, r, g, b, outline_r, outline_g, outline_b, true);
    }
}

std::pair<int, int> HexGrid::get_neighbor_coords(int q, int r, int direction) const {
    auto [dq, dr] = directions[direction];
    return {q + dq, r + dr};
}

// ============================================================================
// END OF HEX GRID CLASS IMPLEMENTATION
// ============================================================================

} // namespace hexaworld