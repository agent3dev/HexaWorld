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

namespace hexaworld {
unsigned int get_seed() {
    if (const char* env_seed = std::getenv("HEXAWORLD_SEED")) {
        try {
            return std::stoi(env_seed);
        } catch (const std::exception&) {
            // Fallback to constant if invalid
        }
    }
    return RANDOM_SEED;
}
std::mt19937 gen(get_seed()); // Fixed seed for repeatable simulation
}

int main() {
    try {
        unsigned int seed = hexaworld::get_seed();
        std::cout << "Using seed: " << seed << std::endl;

        // Create renderer in windowed mode
        hexaworld::SFMLRenderer renderer(1280, 1024, "HexaWorld - Hexagonal Grid", false);
        renderer.setFramerateLimit(60); // Limit FPS to reduce CPU usage

        // Center the grid in the window
        float center_x = renderer.getWidth() / 2.0f;
        float center_y = renderer.getHeight() / 2.0f;

        // Create hex grid
        hexaworld::HexGrid hexGrid(hexaworld::HEX_SIZE);

        // Add center hexagon
        hexGrid.add_hexagon(0, 0);

        // Expand grid to fill screen by adding neighbors that fit fully
        const float sqrt3 = hexaworld::SQRT3;
        while (true) {
            size_t old_size = hexGrid.hexagons.size();
            hexGrid.create_neighbors();
            // Remove hexagons that are not fully visible
            for (auto it = hexGrid.hexagons.begin(); it != hexGrid.hexagons.end(); ) {
                auto [x, y] = it->second;
                float cx = x + center_x;
                float cy = y + center_y;
                float left = cx - hexaworld::HEX_SIZE;
                float right = cx + hexaworld::HEX_SIZE;
                float top = cy - hexaworld::HEX_SIZE * sqrt3 / 2.0f;
                float bottom = cy + hexaworld::HEX_SIZE * sqrt3 / 2.0f;
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
             if (it->second.type == hexaworld::WATER) {
                 bool has_water_neighbor = false;
                 for (int dir = 0; dir < 6; ++dir) {
                     auto [nq, nr] = hexGrid.get_neighbor_coords(q, r, dir);
                     auto nit = hexGrid.terrainTiles.find({nq, nr});
                     if (nit != hexGrid.terrainTiles.end() && nit->second.type == hexaworld::WATER) {
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
            if (tile.type == hexaworld::SOIL) soil_count++;
            else if (tile.type == hexaworld::WATER) water_count++;
            else if (tile.type == hexaworld::ROCK) rock_count++;
        }
         std::cout << "Terrain: SOIL " << soil_count << ", WATER " << water_count << ", ROCK " << rock_count << std::endl;

          // Plants are now spawned in add_hexagon, but ensure some exist
         std::vector<std::pair<int, int>> soil_coords;
         // Add extra plants if needed
         for (const auto& [coord, tile] : hexGrid.terrainTiles) {
             if (tile.type == hexaworld::SOIL) {
                 soil_coords.push_back(coord);
             }
         }
          std::shuffle(soil_coords.begin(), soil_coords.end(), hexaworld::gen);
          size_t num_mature = std::min<size_t>(soil_coords.size() / 2, soil_coords.size());
          size_t num_sprouts = std::min<size_t>(soil_coords.size() / 4, soil_coords.size());
          // Place mature plants first
          for (size_t i = 0; i < num_mature; ++i) {
              auto [q, r] = soil_coords[i];
              hexGrid.plants.insert({{q, r}, hexaworld::Plant(q, r, hexaworld::PLANT, hexGrid.terrainTiles.at({q, r}).nutrients)});
          }
          // Then sprouts
          for (size_t i = num_mature; i < num_mature + num_sprouts; ++i) {
              auto [q, r] = soil_coords[i];
              hexGrid.plants.insert({{q, r}, hexaworld::Plant(q, r, hexaworld::SPROUT, hexGrid.terrainTiles.at({q, r}).nutrients)});
          }
           // Then seeds
           for (size_t i = num_mature + num_sprouts; i < soil_coords.size(); ++i) {
               auto [q, r] = soil_coords[i];
               hexGrid.plants.insert({{q, r}, hexaworld::Plant(q, r, hexaworld::SEED, hexGrid.terrainTiles.at({q, r}).nutrients)});
           }




          // Create a movable object
        hexaworld::HexObject obj(0, 0);
        auto last_move = std::chrono::steady_clock::now();
        bool showObject = false;

        // Hare logging
        bool enableHareLogging = true;
        auto last_log = std::chrono::steady_clock::now();

        // Population graph data
        std::vector<int> hare_history;
        std::vector<int> plant_history;
        float graph_timer = 0.0f;
        const float GRAPH_UPDATE_INTERVAL = 1.0f; // Update every second
        const size_t MAX_HISTORY = 1000;

        // Create hares on plant tiles
        std::vector<hexaworld::Hare> hares;
        std::vector<std::pair<int, int>> plant_coords;
        for (const auto& [coord, plant] : hexGrid.plants) {
            plant_coords.push_back(coord);
        }
        std::shuffle(plant_coords.begin(), plant_coords.end(), hexaworld::gen);
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
            hares.back().genome.reproduction_threshold = thresh_dist(hexaworld::gen);
            hares.back().genome.movement_aggression = aggression_dist(hexaworld::gen);
            hares.back().genome.weight = weight_dist(hexaworld::gen);
            hares.back().update_speed();
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

            // Check for 'c' key to toggle object visibility
            if (renderer.getLastKey() == sf::Keyboard::Key::C) {
                showObject = !showObject;
            }

            // Check for 'g' key to log hare genomes
            if (renderer.getLastKey() == sf::Keyboard::Key::G) {
                std::cout << "Current hare genomes:" << std::endl;
                for (const auto& hare : hares) {
                    if (!hare.is_dead) {
                        std::cout << "Hare at (" << hare.q << "," << hare.r << "): reproduction_threshold = " << hare.genome.reproduction_threshold << ", movement_aggression = " << hare.genome.movement_aggression << ", weight = " << hare.genome.weight << ", speed = " << hare.speed << std::endl;
                    }
                }
            }

            // Update plant growth
            float dt = renderer.getDeltaTime();
            for (auto& [coord, plant] : hexGrid.plants) {
                plant.growth_time += dt;
                float threshold = 20.0f / (plant.nutrients + 0.1f); // Slower growth
                if (plant.growth_time >= threshold) {
                    if (plant.stage < hexaworld::PLANT) {
                        plant.stage = static_cast<hexaworld::PlantStage>(plant.stage + 1);
                    }
                    plant.growth_time = 0.0f;
                }

                // Mature plants drop seeds randomly
                if (plant.stage == hexaworld::PLANT) {
                    plant.drop_time += dt;
                    if (plant.drop_time >= 10.0f) { // Every 10 seconds
                        if ((hexaworld::gen() % 100) < 20) { // 20% chance
                            // Drop seeds in soil neighbors without plants
                            for (int dir = 0; dir < 6; ++dir) {
                                auto [nq, nr] = hexGrid.get_neighbor_coords(plant.q, plant.r, dir);
                                if (hexGrid.has_hexagon(nq, nr) &&
                                    hexGrid.get_terrain_type(nq, nr) == hexaworld::SOIL &&
                                    !hexGrid.get_plant(nq, nr)) {
                                    auto tit = hexGrid.terrainTiles.find({nq, nr});
                                    float nutrients = (tit != hexGrid.terrainTiles.end()) ? tit->second.nutrients : 0.5f;
                                    hexGrid.plants.insert({{nq, nr}, hexaworld::Plant(nq, nr, hexaworld::SEED, nutrients)});
                                }
                            }
                        }
                        plant.drop_time = 0.0f;
                    }
                }
            }

            // Update hares
            for (auto& hare : hares) {
                hare.update(hexGrid, dt, hexaworld::gen);
            }

            // Handle hare birth
            for (auto& hare : hares) {
                if (hare.ready_to_give_birth) {
                    // Create offspring at same position
                    hares.push_back(hexaworld::Hare(hare.q, hare.r));
                    hares.back().genome = hare.genome.mutate(hexaworld::gen);
                    hares.back().energy = 0.5f; // Lower starting energy for evolutionary pressure
                    hare.ready_to_give_birth = false;
                    if (enableHareLogging) {
                        std::cout << "Hare gave birth at (" << hare.q << ", " << hare.r << ")" << std::endl;
                    }
                }
            }

            // Update population graph
            graph_timer += dt;
            if (graph_timer >= GRAPH_UPDATE_INTERVAL) {
                hare_history.push_back(hares.size());
                plant_history.push_back(hexGrid.plants.size());
                if (hare_history.size() > MAX_HISTORY) {
                    hare_history.erase(hare_history.begin());
                    plant_history.erase(plant_history.begin());
                }
                graph_timer = 0.0f;
            }

            // Remove dead hares
            hares.erase(std::remove_if(hares.begin(), hares.end(), [](const hexaworld::Hare& h) {
                return h.is_dead;
            }), hares.end());



            // Move object randomly every second
            if (showObject) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_move > std::chrono::seconds(1)) {
                    std::uniform_int_distribution<> dis(0, 5);
                    int dir = dis(hexaworld::gen);
                    obj.move(dir);
                    last_move = now;
                }
            }

            // Calculate brightness center based on alive hares
            float brightness_center_q = 0, brightness_center_r = 0;
            int alive_count = 0;
            for (const auto& hare : hares) {
                if (!hare.is_dead) {
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

            // Clear screen
            renderer.clear(20, 20, 30); // Dark blue background

            // Draw hexagons
            hexGrid.draw(renderer,
                          100, 150, 200,  // Light blue fill
                          255, 255, 255,  // White outline
                          center_x, center_y,
                          renderer.getWidth(), renderer.getHeight(),
                          brightness_center_q, brightness_center_r,
                          has_alive_hares);

            // Draw plants with pizza slice style
            for (const auto& [coord, plant] : hexGrid.plants) {
                auto [px, py] = hexGrid.axial_to_pixel(plant.q, plant.r);
                float plant_x = px + center_x;
                float plant_y = py + center_y;
                uint8_t r, g, b;
                switch (plant.stage) {
                    case hexaworld::SEED:
                        r = 139; g = 69; b = 19; break; // Brown seed
                    case hexaworld::SPROUT:
                        r = 34; g = 139; b = 34; break; // Forest green sprout
                    case hexaworld::PLANT:
                        r = 0; g = 100; b = 0; break; // Dark green plant
                }
                // Draw drop shadow
                auto shadow_points = renderer.calculateHexagonPoints(plant_x + 2, plant_y + 2, 4.0f);
                std::vector<std::pair<float, float>> shadow_point_pairs;
                for (auto& p : shadow_points) shadow_point_pairs.emplace_back(p.x, p.y);
                renderer.drawConvexShape(shadow_point_pairs, 0, 0, 0, 100);

                // Draw plant as pizza slices
                auto points = renderer.calculateHexagonPoints(plant_x, plant_y, 4.0f);
                for (int i = 0; i < 6; ++i) {
                    sf::ConvexShape triangle(3);
                    triangle.setPoint(0, sf::Vector2f(plant_x, plant_y));
                    triangle.setPoint(1, points[i]);
                    triangle.setPoint(2, points[(i + 1) % 6]);
                    triangle.setFillColor(sf::Color(r, g, b));
                    renderer.getWindow()->draw(triangle);
                }
            }

             // Draw hares
             for (const auto& hare : hares) {
                 auto [hx, hy] = hexGrid.axial_to_pixel(hare.q, hare.r);
                 float hare_x = hx + center_x;
                 float hare_y = hy + center_y;
                 sf::Color color = hare.getColor();
                 // Draw hare as simple face: head circle, ears, eyes
                 renderer.drawCircle(hare_x, hare_y, 6.0f, color.r, color.g, color.b, 255); // Head
                 renderer.drawCircle(hare_x - 3, hare_y - 6, 2.0f, std::max(0, (int)color.r - 20), std::max(0, (int)color.g - 20), std::max(0, (int)color.b - 20), 255); // Left ear
                 renderer.drawCircle(hare_x + 3, hare_y - 6, 2.0f, std::max(0, (int)color.r - 20), std::max(0, (int)color.g - 20), std::max(0, (int)color.b - 20), 255); // Right ear
                 renderer.drawCircle(hare_x - 2, hare_y - 1, 1.0f, 0, 0, 0, 255); // Left eye
                 renderer.drawCircle(hare_x + 2, hare_y - 1, 1.0f, 0, 0, 0, 255); // Right eye
             }

             // Draw population graph (bottom 10% of screen)
             int graph_height = renderer.getHeight() / 10;
             int graph_y = renderer.getHeight() - graph_height;
             renderer.drawRectangle(0, graph_y, renderer.getWidth(), graph_height, 0, 0, 0, 150); // Semi-transparent background
             if (!hare_history.empty()) {
                 int max_count = 0;
                 for (int h : hare_history) max_count = std::max(max_count, h);
                 for (int p : plant_history) max_count = std::max(max_count, p);
                 if (max_count == 0) max_count = 1;
                 for (size_t i = 1; i < hare_history.size(); ++i) {
                     float x1 = (i - 1) * static_cast<float>(renderer.getWidth()) / (hare_history.size() - 1);
                     float x2 = i * static_cast<float>(renderer.getWidth()) / (hare_history.size() - 1);
                     // Hare line (red)
                     float y1_h = graph_y + graph_height - (hare_history[i - 1] * static_cast<float>(graph_height) / max_count);
                     float y2_h = graph_y + graph_height - (hare_history[i] * static_cast<float>(graph_height) / max_count);
                     renderer.drawLine(x1, y1_h, x2, y2_h, 255, 0, 0, 255, 2.0f);
                     // Plant line (green)
                     float y1_p = graph_y + graph_height - (plant_history[i - 1] * static_cast<float>(graph_height) / max_count);
                     float y2_p = graph_y + graph_height - (plant_history[i] * static_cast<float>(graph_height) / max_count);
                     renderer.drawLine(x1, y1_p, x2, y2_p, 0, 255, 0, 255, 2.0f);
                 }
             }

             // Display average genome stats
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
             float avg_threshold = genome_count > 0 ? sum_threshold / genome_count : 0.0f;
             float avg_aggression = genome_count > 0 ? sum_aggression / genome_count : 0.0f;
             float avg_weight = genome_count > 0 ? sum_weight / genome_count : 0.0f;
             std::string stats_text = "Avg Threshold: " + std::to_string(avg_threshold) + " | Avg Aggression: " + std::to_string(avg_aggression) + " | Avg Weight: " + std::to_string(avg_weight);
             renderer.drawText(stats_text, 10, graph_y + 10, 255, 255, 255, 16);

            // Draw object
            if (showObject) {
                auto [ox, oy] = hexGrid.axial_to_pixel(obj.q, obj.r);
                float obj_x = ox + center_x;
                float obj_y = oy + center_y;
                for (int i = 3; i >= 1; --i) {
                    uint8_t alpha = 255 / (1 << i); // 128, 64, 32
                    renderer.drawCircle(obj_x, obj_y, hexaworld::HEX_SIZE + i * 3.0f, 255, 0, 0, alpha);
                }

                // Draw object
                renderer.drawHexagon(obj_x, obj_y, hexaworld::HEX_SIZE,
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
