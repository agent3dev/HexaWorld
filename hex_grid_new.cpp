#include "hex_grid_new.hpp"
#include "animals/hare.hpp"
#include "animals/fox.hpp"
#include "animals/wolf.hpp"
#include "constants.hpp"
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <random>
#include <algorithm>
#include <functional>
#include <random>
#include <chrono>
#include <iostream>



// ============================================================================
// HEX GRID CLASS IMPLEMENTATION
// ============================================================================

std::pair<float, float> HexGrid::axial_to_pixel(int q, int r) const {
    // Convert axial coordinates to pixel position for flat-top hexagons
    const float sqrt3 = SQRT3;
    float x = hex_size * (3.0f/2.0f * q);
    float y = hex_size * (sqrt3/2.0f * q + sqrt3 * r);
    return {x, y};
}

void HexGrid::add_hexagon(int q, int r) {
    int dist = (std::abs(q) + std::abs(r) + std::abs(q + r)) / 2;
    if (dist > max_grid_distance) return;
    if (!has_hexagon(q, r)) {
        auto [x, y] = axial_to_pixel(q, r);
        hexagons[{q, r}] = {x, y};

        // Assign terrain type based on adjacency
        std::uniform_real_distribution<> rand_prob(0.0, 1.0);
        std::uniform_real_distribution<> nutrient_var(-0.2, 0.2);

        // Count neighbor types
        std::map<TerrainType, int> neighbor_counts;
        for (int dir = 0; dir < 6; ++dir) {
            auto [nq, nr] = get_neighbor_coords(q, r, dir);
            if (has_hexagon(nq, nr)) {
                auto it = terrainTiles.find({nq, nr});
                if (it != terrainTiles.end()) {
                    neighbor_counts[it->second.type]++;
                }
            }
        }

        TerrainType type;
        if (neighbor_counts.empty()) {
            // No neighbors, random, allow some rocks
            std::uniform_int_distribution<> dis(0, 9);
            int rand_val = dis(gen);
            if (rand_val < 2) { // 20% rock
                type = ROCK;
            } else if (rand_val < 6) { // 40% soil
                type = SOIL;
            } else {
                type = WATER;
            }
        } else {
                // 30% chance to be random anyway
                if (rand_prob(gen) < 0.3) {
                    // Random with some rocks
                    std::uniform_int_distribution<> dis(0, 9);
                    int rand_val = dis(gen);
                    if (rand_val < 2) { // 20% rock
                        type = ROCK;
                    } else if (rand_val < 6) { // 40% soil
                        type = SOIL;
                    } else {
                        type = WATER;
                    }
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

        // Assign nutrients based on type
        float base_nutrients;
        switch (type) {
            case SOIL: base_nutrients = 0.8f; break;
            case WATER: base_nutrients = 0.5f; break;
            case ROCK: base_nutrients = 0.2f; break;
        }
        float nutrients = std::clamp(base_nutrients + static_cast<float>(nutrient_var(gen)), 0.0f, 1.0f);

        terrainTiles.insert({{q, r}, TerrainTile(q, r, type, nutrients)});

        // Spawn plant on soil with chance
        if (type == SOIL && (gen() % 100) < 10) { // 10% chance
            plants.insert({{q, r}, Plant(q, r, SEED, nutrients)});
        }
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
                   float offset_x, float offset_y, int screen_width, int screen_height,
                   float brightness_center_q, float brightness_center_r,
                   bool has_alive_hares) const {
    const float sqrt3 = SQRT3;
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
            auto it = terrainTiles.find({q, r_coord});
            if (it != terrainTiles.end()) type = it->second.type;

            uint8_t base_r, base_g, base_b;
            switch (type) {
                case SOIL: base_r = 139; base_g = 69; base_b = 19; break; // Brown
                case WATER: base_r = 0; base_g = 150; base_b = 255; break; // Brighter Blue
                case ROCK: base_r = 128; base_g = 128; base_b = 128; break; // Gray
            }

            // Calculate distance for brightness adjustment from brightness center
            float factor;
            if (!has_alive_hares) {
                factor = 0.5f;  // Dim whole map if no alive hares
            } else {
                float dist = std::max({std::abs(q - brightness_center_q), std::abs(r_coord - brightness_center_r), std::abs((q - brightness_center_q) + (r_coord - brightness_center_r))});
                factor = 1.0f - std::min(dist / 15.0f, 1.0f) * 0.5f;
            }
            uint8_t br = (uint8_t)(base_r * factor);
            uint8_t bg = (uint8_t)(base_g * factor);
            uint8_t bb = (uint8_t)(base_b * factor);

            // Add to terrain vertex array (main fill)
            for (int i = 0; i < 6; ++i) {
                sf::Vector2f center(cx, cy);
                sf::Vector2f p1 = center + hexagon_points[i] * hex_size;
                sf::Vector2f p2 = center + hexagon_points[(i + 1) % 6] * hex_size;
                float variation = (i % 3) * 10.0f - 10.0f;
                uint8_t tr = std::clamp((int)br + (int)variation, 0, 255);
                uint8_t tg = std::clamp((int)bg + (int)variation, 0, 255);
                uint8_t tb = std::clamp((int)bb + (int)variation, 0, 255);
                // Note: terrain_vertices is passed by reference or global, but since const, need to modify draw signature
                // For now, keep individual draws but optimized
            }

            // Keep shadows as before for now
            auto shadow_points = renderer.calculateHexagonPoints(cx + 3, cy + 3, hex_size);
            std::vector<std::pair<float, float>> shadow_point_pairs;
            for (auto& p : shadow_points) shadow_point_pairs.emplace_back(p.x, p.y);
            renderer.drawConvexShape(shadow_point_pairs, 0, 0, 0, 100);

            // Draw filled hexagon as pizza slices
            auto points = renderer.calculateHexagonPoints(cx, cy, hex_size);
            for (int i = 0; i < 6; ++i) {
                sf::ConvexShape triangle(3);
                triangle.setPoint(0, sf::Vector2f(cx, cy));
                triangle.setPoint(1, points[i]);
                triangle.setPoint(2, points[(i + 1) % 6]);
                float variation = (i % 3) * 10.0f - 10.0f;
                uint8_t tr = std::clamp((int)br + (int)variation, 0, 255);
                uint8_t tg = std::clamp((int)bg + (int)variation, 0, 255);
                uint8_t tb = std::clamp((int)bb + (int)variation, 0, 255);
                triangle.setFillColor(sf::Color(tr, tg, tb));
                renderer.getWindow()->draw(triangle);
            }

            // Shine and shadow triangles
            uint8_t sr = std::min(255, (int)(br + 80));
            uint8_t sg = std::min(255, (int)(bg + 80));
            uint8_t sb = std::min(255, (int)(bb + 80));
            uint8_t shr = (uint8_t)(br * 0.3f);
            uint8_t shg = (uint8_t)(bg * 0.3f);
            uint8_t shb = (uint8_t)(bb * 0.3f);

            sf::ConvexShape shine1(3);
            shine1.setPoint(0, sf::Vector2f(cx, cy));
            shine1.setPoint(1, points[0]);
            shine1.setPoint(2, points[1]);
            shine1.setFillColor(sf::Color(sr, sg, sb));
            renderer.getWindow()->draw(shine1);

            sf::ConvexShape shine2(3);
            shine2.setPoint(0, sf::Vector2f(cx, cy));
            shine2.setPoint(1, points[1]);
            shine2.setPoint(2, points[2]);
            shine2.setFillColor(sf::Color(sr, sg, sb));
            renderer.getWindow()->draw(shine2);

            sf::ConvexShape shadow1(3);
            shadow1.setPoint(0, sf::Vector2f(cx, cy));
            shadow1.setPoint(1, points[3]);
            shadow1.setPoint(2, points[4]);
            shadow1.setFillColor(sf::Color(shr, shg, shb));
            renderer.getWindow()->draw(shadow1);

            sf::ConvexShape shadow2(3);
            shadow2.setPoint(0, sf::Vector2f(cx, cy));
            shadow2.setPoint(1, points[4]);
            shadow2.setPoint(2, points[5]);
            shadow2.setFillColor(sf::Color(shr, shg, shb));
            renderer.getWindow()->draw(shadow2);

            // Add wavy texture for water
            if (type == WATER) {
                std::mt19937 wave_gen(q * 1000 + r_coord);
                std::uniform_real_distribution<float> offset_dist(-hex_size * 0.5f, hex_size * 0.5f);
                std::uniform_real_distribution<float> size_dist(0.6f, 1.4f);

                int num_back_waves = 15; // Background subtle waves
                int num_front_waves = 12; // Foreground lighter waves
                float wave_radius = hex_size * 0.35f;

                // Draw background subtle waves with base colors (darker, more diffused)
                for (int i = 0; i < num_back_waves; ++i) {
                    float offset_x = offset_dist(wave_gen);
                    float offset_y = offset_dist(wave_gen);
                    float size_mult = size_dist(wave_gen);
                    float radius = wave_radius * size_mult;

                    // Slightly darker than base for subtle depth
                    int color_var = (wave_gen() % 20) - 10;
                    uint8_t wave_r = std::clamp((int)(br * 0.8f) + color_var, 0, 255);
                    uint8_t wave_g = std::clamp((int)(bg * 0.8f) + color_var, 0, 255);
                    uint8_t wave_b = std::clamp((int)(bb * 0.9f) + color_var, 0, 255);
                    uint8_t alpha = 40 + (wave_gen() % 60); // Very diffused

                    renderer.drawCircle(cx + offset_x, cy + offset_y, radius, wave_r, wave_g, wave_b, alpha);
                }

                // Draw foreground lighter waves for highlights
                for (int i = 0; i < num_front_waves; ++i) {
                    float offset_x = offset_dist(wave_gen);
                    float offset_y = offset_dist(wave_gen);
                    float size_mult = size_dist(wave_gen);
                    float radius = wave_radius * size_mult * 0.8f; // Slightly smaller

                    // Lighter blue variants for wave highlights
                    int color_var = (wave_gen() % 40) - 10;
                    uint8_t wave_r = std::clamp((int)br + color_var, 0, 255);
                    uint8_t wave_g = std::clamp((int)bg + color_var, 0, 255);
                    uint8_t wave_b = std::clamp((int)bb + color_var + 20, 0, 255); // More blue
                    uint8_t alpha = 60 + (wave_gen() % 80); // More diffused

                    renderer.drawCircle(cx + offset_x, cy + offset_y, radius, wave_r, wave_g, wave_b, alpha);
                }
            }

            // Add earthy texture for soil
            if (type == SOIL) {
                std::mt19937 soil_gen(q * 1000 + r_coord);
                std::uniform_real_distribution<float> offset_dist(-hex_size * 0.5f, hex_size * 0.5f);
                std::uniform_real_distribution<float> size_dist(0.5f, 1.5f);

                int num_dark_patches = 18; // Dark soil patches
                int num_light_patches = 10; // Lighter dirt spots
                float patch_radius = hex_size * 0.3f;

                // Draw dark patches using shadow colors for depth
                for (int i = 0; i < num_dark_patches; ++i) {
                    float offset_x = offset_dist(soil_gen);
                    float offset_y = offset_dist(soil_gen);
                    float size_mult = size_dist(soil_gen);
                    float radius = patch_radius * size_mult;

                    // Use shadow colors for darker soil patches
                    int color_var = (soil_gen() % 30) - 15;
                    uint8_t patch_r = std::clamp((int)shr + color_var, 0, 255);
                    uint8_t patch_g = std::clamp((int)shg + color_var, 0, 255);
                    uint8_t patch_b = std::clamp((int)shb + color_var, 0, 255);
                    uint8_t alpha = 80 + (soil_gen() % 100); // Semi-transparent

                    renderer.drawCircle(cx + offset_x, cy + offset_y, radius, patch_r, patch_g, patch_b, alpha);
                }

                // Draw lighter patches for variation
                for (int i = 0; i < num_light_patches; ++i) {
                    float offset_x = offset_dist(soil_gen);
                    float offset_y = offset_dist(soil_gen);
                    float size_mult = size_dist(soil_gen);
                    float radius = patch_radius * size_mult * 0.7f; // Smaller

                    // Lighter brown variants
                    int color_var = (soil_gen() % 30);
                    uint8_t patch_r = std::clamp((int)br + color_var, 0, 255);
                    uint8_t patch_g = std::clamp((int)bg + color_var, 0, 255);
                    uint8_t patch_b = std::clamp((int)bb + color_var, 0, 255);
                    uint8_t alpha = 50 + (soil_gen() % 80); // More diffused

                    renderer.drawCircle(cx + offset_x, cy + offset_y, radius, patch_r, patch_g, patch_b, alpha);
                }
            }

            // Add rocky, jagged texture for rocks
            if (type == ROCK) {
                std::mt19937 rock_gen(q * 1000 + r_coord);
                std::uniform_real_distribution<float> offset_dist(-hex_size * 0.5f, hex_size * 0.5f);
                std::uniform_real_distribution<float> length_dist(hex_size * 0.1f, hex_size * 0.4f);
                std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318f); // 0 to 2*PI

                int num_dark_lines = 20; // Dark cracks/lines
                int num_light_lines = 12; // Light edge highlights
                int num_dots = 25; // Small rocky dots

                // Draw dark jagged lines for cracks and depth
                for (int i = 0; i < num_dark_lines; ++i) {
                    float x1 = cx + offset_dist(rock_gen);
                    float y1 = cy + offset_dist(rock_gen);
                    float length = length_dist(rock_gen);
                    float angle = angle_dist(rock_gen);
                    float x2 = x1 + length * std::cos(angle);
                    float y2 = y1 + length * std::sin(angle);

                    // Dark lines using shadow colors
                    int color_var = (rock_gen() % 20) - 10;
                    uint8_t line_r = std::clamp((int)shr + color_var, 0, 255);
                    uint8_t line_g = std::clamp((int)shg + color_var, 0, 255);
                    uint8_t line_b = std::clamp((int)shb + color_var, 0, 255);
                    uint8_t alpha = 120 + (rock_gen() % 100);

                    renderer.drawLine(x1, y1, x2, y2, line_r, line_g, line_b, alpha, 1.5f);
                }

                // Draw lighter lines for highlights
                for (int i = 0; i < num_light_lines; ++i) {
                    float x1 = cx + offset_dist(rock_gen);
                    float y1 = cy + offset_dist(rock_gen);
                    float length = length_dist(rock_gen) * 0.6f;
                    float angle = angle_dist(rock_gen);
                    float x2 = x1 + length * std::cos(angle);
                    float y2 = y1 + length * std::sin(angle);

                    // Lighter grey highlights
                    int color_var = (rock_gen() % 40);
                    uint8_t line_r = std::clamp((int)br + color_var, 0, 255);
                    uint8_t line_g = std::clamp((int)bg + color_var, 0, 255);
                    uint8_t line_b = std::clamp((int)bb + color_var, 0, 255);
                    uint8_t alpha = 80 + (rock_gen() % 80);

                    renderer.drawLine(x1, y1, x2, y2, line_r, line_g, line_b, alpha, 1.0f);
                }

                // Draw small rocky dots for texture
                for (int i = 0; i < num_dots; ++i) {
                    float dot_x = cx + offset_dist(rock_gen);
                    float dot_y = cy + offset_dist(rock_gen);
                    float dot_radius = 0.5f + (rock_gen() % 20) * 0.1f; // 0.5-2.5 pixels

                    // Mix of dark and light dots
                    bool is_dark = (rock_gen() % 2) == 0;
                    int color_var = (rock_gen() % 30) - 15;
                    uint8_t dot_r, dot_g, dot_b;
                    if (is_dark) {
                        dot_r = std::clamp((int)shr + color_var, 0, 255);
                        dot_g = std::clamp((int)shg + color_var, 0, 255);
                        dot_b = std::clamp((int)shb + color_var, 0, 255);
                    } else {
                        dot_r = std::clamp((int)br + color_var + 20, 0, 255);
                        dot_g = std::clamp((int)bg + color_var + 20, 0, 255);
                        dot_b = std::clamp((int)bb + color_var + 20, 0, 255);
                    }
                    uint8_t alpha = 100 + (rock_gen() % 120);

                    renderer.drawCircle(dot_x, dot_y, dot_radius, dot_r, dot_g, dot_b, alpha);
                }
            }

            // Create irregular overlap where soil meets water
            if (type == SOIL) {
                for (int edge = 0; edge < 6; ++edge) {
                    auto [nq, nr] = get_neighbor_coords(q, r_coord, edge);
                    auto neighbor_it = terrainTiles.find({nq, nr});

                    if (neighbor_it != terrainTiles.end() && neighbor_it->second.type == WATER) {
                        // Soil meets water - create irregular dirt invasion
                        sf::Vector2f p1 = points[edge];
                        sf::Vector2f p2 = points[(edge + 1) % 6];

                        // Use edge-specific seed for consistent but varied patterns
                        std::mt19937 edge_gen(q * 10000 + r_coord * 100 + edge);
                        std::uniform_real_distribution<float> offset_dist(0.0f, 1.0f);
                        std::uniform_real_distribution<float> size_dist(0.6f, 1.2f);
                        std::uniform_real_distribution<float> extend_dist(0.2f, 0.6f);

                        // Draw 5-8 irregular soil patches along this edge
                        int num_patches = 5 + (edge_gen() % 4);
                        for (int i = 0; i < num_patches; ++i) {
                            // Position along the edge
                            float t = offset_dist(edge_gen);
                            float edge_x = p1.x + (p2.x - p1.x) * t;
                            float edge_y = p1.y + (p2.y - p1.y) * t;

                            // Calculate direction towards water (perpendicular to edge, outward)
                            float dx = p2.x - p1.x;
                            float dy = p2.y - p1.y;
                            float edge_len = std::sqrt(dx * dx + dy * dy);
                            float perp_x = -dy / edge_len; // Perpendicular direction
                            float perp_y = dx / edge_len;

                            // Extend into water tile
                            float extension = extend_dist(edge_gen) * hex_size;
                            float patch_x = edge_x + perp_x * extension;
                            float patch_y = edge_y + perp_y * extension;

                            // Draw soil patch
                            float radius = (2.0f + offset_dist(edge_gen) * 3.0f) * size_dist(edge_gen);
                            int color_var = (edge_gen() % 20) - 10;
                            uint8_t patch_r = std::clamp((int)br + color_var, 0, 255);
                            uint8_t patch_g = std::clamp((int)bg + color_var, 0, 255);
                            uint8_t patch_b = std::clamp((int)bb + color_var, 0, 255);
                            uint8_t alpha = 150 + (edge_gen() % 80);

                            renderer.drawCircle(patch_x, patch_y, radius, patch_r, patch_g, patch_b, alpha);
                        }
                    }
                }
            }

            // Smooth edges between tiles of the same terrain type
            for (int edge = 0; edge < 6; ++edge) {
                auto [nq, nr] = get_neighbor_coords(q, r_coord, edge);
                auto neighbor_it = terrainTiles.find({nq, nr});

                if (neighbor_it != terrainTiles.end() && neighbor_it->second.type == type) {
                    // Same terrain type - draw blending edge
                    sf::Vector2f p1 = points[edge];
                    sf::Vector2f p2 = points[(edge + 1) % 6];

                    // Use averaged color between this tile and center for smooth blend
                    uint8_t blend_r = (br + base_r) / 2;
                    uint8_t blend_g = (bg + base_g) / 2;
                    uint8_t blend_b = (bb + base_b) / 2;

                    renderer.drawLine(p1.x, p1.y, p2.x, p2.y, blend_r, blend_g, blend_b, 180, 2.0f);
                }
            }
        }


    }
}

std::pair<int, int> HexGrid::get_neighbor_coords(int q, int r, int direction) const {
    auto [dq, dr] = directions[direction % 6];
    return {q + dq, r + dr};
}



// ============================================================================
// END OF HEX GRID CLASS IMPLEMENTATION
// ============================================================================

const std::vector<std::pair<int, int>> HexGrid::directions = {
    {0, -1},  // top
    {1, -1},  // upper-right
    {1, 0},   // lower-right
    {0, 1},   // bottom
    {-1, 1},  // lower-left
    {-1, 0}   // upper-left
};

TerrainType HexGrid::get_terrain_type(int q, int r) const {
    auto it = terrainTiles.find({q, r});
    if (it != terrainTiles.end()) {
        return it->second.type;
    }
    return SOIL; // Default
}

Plant* HexGrid::get_plant(int q, int r) {
    auto it = plants.find({q, r});
    if (it != plants.end()) {
        return &it->second;
    }
    return nullptr;
}

void HexGrid::remove_plant(int q, int r) {
    plants.erase({q, r});
}

float HexGrid::calculate_visibility(sf::Color hare_color, TerrainType terrain) {
    sf::Color ground_color;
    switch (terrain) {
        case SOIL: ground_color = SOIL_COLOR; break;
        case ROCK: ground_color = ROCK_COLOR; break;
        case WATER: ground_color = WATER_COLOR; break;
        default: ground_color = SOIL_COLOR; break;
    }

    // Euclidean distance in RGB space, normalized
    float dr = hare_color.r - ground_color.r;
    float dg = hare_color.g - ground_color.g;
    float db = hare_color.b - ground_color.b;
    float distance = std::sqrt(dr*dr + dg*dg + db*db);
    float max_distance = std::sqrt(3 * 255*255); // Max possible distance
    float visibility = distance / max_distance;

    // Grey hares are harder to spot on rocks
    if (terrain == ROCK) {
        sf::Color grey(128, 128, 128);
        float grey_distance = std::sqrt(
            (hare_color.r - grey.r)*(hare_color.r - grey.r) +
            (hare_color.g - grey.g)*(hare_color.g - grey.g) +
            (hare_color.b - grey.b)*(hare_color.b - grey.b)
        ) / max_distance;
        visibility *= (0.5f + grey_distance * 0.5f); // Reduce visibility for grey hares
    }

    return std::clamp(visibility, 0.0f, 1.0f);
}





// ============================================================================
// END OF HEX GRID CLASS IMPLEMENTATION
// ============================================================================







// ============================================================================
// END OF HEX GRID CLASS IMPLEMENTATION
// ============================================================================

