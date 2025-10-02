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

float HexGrid::calculate_visibility(sf::Color hare_color, TerrainType terrain) {
    sf::Color ground_color;
    switch (terrain) {
        case SOIL: ground_color = hexaworld::SOIL_COLOR; break;
        case ROCK: ground_color = hexaworld::ROCK_COLOR; break;
        case WATER: ground_color = hexaworld::WATER_COLOR; break;
        default: ground_color = hexaworld::SOIL_COLOR; break;
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
    const float anim_speed = 200.0f; // pixels per second
    sf::Vector2f diff = target_pos - current_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (dist > 0.1f) {
        sf::Vector2f dir = diff / dist;
        current_pos += dir * anim_speed * delta_time;
        if ((target_pos - current_pos).length() < anim_speed * delta_time) current_pos = target_pos;
    }

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

    // Update allowed terrains: can go to rocks; fearless can go to water
    allowed_terrains = {SOIL, ROCK};
    if (genome.fear < 0.5f) {
        allowed_terrains.push_back(WATER);
    }

    // Burrowing logic
    if (genome.can_burrow && grid.get_terrain_type(q, r) == SOIL) {
        bool foxes_nearby = false;
        for (const auto& fox : foxes) {
            if (!fox.is_dead) {
                int dq = fox.q - q;
                int dr = fox.r - r;
                int dist = (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
                if (dist <= 2) {
                    foxes_nearby = true;
                    break;
                }
            }
        }
        if (foxes_nearby || energy < 0.3f) {
            is_burrowing = true;
        } else if (!foxes_nearby && energy > 0.5f) {
            is_burrowing = false;
        }
    } else {
        is_burrowing = false;
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
            // Fear: avoid directions with visible foxes
            std::vector<int> safe_dirs = valid_dirs;
            if (genome.fear > 0.7f) {
                safe_dirs.clear();
                for (int dir : valid_dirs) {
                    auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                    bool has_visible_fox = false;
                    for (const auto& fox : foxes) {
                        if (!fox.is_dead && fox.q == nq && fox.r == nr) {
                            TerrainType terr = grid.get_terrain_type(nq, nr);
                            float visibility = HexGrid::calculate_visibility(fox.getColor(), terr);
                            if (visibility > 0.3f) {
                                has_visible_fox = true;
                                break;
                            }
                        }
                    }
                    if (!has_visible_fox) safe_dirs.push_back(dir);
                }
                if (safe_dirs.empty()) safe_dirs = valid_dirs; // If no safe, use all
            }

            // Prefer directions with edible plants (not seeds)
            std::vector<int> plant_dirs;
            for (int dir : safe_dirs) {
                auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                Plant* p = grid.get_plant(nq, nr);
                if (p && p->stage != SEED) {
                    plant_dirs.push_back(dir);
                }
            }
            // Use genome to decide movement strategy
            std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
            float prob = prob_dist(rng);
            const std::vector<int>* chosen_dirs = &safe_dirs;
            if (!plant_dirs.empty() && prob < genome.movement_aggression) {
                chosen_dirs = &plant_dirs;
            }
            int chosen_dir = (*chosen_dirs)[hexaworld::gen() % chosen_dirs->size()];
            TerrainType prev_terr = grid.get_terrain_type(q, r);
            move(chosen_dir);
            TerrainType current_terr = grid.get_terrain_type(q, r);
            energy -= 0.02f / genome.movement_efficiency; // Energy cost per move, modified by efficiency
            move_timer = 0.0f; // Reset timer on move

            // Log entering water
            if (prev_terr != WATER && current_terr == WATER) {
                std::cout << "Hare jumped to water at (" << q << ", " << r << "), fear=" << genome.fear << std::endl;
            }

            // Check for drowning in water
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

    // Handle pregnancy
    if (energy > genome.reproduction_threshold && !is_pregnant) {
        is_pregnant = true;
        pregnancy_timer = 10.0f; // 10 seconds pregnancy
        energy = 0.75f; // Reset energy
    }

    if (is_pregnant) {
        pregnancy_timer -= delta_time;
        if (pregnancy_timer <= 0.0f) {
            ready_to_give_birth = true;
            is_pregnant = false;
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
        // std::cout << "Hare died at (" << q << ", " << r << ")" << std::endl;
    }
}

sf::Color Hare::getColor() const {
    // Quantize genome values to get similar colors for similar genomes
    int thresh_bin = std::clamp(static_cast<int>((genome.reproduction_threshold - 1.0f) / 1.0f * 4), 0, 3);
    int aggression_bin = std::clamp(static_cast<int>(genome.movement_aggression * 3), 0, 2);
    int weight_bin = std::clamp(static_cast<int>((genome.weight - 0.5f) / 1.0f * 2), 0, 1);
    int index = thresh_bin * 6 + aggression_bin * 2 + weight_bin;

    // Palette of colors: interpolated grays, browns, pinks, white
    static const std::vector<sf::Color> palette = {
        // Grays
        sf::Color(128, 128, 128), // Gray
        sf::Color(149, 149, 149), // Mid gray 1
        sf::Color(169, 169, 169), // Dark gray
        sf::Color(190, 190, 190), // Mid gray 2
        sf::Color(211, 211, 211), // Light gray
        // Browns
        sf::Color(139, 69, 19),   // Brown
        sf::Color(150, 76, 32),   // Brown mid 1
        sf::Color(160, 82, 45),   // Sienna
        sf::Color(183, 108, 54),  // Brown mid 2
        sf::Color(205, 133, 63),  // Peru
        // Pinks and white
        sf::Color(255, 192, 203), // Soft pink
        sf::Color(255, 187, 198), // Pink mid 1
        sf::Color(255, 182, 193), // Light pink
        sf::Color(255, 219, 219), // Pink mid 2
        sf::Color(255, 255, 255)  // White
    };

    return palette[index % palette.size()];
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
// FOX IMPLEMENTATION
// ============================================================================

void Fox::update(HexGrid& grid, std::vector<Hare>& hares, const std::vector<Fox>& foxes, float delta_time, std::mt19937& rng) {
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
    energy -= delta_time * 0.012f;  // Slightly faster decay
    energy = std::max(0.0f, energy);
    // Debug: log energy occasionally
    if (hexaworld::gen() % 1000 == 0) std::cout << "Fox energy: " << energy << std::endl;

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

            std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
            const std::vector<int>* chosen_dirs = &valid_dirs;
            if (!hare_dirs.empty() && prob_dist(rng) < genome.hunting_aggression) {
                chosen_dirs = &hare_dirs;
            }
            int chosen_dir = (*chosen_dirs)[hexaworld::gen() % chosen_dirs->size()];
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
// END OF HEX GRID CLASS IMPLEMENTATION
// ============================================================================

} // namespace hexaworld