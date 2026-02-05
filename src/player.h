#ifndef __PLAYER_H__
#define __PLAYER_H__

class Player
{
public:
    Player(float x, float y);

    void Update(float dt);
    void Draw() const;
    void DrawHP() const;

    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetSize() const { return size; }

    float GetHP() const { return hp; }
    void  SetHP(float health) { hp = health; }

private:
    float x;
    float y;
    float speed;
    float size;

    float hp = 75.0f;
    float maxHP = 100.0f;
};

#endif // __PLAYER_H__
