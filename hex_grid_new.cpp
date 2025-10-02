#include "hex_grid_new.hpp"
#include "constants.hpp"
#include <algorithm>
#include <random>
#include <chrono>

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
            TerrainType type = SOIL;
            auto it = terrainTypes.find({q, r_coord});
            if (it != terrainTypes.end()) type = it->second;
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

            // Draw drop shadow
            auto shadow_points = renderer.calculateHexagonPoints(cx + 3, cy + 3, hex_size);
            std::vector<std::pair<float, float>> shadow_point_pairs;
            for (auto& p : shadow_points) shadow_point_pairs.emplace_back(p.x, p.y);
            renderer.drawConvexShape(shadow_point_pairs, 0, 0, 0, 100); // Black semi-transparent

            // Draw filled hexagon as pizza slices with spot colors
            auto points = renderer.calculateHexagonPoints(cx, cy, hex_size);
            for (int i = 0; i < 6; ++i) {
                sf::ConvexShape triangle(3);
                triangle.setPoint(0, sf::Vector2f(cx, cy));
                triangle.setPoint(1, points[i]);
                triangle.setPoint(2, points[(i + 1) % 6]);
                // Wavy spot color animation confined to terrain type
                static auto start_time = std::chrono::steady_clock::now();
                auto current_time = std::chrono::steady_clock::now();
                float time = std::chrono::duration<float>(current_time - start_time).count();
                float speed = (type == ROCK) ? 2.0f : (type == SOIL) ? 4.0f : 8.0f;
                float mod = sin(time * speed) * 20.0f;
                uint8_t tr = std::clamp((int)br + (int)mod, 0, 255);
                uint8_t tg = std::clamp((int)bg + (int)mod, 0, 255);
                uint8_t tb = std::clamp((int)bb + (int)mod, 0, 255);
                triangle.setFillColor(sf::Color(tr, tg, tb));
                renderer.getWindow()->draw(triangle);
            }

            // Shine: lighter
            uint8_t sr = std::min(255, (int)(br + 80));
            uint8_t sg = std::min(255, (int)(bg + 80));
            uint8_t sb = std::min(255, (int)(bb + 80));

            // Shadow: darker
            uint8_t shr = (uint8_t)(br * 0.3f);
            uint8_t shg = (uint8_t)(bg * 0.3f);
            uint8_t shb = (uint8_t)(bb * 0.3f);

            // Draw shine triangles (top-right portions)
            sf::ConvexShape shine1(3);
            shine1.setPoint(0, sf::Vector2f(cx, cy));
            shine1.setPoint(1, points[0]);
            shine1.setPoint(2, points[1]);
            shine1.setFillColor(sf::Color(sr, sg, sb));
            renderer.getWindow()->draw(shine1);

            // Additional shine
            sf::ConvexShape shine2(3);
            shine2.setPoint(0, sf::Vector2f(cx, cy));
            shine2.setPoint(1, points[1]);
            shine2.setPoint(2, points[2]);
            shine2.setFillColor(sf::Color(sr, sg, sb));
            renderer.getWindow()->draw(shine2);

            // Shadow triangles (bottom-left portions)
            sf::ConvexShape shadow1(3);
            shadow1.setPoint(0, sf::Vector2f(cx, cy));
            shadow1.setPoint(1, points[3]);
            shadow1.setPoint(2, points[4]);
            shadow1.setFillColor(sf::Color(shr, shg, shb));
            renderer.getWindow()->draw(shadow1);

            // Additional shadow
            sf::ConvexShape shadow2(3);
            shadow2.setPoint(0, sf::Vector2f(cx, cy));
            shadow2.setPoint(1, points[4]);
            shadow2.setPoint(2, points[5]);
            shadow2.setFillColor(sf::Color(shr, shg, shb));
            renderer.getWindow()->draw(shadow2);
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