#pragma once

#include <SFML/Graphics.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace hexaworld {

class SFMLRenderer {
public:
    SFMLRenderer(int width, int height, const std::string& title, bool fullscreen);
    ~SFMLRenderer();

    // Window management
    bool isOpen() const;
    void clear(uint8_t r, uint8_t g, uint8_t b);
    void display();
    bool pollEvent();
    bool shouldClose() const;

    // Drawing primitives
    void drawConvexShape(const std::vector<std::pair<float, float>>& points,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    void drawConvexShapeOutline(const std::vector<std::pair<float, float>>& points,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255,
                               float thickness = 1.0f);

    void drawText(const std::string& text, float x, float y,
                 uint8_t r, uint8_t g, uint8_t b, int size = 12);

    void drawRectangle(float x, float y, float width, float height,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    // Hexagon-specific helpers
    void drawHexagon(float center_x, float center_y, float side_length,
                    uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                    uint8_t line_r, uint8_t line_g, uint8_t line_b,
                    bool filled = true);

    void drawHexagonWithShading(float center_x, float center_y, float side_length,
                               uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                               uint8_t line_r, uint8_t line_g, uint8_t line_b,
                               uint8_t shine_r, uint8_t shine_g, uint8_t shine_b,
                               uint8_t shadow_r, uint8_t shadow_g, uint8_t shadow_b);

    // Input handling
    bool isKeyPressed(int key) const;
    int getLastKey() const;

    // Performance
    void setFramerateLimit(unsigned int limit);
    float getDeltaTime() const;

    // Window size
    int getWidth() const;
    int getHeight() const;

private:
    std::unique_ptr<sf::RenderWindow> window_;
    std::unique_ptr<sf::Font> font_;
    sf::Clock clock_;
    float deltaTime_;
    int lastKey_;
    bool shouldClose_;

    void loadFont();
    std::vector<sf::Vector2f> calculateHexagonPoints(float center_x, float center_y, float side_length) const;
};

} // namespace hexaworld