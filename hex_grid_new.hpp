#pragma once

#include "sfml_renderer.hpp"
#include "ga.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <random>

// Terrain types
enum TerrainType {
    SOIL,
    WATER,
    ROCK
};

// Terrain tile class
struct TerrainTile {
    int q, r;
    TerrainType type;
    float nutrients; // 0.0 to 1.0, affects plant growth likelihood and quality
    TerrainTile(int q, int r, TerrainType type, float nutrients) : q(q), r(r), type(type), nutrients(nutrients) {}
};

// Plant stages
enum PlantStage { SEED, SPROUT, PLANT, CHARRED };

// Plant class
struct Plant {
    int q, r;
    PlantStage stage;
    float growth_time;
    float drop_time;
    float nutrients; // cached from tile
    Plant(int q, int r, PlantStage stage, float nutrients) : q(q), r(r), stage(stage), growth_time(0.0f), drop_time(0.0f), nutrients(nutrients) {}
};

// ============================================================================
// HEX GRID CLASS - Manages hexagonal grid with proper neighbor relationships
// ============================================================================

class HexGrid {
public:
    float hex_size;
    std::map<std::pair<int, int>, std::pair<float, float>> hexagons;  // (q,r) -> (x,y)
    std::map<std::pair<int, int>, TerrainTile> terrainTiles;  // (q,r) -> tile
    std::map<std::pair<int, int>, Plant> plants;  // (q,r) -> plant
    std::map<std::pair<int, int>, float> fire_timers;  // (q,r) -> time left burning
    std::vector<sf::Vector2f> hexagon_points;  // Cached points for size 1.0
    static const std::vector<std::pair<int, int>> directions;

    HexGrid(float size) : hex_size(size) {
        // Cache hexagon points for size 1.0
        hexagon_points.resize(6);
        for (int i = 0; i < 6; ++i) {
            float angle = i * 3.14159265359f / 3.0f; // 60 degrees in radians
            hexagon_points[i] = sf::Vector2f(std::cos(angle), std::sin(angle));
        }
    }

    // Convert axial coordinates to pixel position
    std::pair<float, float> axial_to_pixel(int q, int r) const;

    // Add hexagon at axial coordinates (q, r)
    void add_hexagon(int q, int r);

    // Check if hexagon exists at coordinates
    bool has_hexagon(int q, int r) const;

    // Create neighbors of existing hexagons
    void create_neighbors();

    // Expand grid by one layer
    void expand_grid(int layers = 1);

    // Draw all hexagons
    void draw(SFMLRenderer& renderer, uint8_t r, uint8_t g, uint8_t b,
              uint8_t outline_r, uint8_t outline_g, uint8_t outline_b,
              float offset_x, float offset_y, int screen_width, int screen_height,
              float brightness_center_q = 0, float brightness_center_r = 0,
              bool has_alive_hares = true) const;

    // Get neighbor coordinates for a given direction
    std::pair<int, int> get_neighbor_coords(int q, int r, int direction) const;

    // Get terrain type at coordinates
    TerrainType get_terrain_type(int q, int r) const;

    // Get plant at coordinates (nullptr if none)
    Plant* get_plant(int q, int r);

    // Remove plant at coordinates
    void remove_plant(int q, int r);

    // Calculate visibility of hare on terrain (0 = invisible, 1 = fully visible)
    static float calculate_visibility(sf::Color hare_color, TerrainType terrain);

private:
};

// ============================================================================
// HEX OBJECT - Movable object on the grid
// ============================================================================

struct HexObject {
    int q, r;
    HexObject(int q, int r) : q(q), r(r) {}
    void move(int direction) {
        auto [dq, dr] = HexGrid::directions[direction % 6];
        q += dq;
        r += dr;
    }
};

// Forward declaration
struct Fox;

// ============================================================================
// HARE - Movable herbivore that eats plants
// ============================================================================

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

// ============================================================================
// FOX - Predator that hunts hares
// ============================================================================

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

// ============================================================================
// WOLF - Apex predator that hunts hares and foxes
// ============================================================================

struct Wolf : public HexObject {
    std::vector<TerrainType> allowed_terrains = {SOIL, ROCK}; // No water
    float energy = 5.0f;
    float thirst = 1.0f; // Hydration level (1.0 = fully hydrated, 0.0 = dehydrated)
    sf::Color base_color = sf::Color(64, 64, 64); // Dark grey
    bool is_dead = false;
    float digestion_time = 0.0f;
    float move_timer = 0.0f;
    WolfGenome genome;
    float pregnancy_timer = 0.0f;
    bool is_pregnant = false;
    bool ready_to_give_birth = false;
    float speed = 1.5f;
    sf::Vector2f current_pos;
    sf::Vector2f target_pos;

    Wolf(int q, int r) : HexObject(q, r), genome() { update_speed(); }

    void update_speed() { speed = 1.5f - genome.weight; } // Slower than foxes
    void update_positions(HexGrid& grid);

    // Get color (fixed for wolves)
    sf::Color getColor() const { return base_color; }

    // Update behavior: hunt and eat hares and foxes
    void update(HexGrid& grid, std::vector<Hare>& hares, std::vector<Fox>& foxes, float delta_time, std::mt19937& rng);

    // Try to hunt a nearby hare or fox
    bool hunt(HexGrid& grid, std::vector<Hare>& hares, std::vector<Fox>& foxes);
};

// ============================================================================
// SALMON - Fish that live in water
// ============================================================================

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

// ============================================================================
// END OF HEX GRID CLASS
// ============================================================================