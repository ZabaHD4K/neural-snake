#include "app.h"
#include "eval/evaluator.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <filesystem>

// ---- Google Snake palette ----
static const ImU32 COL_BG          = IM_COL32(87, 138, 52, 255);
static const ImU32 COL_GRID_LIGHT  = IM_COL32(170, 215, 81, 255);
static const ImU32 COL_GRID_DARK   = IM_COL32(162, 209, 73, 255);
static const ImU32 COL_BORDER      = IM_COL32(56, 107, 34, 255);
static const ImU32 COL_SNAKE       = IM_COL32(70, 116, 233, 255);
static const ImU32 COL_SNAKE_HEAD  = IM_COL32(60, 100, 220, 255);
static const ImU32 COL_SNAKE_FLASH = IM_COL32(130, 180, 255, 255);
static const ImU32 COL_EYE_WHITE   = IM_COL32(255, 255, 255, 255);
static const ImU32 COL_EYE_PUPIL   = IM_COL32(30, 30, 30, 255);
static const ImU32 COL_FOOD        = IM_COL32(231, 71, 29, 255);
static const ImU32 COL_FOOD_HI     = IM_COL32(255, 120, 80, 180);
static const ImU32 COL_LEAF        = IM_COL32(76, 140, 43, 255);
static const ImU32 COL_STEM        = IM_COL32(90, 65, 30, 255);
static const ImU32 COL_HEADER      = IM_COL32(74, 117, 44, 255);
static const ImU32 COL_HEADER_DK   = IM_COL32(55, 90, 33, 255);
static const ImU32 COL_TEXT        = IM_COL32(255, 255, 255, 255);
static const ImU32 COL_TEXT_DIM    = IM_COL32(200, 230, 180, 200);
static const ImU32 COL_OVERLAY     = IM_COL32(0, 0, 0, 150);
static const ImU32 COL_ACCENT      = IM_COL32(70, 116, 233, 255);

static constexpr float HEADER_H = 56.0f;
static constexpr float PAD      = 14.0f;
static constexpr float PANEL_W  = 340.0f;
static constexpr float SHOWCASE_SPEED = 20.0f;

// ========== Init / Shutdown ==========

void App::init(GLFWwindow* win) {
    window_ = win;
    game_ = SnakeGame(20, 15);
}

void App::shutdown() {
    stopTraining();
}

// ========== Training thread ==========

void App::startTraining() {
    if (training_) return;
    NeatParams p;
    population_.init(p);
    stopFlag_ = false;
    trainPaused_ = false;
    training_ = true;
    trainElapsed_ = 0;
    hasShowcaseGenome_ = false;
    totalShowcaseGames_ = 0;
    showcaseMaxScore_ = 0;
    showcaseWins_ = 0;
    avgScoreAccum_ = 0;
    avgScoreCount_ = 0;
    winBatchCount_ = 0;
    winBatchTotal_ = 0;
    avgScoreHist_.clear();
    winRateHist_.clear();
    trainLog_.clear();
    loggedMaxScore_ = 0;

    // Init 48 showcase games with unique seeds
    showcase_.resize(NUM_SHOWCASE);
    for (int i = 0; i < NUM_SHOWCASE; i++) {
        showcase_[i].game = SnakeGame(20, 15, (uint32_t)(i * 7919 + 42));
        showcase_[i].accum = 0;
        showcase_[i].gamesPlayed = 0;
        showcase_[i].maxScore = 0;
    }

    trainThread_ = std::thread(&App::trainLoop, this);
}

void App::stopTraining() {
    if (!training_) return;
    stopFlag_ = true;
    if (trainThread_.joinable()) trainThread_.join();
    training_ = false;

    // Save best genome for PLAY mode
    {
        std::lock_guard<std::mutex> lock(trainMutex_);
        savedBestGenome_ = dBestGenome_;
    }

    saveTrainLog();
}

void App::trainLoop() {
    while (!stopFlag_) {
        if (trainPaused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        population_.epoch();

        std::lock_guard<std::mutex> lock(trainMutex_);
        dGen_       = population_.generation();
        dGames_     = population_.totalGames();
        dBestFit_   = population_.bestFitness();
        dBestScore_ = population_.bestScore();
        dMaxScore_  = population_.allTimeBestScore();
        dSpecies_   = population_.numSpecies();
        dScoreHist_ = population_.scoreHistory();
        dBestGenome_ = population_.bestGenome();
        dNewGen_    = true;
    }
}

// ========== Update ==========

void App::update(float dt) {
    if (eatFlash_ > 0)   eatFlash_ -= dt;
    if (deathShake_ > 0) deathShake_ -= dt;

    if (mode_ == AppMode::PLAY) {
        if (watchAI_ && game_.state() == GameState::PLAYING) {
            // AI auto-play
            stepAccum_ += dt;
            float iv = game_.stepInterval();
            while (stepAccum_ >= iv && game_.state() == GameState::PLAYING) {
                stepAccum_ -= iv;
                float hunger = (float)playStepsSinceFood_ / (float)(200 + game_.score() * 2);
                float inputs[64], outputs[4];
                Evaluator::computeInputs(game_, inputs, hunger);
                playNet_.forward(inputs, outputs);
                int best = 0;
                for (int i = 1; i < 4; i++)
                    if (outputs[i] > outputs[best]) best = i;
                static const Direction dirs[] = {Direction::UP, Direction::RIGHT, Direction::DOWN, Direction::LEFT};
                game_.setDirection(dirs[best]);
                game_.step();
                playStepsSinceFood_++;
                if (game_.justAte()) { eatFlash_ = 0.25f; playStepsSinceFood_ = 0; }
                if (game_.state() == GameState::GAME_OVER) {
                    deathShake_ = 0.3f;
                }
            }
        } else if (watchAI_ && (game_.state() == GameState::GAME_OVER || game_.state() == GameState::WIN)) {
            // Auto-restart after a short delay
            deathShake_ -= dt;
            if (deathShake_ <= 0) {
                game_.reset();
                game_.start();
                stepAccum_ = 0;
                playStepsSinceFood_ = 0;
            }
        } else if (!watchAI_ && game_.state() == GameState::PLAYING) {
            // Manual play
            stepAccum_ += dt;
            float iv = game_.stepInterval();
            while (stepAccum_ >= iv) {
                stepAccum_ -= iv;
                game_.step();
                if (game_.justAte()) eatFlash_ = 0.25f;
                if (game_.state() == GameState::GAME_OVER) deathShake_ = 0.3f;
            }
        }
    } else {
        // AI TRAIN mode — accumulate training time
        if (!trainPaused_) trainElapsed_ += dt;

        // Update showcase genome from training thread
        {
            std::lock_guard<std::mutex> lock(trainMutex_);
            if (dNewGen_) {
                currentShowcaseGenome_ = dBestGenome_;
                hasShowcaseGenome_ = true;
                dNewGen_ = false;

                // Log when max score increases
                if (dMaxScore_ > loggedMaxScore_) {
                    loggedMaxScore_ = dMaxScore_;
                    float wr = totalShowcaseGames_ > 0
                        ? (float)showcaseWins_ / totalShowcaseGames_ * 100.0f : 0;
                    TrainLogEntry entry;
                    entry.time = trainElapsed_;
                    entry.maxScore = dMaxScore_;
                    entry.generation = dGen_;
                    entry.totalGames = dGames_;
                    entry.showcaseGames = totalShowcaseGames_;
                    entry.showcaseWins = showcaseWins_;
                    entry.winRate = wr;
                    entry.bestFitness = dBestFit_;
                    entry.species = dSpecies_;
                    int m = (int)trainElapsed_ / 60;
                    int s = (int)trainElapsed_ % 60;
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                        "[%02d:%02d] MAX %d  |  Avg %.1f  |  Gen %d  |  Games %d  |  WinRate %.1f%%  |  Species %d  |  Fit %.0f",
                        m, s, dMaxScore_, lastAvgScore_, dGen_, dGames_ + totalShowcaseGames_, wr, dSpecies_, dBestFit_);
                    entry.text = buf;
                    trainLog_.push_back(entry);
                }
            }
        }

        // Step all showcase games (skip if paused)
        if (!trainPaused_) {
            for (auto& sg : showcase_) {
                if (sg.game.state() == GameState::PLAYING) {
                    sg.accum += dt * SHOWCASE_SPEED;
                    float iv = sg.game.stepInterval();
                    while (sg.accum >= iv && sg.game.state() == GameState::PLAYING) {
                        sg.accum -= iv;
                        float hunger = (float)sg.stepsSinceFood / (float)(200 + sg.game.score() * 2);
                        float inputs[64], outputs[4];
                        Evaluator::computeInputs(sg.game, inputs, hunger);
                        sg.net.forward(inputs, outputs);
                        int best = 0;
                        for (int i = 1; i < 4; i++)
                            if (outputs[i] > outputs[best]) best = i;
                        static const Direction dirs[] = {Direction::UP, Direction::RIGHT, Direction::DOWN, Direction::LEFT};
                        sg.game.setDirection(dirs[best]);
                        sg.game.step();
                        sg.stepsSinceFood++;
                        if (sg.game.justAte()) { sg.stepsSinceFood = 0; }
                    }
                } else if (sg.game.state() == GameState::GAME_OVER || sg.game.state() == GameState::WIN) {
                    // Record stats
                    int s = sg.game.score();
                    bool won = sg.game.won();
                    avgScoreAccum_ += s;
                    avgScoreCount_++;
                    winBatchTotal_++;
                    if (won) { showcaseWins_++; winBatchCount_++; }
                    sg.gamesPlayed++;
                    totalShowcaseGames_++;
                    if (s > sg.maxScore) sg.maxScore = s;
                    if (s > showcaseMaxScore_) showcaseMaxScore_ = s;

                    // Restart with latest genome
                    if (hasShowcaseGenome_) {
                        sg.net.build(currentShowcaseGenome_, 64, 4);
                        sg.game = SnakeGame(20, 15, (uint32_t)(totalShowcaseGames_ * 7919 + sg.gamesPlayed));
                        sg.game.start();
                        sg.accum = 0;
                        sg.stepsSinceFood = 0;
                    }
                } else if (sg.game.state() == GameState::READY && hasShowcaseGenome_) {
                    // First start
                    sg.net.build(currentShowcaseGenome_, 64, 4);
                    sg.game.start();
                }
            }

            // Push average score and win rate periodically
            if (avgScoreCount_ >= NUM_SHOWCASE) {
                lastAvgScore_ = avgScoreAccum_ / avgScoreCount_;
                avgScoreHist_.push_back(lastAvgScore_);
                avgScoreAccum_ = 0;
                avgScoreCount_ = 0;
            }
            if (winBatchTotal_ >= NUM_SHOWCASE) {
                winRateHist_.push_back((float)winBatchCount_ / winBatchTotal_ * 100.0f);
                winBatchCount_ = 0;
                winBatchTotal_ = 0;
            }
        }
    }
}

// ========== Render ==========

void App::render() {
    if (mode_ == AppMode::PLAY)
        renderPlayMode();
    else
        renderTrainMode();
}

// ========== PLAY MODE ==========

void App::renderPlayMode() {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float winW = ds.x, winH = ds.y;
    float gameAreaW = winW - PANEL_W;

    float cs = floorf(std::min(
        (gameAreaW - 2 * PAD) / game_.gridW,
        (winH - HEADER_H - 2 * PAD) / game_.gridH));
    float gw = cs * game_.gridW;
    float gh = cs * game_.gridH;
    float gx = floorf((gameAreaW - gw) * 0.5f);
    float gy = floorf(HEADER_H + (winH - HEADER_H - gh) * 0.5f);

    if (deathShake_ > 0) {
        float t = deathShake_ / 0.3f;
        gx += sinf(t * 40) * 4 * t;
        gy += cosf(t * 35) * 3 * t;
    }

    // Full-screen ImGui window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ds);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##game", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoNav);
    ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(winW, winH), COL_BG);

    // Header
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(gameAreaW, HEADER_H), COL_HEADER);
    dl->AddLine(ImVec2(0, HEADER_H - 1), ImVec2(gameAreaW, HEADER_H - 1), COL_HEADER_DK, 2);
    drawPlayHeader(gameAreaW);

    // Grid border + game
    dl->AddRectFilled(ImVec2(gx - 4, gy - 4), ImVec2(gx + gw + 4, gy + gh + 4), COL_BORDER, 6);
    drawGrid(gx, gy, cs, game_);
    if (watchAI_) drawRaycasts(gx, gy, cs, game_);
    drawFood(gx, gy, cs, game_);
    drawSnake(gx, gy, cs, game_, eatFlash_);
    drawEyes(gx, gy, cs, game_);
    drawPlayOverlay(gx, gy, gw, gh);

    ImGui::End();

    drawPlayPanel(gameAreaW, 0, PANEL_W, winH);
}

void App::drawPlayHeader(float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    char buf[64];
    if (watchAI_)
        snprintf(buf, sizeof(buf), "AI PLAY  -  Score: %d", game_.score());
    else
        snprintf(buf, sizeof(buf), "SCORE: %d", game_.score());

    if (fontMed) {
        ImVec2 sz = fontMed->CalcTextSizeA(fontMed->FontSize, 1000, 0, buf);
        dl->AddText(fontMed, fontMed->FontSize, ImVec2(20, (HEADER_H - sz.y) * 0.5f), COL_TEXT, buf);
    }

    if (game_.highScore() > 0) {
        snprintf(buf, sizeof(buf), "MAX SCORE: %d", game_.highScore());
        if (fontMed) {
            ImVec2 sz = fontMed->CalcTextSizeA(fontMed->FontSize, 1000, 0, buf);
            dl->AddText(fontMed, fontMed->FontSize, ImVec2(w - sz.x - 20, (HEADER_H - sz.y) * 0.5f), COL_TEXT_DIM, buf);
        }
    }
}

void App::drawPlayOverlay(float gx, float gy, float gw, float gh) {
    float cx = gx + gw * 0.5f, cy = gy + gh * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (game_.state() == GameState::READY) {
        dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + gw, gy + gh), COL_OVERLAY, 4);
        if (watchAI_) {
            drawCenteredText(fontTitle, "AI PLAY", cx, cy - 40, COL_ACCENT);
            drawCenteredText(fontMed, "Press SPACE to start", cx, cy + 25, COL_TEXT);
        } else {
            drawCenteredText(fontTitle, "NEURAL SNAKE", cx, cy - 40, COL_ACCENT);
            drawCenteredText(fontMed, "Press SPACE to play", cx, cy + 25, COL_TEXT);
            drawCenteredText(fontSmall, "Arrow keys or WASD to move", cx, cy + 55, COL_TEXT_DIM);
        }
    }

    if (game_.state() == GameState::GAME_OVER) {
        dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + gw, gy + gh), COL_OVERLAY, 4);
        drawCenteredText(fontLarge, "GAME OVER", cx, cy - 35, IM_COL32(231, 71, 29, 255));
        char buf[64]; snprintf(buf, sizeof(buf), "Score: %d", game_.score());
        drawCenteredText(fontMed, buf, cx, cy + 10, COL_TEXT);
        if (game_.score() >= game_.highScore() && game_.score() > 0)
            drawCenteredText(fontSmall, "NEW HIGH SCORE!", cx, cy + 40, IM_COL32(255, 215, 0, 255));
        drawCenteredText(fontSmall, "Press SPACE to restart", cx, cy + 70, COL_TEXT_DIM);
    }

    if (game_.state() == GameState::WIN) {
        dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + gw, gy + gh), IM_COL32(0, 0, 0, 160), 4);
        // Gold glow border
        for (int i = 0; i < 3; i++)
            dl->AddRect(ImVec2(gx - i*2, gy - i*2), ImVec2(gx + gw + i*2, gy + gh + i*2),
                        IM_COL32(255, 215, 0, 200 - i*50), 6, 0, 2.0f);
        if (watchAI_) {
            drawCenteredText(fontTitle, "AI WINS!", cx, cy - 50, IM_COL32(255, 215, 0, 255));
            drawCenteredText(fontLarge, "PERFECT GAME", cx, cy - 5, IM_COL32(100, 230, 100, 255));
        } else {
            drawCenteredText(fontTitle, "YOU WIN!", cx, cy - 50, IM_COL32(255, 215, 0, 255));
            drawCenteredText(fontLarge, "PERFECT GAME", cx, cy - 5, IM_COL32(100, 230, 100, 255));
        }
        char buf[64]; snprintf(buf, sizeof(buf), "Score: %d / 297", game_.score());
        drawCenteredText(fontMed, buf, cx, cy + 35, COL_TEXT);
        drawCenteredText(fontSmall, "Press SPACE to play again", cx, cy + 65, COL_TEXT_DIM);
    }

    if (game_.state() == GameState::PAUSED) {
        dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + gw, gy + gh), IM_COL32(0, 0, 0, 100), 4);
        drawCenteredText(fontLarge, "PAUSED", cx, cy - 10, COL_TEXT);
        drawCenteredText(fontSmall, "Press SPACE to resume", cx, cy + 30, COL_TEXT_DIM);
    }
}

void App::drawPlayPanel(float px, float py, float pw, float ph) {
    ImGui::SetNextWindowPos(ImVec2(px, py));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.047f, 0.078f, 0.118f, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::Begin("##playpanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6);

    // Mode buttons
    float btnW = (pw - 54) * 0.5f;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.27f, 0.45f, 0.91f, 1));
    ImGui::Button("PLAY", ImVec2(btnW, 36));
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("AI TRAIN", ImVec2(btnW, 36))) {
        mode_ = AppMode::AI_TRAIN;
        startTraining();
    }
    ImGui::PopStyleVar();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.44f, 0.83f, 0.93f, 1));
    ImGui::Text("CONTROLS");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::TextWrapped("Arrow keys / WASD - Move");
    ImGui::TextWrapped("SPACE - Start / Pause");
    ImGui::TextWrapped("ESC - Quit");

    // Watch AI button (only if we have a trained genome)
    if (!savedBestGenome_.nodes.empty()) {
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6);
        if (watchAI_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.27f, 0.45f, 0.91f, 1));
            if (ImGui::Button("STOP AI", ImVec2(pw - 40, 32))) {
                watchAI_ = false;
                game_.reset();
            }
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("WATCH AI PLAY", ImVec2(pw - 40, 32))) {
                watchAI_ = true;
                playNet_.build(savedBestGenome_, 64, 4);
                game_.reset();
                game_.start();
                stepAccum_ = 0;
                playStepsSinceFood_ = 0;
            }
        }
        ImGui::PopStyleVar();
    }

    // Save / Load model buttons
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6);
    float halfBtn = (pw - 46) * 0.5f;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.15f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.65f, 0.2f, 1));
    if (ImGui::Button("SAVE", ImVec2(halfBtn, 28))) saveModel();
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.35f, 0.15f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.45f, 0.2f, 1));
    if (ImGui::Button("LOAD", ImVec2(halfBtn, 28))) {
        loadModel();
        if (!savedBestGenome_.nodes.empty()) {
            watchAI_ = true;
            playNet_.build(savedBestGenome_, 64, 4);
            game_.reset();
            game_.start();
            stepAccum_ = 0;
            playStepsSinceFood_ = 0;
        }
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImGui::End();
}

// ========== TRAIN MODE ==========

void App::renderTrainMode() {
    ImVec2 ds = ImGui::GetIO().DisplaySize;

    // Full-screen window for grid
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ds);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##train", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoNav);
    ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Dark background
    dl->AddRectFilled(ImVec2(0, 0), ds, IM_COL32(18, 28, 18, 255));

    float cellW = ds.x / GRID_COLS;
    float cellH = ds.y / GRID_ROWS;

    // Find best live game (highest current score)
    int bestIdx = -1;
    int bestLiveScore = -1;
    for (int i = 0; i < NUM_SHOWCASE && i < (int)showcase_.size(); i++) {
        int sc = showcase_[i].game.score();
        if (sc > bestLiveScore) { bestLiveScore = sc; bestIdx = i; }
    }

    for (int i = 0; i < NUM_SHOWCASE && i < (int)showcase_.size(); i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        float cx = col * cellW;
        float cy = row * cellH;

        // Highlight best game with blue border
        if (i == bestIdx && bestLiveScore > 0) {
            dl->AddRect(ImVec2(cx + 1, cy + 1), ImVec2(cx + cellW - 1, cy + cellH - 1),
                        IM_COL32(70, 116, 233, 255), 0, 0, 3.0f);
        } else {
            dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cellW, cy + cellH),
                        IM_COL32(40, 65, 35, 255), 0, 0, 1.0f);
        }

        drawMiniGame(dl, cx, cy, cellW, cellH, showcase_[i]);
    }

    ImGui::End();

    // Center stats panel (floating)
    drawTrainStatsPanel();

    // Log panel (only visible when paused)
    if (trainPaused_ && !trainLog_.empty())
        drawTrainLogPanel();
}

void App::drawMiniGame(ImDrawList* dl, float cx, float cy, float cw, float ch, const ShowcaseGame& sg) {
    const auto& g = sg.game;
    float textH = 14.0f;
    float pad = 3.0f;
    float cs = floorf(std::min((cw - 2 * pad) / g.gridW, (ch - textH - 2 * pad) / g.gridH));
    float gw = cs * g.gridW, gh = cs * g.gridH;
    float gx = cx + (cw - gw) * 0.5f;
    float gy = cy + textH + (ch - textH - gh) * 0.5f;

    // Game background
    dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + gw, gy + gh), COL_GRID_DARK);

    // Food
    Vec2i f = g.food();
    float fcx = gx + (f.x + 0.5f) * cs, fcy = gy + (f.y + 0.5f) * cs;
    dl->AddCircleFilled(ImVec2(fcx, fcy), cs * 0.4f, COL_FOOD, 8);

    // Snake body
    const auto& body = g.body();
    for (int i = 0; i < (int)body.size(); i++) {
        auto& s = body[i];
        ImU32 col = (i == 0) ? COL_SNAKE_HEAD : COL_SNAKE;
        dl->AddRectFilled(ImVec2(gx + s.x * cs, gy + s.y * cs),
                          ImVec2(gx + (s.x + 1) * cs, gy + (s.y + 1) * cs), col);
    }

    // Score + games counter (top of cell)
    char buf[32];
    snprintf(buf, sizeof(buf), "S:%d  G:%d", g.score(), sg.gamesPlayed);
    if (fontSmall)
        dl->AddText(fontSmall, 11.0f, ImVec2(cx + 4, cy + 1), COL_TEXT_DIM, buf);

    // Max score badge (top right)
    if (sg.maxScore > 0) {
        char mbuf[16];
        snprintf(mbuf, sizeof(mbuf), "Max:%d", sg.maxScore);
        if (fontSmall) {
            ImVec2 sz = fontSmall->CalcTextSizeA(11.0f, 1000, 0, mbuf);
            dl->AddText(fontSmall, 11.0f, ImVec2(cx + cw - sz.x - 4, cy + 1), COL_TEXT, mbuf);
        }
    }
}

void App::drawTrainStatsPanel() {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float pw = 280, ph = 370;
    ImGui::SetNextWindowPos(ImVec2((ds.x - pw) * 0.5f, (ds.y - ph) * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.07f, 0.10f, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::Begin("##trainstats", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // Big max score (from NEAT training only)
    char buf[64];
    snprintf(buf, sizeof(buf), "MAX SCORE: %d", dMaxScore_);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1));
    if (fontLarge) { ImGui::PushFont(fontLarge); ImGui::Text("%s", buf); ImGui::PopFont(); }
    else ImGui::Text("%s", buf);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Total games
    int totalGames = dGames_ + totalShowcaseGames_;
    ImGui::Text("Total games: %d", totalGames);

    // Training time
    int mins = (int)trainElapsed_ / 60;
    int secs = (int)trainElapsed_ % 60;
    ImGui::Text("Time: %02d:%02d", mins, secs);

    // Average score
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1));
    ImGui::Text("Avg Score: %.1f", lastAvgScore_);
    ImGui::PopStyleColor();

    // Win rate
    float winRate = totalShowcaseGames_ > 0 ? (float)showcaseWins_ / totalShowcaseGames_ * 100.0f : 0;
    ImGui::Text("Wins: %d / %d", showcaseWins_, totalShowcaseGames_);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1));
    snprintf(buf, sizeof(buf), "WIN RATE: %.1f%%", winRate);
    if (fontMed) { ImGui::PushFont(fontMed); ImGui::Text("%s", buf); ImGui::PopFont(); }
    else ImGui::Text("%s", buf);
    ImGui::PopStyleColor();

    // Win rate graph
    if (!winRateHist_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.3f, 1.0f, 0.3f, 1));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.08f, 0.13f, 0.8f));
        ImGui::PlotLines("##winrate", winRateHist_.data(), (int)winRateHist_.size(), 0,
            nullptr, 0, 100.0f, ImVec2(pw - 36, 50));
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Pause/Resume button
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6);
    if (trainPaused_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.15f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.65f, 0.2f, 1));
        if (ImGui::Button("RESUME", ImVec2(pw - 36, 30))) trainPaused_ = false;
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.1f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.6f, 0.15f, 1));
        if (ImGui::Button("PAUSE", ImVec2(pw - 36, 30))) {
            trainPaused_ = true;
            // Snapshot current state into log
            float wr = totalShowcaseGames_ > 0
                ? (float)showcaseWins_ / totalShowcaseGames_ * 100.0f : 0;
            TrainLogEntry entry;
            entry.time = trainElapsed_;
            entry.maxScore = dMaxScore_;
            entry.generation = dGen_;
            entry.totalGames = dGames_;
            entry.showcaseGames = totalShowcaseGames_;
            entry.showcaseWins = showcaseWins_;
            entry.winRate = wr;
            entry.bestFitness = dBestFit_;
            entry.species = dSpecies_;
            int m = (int)trainElapsed_ / 60;
            int s = (int)trainElapsed_ % 60;
            char buf2[256];
            snprintf(buf2, sizeof(buf2),
                "[%02d:%02d] MAX %d  |  Avg %.1f  |  Gen %d  |  Games %d  |  WinRate %.1f%%  |  Species %d  |  Fit %.0f  [PAUSED]",
                m, s, dMaxScore_, lastAvgScore_, dGen_, dGames_ + totalShowcaseGames_, wr, dSpecies_, dBestFit_);
            entry.text = buf2;
            trainLog_.push_back(entry);
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::PopStyleVar();

    ImGui::Spacing();

    // Play mode button — stops training and plays with best model
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.27f, 0.45f, 0.91f, 1));
    if (ImGui::Button("PLAY", ImVec2(pw - 36, 34))) {
        {
            std::lock_guard<std::mutex> lock(trainMutex_);
            savedBestGenome_ = dBestGenome_;
        }
        stopTraining();
        mode_ = AppMode::PLAY;
        if (!savedBestGenome_.nodes.empty()) {
            watchAI_ = true;
            playNet_.build(savedBestGenome_, 64, 4);
            game_.reset();
            game_.start();
            stepAccum_ = 0;
            playStepsSinceFood_ = 0;
        } else {
            watchAI_ = false;
            game_.reset();
        }
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::Spacing();

    // Save / Load model buttons
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6);
    float halfW = (pw - 42) * 0.5f;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.15f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.65f, 0.2f, 1));
    if (ImGui::Button("SAVE MODEL", ImVec2(halfW, 28))) saveModel();
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.35f, 0.15f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.45f, 0.2f, 1));
    if (ImGui::Button("LOAD MODEL", ImVec2(halfW, 28))) loadModel();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImGui::End();
}

// ========== Training Log ==========

void App::saveTrainLog() {
    if (trainLog_.empty()) return;

    // Generate timestamped filename
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm lt;
    localtime_s(&lt, &tt);
    char fname[128];
    snprintf(fname, sizeof(fname), "logs/train_%04d%02d%02d_%02d%02d%02d.log",
        lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
        lt.tm_hour, lt.tm_min, lt.tm_sec);

    // Ensure logs/ directory exists
    std::filesystem::create_directories("logs");

    FILE* f = fopen(fname, "w");
    if (!f) return;

    fprintf(f, "=== Neural Snake Training Log ===\n");
    int mins = (int)trainElapsed_ / 60;
    int secs = (int)trainElapsed_ % 60;
    fprintf(f, "Duration: %02d:%02d\n", mins, secs);
    fprintf(f, "Final max score: %d\n", loggedMaxScore_);
    float wr = totalShowcaseGames_ > 0 ? (float)showcaseWins_ / totalShowcaseGames_ * 100.0f : 0;
    fprintf(f, "Win rate: %.1f%% (%d/%d)\n", wr, showcaseWins_, totalShowcaseGames_);
    fprintf(f, "Total games: %d\n\n", dGames_ + totalShowcaseGames_);
    fprintf(f, "%-8s %-10s %-8s %-12s %-10s %-10s %-10s\n",
        "Time", "MaxScore", "Gen", "Games", "WinRate", "Species", "Fitness");
    fprintf(f, "----------------------------------------------------------------------\n");

    for (auto& e : trainLog_) {
        int m = (int)e.time / 60;
        int s = (int)e.time % 60;
        fprintf(f, "%02d:%02d    %-10d %-8d %-12d %-9.1f%% %-10d %.0f\n",
            m, s, e.maxScore, e.generation, e.totalGames + e.showcaseGames,
            e.winRate, e.species, e.bestFitness);
    }

    fclose(f);
}

void App::drawTrainLogPanel() {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float pw = 520, ph = 320;
    float px = (ds.x - pw) * 0.5f;
    float py = ds.y - ph - 20;

    ImGui::SetNextWindowPos(ImVec2(px, py));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.05f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::Begin("##trainlog", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1));
    ImGui::Text("TRAINING LOG");
    ImGui::PopStyleColor();
    ImGui::SameLine(pw - 170);
    ImGui::TextDisabled("%d entries", (int)trainLog_.size());
    ImGui::SameLine(pw - 90);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.35f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.50f, 1));
    if (ImGui::SmallButton("COPY")) {
        std::string all;
        all += "=== Neural Snake Training Log ===\n";
        int mins = (int)trainElapsed_ / 60;
        int secs = (int)trainElapsed_ % 60;
        char hdr[128];
        snprintf(hdr, sizeof(hdr), "Duration: %02d:%02d  |  Max Score: %d\n\n", mins, secs, loggedMaxScore_);
        all += hdr;
        for (auto& e : trainLog_) {
            all += e.text;
            all += '\n';
        }
        ImGui::SetClipboardText(all.c_str());
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy log to clipboard");
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImGui::Separator();

    // Scrollable log area
    ImGui::BeginChild("##logscroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (trainLog_.empty()) {
        ImGui::TextDisabled("Waiting for max score improvements...");
    } else {
        for (auto& e : trainLog_) {
            // Color based on score thresholds
            if (e.maxScore >= 200)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1));
            else if (e.maxScore >= 100)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1));
            else if (e.maxScore >= 50)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1));

            ImGui::TextUnformatted(e.text.c_str());
            ImGui::PopStyleColor();
        }
        // Auto-scroll to bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20)
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}

// ========== Model Save / Load ==========

void App::saveModel() {
    std::filesystem::create_directories("models");
    Genome toSave;
    if (training_) {
        std::lock_guard<std::mutex> lock(trainMutex_);
        toSave = dBestGenome_;
    } else {
        toSave = savedBestGenome_;
    }
    if (toSave.nodes.empty()) return;

    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm lt;
    localtime_s(&lt, &tt);
    char fname[128];
    snprintf(fname, sizeof(fname), "models/best_%04d%02d%02d_%02d%02d%02d.genome",
        lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
        lt.tm_hour, lt.tm_min, lt.tm_sec);

    toSave.saveToFile(fname);

    // Also save as models/best.genome (latest)
    toSave.saveToFile("models/best.genome");
}

void App::loadModel() {
    Genome loaded = Genome::loadFromFile("models/best.genome");
    if (loaded.nodes.empty()) return;

    savedBestGenome_ = loaded;

    // If in play mode, rebuild the network
    if (mode_ == AppMode::PLAY && watchAI_) {
        playNet_.build(savedBestGenome_, 64, 4);
        game_.reset();
        game_.start();
        stepAccum_ = 0;
        playStepsSinceFood_ = 0;
    }
}

// ========== Drawing helpers ==========

void App::drawGrid(float gx, float gy, float cs, const SnakeGame& g) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int y = 0; y < g.gridH; y++)
        for (int x = 0; x < g.gridW; x++)
            dl->AddRectFilled(
                ImVec2(gx + x * cs, gy + y * cs),
                ImVec2(gx + (x + 1) * cs, gy + (y + 1) * cs),
                (x + y) % 2 == 0 ? COL_GRID_DARK : COL_GRID_LIGHT);
}

void App::drawFood(float gx, float gy, float cs, const SnakeGame& g) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Vec2i f = g.food();
    float cx = gx + (f.x + 0.5f) * cs, cy = gy + (f.y + 0.5f) * cs;
    float r = cs * 0.38f;

    dl->AddCircleFilled(ImVec2(cx + 1.5f, cy + 2), r, IM_COL32(0, 0, 0, 40), 24);
    dl->AddCircleFilled(ImVec2(cx, cy + r * 0.05f), r, COL_FOOD, 24);
    dl->AddCircleFilled(ImVec2(cx - r * 0.28f, cy - r * 0.20f), r * 0.28f, COL_FOOD_HI, 16);
    dl->AddLine(ImVec2(cx, cy - r * 0.35f), ImVec2(cx + r * 0.15f, cy - r * 0.85f), COL_STEM, 2);
    dl->AddCircleFilled(ImVec2(cx + r * 0.22f, cy - r * 0.72f), r * 0.22f, COL_LEAF, 12);
}

void App::drawSnake(float gx, float gy, float cs, const SnakeGame& g, float flash) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto& body = g.body();
    if (body.empty()) return;

    float pad = cs * 0.05f, cornerR = cs * 0.28f;

    for (int i = 0; i < (int)body.size() - 1; i++) {
        auto& a = body[i]; auto& b = body[i + 1];
        if (a.y == b.y) {
            int mx = std::min(a.x, b.x);
            dl->AddRectFilled(ImVec2(gx + mx * cs + pad, gy + a.y * cs + pad),
                              ImVec2(gx + (mx + 2) * cs - pad, gy + (a.y + 1) * cs - pad), COL_SNAKE);
        }
        if (a.x == b.x) {
            int my = std::min(a.y, b.y);
            dl->AddRectFilled(ImVec2(gx + a.x * cs + pad, gy + my * cs + pad),
                              ImVec2(gx + (a.x + 1) * cs - pad, gy + (my + 2) * cs - pad), COL_SNAKE);
        }
    }

    for (int i = 0; i < (int)body.size(); i++) {
        auto& s = body[i];
        ImVec2 p0(gx + s.x * cs + pad, gy + s.y * cs + pad);
        ImVec2 p1(gx + (s.x + 1) * cs - pad, gy + (s.y + 1) * cs - pad);
        ImU32 col = (i == 0) ? ((flash > 0) ? COL_SNAKE_FLASH : COL_SNAKE_HEAD) : COL_SNAKE;
        dl->AddRectFilled(p0, p1, col, cornerR);
    }

    for (int i = 1; i < (int)body.size(); i++) {
        auto& s = body[i];
        dl->AddCircleFilled(ImVec2(gx + (s.x + 0.5f) * cs, gy + (s.y + 0.5f) * cs),
                             cs * 0.08f, IM_COL32(50, 90, 200, 80), 8);
    }
}

void App::drawEyes(float gx, float gy, float cs, const SnakeGame& g) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto& body = g.body();
    if (body.empty()) return;

    auto& h = body[0];
    Direction dir = g.direction();
    float cx = gx + (h.x + 0.5f) * cs, cy = gy + (h.y + 0.5f) * cs;
    float eR = cs * 0.12f, pR = cs * 0.065f;
    float eOff = cs * 0.18f, fOff = cs * 0.13f;
    float dx = 0, dy = 0;
    ImVec2 e1, e2;

    switch (dir) {
        case Direction::RIGHT: e1={cx+fOff,cy-eOff}; e2={cx+fOff,cy+eOff}; dx=1; break;
        case Direction::LEFT:  e1={cx-fOff,cy-eOff}; e2={cx-fOff,cy+eOff}; dx=-1; break;
        case Direction::UP:    e1={cx-eOff,cy-fOff}; e2={cx+eOff,cy-fOff}; dy=-1; break;
        case Direction::DOWN:  e1={cx-eOff,cy+fOff}; e2={cx+eOff,cy+fOff}; dy=1; break;
    }

    dl->AddCircleFilled(e1, eR, COL_EYE_WHITE, 16);
    dl->AddCircleFilled(e2, eR, COL_EYE_WHITE, 16);
    float po = pR * 0.4f;
    dl->AddCircleFilled(ImVec2(e1.x + dx * po, e1.y + dy * po), pR, COL_EYE_PUPIL, 12);
    dl->AddCircleFilled(ImVec2(e2.x + dx * po, e2.y + dy * po), pR, COL_EYE_PUPIL, 12);
}

void App::drawRaycasts(float gx, float gy, float cs, const SnakeGame& g) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto& body = g.body();
    if (body.empty()) return;

    Vec2i head = body[0];
    Vec2i food = g.food();
    float hcx = gx + (head.x + 0.5f) * cs;
    float hcy = gy + (head.y + 0.5f) * cs;

    static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    for (int d = 0; d < 8; d++) {
        int x = head.x, y = head.y;
        int wallDist = 0;
        int bodyDist = 0;
        bool foundFood = false;
        Vec2i wallHit{}, bodyHit{}, foodHit{};

        while (true) {
            x += dx[d]; y += dy[d]; wallDist++;
            if (x < 0 || x >= g.gridW || y < 0 || y >= g.gridH) { wallHit = {x, y}; break; }
            if (x == food.x && y == food.y && !foundFood) { foundFood = true; foodHit = {x, y}; }
            if (bodyDist == 0) {
                for (int i = 1; i < (int)body.size(); i++) {
                    if (body[i].x == x && body[i].y == y) { bodyDist = wallDist; bodyHit = {x, y}; break; }
                }
            }
        }

        float endX = std::clamp(gx + std::clamp(wallHit.x, -1, g.gridW) * cs + cs * 0.5f, gx, gx + g.gridW * cs);
        float endY = std::clamp(gy + std::clamp(wallHit.y, -1, g.gridH) * cs + cs * 0.5f, gy, gy + g.gridH * cs);

        dl->AddLine(ImVec2(hcx, hcy), ImVec2(endX, endY), IM_COL32(255, 255, 255, 90), 1.5f);

        if (bodyDist > 0) {
            float bx = gx + (bodyHit.x + 0.5f) * cs, by = gy + (bodyHit.y + 0.5f) * cs;
            dl->AddLine(ImVec2(hcx, hcy), ImVec2(bx, by), IM_COL32(231, 71, 29, 160), 2.0f);
            dl->AddCircleFilled(ImVec2(bx, by), cs * 0.18f, IM_COL32(231, 71, 29, 220), 10);
        }

        if (foundFood) {
            float fx = gx + (foodHit.x + 0.5f) * cs, fy = gy + (foodHit.y + 0.5f) * cs;
            dl->AddLine(ImVec2(hcx, hcy), ImVec2(fx, fy), IM_COL32(100, 230, 100, 180), 2.5f);
            dl->AddCircleFilled(ImVec2(fx, fy), cs * 0.2f, IM_COL32(100, 230, 100, 240), 10);
        }

        dl->AddCircleFilled(ImVec2(endX, endY), cs * 0.12f, IM_COL32(255, 255, 255, 140), 8);
    }
}

void App::drawCenteredText(ImFont* font, const char* text, float cx, float cy, unsigned col) {
    if (!font) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 sz = font->CalcTextSizeA(font->FontSize, 10000, 0, text);
    dl->AddText(font, font->FontSize, ImVec2(cx - sz.x * 0.5f + 2, cy - sz.y * 0.5f + 2), IM_COL32(0, 0, 0, 120), text);
    dl->AddText(font, font->FontSize, ImVec2(cx - sz.x * 0.5f, cy - sz.y * 0.5f), col, text);
}

// ========== Input ==========

void App::keyCallback(int key, int action) {
    if (action != GLFW_PRESS) return;

    if (mode_ == AppMode::PLAY && !watchAI_) {
        if (key == GLFW_KEY_UP    || key == GLFW_KEY_W) game_.setDirection(Direction::UP);
        if (key == GLFW_KEY_DOWN  || key == GLFW_KEY_S) game_.setDirection(Direction::DOWN);
        if (key == GLFW_KEY_LEFT  || key == GLFW_KEY_A) game_.setDirection(Direction::LEFT);
        if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_D) game_.setDirection(Direction::RIGHT);
    }

    if (mode_ == AppMode::PLAY) {
        if (key == GLFW_KEY_SPACE) {
            if (game_.state() == GameState::READY || game_.state() == GameState::GAME_OVER || game_.state() == GameState::WIN)
                game_.start();
            else
                game_.togglePause();
        }
        if (key == GLFW_KEY_ENTER && (game_.state() == GameState::GAME_OVER || game_.state() == GameState::WIN))
            game_.start();
    }

    if (key == GLFW_KEY_ESCAPE) shouldClose_ = true;
}
