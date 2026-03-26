#pragma once
#include "game/snake_game.h"
#include "neat/population.h"
#include "eval/network.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

struct GLFWwindow;
struct ImFont;

enum class AppMode { PLAY, AI_TRAIN };

class App {
public:
    void init(GLFWwindow* win);
    void shutdown();
    void update(float dt);
    void render();
    void keyCallback(int key, int action);
    bool shouldClose() const { return shouldClose_; }

    ImFont* fontSmall  = nullptr;
    ImFont* fontMed    = nullptr;
    ImFont* fontLarge  = nullptr;
    ImFont* fontTitle  = nullptr;

private:
    // Snake rendering (shared between modes)
    void drawGrid(float gx, float gy, float cs, const SnakeGame& g);
    void drawFood(float gx, float gy, float cs, const SnakeGame& g);
    void drawSnake(float gx, float gy, float cs, const SnakeGame& g, float flash);
    void drawEyes(float gx, float gy, float cs, const SnakeGame& g);
    void drawHeader(float w);
    void drawOverlay(float gx, float gy, float gw, float gh);
    void drawCenteredText(ImFont* f, const char* t, float cx, float cy, unsigned col);

    // AI panel
    void drawPanel(float px, float py, float pw, float ph);
    void drawNetworkViz(float x, float y, float w, float h);

    // Training
    void startTraining();
    void stopTraining();
    void trainLoop();

    GLFWwindow* window_ = nullptr;
    AppMode mode_        = AppMode::PLAY;
    bool shouldClose_    = false;

    // ---- Play mode ----
    SnakeGame game_{20, 15};
    float stepAccum_  = 0;
    float eatFlash_   = 0;
    float deathShake_ = 0;

    // ---- AI mode ----
    std::thread       trainThread_;
    std::mutex        trainMutex_;
    std::atomic<bool> training_{false};
    std::atomic<bool> stopFlag_{false};

    Population population_;
    Network    replayNet_;
    Network    vizNet_;          // copy for visualization
    SnakeGame  replayGame_{20, 15};
    float      replayAccum_  = 0;
    float      replayFlash_  = 0;
    float      replaySpeed_  = 3.0f;
    bool       turbo_        = false;
    bool       replayPaused_ = false;

    // Shared display state (protected by trainMutex_)
    int   dGen_         = 0;
    float dBestFit_     = 0;
    float dAvgFit_      = 0;
    int   dBestScore_   = 0;
    int   dSpecies_     = 0;
    std::vector<float> dFitHist_;
    std::vector<float> dAvgHist_;
    Genome dBestGenome_;
    bool   dNewGen_     = false;

    // Buffered genome: apply only when current replay game ends
    Genome pendingGenome_;
    bool   hasPending_  = false;
};
