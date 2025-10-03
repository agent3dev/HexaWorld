#include "sfml_renderer.hpp"
#include <cmath>
#include <iostream>

SFMLRenderer::SFMLRenderer(int width, int height, const std::string& title, bool fullscreen, bool frameless, bool maximized, int antialiasing)
    : deltaTime_(0.0f),
      lastKey_(sf::Keyboard::Key::Unknown),
      shouldClose_(false) {

    try {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = antialiasing;

        sf::VideoMode mode;
        if (fullscreen || maximized) {
            mode = sf::VideoMode::getDesktopMode();
        } else {
            mode = sf::VideoMode(sf::Vector2u(width, height));
        }
        sf::State state = sf::State::Windowed;
        if (fullscreen) {
            state = sf::State::Fullscreen;
        } else if (frameless) {
            state = static_cast<sf::State>(2); // WindowedNoDecorations
        }
        window_ = std::make_unique<sf::RenderWindow>(mode, title, state, settings);

        if (!window_ || !window_->isOpen()) {
            std::cerr << "ERROR: Failed to create SFML window. Is DISPLAY set?" << std::endl;
            throw std::runtime_error("Failed to create SFML window - no display available");
        }

        window_->setPosition(sf::Vector2i(0, 0));
        window_->setVerticalSyncEnabled(true);
        loadFont();
        precomputeSprites();
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
            std::cerr << "Warning: Could not load DejaVuSansMono font, text rendering may not work" << std::endl;
        }
    }
}

void SFMLRenderer::precomputeSprites() {
    // Precompute hare sprite
    hare_texture_.clear(sf::Color::Transparent);

    float scale = 1.0f;
    float head_size = 4.0f * scale;
    float ear_offset_x = 2.0f * scale;
    float ear_offset_y = -3.0f * scale;
    float ear_width = 1.5f * scale;
    float ear_height = 3.0f * scale;
    float eye_offset = 1.5f * scale;
    float eye_size = 0.8f * scale;

    // Head
    sf::CircleShape head(head_size);
    head.setFillColor(sf::Color::White);
    head.setPosition(sf::Vector2f(32 - head_size, 32 - head_size));
    hare_texture_.draw(head);

    // Left ear
    sf::RectangleShape ear(sf::Vector2f(ear_width, ear_height));
    ear.setFillColor(sf::Color::White);
    ear.setPosition(sf::Vector2f(32 - ear_offset_x - ear_width/2, 32 - ear_offset_y - ear_height/2));
    hare_texture_.draw(ear);

    // Right ear
    ear.setPosition(sf::Vector2f(32 + ear_offset_x - ear_width/2, 32 - ear_offset_y - ear_height/2));
    hare_texture_.draw(ear);

    // Left eye
    sf::CircleShape eye(eye_size);
    eye.setFillColor(sf::Color::Black);
    eye.setPosition(sf::Vector2f(32 - eye_offset - eye_size, 32 - eye_offset - eye_size));
    hare_texture_.draw(eye);

    // Right eye
    eye.setPosition(sf::Vector2f(32 + eye_offset - eye_size, 32 - eye_offset - eye_size));
    hare_texture_.draw(eye);

    hare_texture_.display();
    hare_sprite_ = std::make_unique<sf::Sprite>(hare_texture_.getTexture());
    hare_sprite_->setOrigin(sf::Vector2f(32, 32));

    // Precompute fox sprite
    fox_texture_.clear(sf::Color::Transparent);

    float fox_scale = 0.8f;
    float fox_x = 32.0f;
    float fox_y = 32.0f;

    // Head triangle
    sf::ConvexShape fox_head(3);
    fox_head.setPoint(0, sf::Vector2f(fox_x, fox_y + 9 * fox_scale));
    fox_head.setPoint(1, sf::Vector2f(fox_x - 8 * fox_scale, fox_y - 4.5f * fox_scale));
    fox_head.setPoint(2, sf::Vector2f(fox_x + 8 * fox_scale, fox_y - 4.5f * fox_scale));
    fox_head.setFillColor(sf::Color::White);
    fox_texture_.draw(fox_head);

    // Left ear
    sf::ConvexShape left_ear(3);
    left_ear.setPoint(0, sf::Vector2f(fox_x - 8 * fox_scale, fox_y - 4.5f * fox_scale));
    left_ear.setPoint(1, sf::Vector2f(fox_x - 4.5f * fox_scale, fox_y - 4.5f * fox_scale));
    left_ear.setPoint(2, sf::Vector2f(fox_x - 5.5f * fox_scale, fox_y - 9 * fox_scale));
    left_ear.setFillColor(sf::Color::White);
    fox_texture_.draw(left_ear);

    // Right ear
    sf::ConvexShape right_ear(3);
    right_ear.setPoint(0, sf::Vector2f(fox_x + 4.5f * fox_scale, fox_y - 4.5f * fox_scale));
    right_ear.setPoint(1, sf::Vector2f(fox_x + 8 * fox_scale, fox_y - 4.5f * fox_scale));
    right_ear.setPoint(2, sf::Vector2f(fox_x + 5.5f * fox_scale, fox_y - 9 * fox_scale));
    right_ear.setFillColor(sf::Color::White);
    fox_texture_.draw(right_ear);

    // Eyes
    sf::CircleShape fox_eye(1.2f * fox_scale);
    fox_eye.setFillColor(sf::Color::Black);
    fox_eye.setPosition(sf::Vector2f(fox_x - 2.5f * fox_scale - 1.2f * fox_scale, fox_y + 2 * fox_scale - 1.2f * fox_scale));
    fox_texture_.draw(fox_eye);
    fox_eye.setPosition(sf::Vector2f(fox_x + 2.5f * fox_scale - 1.2f * fox_scale, fox_y + 2 * fox_scale - 1.2f * fox_scale));
    fox_texture_.draw(fox_eye);

    fox_texture_.display();
    fox_sprite_ = std::make_unique<sf::Sprite>(fox_texture_.getTexture());
    fox_sprite_->setOrigin(sf::Vector2f(32, 32));
}

void SFMLRenderer::drawSprite(float x, float y, sf::Color color, sf::Sprite& sprite, float scale) {
    if (!window_) return;
    sprite.setColor(color);
    sprite.setScale(sf::Vector2f(scale, scale));
    sprite.setPosition(sf::Vector2f(x, y));
    window_->draw(sprite);
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

