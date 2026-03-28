#include "snake_game.h"
#include <algorithm>
#include <chrono>

SnakeGame::SnakeGame(int w, int h) : gridW(w), gridH(h) {
    rng_ = (uint32_t)std::chrono::steady_clock::now().time_since_epoch().count();
    reset();
}

SnakeGame::SnakeGame(int w, int h, uint32_t seed) : gridW(w), gridH(h) {
    rng_ = seed;
    reset();
}

uint32_t SnakeGame::xorshift() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

int SnakeGame::randInt(int max) {
    return (int)(xorshift() % (uint32_t)max);
}

void SnakeGame::reset() {
    body_.clear();
    int cx = gridW / 2;
    int cy = gridH / 2;
    for (int i = 0; i < 3; i++)
        body_.push_back({cx - i, cy});

    dir_     = Direction::RIGHT;
    nextDir_ = Direction::RIGHT;
    score_   = 0;
    justAte_ = false;
    state_   = GameState::READY;
    spawnFood();
}

void SnakeGame::start() {
    if (state_ == GameState::READY || state_ == GameState::GAME_OVER || state_ == GameState::WIN) {
        if (state_ == GameState::GAME_OVER || state_ == GameState::WIN) reset();
        state_ = GameState::PLAYING;
    }
}

void SnakeGame::togglePause() {
    if (state_ == GameState::PLAYING)
        state_ = GameState::PAUSED;
    else if (state_ == GameState::PAUSED)
        state_ = GameState::PLAYING;
}

void SnakeGame::setDirection(Direction d) {
    if (dir_ == Direction::UP    && d == Direction::DOWN)  return;
    if (dir_ == Direction::DOWN  && d == Direction::UP)    return;
    if (dir_ == Direction::LEFT  && d == Direction::RIGHT) return;
    if (dir_ == Direction::RIGHT && d == Direction::LEFT)  return;
    nextDir_ = d;
}

void SnakeGame::step() {
    if (state_ != GameState::PLAYING) return;

    dir_ = nextDir_;
    justAte_ = false;

    Vec2i head = body_[0];
    switch (dir_) {
        case Direction::UP:    head.y--; break;
        case Direction::DOWN:  head.y++; break;
        case Direction::LEFT:  head.x--; break;
        case Direction::RIGHT: head.x++; break;
    }

    // Wall collision
    if (head.x < 0 || head.x >= gridW || head.y < 0 || head.y >= gridH) {
        state_ = GameState::GAME_OVER;
        if (score_ > highScore_) highScore_ = score_;
        return;
    }

    // Self collision (ignore tail since it moves away)
    for (int i = 0; i < (int)body_.size() - 1; i++) {
        if (body_[i] == head) {
            state_ = GameState::GAME_OVER;
            if (score_ > highScore_) highScore_ = score_;
            return;
        }
    }

    body_.insert(body_.begin(), head);

    if (head == food_) {
        score_++;
        justAte_ = true;
        spawnFood();
    } else {
        body_.pop_back();
    }
}

void SnakeGame::spawnFood() {
    std::vector<Vec2i> empty;
    empty.reserve(gridW * gridH);
    for (int y = 0; y < gridH; y++) {
        for (int x = 0; x < gridW; x++) {
            Vec2i p{x, y};
            bool occupied = false;
            for (auto& s : body_) {
                if (s == p) { occupied = true; break; }
            }
            if (!occupied) empty.push_back(p);
        }
    }
    if (empty.empty()) {
        state_ = GameState::WIN;
        if (score_ > highScore_) highScore_ = score_;
        return;
    }
    food_ = empty[randInt((int)empty.size())];
}

float SnakeGame::stepInterval() const {
    float base = 0.14f;
    float speedup = score_ * 0.003f;
    return std::max(0.05f, base - speedup);
}
