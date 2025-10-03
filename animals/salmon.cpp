#include "salmon.hpp"
#include <algorithm>

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