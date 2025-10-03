#include "wolf.hpp"
#include "hare.hpp"
#include "fox.hpp"
#include <iostream>
#include <algorithm>

void Wolf::update_positions(HexGrid& grid) {
    auto [x, y] = grid.axial_to_pixel(q, r);
    target_pos = sf::Vector2f(x, y);
    if (current_pos == sf::Vector2f(0, 0)) current_pos = target_pos;
}

bool Wolf::hunt(HexGrid& grid, std::vector<Hare>& hares, std::vector<Fox>& foxes) {
    // First, check if there's a hare on the same hex (automatic catch)
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
                    if (visibility > 0.2f) {
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
                    if (visibility > 0.2f) {
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
            int chosen_dir = (*chosen_dirs)[rng() % chosen_dirs->size()];
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