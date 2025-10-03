#include "hex_grid_new.hpp"
#include "sfml_renderer.hpp"
#include "constants.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <random>
#include <SFML/Window.hpp>
#include <vector>
#include <cstdlib>
#include <string>
#include <utility>
#include <set>
#include <cmath>

std::pair<unsigned int, std::string> get_seed() {
    if (const char* env_seed = std::getenv("HEXAWORLD_SEED")) {
        try {
            return {std::stoi(env_seed), "HEXAWORLD_SEED"};
        } catch (const std::exception&) {
            // Fallback to constant if invalid
        }
    }
    return {RANDOM_SEED, "default"};
}

bool get_frameless() {
    if (const char* env = std::getenv("HEXAWORLD_FRAMELESS")) {
        std::string val(env);
        return !(val == "0" || val == "false");
    }
    return true; // Default to frameless
}

bool get_maximized() {
    if (const char* env = std::getenv("HEXAWORLD_MAXIMIZED")) {
        std::string val(env);
        return !(val == "0" || val == "false");
    }
    return true; // Default to maximized
}
auto [seed_val, source] = get_seed();
std::mt19937 gen(seed_val); // Fixed seed for repeatable simulation

int main() {
    try {
        unsigned int seed = seed_val;
        std::cout << "Using seed: " << seed << " (from " << source << ")" << std::endl;

        bool frameless = get_frameless();
        std::cout << "Frameless window: " << (frameless ? "yes" : "no") << " (set HEXAWORLD_FRAMELESS=0 to disable)" << std::endl;

        bool maximized = get_maximized();
        std::cout << "Maximized window: " << (maximized ? "yes" : "no") << " (set HEXAWORLD_MAXIMIZED=0 to disable)" << std::endl;

        // Create renderer in windowed mode with antialiasing
        SFMLRenderer renderer(1280, 1024, "HexaWorld - Hexagonal Grid", false, frameless, maximized, 4);
        renderer.setFramerateLimit(60); // Limit FPS to reduce CPU usage

        // Center the grid in the window
        float center_x = renderer.getWidth() / 2.0f;
        float center_y = renderer.getHeight() / 2.0f;

        // Create hex grid
        HexGrid hexGrid(HEX_SIZE);

        // Add center hexagon
        hexGrid.add_hexagon(0, 0);

        // Expand grid to fill screen by adding neighbors that fit fully
        const float sqrt3 = SQRT3;
        while (true) {
            size_t old_size = hexGrid.hexagons.size();
            hexGrid.create_neighbors();
            // Remove hexagons that are not fully visible
            for (auto it = hexGrid.hexagons.begin(); it != hexGrid.hexagons.end(); ) {
                auto [x, y] = it->second;
                float cx = x + center_x;
                float cy = y + center_y;
                float left = cx - HEX_SIZE;
                float right = cx + HEX_SIZE;
                float top = cy - HEX_SIZE * sqrt3 / 2.0f;
                float bottom = cy + HEX_SIZE * sqrt3 / 2.0f;
                if (left < 0 || right > renderer.getWidth() || top < 0 || bottom > renderer.getHeight()) {
                    it = hexGrid.hexagons.erase(it);
                } else {
                    ++it;
                }
            }
             if (hexGrid.hexagons.size() == old_size) break;
          }

          // Remove terrain and plants for removed hexagons
          for (auto it = hexGrid.terrainTiles.begin(); it != hexGrid.terrainTiles.end(); ) {
              if (hexGrid.hexagons.find(it->first) == hexGrid.hexagons.end()) {
                  it = hexGrid.terrainTiles.erase(it);
              } else {
                  ++it;
              }
          }
          for (auto it = hexGrid.plants.begin(); it != hexGrid.plants.end(); ) {
              if (hexGrid.hexagons.find(it->first) == hexGrid.hexagons.end()) {
                  it = hexGrid.plants.erase(it);
              } else {
                  ++it;
              }
          }

        // Remove isolated water tiles
       for (auto it = hexGrid.terrainTiles.begin(); it != hexGrid.terrainTiles.end(); ) {
             auto [q, r] = it->first;
             if (it->second.type == WATER) {
                 bool has_water_neighbor = false;
                 for (int dir = 0; dir < 6; ++dir) {
                     auto [nq, nr] = hexGrid.get_neighbor_coords(q, r, dir);
                     auto nit = hexGrid.terrainTiles.find({nq, nr});
                     if (nit != hexGrid.terrainTiles.end() && nit->second.type == WATER) {
                         has_water_neighbor = true;
                         break;
                     }
                 }
                 if (!has_water_neighbor) {
                     it = hexGrid.terrainTiles.erase(it);
                     continue;
                 }
             }
             ++it;
        }

        // Log terrain counts
        int soil_count = 0, water_count = 0, rock_count = 0;
        for (const auto& [coord, tile] : hexGrid.terrainTiles) {
            if (tile.type == SOIL) soil_count++;
            else if (tile.type == WATER) water_count++;
            else if (tile.type == ROCK) rock_count++;
        }
         std::cout << "Terrain: SOIL " << soil_count << ", WATER " << water_count << ", ROCK " << rock_count << std::endl;

          // Plants are now spawned in add_hexagon, but ensure some exist
         std::vector<std::pair<int, int>> soil_coords;
         // Add extra plants if needed
         for (const auto& [coord, tile] : hexGrid.terrainTiles) {
             if (tile.type == SOIL) {
                 soil_coords.push_back(coord);
             }
         }
          std::shuffle(soil_coords.begin(), soil_coords.end(), gen);
          size_t num_mature = std::min<size_t>(soil_coords.size() / 2, soil_coords.size());
          size_t num_sprouts = std::min<size_t>(soil_coords.size() / 4, soil_coords.size());
          // Place mature plants first
          for (size_t i = 0; i < num_mature; ++i) {
              auto [q, r] = soil_coords[i];
              hexGrid.plants.insert({{q, r}, Plant(q, r, PLANT, hexGrid.terrainTiles.at({q, r}).nutrients)});
          }
          // Then sprouts
          for (size_t i = num_mature; i < num_mature + num_sprouts; ++i) {
              auto [q, r] = soil_coords[i];
              hexGrid.plants.insert({{q, r}, Plant(q, r, SPROUT, hexGrid.terrainTiles.at({q, r}).nutrients)});
          }
            // Then seeds
            for (size_t i = num_mature + num_sprouts; i < soil_coords.size(); ++i) {
                auto [q, r] = soil_coords[i];
                hexGrid.plants.insert({{q, r}, Plant(q, r, SEED, hexGrid.terrainTiles.at({q, r}).nutrients)});
            }




          // Create a movable object
        HexObject obj(0, 0);
        auto last_move = std::chrono::steady_clock::now();
        bool showObject = false;

        // Dashboard toggle
        bool show_dashboard = true;

        // Hare logging
        bool enableHareLogging = true;
        auto last_log = std::chrono::steady_clock::now();

         // Population graph data
         std::vector<int> hare_history;
         std::vector<int> plant_history;
         std::vector<int> salmon_history;
         std::vector<int> fox_history;
         std::vector<int> wolf_history;
        float graph_timer = 0.0f;
        const float GRAPH_UPDATE_INTERVAL = 1.0f; // Update every second
        const size_t MAX_HISTORY = 1000;

        // Console logging
        float log_timer = 0.0f;
        const float LOG_INTERVAL = 10.0f; // Log every 10 seconds

        // Create hares on plant tiles
        std::vector<Hare> hares;
        std::vector<Salmon> salmons;
        std::vector<Wolf> wolves;
        std::vector<std::pair<int, int>> plant_coords;
        for (const auto& [coord, plant] : hexGrid.plants) {
            plant_coords.push_back(coord);
        }
        std::shuffle(plant_coords.begin(), plant_coords.end(), gen);
        size_t grid_size = hexGrid.hexagons.size();
        size_t num_hares = std::max<size_t>(10, grid_size / 500);
        num_hares = std::min(num_hares, plant_coords.size());
        for (size_t i = 0; i < num_hares; ++i) {
            auto [q, r] = plant_coords[i];
            hares.emplace_back(q, r);
            // Randomize genome for initial population
            std::uniform_real_distribution<float> thresh_dist(1.0f, 2.0f);
            std::uniform_real_distribution<float> aggression_dist(0.0f, 1.0f);
            std::uniform_real_distribution<float> weight_dist(0.5f, 1.5f);
            std::uniform_real_distribution<float> fear_dist(0.0f, 1.0f);
            std::uniform_real_distribution<float> efficiency_dist(0.5f, 1.5f);
            hares.back().genome.reproduction_threshold = thresh_dist(gen);
            hares.back().genome.movement_aggression = aggression_dist(gen);
            hares.back().genome.weight = weight_dist(gen);
            hares.back().genome.weight = weight_dist(gen);
            hares.back().genome.fear = fear_dist(gen);
            hares.back().genome.movement_efficiency = efficiency_dist(gen);
            hares.back().update_speed();
         }

         // Create salmons on water tiles
         std::vector<std::pair<int, int>> water_coords;
         for (const auto& [coord, tile] : hexGrid.terrainTiles) {
             if (tile.type == WATER) {
                 water_coords.push_back(coord);
             }
         }
         std::shuffle(water_coords.begin(), water_coords.end(), gen);
         size_t num_salmons = std::max<size_t>(5, grid_size / 2000);
         num_salmons = std::min(num_salmons, water_coords.size());
         for (size_t i = 0; i < num_salmons; ++i) {
             auto [q, r] = water_coords[i];
             salmons.emplace_back(q, r);
         }

         // Create foxes on soil tiles
        std::vector<Fox> foxes;
        std::vector<std::pair<int, int>> fox_soil_coords;
        for (const auto& [coord, tile] : hexGrid.terrainTiles) {
            if (tile.type == SOIL) {
                fox_soil_coords.push_back(coord);
            }
        }
        std::shuffle(fox_soil_coords.begin(), fox_soil_coords.end(), gen);
        size_t num_foxes = std::max<size_t>(5, grid_size / 1000);
        num_foxes = std::min(num_foxes, fox_soil_coords.size());
        for (size_t i = 0; i < num_foxes; ++i) {
            auto [q, r] = fox_soil_coords[i];
            foxes.emplace_back(q, r);
            // Randomize genome for initial population
            std::uniform_real_distribution<float> thresh_dist(2.0f, 4.0f);
            std::uniform_real_distribution<float> aggression_dist(0.0f, 1.0f);
            std::uniform_real_distribution<float> weight_dist(0.5f, 1.5f);
            std::uniform_real_distribution<float> efficiency_dist(0.5f, 1.5f);
            foxes.back().genome.reproduction_threshold = thresh_dist(gen);
            foxes.back().genome.hunting_aggression = aggression_dist(gen);
            foxes.back().genome.weight = weight_dist(gen);
            foxes.back().genome.movement_efficiency = efficiency_dist(gen);
             foxes.back().update_speed();
         }

        // Create wolves on soil tiles
        std::vector<std::pair<int, int>> wolf_soil_coords;
        for (const auto& [coord, tile] : hexGrid.terrainTiles) {
            if (tile.type == SOIL) {
                wolf_soil_coords.push_back(coord);
            }
        }
        std::shuffle(wolf_soil_coords.begin(), wolf_soil_coords.end(), gen);
        size_t num_wolves = std::max<size_t>(2, grid_size / 2000);
        num_wolves = std::min(num_wolves, wolf_soil_coords.size());
        for (size_t i = 0; i < num_wolves; ++i) {
            auto [q, r] = wolf_soil_coords[i];
            wolves.emplace_back(q, r);
            // Randomize genome for initial population
            std::uniform_real_distribution<float> thresh_dist(5.0f, 7.0f);
            std::uniform_real_distribution<float> aggression_dist(0.0f, 1.0f);
            std::uniform_real_distribution<float> weight_dist(0.5f, 1.5f);
            std::uniform_real_distribution<float> efficiency_dist(0.5f, 1.5f);
            wolves.back().genome.reproduction_threshold = thresh_dist(gen);
            wolves.back().genome.hunting_aggression = aggression_dist(gen);
            wolves.back().genome.weight = weight_dist(gen);
            wolves.back().genome.movement_efficiency = efficiency_dist(gen);
            wolves.back().update_speed();
        }

        // Initial brightness center on hares
        float brightness_center_q = 0, brightness_center_r = 0;
        int alive_count = 0;
        for (const auto& hare : hares) {
            if (hare.energy > 0) {
                brightness_center_q += hare.q;
                brightness_center_r += hare.r;
                alive_count++;
            }
        }
        bool has_alive_hares = alive_count > 0;
        if (has_alive_hares) {
            brightness_center_q /= alive_count;
            brightness_center_r /= alive_count;
        }

        // Main render loop
        while (!renderer.shouldClose()) {
            // Handle events
            renderer.pollEvent();

            // Log pressed keys
            static sf::Keyboard::Key lastLoggedKey = sf::Keyboard::Key::Unknown;
            sf::Keyboard::Key currentKey = renderer.getLastKey();
            if (currentKey != sf::Keyboard::Key::Unknown && currentKey != lastLoggedKey) {
                std::cout << "Key pressed: " << static_cast<int>(currentKey) << std::endl;
                lastLoggedKey = currentKey;
            }

            // Fallback ESC check
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) {
                break;
            }

            // Check for 'c' key to toggle object visibility
            if (renderer.getLastKey() == sf::Keyboard::Key::C) {
                showObject = !showObject;
            }

            // Check for 'd' key to toggle dashboard visibility
            static bool dPressed = false;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
                if (!dPressed) {
                    show_dashboard = !show_dashboard;
                    dPressed = true;
                }
            } else {
                dPressed = false;
            }

            // Check for 'f' key to start a fire (one-shot per press)
            static bool fPressed = false;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F)) {
                if (!fPressed) {
                    if (!hexGrid.plants.empty()) {
                        auto it = hexGrid.plants.begin();
                        std::advance(it, gen() % hexGrid.plants.size());
                        auto [fq, fr] = it->first;
                        hexGrid.fire_timers[{fq, fr}] = 5.0f;
                        std::cout << "Fire started at (" << fq << ", " << fr << ")" << std::endl;
                    }
                    fPressed = true;
                }
            } else {
                fPressed = false;
            }

            // Check for 'g' key to log hare genomes (one-shot per press)
            static bool gPressed = false;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::G)) {
                if (!gPressed) {
                    std::cout << "Current hare genomes:" << std::endl;
                    for (const auto& hare : hares) {
                        if (!hare.is_dead) {
                            std::cout << "Hare at (" << hare.q << "," << hare.r << "): reproduction_threshold = " << hare.genome.reproduction_threshold << ", movement_aggression = " << hare.genome.movement_aggression << ", weight = " << hare.genome.weight << ", speed = " << hare.speed << std::endl;
                        }
                    }
                    gPressed = true;
                }
            } else {
                gPressed = false;
            }

             // Update plant growth
             float dt = renderer.getDeltaTime();
             for (auto& [coord, plant] : hexGrid.plants) {
                 plant.growth_time += dt;

                 // Charred plants regrow after 30 seconds
                 if (plant.stage == CHARRED) {
                     if (plant.growth_time >= 30.0f) {
                         plant.stage = SEED;
                         plant.growth_time = 0.0f;
                     }
                     continue; // Skip normal growth processing
                 }

                 float threshold = 20.0f / (plant.nutrients + 0.1f); // Slower growth
                 if (plant.growth_time >= threshold) {
                     if (plant.stage < PLANT) {
                         plant.stage = static_cast<PlantStage>(plant.stage + 1);
                     }
                     plant.growth_time = 0.0f;
                 }

                 // Mature plants drop seeds randomly
                 if (plant.stage == PLANT) {
                     plant.drop_time += dt;
                     if (plant.drop_time >= 10.0f) { // Every 10 seconds
                         if ((gen() % 100) < 20) { // 20% chance
                             // Drop seeds in soil neighbors without plants
                             for (int dir = 0; dir < 6; ++dir) {
                                 auto [nq, nr] = hexGrid.get_neighbor_coords(plant.q, plant.r, dir);
                                 if (hexGrid.has_hexagon(nq, nr) &&
                                     hexGrid.get_terrain_type(nq, nr) == SOIL &&
                                     !hexGrid.get_plant(nq, nr)) {
                                     auto tit = hexGrid.terrainTiles.find({nq, nr});
                                     float nutrients = (tit != hexGrid.terrainTiles.end()) ? tit->second.nutrients : 0.5f;
                                     hexGrid.plants.insert({{nq, nr}, Plant(nq, nr, SEED, nutrients)});
                                 }
                             }
                         }
                         plant.drop_time = 0.0f;
                     }
                 }
             }

             // Update fires
             for (auto it = hexGrid.fire_timers.begin(); it != hexGrid.fire_timers.end(); ) {
                 it->second -= dt;
                 if (it->second <= 0) {
                     auto coord = it->first;
                     // Burn the plant - set to CHARRED state
                     auto plant_it = hexGrid.plants.find(coord);
                     if (plant_it != hexGrid.plants.end()) {
                         plant_it->second.stage = CHARRED;
                         plant_it->second.growth_time = 0.0f; // Reset growth timer
                     }
                     it = hexGrid.fire_timers.erase(it);
                 } else {
                     ++it;
                 }
             }

             // Start fire if many plants (very unlikely)
             if (hexGrid.plants.size() > 50 && (gen() % 10000) == 0) {
                 // Pick random plant
                 auto it = hexGrid.plants.begin();
                 std::advance(it, gen() % hexGrid.plants.size());
                 auto [q, r] = it->first;
                 hexGrid.fire_timers[{q, r}] = 5.0f;
             }

             // Spread fire to adjacent plants (every 2 seconds)
             static float fire_spread_timer = 0.0f;
             fire_spread_timer += dt;
             if (fire_spread_timer >= 2.0f) {
                 std::set<std::pair<int, int>> new_fires;
                 for (auto& [coord, timer] : hexGrid.fire_timers) {
                     auto [q, r] = coord;
                     for (int dir = 0; dir < 6; ++dir) {
                         auto [nq, nr] = hexGrid.get_neighbor_coords(q, r, dir);
                         auto neighbor_plant = hexGrid.plants.find({nq, nr});
                         // Only spread to non-charred plants that aren't already burning
                         if (neighbor_plant != hexGrid.plants.end() &&
                             neighbor_plant->second.stage != CHARRED &&
                             hexGrid.fire_timers.find({nq, nr}) == hexGrid.fire_timers.end()) {
                             new_fires.insert({nq, nr});
                         }
                     }
                 }
             for (auto& coord : new_fires) {
                 hexGrid.fire_timers[coord] = 5.0f;
             }
                 fire_spread_timer -= 2.0f; // or = 0.0f
             }

             // Update hares
             for (auto& hare : hares) {
                 hare.update(hexGrid, foxes, dt, gen);
             }

             // Update salmons
             for (auto& salmon : salmons) {
                 salmon.update(hexGrid, dt, gen);
             }

             // Update foxes
             for (auto& fox : foxes) {
                 fox.update(hexGrid, hares, foxes, dt, gen);
             }

             // Update wolves
             for (auto& wolf : wolves) {
                 wolf.update(hexGrid, hares, foxes, dt, gen);
             }

             // Animals die in fire
             for (auto& hare : hares) {
                 if (!hare.is_dead && hexGrid.fire_timers.find({hare.q, hare.r}) != hexGrid.fire_timers.end()) {
                     hare.is_dead = true;
                 }
             }
             for (auto& salmon : salmons) {
                 if (!salmon.is_dead && hexGrid.fire_timers.find({salmon.q, salmon.r}) != hexGrid.fire_timers.end()) {
                     salmon.is_dead = true;
                 }
             }
             for (auto& fox : foxes) {
                 if (!fox.is_dead && hexGrid.fire_timers.find({fox.q, fox.r}) != hexGrid.fire_timers.end()) {
                     fox.is_dead = true;
                 }
             }
             for (auto& wolf : wolves) {
                 if (!wolf.is_dead && hexGrid.fire_timers.find({wolf.q, wolf.r}) != hexGrid.fire_timers.end()) {
                     wolf.is_dead = true;
                 }
             }

             // Handle hare birth
             for (auto& hare : hares) {
                 if (hare.ready_to_give_birth) {
                     // Create offspring at same position
                     hares.push_back(Hare(hare.q, hare.r));
                     hares.back().genome = hare.genome.mutate(gen);
                     hares.back().energy = 0.5f; // Lower starting energy for evolutionary pressure
                     hare.ready_to_give_birth = false;
                     if (enableHareLogging) {
                         // std::cout << "Hare gave birth at (" << hare.q << ", " << hare.r << ")" << std::endl;
                     }
                 }
             }

             // Handle salmon birth
             for (auto& salmon : salmons) {
                 if (salmon.ready_to_give_birth) {
                     // Create offspring at same position
                     salmons.push_back(Salmon(salmon.q, salmon.r));
                     salmons.back().energy = 0.5f;
                     salmon.ready_to_give_birth = false;
                 }
             }

            // Handle fox birth
            for (auto& fox : foxes) {
                if (fox.ready_to_give_birth) {
                    // Create offspring at same position
                    foxes.push_back(Fox(fox.q, fox.r));
                    foxes.back().genome = fox.genome.mutate(gen);
                    foxes.back().energy = 3.0f; // Starting energy for offspring
                    fox.ready_to_give_birth = false;
                    std::cout << "Fox gave birth at (" << fox.q << ", " << fox.r << ")" << std::endl;
                }
            }

            // Handle wolf birth
            for (auto& wolf : wolves) {
                if (wolf.ready_to_give_birth) {
                    // Create offspring at same position
                    wolves.push_back(Wolf(wolf.q, wolf.r));
                    wolves.back().genome = wolf.genome.mutate(gen);
                    wolves.back().energy = 4.0f; // Starting energy for offspring
                    wolf.ready_to_give_birth = false;
                    std::cout << "Wolf gave birth at (" << wolf.q << ", " << wolf.r << ")" << std::endl;
                }
            }

             // Update population graph
             graph_timer += dt;
             if (graph_timer >= GRAPH_UPDATE_INTERVAL) {
                 hare_history.push_back(hares.size());
                 plant_history.push_back(hexGrid.plants.size());
                 salmon_history.push_back(salmons.size());
                 fox_history.push_back(foxes.size());
                 wolf_history.push_back(wolves.size());
                 if (hare_history.size() > MAX_HISTORY) {
                     hare_history.erase(hare_history.begin());
                     plant_history.erase(plant_history.begin());
                     salmon_history.erase(salmon_history.begin());
                     fox_history.erase(fox_history.begin());
                     wolf_history.erase(wolf_history.begin());
                 }
                 graph_timer = 0.0f;
             }

             // Console logging
             log_timer += dt;
             if (log_timer >= LOG_INTERVAL) {
                 std::cout << "Populations - Hares: " << hares.size() << ", Plants: " << hexGrid.plants.size() << ", Salmons: " << salmons.size() << ", Foxes: " << foxes.size() << ", Wolves: " << wolves.size() << std::endl;
                 log_timer = 0.0f;
             }

             // Remove dead hares
             hares.erase(std::remove_if(hares.begin(), hares.end(), [](const Hare& h) {
                 return h.is_dead;
             }), hares.end());

             // Remove dead salmons
             salmons.erase(std::remove_if(salmons.begin(), salmons.end(), [](const Salmon& s) {
                 return s.is_dead;
             }), salmons.end());

             // Remove dead foxes
             foxes.erase(std::remove_if(foxes.begin(), foxes.end(), [](const Fox& f) {
                 return f.is_dead;
             }), foxes.end());

             // Remove dead wolves
             wolves.erase(std::remove_if(wolves.begin(), wolves.end(), [](const Wolf& w) {
                 return w.is_dead;
             }), wolves.end());



            // Move object randomly every second
            if (showObject) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_move > std::chrono::seconds(1)) {
                    std::uniform_int_distribution<> dis(0, 5);
                    int dir = dis(gen);
                    obj.move(dir);
                    last_move = now;
                }
            }

            // Calculate brightness center based on alive animals
            float brightness_center_q = 0, brightness_center_r = 0;
            int alive_count = 0;
            for (const auto& hare : hares) {
                if (!hare.is_dead) {
                    brightness_center_q += hare.q;
                    brightness_center_r += hare.r;
                    alive_count++;
                }
            }
            for (const auto& fox : foxes) {
                if (!fox.is_dead) {
                    brightness_center_q += fox.q;
                    brightness_center_r += fox.r;
                    alive_count++;
                }
            }
            for (const auto& wolf : wolves) {
                if (!wolf.is_dead) {
                    brightness_center_q += wolf.q;
                    brightness_center_r += wolf.r;
                    alive_count++;
                }
            }
            bool has_alive_animals = alive_count > 0;
            if (has_alive_animals) {
                brightness_center_q /= alive_count;
                brightness_center_r /= alive_count;
            }

            // Clear screen
            renderer.clear(20, 20, 30); // Dark blue background

            // Draw hexagons
            hexGrid.draw(renderer,
                          100, 150, 200,  // Light blue fill
                          255, 255, 255,  // White outline
                          center_x, center_y,
                          renderer.getWidth(), renderer.getHeight(),
                          brightness_center_q, brightness_center_r,
                          has_alive_animals);

             // Draw plants as bushes (overlapping circles)
             for (const auto& [coord, plant] : hexGrid.plants) {
                 auto [px, py] = hexGrid.axial_to_pixel(plant.q, plant.r);
                 float plant_x = px + center_x;
                 float plant_y = py + center_y;
                 uint8_t base_r, base_g, base_b;
                 int num_circles = 1;
                 float base_radius = 2.0f;

                 switch (plant.stage) {
                     case SEED:
                         base_r = 139; base_g = 69; base_b = 19; // Brown seed
                         num_circles = 2;
                         base_radius = 2.0f;
                         break;
                     case SPROUT:
                         base_r = 34; base_g = 139; base_b = 34; // Forest green sprout
                         num_circles = 4;
                         base_radius = 3.0f;
                         break;
                     case PLANT:
                         base_r = 0; base_g = 100; base_b = 0; // Dark green plant
                         num_circles = 7;
                         base_radius = 4.0f;
                         break;
                     case CHARRED:
                         base_r = 40; base_g = 40; base_b = 40; // Dark grey charred remains
                         num_circles = 5;
                         base_radius = 3.0f;
                         break;
                 }

                 // Use deterministic random based on position for consistent bush shape
                 std::mt19937 bush_gen(plant.q * 1000 + plant.r);
                 std::uniform_real_distribution<float> offset_dist(-base_radius * 0.6f, base_radius * 0.6f);
                 std::uniform_real_distribution<float> size_dist(0.7f, 1.3f);

                 // Draw overlapping circles to create bush effect
                 for (int i = 0; i < num_circles; ++i) {
                     float offset_x = offset_dist(bush_gen);
                     float offset_y = offset_dist(bush_gen);
                     float size_mult = size_dist(bush_gen);
                     float radius = base_radius * size_mult;

                     // Vary color slightly for depth
                     int color_var = (bush_gen() % 40) - 20;
                     uint8_t r = std::clamp(base_r + color_var, 0, 255);
                     uint8_t g = std::clamp(base_g + color_var, 0, 255);
                     uint8_t b = std::clamp(base_b + color_var, 0, 255);

                     renderer.drawCircle(plant_x + offset_x, plant_y + offset_y, radius, r, g, b, 255);
                 }
             }

             // Draw fires
             for (const auto& [coord, timer] : hexGrid.fire_timers) {
                 auto [px, py] = hexGrid.axial_to_pixel(coord.first, coord.second);
                 float fire_x = px + center_x;
                 float fire_y = py + center_y;
                 // Scale based on burn time (grows as it burns)
                 float scale = 0.5f + 0.5f * ((5.0f - timer) / 5.0f);
                 // Base circle
                 renderer.drawCircle(fire_x, fire_y - HEX_SIZE / 2 + 3 * scale, 3.0f * scale, 255, 50, 0, 255); // Red circle at base
                 // Outer flame triangle
                 sf::ConvexShape flame(3);
                 flame.setPoint(0, sf::Vector2f(fire_x, fire_y - HEX_SIZE / 2));
                 flame.setPoint(1, sf::Vector2f(fire_x - 6 * scale, fire_y - HEX_SIZE * scale));
                 flame.setPoint(2, sf::Vector2f(fire_x + 6 * scale, fire_y - HEX_SIZE * scale));
                 flame.setFillColor(sf::Color(255, 100, 0)); // Orange
                 renderer.getWindow()->draw(flame);
                 // Inner yellow triangle
                 sf::ConvexShape inner_flame(3);
                 inner_flame.setPoint(0, sf::Vector2f(fire_x, fire_y - HEX_SIZE / 2 + 2 * scale));
                 inner_flame.setPoint(1, sf::Vector2f(fire_x - 3 * scale, fire_y - HEX_SIZE * 0.8f * scale));
                 inner_flame.setPoint(2, sf::Vector2f(fire_x + 3 * scale, fire_y - HEX_SIZE * 0.8f * scale));
                 inner_flame.setFillColor(sf::Color(255, 200, 0)); // Yellow
                 renderer.getWindow()->draw(inner_flame);
             }

               // Draw hares
               for (const auto& hare : hares) {
                   float hare_x = hare.current_pos.x + center_x;
                   float hare_y = hare.current_pos.y + center_y;
                  sf::Color color = hare.getColor();
                  // Scale based on energy (newborns start small but visible)
                  float scale = std::max(0.8f, std::min(hare.energy / 1.0f, 1.0f));
                  float head_size = 6.0f * scale;
                  float ear_size = 2.0f * scale;
                  float eye_size = 1.0f * scale;
                  float ear_offset_x = 3.0f * scale;
                  float ear_offset_y = 6.0f * scale;
                  float eye_offset = 2.0f * scale;
                  // Draw hare as simple face: head circle, ears, eyes
                  renderer.drawCircle(hare_x, hare_y, head_size, color.r, color.g, color.b, 255); // Head
                  renderer.drawCircle(hare_x - ear_offset_x, hare_y - ear_offset_y, ear_size, std::max(0, (int)color.r - 20), std::max(0, (int)color.g - 20), std::max(0, (int)color.b - 20), 255); // Left ear
                  renderer.drawCircle(hare_x + ear_offset_x, hare_y - ear_offset_y, ear_size, std::max(0, (int)color.r - 20), std::max(0, (int)color.g - 20), std::max(0, (int)color.b - 20), 255); // Right ear
                  renderer.drawCircle(hare_x - eye_offset, hare_y - eye_offset, eye_size, 0, 0, 0, 255); // Left eye
                  renderer.drawCircle(hare_x + eye_offset, hare_y - eye_offset, eye_size, 0, 0, 0, 255); // Right eye
              }

               // Draw salmons
               for (const auto& salmon : salmons) {
                   float salmon_x = salmon.current_pos.x + center_x;
                   float salmon_y = salmon.current_pos.y + center_y;
                   sf::Color color = salmon.getColor();
                   // Scale based on energy
                   float scale = std::max(0.8f, std::min(salmon.energy / 1.0f, 1.0f));

                   // Draw fish body (elongated with circles)
                   float body_length = 8.0f * scale;
                   float body_width = 4.0f * scale;

                   // Draw body as overlapping circles for smooth fish shape
                   renderer.drawCircle(salmon_x - 2 * scale, salmon_y, body_width * 0.8f, color.r, color.g, color.b, 255); // Back
                   renderer.drawCircle(salmon_x, salmon_y, body_width, color.r, color.g, color.b, 255); // Middle (widest)
                   renderer.drawCircle(salmon_x + 2 * scale, salmon_y, body_width * 0.8f, color.r, color.g, color.b, 255); // Front

                   // Draw tail fin (triangle)
                   std::vector<std::pair<float, float>> tail = {
                       {salmon_x - 4 * scale, salmon_y}, // Tip
                       {salmon_x - 2 * scale, salmon_y - 3 * scale}, // Top
                       {salmon_x - 2 * scale, salmon_y + 3 * scale}  // Bottom
                   };
                   uint8_t tail_r = std::max(0, (int)color.r - 30);
                   uint8_t tail_g = std::max(0, (int)color.g - 30);
                   uint8_t tail_b = std::max(0, (int)color.b - 30);
                   renderer.drawConvexShape(tail, tail_r, tail_g, tail_b, 255);

                   // Draw small dorsal fin on top
                   std::vector<std::pair<float, float>> dorsal_fin = {
                       {salmon_x - 1 * scale, salmon_y - body_width * 0.8f}, // Base left
                       {salmon_x + 1 * scale, salmon_y - body_width * 0.8f}, // Base right
                       {salmon_x, salmon_y - body_width * 1.3f}  // Tip
                   };
                   renderer.drawConvexShape(dorsal_fin, tail_r, tail_g, tail_b, 255);

                   // Draw eye
                   renderer.drawCircle(salmon_x + 2 * scale, salmon_y - 1 * scale, 0.8f * scale, 0, 0, 0, 255);
               }

               // Draw foxes
               for (const auto& fox : foxes) {
                   float fox_x = fox.current_pos.x + center_x;
                   float fox_y = fox.current_pos.y + center_y;
                  sf::Color color = fox.getColor();
                  // Scale based on energy (newborns start smaller but visible)
                  float scale = std::max(0.9f, std::min(fox.energy / 3.5f, 1.0f));
                  // Draw fox as triangle head with ears
                  std::vector<std::pair<float, float>> head_points = {
                      {fox_x, fox_y + 7 * scale}, // Bottom
                      {fox_x - 6 * scale, fox_y - 3.5f * scale}, // Top left
                      {fox_x + 6 * scale, fox_y - 3.5f * scale}  // Top right
                  };
                  renderer.drawConvexShape(head_points, color.r, color.g, color.b, 255);
                  // Ears
                  std::vector<std::pair<float, float>> left_ear = {
                      {fox_x - 6 * scale, fox_y - 3.5f * scale}, // Base left
                      {fox_x - 3.5f * scale, fox_y - 3.5f * scale}, // Base right
                      {fox_x - 4.5f * scale, fox_y - 7 * scale}  // Tip
                  };
                  renderer.drawConvexShape(left_ear, color.r, color.g, color.b, 255);
                  std::vector<std::pair<float, float>> right_ear = {
                      {fox_x + 3.5f * scale, fox_y - 3.5f * scale}, // Base left
                      {fox_x + 6 * scale, fox_y - 3.5f * scale}, // Base right
                      {fox_x + 4.5f * scale, fox_y - 7 * scale}  // Tip
                  };
                  renderer.drawConvexShape(right_ear, color.r, color.g, color.b, 255);
                  // Eyes
                  renderer.drawCircle(fox_x - 2 * scale, fox_y + 1.5f * scale, 1.0f * scale, 255, 255, 255, 255); // Left eye
                  renderer.drawCircle(fox_x + 2 * scale, fox_y + 1.5f * scale, 1.0f * scale, 255, 255, 255, 255); // Right eye
              }

               // Draw wolves
               for (const auto& wolf : wolves) {
                   float wolf_x = wolf.current_pos.x + center_x;
                   float wolf_y = wolf.current_pos.y + center_y;
                   sf::Color color = wolf.getColor();
                   // Scale based on energy (newborns start smaller but visible)
                   float scale = std::max(0.9f, std::min(wolf.energy / 5.0f, 1.0f));
                   // Draw wolf as larger triangle head with ears
                   std::vector<std::pair<float, float>> head_points = {
                       {wolf_x, wolf_y + 10 * scale}, // Bottom
                       {wolf_x - 8 * scale, wolf_y - 5 * scale}, // Top left
                       {wolf_x + 8 * scale, wolf_y - 5 * scale}  // Top right
                   };
                   renderer.drawConvexShape(head_points, color.r, color.g, color.b, 255);
                   // Ears
                   std::vector<std::pair<float, float>> left_ear = {
                       {wolf_x - 8 * scale, wolf_y - 5 * scale}, // Base left
                       {wolf_x - 5 * scale, wolf_y - 5 * scale}, // Base right
                       {wolf_x - 6.5f * scale, wolf_y - 10 * scale}  // Tip
                   };
                   renderer.drawConvexShape(left_ear, color.r, color.g, color.b, 255);
                   std::vector<std::pair<float, float>> right_ear = {
                       {wolf_x + 5 * scale, wolf_y - 5 * scale}, // Base left
                       {wolf_x + 8 * scale, wolf_y - 5 * scale}, // Base right
                       {wolf_x + 6.5f * scale, wolf_y - 10 * scale}  // Tip
                   };
                   renderer.drawConvexShape(right_ear, color.r, color.g, color.b, 255);
                   // Eyes
                   renderer.drawCircle(wolf_x - 2.5f * scale, wolf_y + 2 * scale, 1.2f * scale, 255, 255, 255, 255); // Left eye
                   renderer.drawCircle(wolf_x + 2.5f * scale, wolf_y + 2 * scale, 1.2f * scale, 255, 255, 255, 255); // Right eye
               }

              if (show_dashboard) {
                  // Draw population graph (bottom 4% of screen)
                  int graph_height = renderer.getHeight() / 25;
                  int graph_y = renderer.getHeight() - graph_height;
                  renderer.drawRectangle(0, graph_y, renderer.getWidth(), graph_height, 0, 0, 0, 150); // Semi-transparent background
                   if (!hare_history.empty()) {
                      int max_count = 0;
                      for (int h : hare_history) max_count = std::max(max_count, h);
                      for (int p : plant_history) max_count = std::max(max_count, p);
                      for (int s : salmon_history) max_count = std::max(max_count, s);
                      for (int f : fox_history) max_count = std::max(max_count, f);
                      for (int w : wolf_history) max_count = std::max(max_count, w);
                      if (max_count == 0) max_count = 1;
                      // Compute log scales for hares, foxes, and wolves
                      float max_log_hfw = std::log(max_count + 1.0f);
                      for (size_t i = 1; i < hare_history.size(); ++i) {
                          float x1 = (i - 1) * static_cast<float>(renderer.getWidth()) / (hare_history.size() - 1);
                          float x2 = i * static_cast<float>(renderer.getWidth()) / (hare_history.size() - 1);
                          // Hare line (gray) - logarithmic
                          float log_h1 = std::log(hare_history[i - 1] + 1.0f);
                          float log_h2 = std::log(hare_history[i] + 1.0f);
                          float y1_h = graph_y + graph_height - (log_h1 * static_cast<float>(graph_height) / max_log_hfw);
                          float y2_h = graph_y + graph_height - (log_h2 * static_cast<float>(graph_height) / max_log_hfw);
                          renderer.drawLine(x1, y1_h, x2, y2_h, 128, 128, 128, 255, 2.0f);
                          // Plant line (mature plant color: dark green) - linear
                          float y1_p = graph_y + graph_height - (plant_history[i - 1] * static_cast<float>(graph_height) / max_count);
                          float y2_p = graph_y + graph_height - (plant_history[i] * static_cast<float>(graph_height) / max_count);
                          renderer.drawLine(x1, y1_p, x2, y2_p, 0, 100, 0, 255, 2.0f);
                          // Salmon line (blue) - logarithmic
                          float log_s1 = std::log(salmon_history[i - 1] + 1.0f);
                          float log_s2 = std::log(salmon_history[i] + 1.0f);
                          float y1_s = graph_y + graph_height - (log_s1 * static_cast<float>(graph_height) / max_log_hfw);
                          float y2_s = graph_y + graph_height - (log_s2 * static_cast<float>(graph_height) / max_log_hfw);
                          renderer.drawLine(x1, y1_s, x2, y2_s, 0, 100, 255, 255, 2.0f);
                          // Fox line (orange) - logarithmic
                          float log_f1 = std::log(fox_history[i - 1] + 1.0f);
                          float log_f2 = std::log(fox_history[i] + 1.0f);
                          float y1_f = graph_y + graph_height - (log_f1 * static_cast<float>(graph_height) / max_log_hfw);
                          float y2_f = graph_y + graph_height - (log_f2 * static_cast<float>(graph_height) / max_log_hfw);
                          renderer.drawLine(x1, y1_f, x2, y2_f, 255, 140, 0, 255, 2.0f);
                          // Wolf line (black) - logarithmic
                          float log_w1 = std::log(wolf_history[i - 1] + 1.0f);
                          float log_w2 = std::log(wolf_history[i] + 1.0f);
                          float y1_w = graph_y + graph_height - (log_w1 * static_cast<float>(graph_height) / max_log_hfw);
                          float y2_w = graph_y + graph_height - (log_w2 * static_cast<float>(graph_height) / max_log_hfw);
                          renderer.drawLine(x1, y1_w, x2, y2_w, 0, 0, 0, 255, 2.0f);
                      }
                  }

                  // Display average genome stats and current population
                  float sum_threshold = 0.0f;
                  float sum_aggression = 0.0f;
                  float sum_weight = 0.0f;
                  int genome_count = 0;
                  for (const auto& hare : hares) {
                      if (!hare.is_dead) {
                          sum_threshold += hare.genome.reproduction_threshold;
                          sum_aggression += hare.genome.movement_aggression;
                          sum_weight += hare.genome.weight;
                          genome_count++;
                      }
                  }
                  int plant_count = hexGrid.plants.size();
                  int hare_count = hares.size();
                  int salmon_count = salmons.size();
                  int fox_count = foxes.size();
                  int wolf_count = wolves.size();
                  std::string stats_text = "Plants: " + std::to_string(plant_count) + " | Hares: " + std::to_string(hare_count) + " | Salmons: " + std::to_string(salmon_count) + " | Foxes: " + std::to_string(fox_count) + " | Wolves: " + std::to_string(wolf_count);
                  renderer.drawText(stats_text, 10, graph_y + 10, 255, 255, 255, 16);
              }

            // Draw object
            if (showObject) {
                auto [ox, oy] = hexGrid.axial_to_pixel(obj.q, obj.r);
                float obj_x = ox + center_x;
                float obj_y = oy + center_y;
                for (int i = 3; i >= 1; --i) {
                    uint8_t alpha = 255 / (1 << i); // 128, 64, 32
                    renderer.drawCircle(obj_x, obj_y, HEX_SIZE + i * 3.0f, 255, 0, 0, alpha);
                }

                // Draw object
                renderer.drawHexagon(obj_x, obj_y, HEX_SIZE,
                                      255, 0, 0,    // Red fill
                                      255, 255, 255, // White outline
                                      true);
            }


            // Display frame
            renderer.display();

            // Small delay to prevent excessive CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }

        std::cout << "HexaWorld closed successfully" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
