#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "engine/forge.h"

class Player
{
public:
    Player(float x, float y);

    void Update(float dt, float moveX, float moveY, ForgeWorld* world, float tileSize);
    void Draw() const;
    void DrawHP() const;
    void DrawStamina() const;
    bool Attack(float dirX, float dirY);
    void SetWeaponSprite(Texture2D* sprite) { weaponSprite = sprite; }
    void SetPosition(float nx, float ny) { x = nx; y = ny; }
    void SetAttackState(bool attacking, float progress, float dirX, float dirY, float baseAngle);
    void SetColor(Color c) { color = c; }

    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetSize() const { return size; }
    float GetAttackRange() const { return attackRange; }
    float GetAttackDamage() const { return attackDamage; }
    float GetAttackArcCos() const { return attackArcCos; }

    void SetX(float nx) { x = nx; }
    void SetY(float ny) { y = ny; }
    float GetHP() const { return hp; }
    void  SetHP(float health) { hp = health; }
    float GetStamina() const { return stamina; }
    float GetMaxStamina() const { return maxStamina; }

private:
    float x;
    float y;
    float speed;
    float size;

    float hp = 100.0f;
    float maxHP = 100.0f;
    float stamina = 100.0f;
    float maxStamina = 100.0f;
    float staminaRegen = 25.0f;
    float staminaAttackCost = 20.0f;

    Texture2D* weaponSprite = nullptr;
    float attackTimer = 0.0f;
    float attackProgress = 0.0f;
    bool isAttacking = false;
    float attackCooldownTimer = 0.0f;
    float attackDuration = 0.3f;
    float attackCooldown = 0.4f;
    float attackRange = 55.0f;
    float attackDamage = 12.0f;
    float attackArcCos = 0.35f;
    float attackDirX = 1.0f;
    float attackDirY = 0.0f;
    float attackBaseAngle = 0.0f;
    float attackSwingArc = 1.4f;
    float attackSpriteAngleOffset = 0.0f;

    Color color = {1.0f, 0.0f, 0.0f, 1.0f};
};

#endif // __PLAYER_H__
