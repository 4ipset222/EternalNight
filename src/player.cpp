#include "player.h"
#include "engine/forge.h"


Player::Player(float x, float y)
    : x(x), y(y), speed(200.0f), size(16.0f)
{
}

void Player::Update(float dt)
{
    if (Input_IsKeyDown(KEY_W)) y -= speed * dt;
    if (Input_IsKeyDown(KEY_S)) y += speed * dt;
    if (Input_IsKeyDown(KEY_A)) x -= speed * dt;
    if (Input_IsKeyDown(KEY_D)) x += speed * dt;
}

void Player::Draw() const
{
    DrawRectangle(
        Rect{ x - size / 2.0f, y - size / 2.0f, size, size },
        Color{1.0f, 0.0f, 0.0f, 1.0f}
    );
}

void Player::DrawHP() const
{
    Renderer* renderer = GetGlobalRenderer();
    if (!renderer) return;

    float barWidth = 200.0f;
    float barHeight = 20.0f;
    float margin = 10.0f;

    float healthPercent = hp / maxHP;
    float filledWidth = barWidth * healthPercent;

    float xPos = renderer->width - barWidth - margin;
    float yPos = renderer->height - barHeight - margin;

    DrawText("HP", xPos - 30.0f, yPos + 2.0f, 14, Color{1,1,1,1});
    DrawRectangle(Rect{xPos, yPos, barWidth, barHeight}, Color{0.2f, 0.2f, 0.2f, 1.0f});
    DrawRectangle(Rect{xPos, yPos, filledWidth, barHeight}, Color{1.0f, 0.0f, 0.0f, 1.0f});
    DrawRectangleLines(Rect{xPos, yPos, barWidth, barHeight}, 2.0f, Color{1.0f, 1.0f, 1.0f, 1.0f});
}