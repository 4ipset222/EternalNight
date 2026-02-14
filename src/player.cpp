#include "player.h"
#include <math.h>

Player::Player(Vec2 position)
    : position(position), speed(200.0f), size(16.0f)
{
}

void Player::Update(float dt, Vec2 move, ForgeWorld* world, float tileSize)
{
    float mx = move.x;
    float my = move.y;
    float lenSq = mx * mx + my * my;
    if (lenSq > 1.0f)
    {
        float inv = 1.0f / sqrtf(lenSq);
        mx *= inv;
        my *= inv;
    }

    float dx = mx * speed * dt;
    float dy = my * speed * dt;
    float radius = size * 0.45f;
    World_MoveWithCollision(world, tileSize, radius, &position.x, &position.y, dx, dy);

    if (stamina < maxStamina)
    {
        stamina += staminaRegen * dt;
        if (stamina > maxStamina) stamina = maxStamina;
    }

    if (attackTimer > 0.0f)
        attackTimer -= dt;
    if (attackCooldownTimer > 0.0f)
        attackCooldownTimer -= dt;

    if (isAttacking)
    {
        attackProgress += dt / attackDuration;
        if (attackProgress >= 1.0f)
        {
            attackProgress = 1.0f;
            isAttacking = false;
        }
    }
}

void Player::Draw() const
{
    Renderer_DrawRectangle(
        Rect{ position.x - size / 2.0f, position.y - size / 2.0f, size, size },
        color
    );

    if (isAttacking && weaponSprite)
    {
        float t = attackProgress;
        t = t * t * (3.0f - 2.0f * t);
        float swingAngle = attackBaseAngle - attackSwingArc * 0.5f + attackSwingArc * t;
        float angleRad = swingAngle + attackSpriteAngleOffset;

        float drawW = (float)weaponSprite->width;
        float drawH = (float)weaponSprite->height;
        float maxDim = (drawW > drawH) ? drawW : drawH;
        float targetLength = size * 1.9f;
        float scale = targetLength / maxDim;
        Rect src = { 0, 0, drawW, drawH };
        float reach = size * 0.55f;
        float sx = position.x + attackDir.x * reach;
        float sy = position.y + attackDir.y * reach;
        Rect dst = { sx, sy, drawW * scale, drawH * scale };
        Vec2 origin = { (drawW * scale) * 0.15f, (drawH * scale) * 0.5f };

        Renderer_DrawTexturePro(*weaponSprite, src, dst, origin, angleRad, Color{1, 1, 1, 1});
    }
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

    Renderer_DrawText("HP", xPos - 30.0f, yPos + 2.0f, 14, Color{1,1,1,1});
    Renderer_DrawRectangle(Rect{xPos, yPos, barWidth, barHeight}, Color{0.2f, 0.2f, 0.2f, 1.0f});
    Renderer_DrawRectangle(Rect{xPos, yPos, filledWidth, barHeight}, Color{1.0f, 0.0f, 0.0f, 1.0f});
    Renderer_DrawRectangleLines(Rect{xPos, yPos, barWidth, barHeight}, 2.0f, Color{1.0f, 1.0f, 1.0f, 1.0f});
}

void Player::DrawStamina() const
{
    Renderer* renderer = GetGlobalRenderer();
    if (!renderer) return;

    float barWidth = 200.0f;
    float barHeight = 18.0f;
    float margin = 10.0f;
    float gap = 6.0f;

    float staminaPercent = stamina / maxStamina;
    float filledWidth = barWidth * staminaPercent;

    float xPos = renderer->width - barWidth - margin;
    float yPos = renderer->height - barHeight - margin - (20.0f + gap);

    Renderer_DrawText("ST", xPos - 30.0f, yPos + 2.0f, 14, Color{1,1,1,1});
    Renderer_DrawRectangle(Rect{xPos, yPos, barWidth, barHeight}, Color{0.2f, 0.2f, 0.2f, 1.0f});
    Renderer_DrawRectangle(Rect{xPos, yPos, filledWidth, barHeight}, Color{0.2f, 0.6f, 1.0f, 1.0f});
    Renderer_DrawRectangleLines(Rect{xPos, yPos, barWidth, barHeight}, 2.0f, Color{1.0f, 1.0f, 1.0f, 1.0f});
}

bool Player::Attack(Vec2 dir)
{
    if (attackCooldownTimer > 0.0f)
        return false;
    if (stamina < staminaAttackCost)
        return false;

    float lenSq = dir.x * dir.x + dir.y * dir.y;
    if (lenSq < 0.0001f)
        return false;

    float invLen = 1.0f / sqrtf(lenSq);
    attackDir.x = dir.x * invLen;
    attackDir.y = dir.y * invLen;
    attackBaseAngle = atan2f(attackDir.y, attackDir.x);
    attackTimer = attackDuration;
    attackCooldownTimer = attackCooldown;
    attackProgress = 0.0f;
    isAttacking = true;
    stamina -= staminaAttackCost;
    if (stamina < 0.0f) stamina = 0.0f;
    return true;
}

void Player::SetAttackState(bool attacking, float progress, Vec2 dir, float baseAngle)
{
    isAttacking = attacking;
    attackProgress = progress;
    attackDir = dir;
    attackBaseAngle = baseAngle;
}
