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

int main() {
    try {

        // Create renderer in fullscreen mode
        hexaworld::SFMLRenderer renderer(1200, 800, "HexaWorld - Hexagonal Grid", true);
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
        // Add extra plants if needed
        std::vector<std::pair<int, int>> soil_coords;
        for (const auto& [coord, tile] : hexGrid.terrainTiles) {
            if (tile.type == hexaworld::SOIL) {
                soil_coords.push_back(coord);
            }
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(soil_coords.begin(), soil_coords.end(), gen);
        size_t num_plants = std::min(20, (int)soil_coords.size());
        for (size_t i = 0; i < num_plants; ++i) {
            auto [q, r] = soil_coords[i];
            if (!hexGrid.get_plant(q, r)) {
                auto it = hexGrid.terrainTiles.find({q, r});
                if (it != hexGrid.terrainTiles.end()) {
                    hexGrid.plants.insert({{q, r}, hexaworld::Plant(q, r, hexaworld::SEED, it->second.nutrients)});
                }
            }
        }


         // Create a movable object
        hexaworld::HexObject obj(0, 0);
        auto last_move = std::chrono::steady_clock::now();
        bool showObject = false;

        // Hare logging
        bool enableHareLogging = true;
        auto last_log = std::chrono::steady_clock::now();

        // Create hares on plant tiles
        std::vector<hexaworld::Hare> hares;
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

            // Update plant growth
            float dt = renderer.getDeltaTime();
            for (auto& [coord, plant] : hexGrid.plants) {
                plant.growth_time += dt;
                float threshold = 10.0f / (plant.nutrients + 0.1f); // avoid division by zero
                if (plant.growth_time >= threshold) {
                    if (plant.stage < hexaworld::PLANT) {
                        plant.stage = static_cast<hexaworld::PlantStage>(plant.stage + 1);
                    }
                    plant.growth_time = 0.0f;
                }
            }

            // Update hares
            for (auto& hare : hares) {
                hare.update(hexGrid, dt);
            }

            // Remove dead hares
            hares.erase(std::remove_if(hares.begin(), hares.end(), [](const hexaworld::Hare& h) {
                return h.is_dead;
            }), hares.end());

            // Log hare status every 5 seconds
            if (enableHareLogging) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_log > std::chrono::seconds(5)) {
                    for (size_t i = 0; i < hares.size(); ++i) {
                        const auto& hare = hares[i];
                        if (hare.energy > 0) {
                            std::cout << "Hare #" << i << " is alive (energy: " << hare.energy << ")" << std::endl;
                        }
                    }
                    last_log = now;
                }
            }

            // Move object randomly every second
            if (showObject) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_move > std::chrono::seconds(1)) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(0, 5);
                    int dir = dis(gen);
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
                // Draw hare as khaki hexagon
                auto points = renderer.calculateHexagonPoints(hare_x, hare_y, 6.0f);
                std::vector<std::pair<float, float>> point_pairs;
                for (auto& p : points) point_pairs.emplace_back(p.x, p.y);
                renderer.drawConvexShape(point_pairs, hare.base_color.r, hare.base_color.g, hare.base_color.b, 255);
            }

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
