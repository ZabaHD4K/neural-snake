#include "evaluator.h"
#include "game/snake_game.h"
#include <algorithm>
#include <cmath>

void Evaluator::computeInputs(const SnakeGame& game, float* inputs) {
    const auto& body = game.body();
    Vec2i head = body[0];
    Vec2i food = game.food();

    // Food direction [-1, 1]
    inputs[0] = (float)(food.x - head.x) / game.gridW;
    inputs[1] = (float)(food.y - head.y) / game.gridH;

    // Danger in 8 directions: N NE E SE S SW W NW
    static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    for (int d = 0; d < 8; d++) {
        int nx = head.x + dx[d], ny = head.y + dy[d];
        bool danger = false;
        if (nx < 0 || nx >= game.gridW || ny < 0 || ny >= game.gridH) {
            danger = true;
        } else {
            for (int i = 0; i < (int)body.size() - 1; i++)
                if (body[i].x == nx && body[i].y == ny) { danger = true; break; }
        }
        inputs[2 + d] = danger ? 1.0f : 0.0f;
    }

    // Direction one-hot
    Direction dir = game.direction();
    inputs[10] = (dir == Direction::UP)    ? 1.0f : 0.0f;
    inputs[11] = (dir == Direction::RIGHT) ? 1.0f : 0.0f;
    inputs[12] = (dir == Direction::DOWN)  ? 1.0f : 0.0f;
    inputs[13] = (dir == Direction::LEFT)  ? 1.0f : 0.0f;
}

EvalResult Evaluator::run(Network& net, const NeatParams& params) {
    SnakeGame game(20, 15);
    game.start();

    int stepsNoFood = 0, totalSteps = 0, lastScore = 0;
    float inputs[14], outputs[4];

    // Track distance to food to reward approaching it
    auto manhattan = [](Vec2i a, Vec2i b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); };
    int prevDist = manhattan(game.body()[0], game.food());
    float approachBonus = 0;

    while (game.state() == GameState::PLAYING) {
        computeInputs(game, inputs);
        net.forward(inputs, outputs);

        // Argmax
        int best = 0;
        for (int i = 1; i < 4; i++)
            if (outputs[i] > outputs[best]) best = i;

        static const Direction dirs[] = {Direction::UP, Direction::RIGHT, Direction::DOWN, Direction::LEFT};
        game.setDirection(dirs[best]);
        game.step();
        totalSteps++;

        if (game.score() > lastScore) {
            stepsNoFood = 0;
            lastScore = game.score();
            prevDist = manhattan(game.body()[0], game.food());
        } else {
            stepsNoFood++;
            // Reward getting closer to food, penalize moving away
            if (game.state() == GameState::PLAYING) {
                int dist = manhattan(game.body()[0], game.food());
                if (dist < prevDist) approachBonus += 1.0f;
                else approachBonus -= 1.2f;
                prevDist = dist;
            }
        }

        if (stepsNoFood >= params.maxStepsPerFood) break;
    }

    int s = game.score();
    // Score is king: big reward per food eaten + quadratic bonus
    // Small approach bonus encourages moving toward food (not looping)
    // No raw totalSteps bonus — looping is not rewarded
    float fit = (float)s * 5000.0f + (float)(s * s) * 500.0f
              + std::max(0.0f, approachBonus);
    return {fit, s, totalSteps};
}
