#include "evaluator.h"
#include "game/snake_game.h"
#include <algorithm>
#include <cmath>
#include <queue>

struct FloodResult { int count; bool foodReachable; int foodDist; };

// Flood fill from a cell using a pre-built occupancy grid.
// Returns count of reachable cells, whether food was found, and BFS distance to food.
static FloodResult floodFill(int W, int H, const bool* occupied, int startX, int startY, int foodIdx) {
    if (startX < 0 || startX >= W || startY < 0 || startY >= H) return {0, false, -1};
    int startIdx = startY * W + startX;
    if (occupied[startIdx]) return {0, false, -1};

    bool visited[20 * 15] = {};
    int dist[20 * 15];
    std::fill(dist, dist + 20 * 15, -1);
    std::queue<int> q;
    visited[startIdx] = true;
    dist[startIdx] = 0;
    q.push(startIdx);
    int count = 0;
    bool foundFood = false;
    int foodDist = -1;

    while (!q.empty()) {
        int cur = q.front(); q.pop();
        count++;
        if (cur == foodIdx) { foundFood = true; foodDist = dist[cur]; }
        int cx = cur % W, cy = cur / W;
        static const int ddx[] = {0, 0, 1, -1};
        static const int ddy[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; d++) {
            int nx = cx + ddx[d], ny = cy + ddy[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            int ni = ny * W + nx;
            if (visited[ni] || occupied[ni]) continue;
            visited[ni] = true;
            dist[ni] = dist[cur] + 1;
            q.push(ni);
        }
    }
    return {count, foundFood, foodDist};
}

void Evaluator::computeInputs(const SnakeGame& game, float* inputs, float hunger) {
    const auto& body = game.body();
    Vec2i head = body[0];
    Vec2i food = game.food();
    int W = game.gridW, H = game.gridH;
    float maxDist = (float)(W * H);

    // Build occupancy grid once
    bool occupied[20 * 15] = {};
    for (auto& s : body)
        occupied[s.y * W + s.x] = true;
    int foodIdx = food.y * W + food.x;

    // ---- [0-15] Raycast 8 dir × 2 canales: wall + food (16 inputs) ----
    // Body distance REMOVED — the 5×5 local vision covers nearby body better
    static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    for (int d = 0; d < 8; d++) {
        float distWall = 0;
        float foodFound = 0;
        int x = head.x, y = head.y;
        int steps = 0;

        while (true) {
            x += dx[d]; y += dy[d]; steps++;
            if (x < 0 || x >= W || y < 0 || y >= H) { distWall = 1.0f / steps; break; }
            if (x == food.x && y == food.y) foodFound = 1.0f;
        }
        inputs[d * 2 + 0] = distWall;
        inputs[d * 2 + 1] = foodFound;
    }

    // ---- [16-19] Direction one-hot (4 inputs) ----
    Direction dir = game.direction();
    inputs[16] = (dir == Direction::UP)    ? 1.0f : 0.0f;
    inputs[17] = (dir == Direction::RIGHT) ? 1.0f : 0.0f;
    inputs[18] = (dir == Direction::DOWN)  ? 1.0f : 0.0f;
    inputs[19] = (dir == Direction::LEFT)  ? 1.0f : 0.0f;

    // ---- BFS food: 4 flood fills (reused for multiple inputs) ----
    int totalEmpty = W * H - (int)body.size();
    float norm = (totalEmpty > 0) ? 1.0f / totalEmpty : 0.0f;

    auto ffUp    = floodFill(W, H, occupied, head.x, head.y - 1, foodIdx);
    auto ffRight = floodFill(W, H, occupied, head.x + 1, head.y, foodIdx);
    auto ffDown  = floodFill(W, H, occupied, head.x, head.y + 1, foodIdx);
    auto ffLeft  = floodFill(W, H, occupied, head.x - 1, head.y, foodIdx);

    // ---- [20-23] Flood fill space per direction (4 inputs) ----
    inputs[20] = ffUp.count * norm;
    inputs[21] = ffRight.count * norm;
    inputs[22] = ffDown.count * norm;
    inputs[23] = ffLeft.count * norm;

    // ---- [24] Hunger (1 input) ----
    inputs[24] = hunger;

    // ---- [25-28] BFS first-step direction to FOOD, one-hot (4 inputs) ----
    int bestFoodDist = -1;
    int bestFoodDir = -1;
    if (ffUp.foodReachable    && (bestFoodDist < 0 || ffUp.foodDist + 1    < bestFoodDist)) { bestFoodDist = ffUp.foodDist + 1;    bestFoodDir = 0; }
    if (ffRight.foodReachable && (bestFoodDist < 0 || ffRight.foodDist + 1 < bestFoodDist)) { bestFoodDist = ffRight.foodDist + 1; bestFoodDir = 1; }
    if (ffDown.foodReachable  && (bestFoodDist < 0 || ffDown.foodDist + 1  < bestFoodDist)) { bestFoodDist = ffDown.foodDist + 1;  bestFoodDir = 2; }
    if (ffLeft.foodReachable  && (bestFoodDist < 0 || ffLeft.foodDist + 1  < bestFoodDist)) { bestFoodDist = ffLeft.foodDist + 1;  bestFoodDir = 3; }
    for (int i = 0; i < 4; i++) inputs[25 + i] = (i == bestFoodDir) ? 1.0f : 0.0f;

    // ---- [29-30] Food relative direction, normalized vector (2 inputs) ----
    inputs[29] = (float)(food.x - head.x) / (float)W;
    inputs[30] = (float)(food.y - head.y) / (float)H;

    // ---- BFS tail: 4 flood fills with tail passable ----
    Vec2i tail = body.back();
    int tailIdx = tail.y * W + tail.x;
    bool occTail[20 * 15];
    std::copy(occupied, occupied + W * H, occTail);
    occTail[tailIdx] = false;

    auto ftUp    = floodFill(W, H, occTail, head.x, head.y - 1, tailIdx);
    auto ftRight = floodFill(W, H, occTail, head.x + 1, head.y, tailIdx);
    auto ftDown  = floodFill(W, H, occTail, head.x, head.y + 1, tailIdx);
    auto ftLeft  = floodFill(W, H, occTail, head.x - 1, head.y, tailIdx);

    // ---- [31-34] BFS first-step direction to TAIL, one-hot (4 inputs) ----
    int bestTailDist = -1;
    int bestTailDir = -1;
    if (ftUp.foodReachable)    { bestTailDist = ftUp.foodDist + 1; bestTailDir = 0; }
    if (ftRight.foodReachable && (bestTailDist < 0 || ftRight.foodDist + 1 < bestTailDist)) { bestTailDist = ftRight.foodDist + 1; bestTailDir = 1; }
    if (ftDown.foodReachable  && (bestTailDist < 0 || ftDown.foodDist + 1  < bestTailDist)) { bestTailDist = ftDown.foodDist + 1;  bestTailDir = 2; }
    if (ftLeft.foodReachable  && (bestTailDist < 0 || ftLeft.foodDist + 1  < bestTailDist)) { bestTailDist = ftLeft.foodDist + 1;  bestTailDir = 3; }
    for (int i = 0; i < 4; i++) inputs[31 + i] = (i == bestTailDir) ? 1.0f : 0.0f;

    // ---- [35-38] Safety per direction: can reach tail (4 inputs) ----
    inputs[35] = ftUp.foodReachable    ? 1.0f : 0.0f;
    inputs[36] = ftRight.foodReachable ? 1.0f : 0.0f;
    inputs[37] = ftDown.foodReachable  ? 1.0f : 0.0f;
    inputs[38] = ftLeft.foodReachable  ? 1.0f : 0.0f;

    // ---- [39-63] Local 5×5 vision (25 inputs) ----
    for (int ly = -2; ly <= 2; ly++) {
        for (int lx = -2; lx <= 2; lx++) {
            int wx = head.x + lx, wy = head.y + ly;
            float val = 0.0f;
            if (wx < 0 || wx >= W || wy < 0 || wy >= H)
                val = 1.0f;
            else if (occupied[wy * W + wx])
                val = 1.0f;
            inputs[39 + (ly + 2) * 5 + (lx + 2)] = val;
        }
    }
}

EvalResult Evaluator::run(Network& net, const NeatParams& params) {
    SnakeGame game(20, 15);
    game.start();

    int stepsNoFood = 0, totalSteps = 0, lastScore = 0;
    float inputs[64], outputs[4];

    auto manhattan = [](Vec2i a, Vec2i b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); };
    int prevDist = manhattan(game.body()[0], game.food());

    float approachBonus = 0;
    float stepsToward = 0;
    float stepsAway = 0;
    float efficiencyBonus = 0;
    float explorationBonus = 0;
    int stepsSinceLastFood = 0;

    // Track unique cells visited between fruits
    bool visitedCells[20 * 15] = {};
    int uniqueVisited = 0;
    int stepsThisFruit = 0;

    // Track death type
    bool diedToBody = false;

    while (game.state() == GameState::PLAYING) {
        int dynMaxSteps = 200 + game.score() * 2;
        float hunger = (float)stepsSinceLastFood / (float)dynMaxSteps;
        computeInputs(game, inputs, hunger);
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
        stepsThisFruit++;

        // Track unique cells visited
        Vec2i hd = game.body()[0];
        int cellIdx = hd.y * game.gridW + hd.x;
        if (!visitedCells[cellIdx]) {
            visitedCells[cellIdx] = true;
            uniqueVisited++;
        }

        if (game.score() > lastScore) {
            stepsNoFood = 0;
            lastScore = game.score();
            prevDist = manhattan(game.body()[0], game.food());
            efficiencyBonus += std::max(0.0f, 1.0f - (float)stepsSinceLastFood / (float)dynMaxSteps) * 1000.0f;
            stepsSinceLastFood = 0;
            approachBonus += stepsToward * 1.5f - stepsAway * 0.75f;
            stepsToward = 0;
            stepsAway = 0;
            if (stepsThisFruit > 0) {
                explorationBonus += (float)uniqueVisited / (float)stepsThisFruit * 500.0f;
            }
            std::fill(visitedCells, visitedCells + 20 * 15, false);
            uniqueVisited = 0;
            stepsThisFruit = 0;
        } else {
            stepsNoFood++;
            if (game.state() == GameState::PLAYING) {
                int dist = manhattan(game.body()[0], game.food());
                if (dist < prevDist) stepsToward += 1.0f;
                else if (dist > prevDist) stepsAway += 1.0f;
                prevDist = dist;
            }
            // Detect body collision death
            if (game.state() == GameState::GAME_OVER) {
                diedToBody = true;  // wall collision also ends here but body collision is more penalizable
            }
        }

        if (stepsNoFood >= dynMaxSteps) break;
    }

    int s = game.score();
    approachBonus += stepsToward * 1.5f - stepsAway * 0.75f;

    float fit = (float)s * 5000.0f + (float)(s * s) * 500.0f
              + efficiencyBonus
              + approachBonus
              + explorationBonus;

    // Endgame bonus: cubic ramp for scores >200 — creates strong gradient toward winning
    if (s > 200) {
        float d = (float)(s - 200);
        fit += d * d * d * 50.0f;  // 250→6.25M, 280→25.6M, 297→45.6M
    }

    // Penalize dying by collision (wall/body) harder than timeout
    // Timeout = snake tried but couldn't find food. Collision = bad navigation.
    if (diedToBody && s > 10) {
        fit *= 0.9f;  // 10% penalty for collision death at high scores
    }

    bool won = game.won();
    if (won) fit += 500000.0f;  // massive win bonus (was 50K, now 500K)
    if (fit < 0) fit = 0.001f;
    return {fit, s, totalSteps, won};
}
