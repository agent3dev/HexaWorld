#pragma once

#include "hex_grid_new.hpp"
#include "ga.hpp"
#include "fox.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <random>

struct Hare : public HexObject {
    std::vector<TerrainType> allowed_terrains = {SOIL, ROCK}; // Allow rocks, water only when thirsty
    float energy = 1.0f;
    float thirst = 1.0f; // Hydration level (1.0 = fully hydrated, 0.0 = dehydrated)
    sf::Color base_color = sf::Color(210, 180, 140); // Khaki
    bool is_dead = false;
    float digestion_time = 0.0f;
    float move_timer = 0.0f;
    int consecutive_water_moves = 0;
    HareGenome genome;
    float pregnancy_timer = 0.0f;
    bool is_pregnant = false;
    bool ready_to_give_birth = false;
    float speed = 1.0f;
    sf::Vector2f current_pos;
    sf::Vector2f target_pos;
    bool is_burrowing = false;
    float eating_timer = 0.0f;
    bool is_eating = false;

    Hare(int q, int r) : HexObject(q, r), genome() { update_speed(); }

    void update_speed() { speed = 2.0f - genome.weight; }
    void update_positions(HexGrid& grid);

    // Get color based on genome
    sf::Color getColor() const;

    // Update behavior: move and eat if possible
    void update(HexGrid& grid, const std::vector<Fox>& foxes, float delta_time, std::mt19937& rng);

    // Eat plant at current position
    bool eat(HexGrid& grid);
};