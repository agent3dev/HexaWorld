#include "hex_grid_new.hpp"
#include "sfml_renderer.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        std::cout << "HexaWorld - Hexagonal Grid Visualization" << std::endl;
        std::cout << "=========================================" << std::endl;

        // Create renderer
        const int WINDOW_WIDTH = 1200;
        const int WINDOW_HEIGHT = 800;
        hexaworld::SFMLRenderer renderer(WINDOW_WIDTH, WINDOW_HEIGHT, "HexaWorld - Hexagonal Grid");

        // Create hex grid
        const float HEX_SIZE = 30.0f;
        hexaworld::HexGrid hexGrid(HEX_SIZE);

        // Add center hexagon
        hexGrid.add_hexagon(0, 0);

        // Expand grid to show multiple layers
        const int LAYERS = 3;
        hexGrid.expand_grid(LAYERS);

        std::cout << "Created hexagonal grid with " << hexGrid.hexagons.size() << " hexagons" << std::endl;
        std::cout << "Press ESC to exit" << std::endl;

        // Center the grid in the window
        float center_x = WINDOW_WIDTH / 2.0f;
        float center_y = WINDOW_HEIGHT / 2.0f;

        // Main render loop
        while (!renderer.shouldClose()) {
            // Handle events
            renderer.pollEvent();

            // Clear screen
            renderer.clear(20, 20, 30); // Dark blue background

            // Draw hexagons
            hexGrid.draw(renderer,
                        100, 150, 200,  // Light blue fill
                        255, 255, 255,  // White outline
                        center_x, center_y);

            // Draw info text
            renderer.drawText("HexaWorld - " + std::to_string(hexGrid.hexagons.size()) + " Hexagons",
                            10, 10, 255, 255, 255, 16);

            renderer.drawText("Press ESC to exit", 10, 35, 200, 200, 200, 12);

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