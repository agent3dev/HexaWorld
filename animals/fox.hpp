#pragma once

#include "hex_grid_new.hpp"
#include "ga.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <random>

struct Hare; // Forward declaration

struct Fox : public HexObject {
    std::vector<TerrainType> allowed_terrains = {SOIL, ROCK}; // No water
    float energy = 3.5f;
    float thirst = 1.0f; // Hydration level (1.0 = fully hydrated, 0.0 = dehydrated)
    sf::Color base_color = sf::Color(255, 140, 0); // Orange
    bool is_dead = false;
    float digestion_time = 0.0f;
    float move_timer = 0.0f;
    FoxGenome genome;
    float pregnancy_timer = 0.0f;
    bool is_pregnant = false;
    bool ready_to_give_birth = false;
    float speed = 2.5f;
    sf::Vector2f current_pos;
    sf::Vector2f target_pos;

    Fox(int q, int r) : HexObject(q, r), genome() { update_speed(); }

    void update_speed() { speed = 3.0f - genome.weight; }
    void update_positions(HexGrid& grid);

    // Get color (fixed for foxes)
    sf::Color getColor() const { return base_color; }

    // Update behavior: hunt and eat hares
    void update(HexGrid& grid, std::vector<Hare>& hares, const std::vector<Fox>& foxes, float delta_time, std::mt19937& rng);

    // Try to hunt a nearby hare
    bool hunt(HexGrid& grid, std::vector<Hare>& hares, const std::vector<Fox>& foxes);
};