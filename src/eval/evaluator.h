#pragma once
#include "network.h"
#include "neat/neat_params.h"
#include "game/snake_game.h"

struct EvalResult {
    float fitness;
    int   score;
    int   steps;
    bool  won = false;
};

namespace Evaluator {
    EvalResult run(Network& net, const NeatParams& params);
    void computeInputs(const SnakeGame& game, float* inputs, float hunger = 0.0f);
}
