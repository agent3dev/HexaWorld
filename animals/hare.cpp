#include "hare.hpp"
#include <iostream>
#include <algorithm>

void Hare::update_positions(HexGrid& grid) {
    if (is_burrowing) return; // Don't move while burrowing

    auto [x, y] = grid.axial_to_pixel(q, r);
    target_pos = sf::Vector2f(x, y);
}

sf::Color Hare::getColor() const {
    // Base color modified by genome
    sf::Color color = base_color;
    // Fear makes them paler
    color.r = std::min(255, (int)(color.r + (1.0f - genome.fear) * 50));
    color.g = std::min(255, (int)(color.g + (1.0f - genome.fear) * 50));
    color.b = std::min(255, (int)(color.b + (1.0f - genome.fear) * 50));
    // Weight makes them darker
    color.r = std::max(0, (int)(color.r - (genome.weight - 1.0f) * 50));
    color.g = std::max(0, (int)(color.g - (genome.weight - 1.0f) * 50));
    color.b = std::max(0, (int)(color.b - (genome.weight - 1.0f) * 50));
    // Burrowing makes them grey
    if (is_burrowing) {
        color = sf::Color(128, 128, 128);
    }
    return color;
}

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
        // Gain energy gradually while eating
        energy += delta_time * 0.25f;  // 0.5f over 2 seconds
        energy = std::min(2.0f, energy);
        if (eating_timer <= 0) {
            Plant* plant = grid.get_plant(q, r);
            if (plant && plant->stage >= SPROUT) {
                grid.remove_plant(q, r);
            }
            is_eating = false;
        }
        return; // Don't move while eating
    }

    // Small time-based energy decay
    energy -= delta_time * 0.004f;  // Even slower decay
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
        std::cout << "Hare pregnant, energy was " << energy << ", reset to 3.0" << std::endl;
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
            if (grid.has_hexagon(nq, nr) && grid.hare_positions.find({nq, nr}) == grid.hare_positions.end()) {
                TerrainType terr = grid.get_terrain_type(nq, nr);
                if (std::find(current_allowed.begin(), current_allowed.end(), terr) != current_allowed.end()) {
                    valid_dirs.push_back(dir);
                }
            }
        }

        if (!valid_dirs.empty()) {
            int old_q = q, old_r = r;
            // Avoid fire (always)
            std::vector<int> no_fire_dirs;
            for (int dir : valid_dirs) {
                auto [nq, nr] = grid.get_neighbor_coords(q, r, dir);
                if (grid.fire_timers.find({nq, nr}) == grid.fire_timers.end()) {
                    no_fire_dirs.push_back(dir);
                }

                if (!no_fire_dirs.empty()) {
                    valid_dirs = no_fire_dirs;
                }
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

            const std::vector<int>* chosen_dirs = &valid_dirs;
            // Prioritize water when extremely thirsty
            if (!water_dirs.empty() && thirst < 0.2f) {
                chosen_dirs = &water_dirs;
            } else if (!avoid_dirs.empty()) {
                chosen_dirs = &avoid_dirs;
            }
            int chosen_dir = (*chosen_dirs)[0];
            move(chosen_dir);
            grid.hare_positions.erase({old_q, old_r});
            grid.hare_positions.insert({q, r});
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
        std::cout << "Hare died at (" << q << ", " << r << "), starved" << std::endl;
    }

    // Check for death by dehydration
    if (thirst <= 0.0f && !is_dead) {
        is_dead = true;
        // Increase nutrients on soil
        auto it = grid.terrainTiles.find({q, r});
        if (it != grid.terrainTiles.end() && it->second.type == SOIL) {
            it->second.nutrients = std::min(1.0f, it->second.nutrients + 0.3f);
        }
        std::cout << "Hare died of dehydration at (" << q << ", " << r << ")" << std::endl;
    }
}

bool Hare::eat(HexGrid& grid) {
    Plant* plant = grid.get_plant(q, r);
    if (plant && plant->stage >= SPROUT) {  // Allow eating sprouts too
        is_eating = true;
        eating_timer = 2.0f;
        return true;
    }
    return false;
}