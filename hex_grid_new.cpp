#include "hex_grid_new.hpp"
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
// HARE IMPLEMENTATION
// ============================================================================

void Hare::update(HexGrid& grid, const std::vector<Fox>& foxes, float delta_time, std::mt19937& rng) {
    if (is_dead) return;  // Already dead

    // Update positions
    update_positions(grid);

    // Interpolate position
    const float anim_speed = 50.0f; // pixels per second, slow motion
    sf::Vector2f diff = target_pos - current_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (dist > 0.1f) {
        sf::Vector2f dir = diff / dist;
        current_pos += dir * anim_speed * delta_time;
        if ((target_pos - current_pos).length() < anim_speed * delta_time) current_pos = target_pos;
    }

    // Handle eating
    if (is_eating) {
        eating_timer -= delta_time;
        if (eating_timer <= 0) {
            Plant* plant = grid.get_plant(q, r);
            if (plant && plant->stage == PLANT) {
                energy += 0.5f;
                energy = std::min(2.0f, energy);
                grid.remove_plant(q, r);
            }
            is_eating = false;
        }
        return; // Don't move while eating
    }

    // Small time-based energy decay
    energy -= delta_time * 0.008f;  // Slower decay for longer lifespan
    energy = std::max(0.0f, energy);

    // Thirst decay over time
    thirst -= delta_time * 0.008f; // Slower decay for longer lifespan
    thirst = std::max(0.0f, thirst);

    // Drink if on water edge (foxes don't swim but drink at edges)
    if (grid.get_terrain_type(q, r) == WATER) {
        thirst = std::min(1.0f, thirst + delta_time * 0.5f); // Quick rehydration
    }

    // Digest
    digestion_time -= delta_time;

    // Eat if possible
    if (!is_eating && digestion_time <= 0.0f) {
        if (eat(grid)) {
            digestion_time = 2.0f;
            is_eating = true;
            eating_timer = 2.0f;
        }
    }

    // Handle pregnancy
    if (energy > genome.reproduction_threshold && !is_pregnant) {
        is_pregnant = true;
        pregnancy_timer = 20.0f; // Longer pregnancy
        std::cout << "Fox pregnant, energy was " << energy << ", reset to 3.0" << std::endl;
        energy = 3.0f; // Reset energy
    }

    if (is_pregnant) {
        pregnancy_timer -= delta_time;
        if (pregnancy_timer <= 0.0f) {
            ready_to_give_birth = true;
            is_pregnant = false;
        }
    }

    // Allow water when very thirsty
    std::vector<TerrainType> current_allowed = allowed_terrains;
    if (thirst < 0.3f) {
        current_allowed.push_back(WATER);
    }

    // Move if energy allows and cooldown passed
    move_timer += delta_time;
    if (move_timer >= 0.4f && energy > 0.0f) {  // Keep hunting even on low energy
        // Find valid directions
        std::vector<int> valid_dirs;
        for (int dir = 0; dir < 6; ++dir) {
            auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
            if (grid.has_hexagon(nq, nr)) {
                TerrainType terr = grid.get_terrain_type(nq, nr);
                if (std::find(current_allowed.begin(), current_allowed.end(), terr) != current_allowed.end()) {
                    valid_dirs.push_back(dir);
                }
            }
        }

        if (!valid_dirs.empty()) {
            // Avoid fire (always)
            std::vector<int> no_fire_dirs;
            for (int dir : valid_dirs) {
                auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                if (grid.fire_timers.find({nq, nr}) == grid.fire_timers.end()) {
                    no_fire_dirs.push_back(dir);
                }
            }
            if (!no_fire_dirs.empty()) {
                valid_dirs = no_fire_dirs;
            }

            // Prefer directions away from visible foxes within vision range
            std::vector<int> avoid_dirs;
            const int vision_range = 3; // Hexes away
            int closest_fox_q = -1000, closest_fox_r = -1000;
            int min_dist = vision_range + 1;
            for (const auto& fox : foxes) {
                if (fox.is_dead) continue;
                int dq = fox.q - q;
                int dr = fox.r - r;
                int dist = (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2; // Hex distance
                if (dist <= vision_range && dist > 0 && dist < min_dist) {
                    TerrainType terr = grid.get_terrain_type(fox.q, fox.r);
                    float visibility = HexGrid::calculate_visibility(fox.getColor(), terr);
                    if (visibility > 0.1f) {
                        min_dist = dist;
                        closest_fox_q = fox.q;
                        closest_fox_r = fox.r;
                    }
                }
            }
            if (closest_fox_q != -1000) {
                // Move away from the closest visible fox
                for (int dir : valid_dirs) {
                    auto [nnq, nnr] = grid.get_neighbor_coords(q, r, dir);
                    int new_dq = closest_fox_q - nnq;
                    int new_dr = closest_fox_r - nnr;
                    int new_dist = (std::abs(new_dq) + std::abs(new_dr) + std::abs(new_dq + new_dr)) / 2;
                    if (new_dist > min_dist) {
                        avoid_dirs.push_back(dir);
                    }
                }
            }

            // Prefer directions with water if very thirsty
            std::vector<int> water_dirs;
            if (thirst < 0.3f) {
                for (int dir : valid_dirs) {
                    auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                    if (grid.get_terrain_type(nq, nr) == WATER) {
                        water_dirs.push_back(dir);
                    }
                }
            }

            std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
            const std::vector<int>* chosen_dirs = &valid_dirs;
            // Prioritize water when extremely thirsty
            if (!water_dirs.empty() && thirst < 0.2f) {
                chosen_dirs = &water_dirs;
            } else if (!avoid_dirs.empty() && prob_dist(rng) < genome.fear) {
                chosen_dirs = &avoid_dirs;
            }
            int chosen_dir = (*chosen_dirs)[gen() % chosen_dirs->size()];
            move(chosen_dir);
            energy -= 0.05f / genome.movement_efficiency; // Energy cost, modified by efficiency
            move_timer = 0.0f;

        }
    }

    // Check for death
    if (energy <= 0.0f && !is_dead) {
        is_dead = true;
        // Increase nutrients on soil
        auto it = grid.terrainTiles.find({q, r});
        if (it != grid.terrainTiles.end() && it->second.type == SOIL) {
            it->second.nutrients = std::min(1.0f, it->second.nutrients + 0.3f);
        }
        std::cout << "Fox died at (" << q << ", " << r << "), starved" << std::endl;
    }

    // Check for death by dehydration
    if (thirst <= 0.0f && !is_dead) {
        is_dead = true;
        // Increase nutrients on soil
        auto it = grid.terrainTiles.find({q, r});
        if (it != grid.terrainTiles.end() && it->second.type == SOIL) {
            it->second.nutrients = std::min(1.0f, it->second.nutrients + 0.3f);
        }
        std::cout << "Fox died of dehydration at (" << q << ", " << r << ")" << std::endl;
    }
}

bool Fox::hunt(HexGrid& grid, std::vector<Hare>& hares, const std::vector<Fox>& foxes) {
    // First, check if there's a hare on the same hex (automatic catch)
    for (auto it = hares.begin(); it != hares.end(); ++it) {
        if (!it->is_dead && it->q == q && it->r == r) {
            float gained = it->energy;
            energy += gained;
            energy = std::min(7.0f, energy);
            hares.erase(it);
            std::cout << "Fox caught hare at (" << q << ", " << r << "), gained " << gained << " energy, now " << energy << std::endl;
            return true;
        }
    }
    // Check adjacent hexes for catchable hares
    for (int dir = 0; dir < 6; ++dir) {
        auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
        for (auto it = hares.begin(); it != hares.end(); ++it) {
            if (!it->is_dead && it->q == nq && it->r == nr) {
                TerrainType terr = grid.get_terrain_type(nq, nr);
                float visibility = it->is_burrowing ? 0.0f : HexGrid::calculate_visibility(it->getColor(), terr);
                // Pack bonus: count nearby foxes
                int nearby_foxes = 0;
                for (const auto& fox : foxes) {
                    if (!fox.is_dead && std::abs(fox.q - q) <= 1 && std::abs(fox.r - r) <= 1 && !(fox.q == q && fox.r == r)) {
                        nearby_foxes++;
                    }
                }
                float pack_bonus = 1.0f + nearby_foxes * 0.2f; // 20% per nearby fox
                if (visibility * pack_bonus > 0.3f && speed > it->speed) {
                    // Catch and eat the hare
                    energy += it->energy; // Gain the hare's energy
            energy = std::min(6.0f, energy);
                    // Remove hare
                    hares.erase(it);
                    std::cout << "Fox caught hare at (" << nq << ", " << nr << ")" << std::endl;
                    return true;
                }
                break; // Only one hare per hex
            }
        }
    }
    return false;
}

// ============================================================================
// HARE AND FOX POSITION UPDATE
// ============================================================================

void Hare::update_positions(HexGrid& grid) {
    auto [x, y] = grid.axial_to_pixel(q, r);
    target_pos = sf::Vector2f(x, y);
    if (current_pos == sf::Vector2f(0, 0)) current_pos = target_pos;
}

void Fox::update_positions(HexGrid& grid) {
    auto [x, y] = grid.axial_to_pixel(q, r);
    target_pos = sf::Vector2f(x, y);
    if (current_pos == sf::Vector2f(0, 0)) current_pos = target_pos;
}

// ============================================================================
// WOLF IMPLEMENTATION
// ============================================================================

void Wolf::update(HexGrid& grid, std::vector<Hare>& hares, std::vector<Fox>& foxes, float delta_time, std::mt19937& rng) {
    if (is_dead) return;  // Already dead

    // Update positions
    update_positions(grid);

    // Interpolate position
    const float anim_speed = 200.0f; // pixels per second
    sf::Vector2f diff = target_pos - current_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (dist > 0.1f) {
        sf::Vector2f dir = diff / dist;
        current_pos += dir * anim_speed * delta_time;
        if ((target_pos - current_pos).length() < anim_speed * delta_time) current_pos = target_pos;
    }

    // Small time-based energy decay
    energy -= delta_time * 0.01f;  // Slower decay
    energy = std::max(0.0f, energy);

    // Thirst decay over time
    thirst -= delta_time * 0.009f; // Slower than foxes
    thirst = std::max(0.0f, thirst);

    // Drink if on water edge (wolves don't swim but drink at edges)
    if (grid.get_terrain_type(q, r) == WATER) {
        thirst = std::min(1.0f, thirst + delta_time * 0.5f); // Quick rehydration
    }

    // Digest
    digestion_time -= delta_time;

    // Hunt if possible
    bool hunted = hunt(grid, hares, foxes);
    if (hunted) {
        digestion_time = 15.0f;  // Longer digestion
    }

    // Handle pregnancy
    if (energy > genome.reproduction_threshold && !is_pregnant) {
        is_pregnant = true;
        pregnancy_timer = 25.0f; // Longer pregnancy
        energy = 4.0f; // Reset energy
    }

    if (is_pregnant) {
        pregnancy_timer -= delta_time;
        if (pregnancy_timer <= 0.0f) {
            ready_to_give_birth = true;
            is_pregnant = false;
        }
    }

    // Allow water when very thirsty
    std::vector<TerrainType> current_allowed = allowed_terrains;
    if (thirst < 0.3f) {
        current_allowed.push_back(WATER);
    }

    // Move if energy allows and cooldown passed
    move_timer += delta_time;
    if (move_timer >= 0.6f && energy > 2.0f) {  // Slower movement
        // Find valid directions
        std::vector<int> valid_dirs;
        for (int dir = 0; dir < 6; ++dir) {
            auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
            if (grid.has_hexagon(nq, nr)) {
                TerrainType terr = grid.get_terrain_type(nq, nr);
                if (std::find(current_allowed.begin(), current_allowed.end(), terr) != current_allowed.end()) {
                    valid_dirs.push_back(dir);
                }
            }
        }

        if (!valid_dirs.empty()) {
            // Avoid fire (always)
            std::vector<int> no_fire_dirs;
            for (int dir : valid_dirs) {
                auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                if (grid.fire_timers.find({nq, nr}) == grid.fire_timers.end()) {
                    no_fire_dirs.push_back(dir);
                }
            }
            if (!no_fire_dirs.empty()) {
                valid_dirs = no_fire_dirs;
            }

            // Prefer directions with visible prey
            std::vector<int> prey_dirs;
            const int vision_range = 4; // Better vision
            int closest_prey_q = -1000, closest_prey_r = -1000;
            int min_dist = vision_range + 1;
            // Check hares
            for (const auto& hare : hares) {
                if (hare.is_dead || hare.is_burrowing) continue;
                int dq = hare.q - q;
                int dr = hare.r - r;
                int dist = (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
                if (dist <= vision_range && dist > 0 && dist < min_dist) {
                    TerrainType terr = grid.get_terrain_type(hare.q, hare.r);
                    float visibility = HexGrid::calculate_visibility(hare.getColor(), terr);
                    if (visibility > 0.1f) {
                        min_dist = dist;
                        closest_prey_q = hare.q;
                        closest_prey_r = hare.r;
                    }
                }
            }
            // Check foxes
            for (const auto& fox : foxes) {
                if (fox.is_dead) continue;
                int dq = fox.q - q;
                int dr = fox.r - r;
                int dist = (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
                if (dist <= vision_range && dist > 0 && dist < min_dist) {
                    TerrainType terr = grid.get_terrain_type(fox.q, fox.r);
                    float visibility = HexGrid::calculate_visibility(fox.getColor(), terr);
                    if (visibility > 0.1f) {
                        min_dist = dist;
                        closest_prey_q = fox.q;
                        closest_prey_r = fox.r;
                    }
                }
            }
            if (closest_prey_q != -1000) {
                // Move towards the closest visible prey
                for (int dir : valid_dirs) {
                    auto [nnq, nnr] = grid.get_neighbor_coords(q, r, dir);
                    int new_dq = closest_prey_q - nnq;
                    int new_dr = closest_prey_r - nnr;
                    int new_dist = (std::abs(new_dq) + std::abs(new_dr) + std::abs(new_dq + new_dr)) / 2;
                    if (new_dist < min_dist) {
                        prey_dirs.push_back(dir);
                    }
                }
            }

            // Prefer directions with water if very thirsty
            std::vector<int> water_dirs;
            if (thirst < 0.3f) {
                for (int dir : valid_dirs) {
                    auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                    if (grid.get_terrain_type(nq, nr) == WATER) {
                        water_dirs.push_back(dir);
                    }
                }
            }

            std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
            const std::vector<int>* chosen_dirs = &valid_dirs;
            // Prioritize water when extremely thirsty
            if (!water_dirs.empty() && thirst < 0.2f) {
                chosen_dirs = &water_dirs;
            } else if (!prey_dirs.empty() && prob_dist(rng) < genome.hunting_aggression) {
                chosen_dirs = &prey_dirs;
            }
            int chosen_dir = (*chosen_dirs)[gen() % chosen_dirs->size()];
            move(chosen_dir);
            energy -= 0.08f / genome.movement_efficiency; // Higher cost
            move_timer = 0.0f;
        }
    }

    // Check for death
    if (energy <= 0.0f && !is_dead) {
        is_dead = true;
        // Increase nutrients
        auto it = grid.terrainTiles.find({q, r});
        if (it != grid.terrainTiles.end() && it->second.type == SOIL) {
            it->second.nutrients = std::min(1.0f, it->second.nutrients + 0.4f);
        }
        std::cout << "Wolf died at (" << q << ", " << r << ")" << std::endl;
    }

    // Check for death by dehydration
    if (thirst <= 0.0f && !is_dead) {
        is_dead = true;
        // Increase nutrients
        auto it = grid.terrainTiles.find({q, r});
        if (it != grid.terrainTiles.end() && it->second.type == SOIL) {
            it->second.nutrients = std::min(1.0f, it->second.nutrients + 0.4f);
        }
        std::cout << "Wolf died of dehydration at (" << q << ", " << r << ")" << std::endl;
    }
}

bool Wolf::hunt(HexGrid& grid, std::vector<Hare>& hares, std::vector<Fox>& foxes) {
    // First, check if there's prey on the same hex
    for (auto it = hares.begin(); it != hares.end(); ++it) {
        if (!it->is_dead && it->q == q && it->r == r) {
            energy += it->energy;
            energy = std::min(8.0f, energy);
            hares.erase(it);
            std::cout << "Wolf caught hare at (" << q << ", " << r << ")" << std::endl;
            return true;
        }
    }
    for (auto it = foxes.begin(); it != foxes.end(); ++it) {
        if (!it->is_dead && it->q == q && it->r == r) {
            energy += it->energy;
            energy = std::min(8.0f, energy);
            foxes.erase(it);
            std::cout << "Wolf caught fox at (" << q << ", " << r << ")" << std::endl;
            return true;
        }
    }
    // Check adjacent hexes
    for (int dir = 0; dir < 6; ++dir) {
        auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
        for (auto it = hares.begin(); it != hares.end(); ++it) {
            if (!it->is_dead && it->q == nq && it->r == nr && !it->is_burrowing) {
                TerrainType terr = grid.get_terrain_type(nq, nr);
                float visibility = HexGrid::calculate_visibility(it->getColor(), terr);
                if (visibility > 0.2f && speed > it->speed) {  // Better vision
                    energy += it->energy;
                    energy = std::min(8.0f, energy);
                    hares.erase(it);
                    std::cout << "Wolf caught hare at (" << nq << ", " << nr << ")" << std::endl;
                    return true;
                }
                break;
            }
        }
        for (auto it = foxes.begin(); it != foxes.end(); ++it) {
            if (!it->is_dead && it->q == nq && it->r == nr) {
                TerrainType terr = grid.get_terrain_type(nq, nr);
                float visibility = HexGrid::calculate_visibility(it->getColor(), terr);
                if (visibility > 0.2f && speed > it->speed) {
                    energy += it->energy;
                    energy = std::min(8.0f, energy);
                    foxes.erase(it);
                    std::cout << "Wolf caught fox at (" << nq << ", " << nr << ")" << std::endl;
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

void Wolf::update_positions(HexGrid& grid) {
    auto [x, y] = grid.axial_to_pixel(q, r);
    target_pos = sf::Vector2f(x, y);
    if (current_pos == sf::Vector2f(0, 0)) current_pos = target_pos;
}

// ============================================================================
// SALMON IMPLEMENTATION
// ============================================================================

void Salmon::update_positions(HexGrid& grid) {
    auto [x, y] = grid.axial_to_pixel(q, r);
    target_pos = sf::Vector2f(x, y);
    if (current_pos == sf::Vector2f(0, 0)) current_pos = target_pos;
}

void Salmon::update(HexGrid& grid, float delta_time, std::mt19937& rng) {
    if (is_dead) return;

    // Update positions
    update_positions(grid);

    // Interpolate position
    const float anim_speed = 150.0f; // pixels per second
    sf::Vector2f diff = target_pos - current_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (dist > 0.1f) {
        sf::Vector2f dir = diff / dist;
        current_pos += dir * anim_speed * delta_time;
        if ((target_pos - current_pos).length() < anim_speed * delta_time) current_pos = target_pos;
    }

    // Small energy decay
    energy -= delta_time * 0.005f;
    energy = std::max(0.0f, energy);

    // Move if timer allows
    move_timer += delta_time;
    if (move_timer >= 1.0f && energy > 0.0f) {
        // Find valid directions (water only)
        std::vector<int> valid_dirs;
        for (int dir = 0; dir < 6; ++dir) {
            auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
            if (grid.has_hexagon(nq, nr) && grid.get_terrain_type(nq, nr) == WATER) {
                valid_dirs.push_back(dir);
            }
        }

        if (!valid_dirs.empty()) {
            // Avoid fire (always)
            std::vector<int> no_fire_dirs;
            for (int dir : valid_dirs) {
                auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                if (grid.fire_timers.find({nq, nr}) == grid.fire_timers.end()) {
                    no_fire_dirs.push_back(dir);
                }
            }
            if (!no_fire_dirs.empty()) {
                valid_dirs = no_fire_dirs;
            }

            std::uniform_int_distribution<> dis(0, valid_dirs.size() - 1);
            int dir = valid_dirs[dis(rng)];
            auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
            q = nq;
            r = nr;
            move_timer = 0.0f;
        }
    }

    // Reproduction
    if (energy > reproduction_threshold) {
        ready_to_give_birth = true;
    }

    // Die if no energy
    if (energy <= 0.0f) {
        is_dead = true;
    }
}

bool Hare::eat(HexGrid& grid) {
    Plant* plant = grid.get_plant(q, r);
    if (plant && plant->stage == PLANT) {
        return true;
    }
    return false;
}

sf::Color Hare::getColor() const {
    return base_color;
}

// ============================================================================
// FOX IMPLEMENTATION
// ============================================================================

void Fox::update(HexGrid& grid, std::vector<Hare>& hares, const std::vector<Fox>& foxes, float delta_time, std::mt19937& rng) {
    if (is_dead) return;  // Already dead

    // Update positions
    update_positions(grid);

    // Interpolate position
    const float anim_speed = 50.0f; // pixels per second, slow motion
    sf::Vector2f diff = target_pos - current_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (dist > 0.1f) {
        sf::Vector2f dir = diff / dist;
        current_pos += dir * anim_speed * delta_time;
        if ((target_pos - current_pos).length() < anim_speed * delta_time) current_pos = target_pos;
    }

    // Small time-based energy decay
    energy -= delta_time * 0.008f;  // Slower decay for longer lifespan
    energy = std::max(0.0f, energy);

    // Thirst decay over time
    thirst -= delta_time * 0.008f; // Slower decay for longer lifespan
    thirst = std::max(0.0f, thirst);

    // Drink if on water edge (foxes don't swim but drink at edges)
    if (grid.get_terrain_type(q, r) == WATER) {
        thirst = std::min(1.0f, thirst + delta_time * 0.5f); // Quick rehydration
    }

    // Digest
    digestion_time -= delta_time;

    // Hunt if possible
    bool hunted = hunt(grid, hares, foxes);
    if (hunted) {
        digestion_time = 10.0f;  // 10 seconds digestion
    }

    // Handle pregnancy
    if (energy > genome.reproduction_threshold && !is_pregnant) {
        is_pregnant = true;
        pregnancy_timer = 20.0f; // 20 seconds pregnancy
        energy = 1.5f; // Reset energy
    }

    if (is_pregnant) {
        pregnancy_timer -= delta_time;
        if (pregnancy_timer <= 0.0f) {
            ready_to_give_birth = true;
            is_pregnant = false;
        }
    }

    // Move if energy allows and cooldown passed
    move_timer += delta_time;
    if (move_timer >= 0.4f && energy > 0.0f) {  // Keep hunting even on low energy
        // Find valid directions
        std::vector<int> valid_dirs;
        for (int dir = 0; dir < 6; ++dir) {
            auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
            if (grid.has_hexagon(nq, nr)) {
                TerrainType terr = grid.get_terrain_type(nq, nr);
                if (std::find(allowed_terrains.begin(), allowed_terrains.end(), terr) != allowed_terrains.end()) {
                    valid_dirs.push_back(dir);
                }
            }
        }

        if (!valid_dirs.empty()) {
            // Avoid fire (always)
            std::vector<int> no_fire_dirs;
            for (int dir : valid_dirs) {
                auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                if (grid.fire_timers.find({nq, nr}) == grid.fire_timers.end()) {
                    no_fire_dirs.push_back(dir);
                }
            }
            if (!no_fire_dirs.empty()) {
                valid_dirs = no_fire_dirs;
            }

            // Prefer directions towards visible hares within vision range
            std::vector<int> hare_dirs;
            const int vision_range = 3; // Hexes away
            int closest_hare_q = -1000, closest_hare_r = -1000;
            int min_dist = vision_range + 1;
            for (const auto& hare : hares) {
                if (hare.is_dead) continue;
                int dq = hare.q - q;
                int dr = hare.r - r;
                int dist = (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2; // Hex distance
                if (dist <= vision_range && dist > 0 && dist < min_dist) {
                    TerrainType terr = grid.get_terrain_type(hare.q, hare.r);
                    float visibility = hare.is_burrowing ? 0.0f : HexGrid::calculate_visibility(hare.getColor(), terr);
                    if (visibility > 0.1f) {
                        min_dist = dist;
                        closest_hare_q = hare.q;
                        closest_hare_r = hare.r;
                    }
                }
            }
            if (closest_hare_q != -1000) {
                // Move towards the closest visible hare
                for (int dir : valid_dirs) {
                    auto [nnq, nnr] = grid.get_neighbor_coords(q, r, dir);
                    int new_dq = closest_hare_q - nnq;
                    int new_dr = closest_hare_r - nnr;
                    int new_dist = (std::abs(new_dq) + std::abs(new_dr) + std::abs(new_dq + new_dr)) / 2;
                    if (new_dist < min_dist) {
                        hare_dirs.push_back(dir);
                    }
                }
            }

            // Prefer directions with water if very thirsty
            std::vector<int> water_dirs;
            if (thirst < 0.3f) {
                for (int dir : valid_dirs) {
                    auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                    if (grid.get_terrain_type(nq, nr) == WATER) {
                        water_dirs.push_back(dir);
                    }
                }
            }

            std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
            const std::vector<int>* chosen_dirs = &valid_dirs;
            // Prioritize water when extremely thirsty
            if (!water_dirs.empty() && thirst < 0.2f) {
                chosen_dirs = &water_dirs;
            } else if (!hare_dirs.empty() && prob_dist(rng) < genome.hunting_aggression) {
                chosen_dirs = &hare_dirs;
            }
            int chosen_dir = (*chosen_dirs)[gen() % chosen_dirs->size()];
            move(chosen_dir);
            energy -= 0.05f / genome.movement_efficiency; // Energy cost, modified by efficiency
            move_timer = 0.0f;
        }
    }

    // Check for death
    if (energy <= 0.0f && !is_dead) {
        is_dead = true;
        // Increase nutrients on soil
        auto it = grid.terrainTiles.find({q, r});
        if (it != grid.terrainTiles.end() && it->second.type == SOIL) {
            it->second.nutrients = std::min(1.0f, it->second.nutrients + 0.3f);
        }
        std::cout << "Fox died at (" << q << ", " << r << "), starved" << std::endl;
    }

    // Check for death by dehydration
    if (thirst <= 0.0f && !is_dead) {
        is_dead = true;
        // Increase nutrients on soil
        auto it = grid.terrainTiles.find({q, r});
        if (it != grid.terrainTiles.end() && it->second.type == SOIL) {
            it->second.nutrients = std::min(1.0f, it->second.nutrients + 0.3f);
        }
        std::cout << "Fox died of dehydration at (" << q << ", " << r << ")" << std::endl;
    }
}

// ============================================================================
// END OF HEX GRID CLASS IMPLEMENTATION
// ============================================================================

