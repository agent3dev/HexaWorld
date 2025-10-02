#pragma once

#include <random>

namespace hexaworld {

const float HEX_SIZE = 18.0f;
const float SQRT3 = 1.73205080757f;

// Animation speeds for terrain types
const float ROCK_ANIM_SPEED = 1.5f;
const float SOIL_ANIM_SPEED = 3.0f;
const float WATER_ANIM_SPEED = 6.0f;

// Random generator for repeatable simulation
extern std::mt19937 gen;

} // namespace hexaworld
