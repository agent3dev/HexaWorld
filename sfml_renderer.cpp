#include "sfml_renderer.hpp"
#include <cmath>
#include <iostream>

namespace hexaworld {

SFMLRenderer::SFMLRenderer(int width, int height, const std::string& title, bool fullscreen, int antialiasing)
    : deltaTime_(0.0f),
      lastKey_(sf::Keyboard::Key::Unknown),
      shouldClose_(false) {

    try {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = antialiasing;

        sf::VideoMode mode = fullscreen ? sf::VideoMode::getDesktopMode() : sf::VideoMode(sf::Vector2u(width, height));
        sf::State state = fullscreen ? sf::State::Fullscreen : sf::State::Windowed;
        window_ = std::make_unique<sf::RenderWindow>(mode, title, state, settings);

        if (!window_ || !window_->isOpen()) {
            std::cerr << "ERROR: Failed to create SFML window. Is DISPLAY set?" << std::endl;
            throw std::runtime_error("Failed to create SFML window - no display available");
        }

        window_->setPosition(sf::Vector2i(0, 0));
        window_->setVerticalSyncEnabled(true);
        loadFont();
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Exception creating SFML window: " << e.what() << std::endl;
        throw;
    }
}

SFMLRenderer::~SFMLRenderer() {
    if (window_ && window_->isOpen()) {
        window_->close();
    }
}

void SFMLRenderer::loadFont() {
    font_ = std::make_unique<sf::Font>();
    // Try to load a monospace font
    if (!font_->openFromFile("/usr/share/fonts/TTF/DejaVuSansMono.ttf")) {
        if (!font_->openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")) {
            std::cerr << "Warning: Could not load default font" << std::endl;
        }
    }
}

bool SFMLRenderer::isOpen() const {
    return window_ && window_->isOpen();
}

void SFMLRenderer::clear(uint8_t r, uint8_t g, uint8_t b) {
    if (window_) {
        window_->clear(sf::Color(r, g, b));
    }
}

void SFMLRenderer::display() {
    if (window_) {
        window_->display();
        deltaTime_ = clock_.restart().asSeconds();
    }
}

bool SFMLRenderer::pollEvent() {
    if (!window_) return false;

    // Reset last key at the start of each frame
    static bool keyProcessed = true;
    if (keyProcessed) {
        lastKey_ = sf::Keyboard::Key::Unknown;
        keyProcessed = false;
    }

    auto event = window_->pollEvent();
    if (event) {
        if (event->is<sf::Event::Closed>()) {
            shouldClose_ = true;
            return false;
        }
        if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
            lastKey_ = keyEvent->code;
            keyProcessed = true;

            if (keyEvent->code == sf::Keyboard::Key::Escape) {
                shouldClose_ = true;
            }
        }
        return true;
    }
    return false;
}

bool SFMLRenderer::shouldClose() const {
    return shouldClose_ || !isOpen();
}

void SFMLRenderer::drawConvexShape(
    const std::vector<std::pair<float, float>>& points,
    uint8_t r, uint8_t g, uint8_t b, uint8_t a) {

    if (!window_ || points.empty()) return;

    sf::ConvexShape shape(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        shape.setPoint(i, sf::Vector2f(points[i].first, points[i].second));
    }
    shape.setFillColor(sf::Color(r, g, b, a));
    window_->draw(shape);
}

void SFMLRenderer::drawConvexShapeOutline(
    const std::vector<std::pair<float, float>>& points,
    uint8_t r, uint8_t g, uint8_t b, uint8_t a, float thickness) {

    if (!window_ || points.empty()) return;

    sf::ConvexShape shape(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        shape.setPoint(i, sf::Vector2f(points[i].first, points[i].second));
    }
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineColor(sf::Color(r, g, b, a));
    shape.setOutlineThickness(thickness);
    window_->draw(shape);
}

void SFMLRenderer::drawText(const std::string& text, float x, float y,
                           uint8_t r, uint8_t g, uint8_t b, int size) {
    if (!window_ || !font_) return;

    sf::Text textObj(*font_);
    textObj.setString(text);
    textObj.setCharacterSize(size);
    textObj.setPosition(sf::Vector2f(x, y));
    textObj.setFillColor(sf::Color(r, g, b));
    window_->draw(textObj);
}

void SFMLRenderer::drawRectangle(float x, float y, float width, float height,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!window_) return;

    sf::RectangleShape rect(sf::Vector2f(width, height));
    rect.setPosition(sf::Vector2f(x, y));
    rect.setFillColor(sf::Color(r, g, b, a));
    window_->draw(rect);
}

void SFMLRenderer::drawCircle(float center_x, float center_y, float radius,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!window_) return;

    sf::CircleShape circle(radius);
    circle.setPosition(sf::Vector2f(center_x - radius, center_y - radius));
    circle.setFillColor(sf::Color(r, g, b, a));
    window_->draw(circle);
}

void SFMLRenderer::drawLine(float x1, float y1, float x2, float y2,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a, float thickness) {
    if (!window_) return;

    sf::Vector2f start(x1, y1);
    sf::Vector2f end(x2, y2);
    sf::Vector2f direction = end - start;
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length == 0) return;

    sf::RectangleShape line(sf::Vector2f(length, thickness));
    line.setPosition(start);
    line.setFillColor(sf::Color(r, g, b, a));
    float angle = std::atan2(direction.y, direction.x) * 180.0f / 3.14159f;
    line.setRotation(sf::degrees(angle));
    window_->draw(line);
}

// ============================================================================
// HEXAGON POINT CALCULATION - DO NOT MODIFY
// Uses 0° starting angle for flat-top hexagons (verified with FlatHexagon)
// ============================================================================
std::vector<sf::Vector2f> SFMLRenderer::calculateHexagonPoints(
    float center_x, float center_y, float side_length) const {

    std::vector<sf::Vector2f> points(6);
    const float angle_offset = 0.0f; // 0 degrees for flat-top hexagons

    for (int i = 0; i < 6; ++i) {
        float angle = angle_offset + i * M_PI / 3.0f; // 60 degrees between points
        points[i] = sf::Vector2f(
            center_x + side_length * std::cos(angle),
            center_y + side_length * std::sin(angle)
        );
    }
    return points;
}
// ============================================================================

void SFMLRenderer::drawHexagon(float center_x, float center_y, float side_length,
                              uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                              uint8_t line_r, uint8_t line_g, uint8_t line_b,
                              bool filled) {
    if (!window_) return;

    auto points = calculateHexagonPoints(center_x, center_y, side_length);

    sf::ConvexShape hexagon(6);
    for (size_t i = 0; i < points.size(); ++i) {
        hexagon.setPoint(i, points[i]);
    }

    if (filled) {
        hexagon.setFillColor(sf::Color(bg_r, bg_g, bg_b));
    } else {
        hexagon.setFillColor(sf::Color::Transparent);
    }
    hexagon.setOutlineColor(sf::Color(line_r, line_g, line_b));
    hexagon.setOutlineThickness(1.0f);

    window_->draw(hexagon);
}

void SFMLRenderer::drawHexagonWithShading(
    float center_x, float center_y, float side_length,
    uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
    uint8_t line_r, uint8_t line_g, uint8_t line_b,
    uint8_t shine_r, uint8_t shine_g, uint8_t shine_b,
    uint8_t shadow_r, uint8_t shadow_g, uint8_t shadow_b) {

    if (!window_) return;

    // Draw base hexagon
    drawHexagon(center_x, center_y, side_length, bg_r, bg_g, bg_b,
               line_r, line_g, line_b, true);

    // Draw shine triangles (top-right portions)
    const float shine_offset = 2.0f;
    auto points = calculateHexagonPoints(center_x, center_y, side_length);

    // Shine triangle 1 (top-right)
    sf::ConvexShape shine1(3);
    shine1.setPoint(0, sf::Vector2f(center_x, center_y));
    shine1.setPoint(1, points[0]);
    shine1.setPoint(2, points[1]);
    shine1.setFillColor(sf::Color(shine_r, shine_g, shine_b));
    window_->draw(shine1);

    // Shadow triangles (bottom-left portions)
    sf::ConvexShape shadow1(3);
    shadow1.setPoint(0, sf::Vector2f(center_x, center_y));
    shadow1.setPoint(1, points[3]);
    shadow1.setPoint(2, points[4]);
    shadow1.setFillColor(sf::Color(shadow_r, shadow_g, shadow_b));
    window_->draw(shadow1);
}

bool SFMLRenderer::isKeyPressed(int key) const {
    return sf::Keyboard::isKeyPressed(static_cast<sf::Keyboard::Key>(key));
}

sf::Keyboard::Key SFMLRenderer::getLastKey() const {
    return lastKey_;
}

void SFMLRenderer::setFramerateLimit(unsigned int limit) {
    if (window_) {
        window_->setFramerateLimit(limit);
    }
}

float SFMLRenderer::getDeltaTime() const {
    return deltaTime_;
}

int SFMLRenderer::getWidth() const {
    return window_ ? static_cast<int>(window_->getSize().x) : 0;
}

int SFMLRenderer::getHeight() const {
    return window_ ? static_cast<int>(window_->getSize().y) : 0;
}

sf::RenderWindow* SFMLRenderer::getWindow() const {
    return window_.get();
}

} // namespace hexaworld