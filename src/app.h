#pragma once
#include "game/snake_game.h"
#include "neat/population.h"
#include "eval/network.h"
#include "eval/evaluator.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

struct GLFWwindow;
struct ImFont;
struct ImDrawList;

enum class AppMode { PLAY, AI_TRAIN };

static constexpr int NUM_SHOWCASE = 24;
static constexpr int GRID_COLS = 6;
static constexpr int GRID_ROWS = 4;

struct TrainLogEntry {
    float   time;         // seconds since training started
    int     maxScore;
    int     generation;
    int     totalGames;   // training games
    int     showcaseGames;
    int     showcaseWins;
    float   winRate;      // showcase win rate %
    float   bestFitness;
    int     species;
    std::string text;     // formatted line for display
};

struct ShowcaseGame {
    SnakeGame game{20, 15};
    Network   net;
    float     accum = 0.0f;
    int       gamesPlayed = 0;
    int       maxScore = 0;
    int       stepsSinceFood = 0;
};

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
    // Rendering modes
    void renderPlayMode();
    void renderTrainMode();

    // Play mode drawing
    void drawGrid(float gx, float gy, float cs, const SnakeGame& g);
    void drawFood(float gx, float gy, float cs, const SnakeGame& g);
    void drawSnake(float gx, float gy, float cs, const SnakeGame& g, float flash);
    void drawEyes(float gx, float gy, float cs, const SnakeGame& g);
    void drawRaycasts(float gx, float gy, float cs, const SnakeGame& g);
    void drawCenteredText(ImFont* f, const char* t, float cx, float cy, unsigned col);

    // Train mode drawing
    void drawMiniGame(ImDrawList* dl, float cx, float cy, float cw, float ch, const ShowcaseGame& sg);
    void drawTrainStatsPanel();
    void drawTrainLogPanel();
    void saveTrainLog();

    // Play mode UI
    void drawPlayPanel(float px, float py, float pw, float ph);
    void drawPlayHeader(float w);
    void drawPlayOverlay(float gx, float gy, float gw, float gh);

    // Training
    void startTraining();
    void stopTraining();
    void trainLoop();

    // Model save/load
    void saveModel();
    void loadModel();

    GLFWwindow* window_ = nullptr;
    AppMode mode_        = AppMode::PLAY;
    bool shouldClose_    = false;

    // ---- Play mode ----
    SnakeGame game_{20, 15};
    float stepAccum_  = 0;
    float eatFlash_   = 0;
    float deathShake_ = 0;
    bool  watchAI_    = false;      // auto-play with best genome in PLAY mode
    int   playStepsSinceFood_ = 0;
    Network playNet_;
    Genome  savedBestGenome_;

    // ---- AI Training thread ----
    std::thread       trainThread_;
    std::mutex        trainMutex_;
    std::atomic<bool> training_{false};
    std::atomic<bool> stopFlag_{false};
    std::atomic<bool> trainPaused_{false};
    Population population_;

    float trainElapsed_ = 0;  // seconds of training time

    // ---- Showcase (24 visible games) ----
    std::vector<ShowcaseGame> showcase_;
    Genome currentShowcaseGenome_;
    bool   hasShowcaseGenome_ = false;
    int    totalShowcaseGames_ = 0;
    int    showcaseMaxScore_   = 0;
    int    showcaseWins_       = 0;
    float  avgScoreAccum_     = 0;
    int    avgScoreCount_     = 0;
    int    winBatchCount_     = 0;
    int    winBatchTotal_     = 0;
    float  lastAvgScore_       = 0;
    std::vector<float> avgScoreHist_;
    std::vector<float> winRateHist_;

    // Shared display state (protected by trainMutex_)
    int   dGen_         = 0;
    int   dGames_       = 0;
    float dBestFit_     = 0;
    int   dBestScore_   = 0;
    int   dMaxScore_    = 0;
    int   dSpecies_     = 0;
    std::vector<float> dScoreHist_;
    Genome dBestGenome_;
    bool   dNewGen_     = false;

    // ---- Training log ----
    std::vector<TrainLogEntry> trainLog_;
    int loggedMaxScore_ = 0;
};
