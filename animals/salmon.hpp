#pragma once

#include "hex_grid_new.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <random>

struct Salmon : public HexObject {
    std::vector<TerrainType> allowed_terrains = {WATER}; // Only water
    float energy = 1.0f;
    sf::Color base_color = sf::Color(255, 100, 100); // Light red
    bool is_dead = false;
    float digestion_time = 0.0f;
    float move_timer = 0.0f;
    // Simple genome for now
    float reproduction_threshold = 2.0f;
    float pregnancy_timer = 0.0f;
    bool is_pregnant = false;
    bool ready_to_give_birth = false;
    float speed = 1.0f;
    sf::Vector2f current_pos;
    sf::Vector2f target_pos;

    Salmon(int q, int r) : HexObject(q, r) {}

    void update_positions(HexGrid& grid);

    // Get color (fixed for salmons)
    sf::Color getColor() const { return base_color; }

    // Update behavior: swim and reproduce
    void update(HexGrid& grid, float delta_time, std::mt19937& rng);
};