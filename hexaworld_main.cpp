#include "hex_grid_new.hpp"
#include "sfml_renderer.hpp"
#include "constants.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

int main() {
    try {

        // Create renderer in fullscreen mode
        hexaworld::SFMLRenderer renderer(1200, 800, "HexaWorld - Hexagonal Grid", true);

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


        // Create a movable object
        hexaworld::HexObject obj(0, 0);
        auto last_move = std::chrono::steady_clock::now();

        // Main render loop
        while (!renderer.shouldClose()) {
            // Handle events
            renderer.pollEvent();

            // Move object randomly every second
            auto now = std::chrono::steady_clock::now();
            if (now - last_move > std::chrono::seconds(1)) {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, 5);
                int dir = dis(gen);
                obj.move(dir);
                last_move = now;
            }

            // Clear screen
            renderer.clear(20, 20, 30); // Dark blue background

            // Draw hexagons
            hexGrid.draw(renderer,
                          100, 150, 200,  // Light blue fill
                          255, 255, 255,  // White outline
                          center_x, center_y,
                          renderer.getWidth(), renderer.getHeight());

            // Draw object
            auto [ox, oy] = hexGrid.axial_to_pixel(obj.q, obj.r);
            renderer.drawHexagon(ox + center_x, oy + center_y, hexaworld::HEX_SIZE,
                                 255, 0, 0,    // Red fill
                                 255, 255, 255, // White outline
                                 true);


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
