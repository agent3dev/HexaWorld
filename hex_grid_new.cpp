#include "hex_grid_new.hpp"
#include "constants.hpp"
#include <algorithm>

namespace hexaworld {

// ============================================================================
// HEX GRID CLASS IMPLEMENTATION
// ============================================================================

std::pair<float, float> HexGrid::axial_to_pixel(int q, int r) const {
    // Convert axial coordinates to pixel position for flat-top hexagons
    const float sqrt3 = hexaworld::SQRT3;
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
                  float offset_x, float offset_y, int screen_width, int screen_height) const {
    const float sqrt3 = hexaworld::SQRT3;
    for (const auto& [coords, pos] : hexagons) {
        auto [q, r_coord] = coords;
        auto [x, y] = pos;
        float cx = x + offset_x;
        float cy = y + offset_y;
        float left = cx - hex_size;
        float right = cx + hex_size;
        float top = cy - hex_size * sqrt3 / 2.0f;
        float bottom = cy + hex_size * sqrt3 / 2.0f;
        if (left >= 0 && right <= screen_width && top >= 0 && bottom <= screen_height) {
            // Calculate distance for brightness adjustment
            float dist = std::max({std::abs(q), std::abs(r_coord), std::abs(q + r_coord)});
            float factor = 1.0f - std::min(dist / 15.0f, 1.0f) * 0.5f;
            uint8_t br = (uint8_t)(r * factor);
            uint8_t bg = (uint8_t)(g * factor);
            uint8_t bb = (uint8_t)(b * factor);

            // Shine: lighter
            uint8_t sr = std::min(255, (int)(br + 50));
            uint8_t sg = std::min(255, (int)(bg + 50));
            uint8_t sb = std::min(255, (int)(bb + 50));

            // Shadow: darker
            uint8_t shr = (uint8_t)(br * 0.5f);
            uint8_t shg = (uint8_t)(bg * 0.5f);
            uint8_t shb = (uint8_t)(bb * 0.5f);

            renderer.drawHexagonWithShading(cx, cy, hex_size, br, bg, bb, outline_r, outline_g, outline_b, sr, sg, sb, shr, shg, shb);
        }
    }
}

std::pair<int, int> HexGrid::get_neighbor_coords(int q, int r, int direction) const {
    auto [dq, dr] = directions[direction % 6];
    return {q + dq, r + dr};
}

const std::vector<std::pair<int, int>> HexGrid::directions = {
    {0, -1},  // top
    {1, -1},  // upper-right
    {1, 0},   // lower-right
    {0, 1},   // bottom
    {-1, 1},  // lower-left
    {-1, 0}   // upper-left
};

// ============================================================================
// END OF HEX GRID CLASS IMPLEMENTATION
// ============================================================================

} // namespace hexaworld