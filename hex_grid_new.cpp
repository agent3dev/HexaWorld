#include "hex_grid_new.hpp"
#include "constants.hpp"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>

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
            int rand_val = dis(hexaworld::gen);
            if (rand_val < 2) { // 20% rock
                type = ROCK;
            } else if (rand_val < 6) { // 40% soil
                type = SOIL;
            } else {
                type = WATER;
            }
        } else {
                // 30% chance to be random anyway
                if (rand_prob(hexaworld::gen) < 0.3) {
                    // Random with some rocks
                    std::uniform_int_distribution<> dis(0, 9);
                    int rand_val = dis(hexaworld::gen);
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
        float nutrients = std::clamp(base_nutrients + static_cast<float>(nutrient_var(hexaworld::gen)), 0.0f, 1.0f);

        terrainTiles.insert({{q, r}, TerrainTile(q, r, type, nutrients)});

        // Spawn plant on soil with chance
        if (type == SOIL && (hexaworld::gen() % 100) < 10) { // 10% chance
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

// ============================================================================
// HARE IMPLEMENTATION
// ============================================================================

void Hare::update(HexGrid& grid, float delta_time) {
    if (is_dead) return;  // Already dead

    // Small time-based energy decay
    energy -= delta_time * 0.01f;
    energy = std::max(0.0f, energy);

    // Digest
    digestion_time -= delta_time;

    // Eat if on plant
    bool ate = eat(grid);
    if (ate) {
        digestion_time = 5.0f;  // 5 seconds digestion
    }

    // Poop after digestion
    if (digestion_time <= 0.0f && digestion_time > -1.0f) {  // Trigger once
        auto it = grid.terrainTiles.find({q, r});
        if (it != grid.terrainTiles.end() && it->second.type == SOIL) {
            grid.plants.insert({{q, r}, Plant(q, r, SEED, it->second.nutrients)});
        }
        digestion_time = -1.0f;  // Prevent repeat
    }

    // Update allowed terrains: in panic mode (low energy), can go to rocks
    allowed_terrains = {SOIL};
    if (energy < 0.3f) {
        allowed_terrains.push_back(ROCK);
    }

    // Move if energy allows and cooldown passed
    move_timer += delta_time;
    if (move_timer >= 0.5f && energy > 0.2f) {
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
            // Prefer directions with edible plants (not seeds)
            std::vector<int> plant_dirs;
            for (int dir : valid_dirs) {
                auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                Plant* p = grid.get_plant(nq, nr);
                if (p && p->stage != SEED) {
                    plant_dirs.push_back(dir);
                }
            }
            int chosen_dir = plant_dirs.empty() ? valid_dirs[hexaworld::gen() % valid_dirs.size()] : plant_dirs[hexaworld::gen() % plant_dirs.size()];
            move(chosen_dir);
            energy -= 0.02f; // Lower energy cost per move
            move_timer = 0.0f; // Reset timer on move

            // Check for drowning in water
            TerrainType current_terr = grid.get_terrain_type(q, r);
            if (current_terr == WATER) {
                consecutive_water_moves++;
                if (consecutive_water_moves > 2) {
                    is_dead = true;
                    std::cout << "Hare drowned at (" << q << ", " << r << ")" << std::endl;
                }
            } else {
                consecutive_water_moves = 0;
            }
        }
    }

    // Check for death
    if (energy <= 0.0f && !is_dead) {
        is_dead = true;
        // Increase nutrients on soil
        auto it = grid.terrainTiles.find({q, r});
        if (it != grid.terrainTiles.end() && it->second.type == SOIL) {
            it->second.nutrients = std::min(1.0f, it->second.nutrients + 0.2f);
        }
        std::cout << "Hare died at (" << q << ", " << r << ")" << std::endl;
    }
}

bool Hare::eat(HexGrid& grid) {
    Plant* plant = grid.get_plant(q, r);
    if (plant && plant->stage != SEED) {  // Cannot eat seeds
        // Gain energy based on stage
        switch (plant->stage) {
            case SPROUT: energy += 0.3f; break;
            case PLANT: energy += 0.5f; break;
            default: break;
        }
        // No cap here; allow energy to exceed 1.5 for reproduction
        grid.remove_plant(q, r);
        return true;
    }
    return false;
}

// ============================================================================
// END OF HEX GRID CLASS IMPLEMENTATION
// ============================================================================

} // namespace hexaworld