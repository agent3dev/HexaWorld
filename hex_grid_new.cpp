#include "hex_grid_new.hpp"
#include "constants.hpp"
#include <algorithm>
#include <random>

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

        // Assign terrain type based on adjacency
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> rand_prob(0.0, 1.0);

        // Count neighbor types
        std::map<TerrainType, int> neighbor_counts;
        for (int dir = 0; dir < 6; ++dir) {
            auto [nq, nr] = get_neighbor_coords(q, r, dir);
            if (has_hexagon(nq, nr)) {
                neighbor_counts[terrainTypes[{nq, nr}]]++;
            }
        }

        TerrainType type;
        if (neighbor_counts.empty()) {
            // No neighbors, random
            std::uniform_int_distribution<> dis(0, 2);
            type = static_cast<TerrainType>(dis(gen));
        } else {
            // 20% chance to be random anyway
            if (rand_prob(gen) < 0.2) {
                std::uniform_int_distribution<> dis(0, 2);
                type = static_cast<TerrainType>(dis(gen));
            } else {
                // Choose the most common neighbor type
                TerrainType most_common = SOIL;
                int max_count = 0;
                for (auto& [t, count] : neighbor_counts) {
                    if (count > max_count) {
                        max_count = count;
                        most_common = t;
                    }
                }
                type = most_common;
            }
        }

        terrainTypes[{q, r}] = type;
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
            // Get terrain type and base colors
            TerrainType type = terrainTypes.at({q, r_coord});
            uint8_t base_r, base_g, base_b;
            switch (type) {
                case SOIL: base_r = 139; base_g = 69; base_b = 19; break; // Brown
                case WATER: base_r = 0; base_g = 100; base_b = 200; break; // Blue
                case ROCK: base_r = 128; base_g = 128; base_b = 128; break; // Gray
            }

            // Calculate distance for brightness adjustment
            float dist = std::max({std::abs(q), std::abs(r_coord), std::abs(q + r_coord)});
            float factor = 1.0f - std::min(dist / 15.0f, 1.0f) * 0.5f;
            uint8_t br = (uint8_t)(base_r * factor);
            uint8_t bg = (uint8_t)(base_g * factor);
            uint8_t bb = (uint8_t)(base_b * factor);

            // Draw filled hexagon
            auto points = renderer.calculateHexagonPoints(cx, cy, hex_size);
            std::vector<std::pair<float, float>> point_pairs;
            for (auto& p : points) point_pairs.emplace_back(p.x, p.y);
            renderer.drawConvexShape(point_pairs, br, bg, bb);

            // Shine: lighter
            uint8_t sr = std::min(255, (int)(br + 50));
            uint8_t sg = std::min(255, (int)(bg + 50));
            uint8_t sb = std::min(255, (int)(bb + 50));

            // Shadow: darker
            uint8_t shr = (uint8_t)(br * 0.5f);
            uint8_t shg = (uint8_t)(bg * 0.5f);
            uint8_t shb = (uint8_t)(bb * 0.5f);

            // Draw shine triangles (top-right portions)
            sf::ConvexShape shine1(3);
            shine1.setPoint(0, sf::Vector2f(cx, cy));
            shine1.setPoint(1, points[0]);
            shine1.setPoint(2, points[1]);
            shine1.setFillColor(sf::Color(sr, sg, sb));
            renderer.getWindow()->draw(shine1);

            // Shadow triangles (bottom-left portions)
            sf::ConvexShape shadow1(3);
            shadow1.setPoint(0, sf::Vector2f(cx, cy));
            shadow1.setPoint(1, points[3]);
            shadow1.setPoint(2, points[4]);
            shadow1.setFillColor(sf::Color(shr, shg, shb));
            renderer.getWindow()->draw(shadow1);

            // Draw outlines per edge
            for (int i = 0; i < 6; ++i) {
                int next = (i + 1) % 6;
                auto [nq, nr] = get_neighbor_coords(q, r_coord, i);
                uint8_t or_ = outline_r, og = outline_g, ob = outline_b; // default white
                if (has_hexagon(nq, nr) && terrainTypes.at({nq, nr}) == type) {
                    or_ = base_r; og = base_g; ob = base_b; // type color
                }
                renderer.drawLine(points[i].x, points[i].y, points[next].x, points[next].y, or_, og, ob, 255, 2.0f);
            }
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