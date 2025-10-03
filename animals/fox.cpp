#include "fox.hpp"
#include "hare.hpp"
#include <iostream>
#include <algorithm>

void Fox::update_positions(HexGrid& grid) {
    auto [x, y] = grid.axial_to_pixel(q, r);
    target_pos = sf::Vector2f(x, y);
    if (current_pos == sf::Vector2f(0, 0)) current_pos = target_pos;
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
            int chosen_dir = (*chosen_dirs)[rng() % chosen_dirs->size()];
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