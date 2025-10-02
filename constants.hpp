#pragma once

#include <random>
#include <SFML/Graphics.hpp>

namespace hexaworld {

const float HEX_SIZE = 12.0f;
const float SQRT3 = 1.73205080757f;

// Animation speeds for terrain types
const float ROCK_ANIM_SPEED = 1.5f;
const float SOIL_ANIM_SPEED = 3.0f;
const float WATER_ANIM_SPEED = 6.0f;

// Ground colors for visibility calculation
const sf::Color SOIL_COLOR(139, 69, 19);   // Brown
const sf::Color ROCK_COLOR(128, 128, 128); // Grey
const sf::Color WATER_COLOR(0, 100, 200);  // Blue

// Seed for random generator
const unsigned int RANDOM_SEED = 444;

// Random generator for repeatable simulation
extern std::mt19937 gen;

} // namespace hexaworld
