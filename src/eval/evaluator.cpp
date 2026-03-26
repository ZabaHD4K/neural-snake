#include "evaluator.h"
#include "game/snake_game.h"
#include <algorithm>
#include <cmath>

void Evaluator::computeInputs(const SnakeGame& game, float* inputs) {
    const auto& body = game.body();
    Vec2i head = body[0];
    Vec2i food = game.food();

    // 8-direction raycast: for each direction, report:
    //   - normalized distance to wall (1/dist)
    //   - normalized distance to body (1/dist, 0 if none)
    //   - food visible in this direction (1 or 0)
    // Total: 8 × 3 = 24 inputs
    static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    for (int d = 0; d < 8; d++) {
        float distWall = 0;
        float distBody = 0;
        float foodFound = 0;

        int x = head.x, y = head.y;
        int steps = 0;
        bool bodyHit = false;

        while (true) {
            x += dx[d];
            y += dy[d];
            steps++;

            // Hit wall?
            if (x < 0 || x >= game.gridW || y < 0 || y >= game.gridH) {
                distWall = 1.0f / steps;
                break;
            }

            // Food in this direction?
            if (x == food.x && y == food.y)
                foodFound = 1.0f;

            // Body in this direction? (record first hit)
            if (!bodyHit) {
                for (int i = 1; i < (int)body.size(); i++) {
                    if (body[i].x == x && body[i].y == y) {
                        distBody = 1.0f / steps;
                        bodyHit = true;
                        break;
                    }
                }
            }
        }

        inputs[d * 3 + 0] = distWall;
        inputs[d * 3 + 1] = distBody;
        inputs[d * 3 + 2] = foodFound;
    }

    // Direction one-hot (4 inputs)
    Direction dir = game.direction();
    inputs[24] = (dir == Direction::UP)    ? 1.0f : 0.0f;
    inputs[25] = (dir == Direction::RIGHT) ? 1.0f : 0.0f;
    inputs[26] = (dir == Direction::DOWN)  ? 1.0f : 0.0f;
    inputs[27] = (dir == Direction::LEFT)  ? 1.0f : 0.0f;
}

EvalResult Evaluator::run(Network& net, const NeatParams& params) {
    SnakeGame game(20, 15);
    game.start();

    int stepsNoFood = 0, totalSteps = 0, lastScore = 0;
    float inputs[28], outputs[4];

    auto manhattan = [](Vec2i a, Vec2i b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); };
    int prevDist = manhattan(game.body()[0], game.food());

    float stepsToward = 0;
    float stepsAway = 0;
    float efficiencyBonus = 0;  // bonus for eating quickly
    int stepsSinceLastFood = 0;

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
        stepsSinceLastFood++;

        if (game.score() > lastScore) {
            stepsNoFood = 0;
            lastScore = game.score();
            prevDist = manhattan(game.body()[0], game.food());
            // Bonus for eating fast: max bonus if eaten in ~10 steps, less if slower
            efficiencyBonus += std::max(0.0f, 1.0f - (float)stepsSinceLastFood / (float)params.maxStepsPerFood) * 1000.0f;
            stepsSinceLastFood = 0;
        } else {
            stepsNoFood++;
            if (game.state() == GameState::PLAYING) {
                int dist = manhattan(game.body()[0], game.food());
                if (dist < prevDist) stepsToward += 1.0f;
                else stepsAway += 1.0f;
                prevDist = dist;
            }
        }

        if (stepsNoFood >= params.maxStepsPerFood) break;
    }

    int s = game.score();
    // Gradient fitness:
    //   - score is king (5000 per food + quadratic)
    //   - efficiency bonus rewards eating quickly (anti-loop)
    //   - approach/retreat gives continuous signal for non-eaters
    float fit = (float)s * 5000.0f + (float)(s * s) * 500.0f
              + efficiencyBonus
              + stepsToward * 1.5f - stepsAway * 2.0f;
    if (fit < 0) fit = 0.001f;
    return {fit, s, totalSteps};
}
