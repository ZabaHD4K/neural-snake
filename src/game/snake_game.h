#pragma once
#include <vector>
#include <cstdint>

struct Vec2i {
    int x, y;
    bool operator==(const Vec2i& o) const { return x == o.x && y == o.y; }
};

enum class Direction { UP, RIGHT, DOWN, LEFT };
enum class GameState { READY, PLAYING, PAUSED, GAME_OVER, WIN };

class SnakeGame {
public:
    int gridW = 20;
    int gridH = 15;

    SnakeGame(int w = 20, int h = 15);
    SnakeGame(int w, int h, uint32_t seed);
    void reset();
    void setDirection(Direction dir);
    void step();
    void start();
    void togglePause();

    const std::vector<Vec2i>& body() const { return body_; }
    Vec2i food() const { return food_; }
    Direction direction() const { return dir_; }
    GameState state() const { return state_; }
    int score() const { return score_; }
    int highScore() const { return highScore_; }
    float stepInterval() const;
    bool justAte() const { return justAte_; }
    bool won() const { return state_ == GameState::WIN; }

private:
    void spawnFood();

    std::vector<Vec2i> body_;
    Vec2i food_{};
    Direction dir_       = Direction::RIGHT;
    Direction nextDir_   = Direction::RIGHT;
    GameState state_     = GameState::READY;
    int  score_          = 0;
    int  highScore_      = 0;
    bool justAte_        = false;
    uint32_t rng_;

    uint32_t xorshift();
    int randInt(int max);
};
