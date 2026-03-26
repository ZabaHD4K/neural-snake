#include "app.h"
#include "eval/evaluator.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <chrono>

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
static const ImU32 COL_PANEL       = IM_COL32(12, 20, 30, 235);
static const ImU32 COL_ACCENT      = IM_COL32(70, 116, 233, 255);

static constexpr float HEADER_H = 56.0f;
static constexpr float PAD      = 14.0f;
static constexpr float PANEL_W  = 380.0f;

// ========== Init / Shutdown ==========

void App::init(GLFWwindow* win) {
    window_ = win;
    game_ = SnakeGame(20, 15);
    replayGame_ = SnakeGame(20, 15);
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
    training_ = true;
    trainThread_ = std::thread(&App::trainLoop, this);
}

void App::stopTraining() {
    if (!training_) return;
    stopFlag_ = true;
    if (trainThread_.joinable()) trainThread_.join();
    training_ = false;
}

void App::trainLoop() {
    while (!stopFlag_) {
        population_.epoch();

        std::lock_guard<std::mutex> lock(trainMutex_);
        dGen_       = population_.generation();
        dBestFit_   = population_.bestFitness();
        dBestScore_ = population_.bestScore();
        dSpecies_   = population_.numSpecies();
        dFitHist_   = population_.fitHistory();
        dAvgHist_   = population_.avgHistory();
        dBestGenome_ = population_.bestGenome();
        dNewGen_    = true;

        if (!turbo_)
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

// ========== Update ==========

void App::update(float dt) {
    if (eatFlash_ > 0)   eatFlash_ -= dt;
    if (replayFlash_ > 0) replayFlash_ -= dt;
    if (deathShake_ > 0) deathShake_ -= dt;

    if (mode_ == AppMode::PLAY) {
        if (game_.state() == GameState::PLAYING) {
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
        // Buffer new best genome from training thread (don't reset mid-game)
        {
            std::lock_guard<std::mutex> lock(trainMutex_);
            if (dNewGen_) {
                pendingGenome_ = dBestGenome_;
                hasPending_ = true;
                dNewGen_ = false;
            }
        }

        // Apply pending genome only when current game ends (or hasn't started)
        if (hasPending_ && replayGame_.state() != GameState::PLAYING) {
            replayNet_.build(pendingGenome_, 14, 4);
            vizNet_ = replayNet_;
            replayGame_ = SnakeGame(20, 15);
            replayGame_.start();
            replayAccum_ = 0;
            hasPending_ = false;
        }

        // Step replay (skip if paused)
        if (replayGame_.state() == GameState::PLAYING && !replayPaused_) {
            replayAccum_ += dt * replaySpeed_;
            float iv = replayGame_.stepInterval();
            while (replayAccum_ >= iv && replayGame_.state() == GameState::PLAYING) {
                replayAccum_ -= iv;

                float inputs[14], outputs[4];
                Evaluator::computeInputs(replayGame_, inputs);
                replayNet_.forward(inputs, outputs);

                // Update viz network activations for live display
                vizNet_ = replayNet_;

                int best = 0;
                for (int i = 1; i < 4; i++)
                    if (outputs[i] > outputs[best]) best = i;

                static const Direction dirs[] = {
                    Direction::UP, Direction::RIGHT, Direction::DOWN, Direction::LEFT};
                replayGame_.setDirection(dirs[best]);
                replayGame_.step();
                if (replayGame_.justAte()) replayFlash_ = 0.25f;
            }
        } else if (replayGame_.state() == GameState::GAME_OVER && !hasPending_) {
            // Auto-restart replay with same genome after a brief pause
            replayAccum_ += dt;
            if (replayAccum_ > 1.0f) {  // 1 second pause before restart
                replayGame_ = SnakeGame(20, 15);
                replayGame_.start();
                replayAccum_ = 0;
            }
        }
    }
}

// ========== Render ==========

void App::render() {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float winW = ds.x, winH = ds.y;
    float gameAreaW = winW - PANEL_W;

    const SnakeGame& activeGame = (mode_ == AppMode::AI_TRAIN) ? replayGame_ : game_;
    float activeFlash = (mode_ == AppMode::AI_TRAIN) ? replayFlash_ : eatFlash_;

    float cs = floorf(std::min(
        (gameAreaW - 2 * PAD) / activeGame.gridW,
        (winH - HEADER_H - 2 * PAD) / activeGame.gridH));
    float gw = cs * activeGame.gridW;
    float gh = cs * activeGame.gridH;
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

    // Background
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(winW, winH), COL_BG);

    // Header (game area only)
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(gameAreaW, HEADER_H), COL_HEADER);
    dl->AddLine(ImVec2(0, HEADER_H - 1), ImVec2(gameAreaW, HEADER_H - 1), COL_HEADER_DK, 2);
    drawHeader(gameAreaW);

    // Grid border
    dl->AddRectFilled(ImVec2(gx - 4, gy - 4), ImVec2(gx + gw + 4, gy + gh + 4), COL_BORDER, 6);

    drawGrid(gx, gy, cs, activeGame);
    drawFood(gx, gy, cs, activeGame);
    drawSnake(gx, gy, cs, activeGame, activeFlash);
    drawEyes(gx, gy, cs, activeGame);

    if (mode_ == AppMode::PLAY)
        drawOverlay(gx, gy, gw, gh);

    ImGui::End();

    // Right panel
    drawPanel(gameAreaW, 0, PANEL_W, winH);
}

// ========== Drawing helpers ==========

void App::drawHeader(float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const SnakeGame& g = (mode_ == AppMode::AI_TRAIN) ? replayGame_ : game_;

    char buf[64];
    if (mode_ == AppMode::AI_TRAIN)
        snprintf(buf, sizeof(buf), "AI  -  Gen %d  -  Score: %d", dGen_, g.score());
    else
        snprintf(buf, sizeof(buf), "SCORE: %d", g.score());

    if (fontMed) {
        ImVec2 sz = fontMed->CalcTextSizeA(fontMed->FontSize, 1000, 0, buf);
        dl->AddText(fontMed, fontMed->FontSize, ImVec2(20, (HEADER_H - sz.y) * 0.5f), COL_TEXT, buf);
    }

    if (mode_ == AppMode::PLAY && game_.highScore() > 0) {
        snprintf(buf, sizeof(buf), "HIGH SCORE: %d", game_.highScore());
        if (fontMed) {
            ImVec2 sz = fontMed->CalcTextSizeA(fontMed->FontSize, 1000, 0, buf);
            dl->AddText(fontMed, fontMed->FontSize, ImVec2(w - sz.x - 20, (HEADER_H - sz.y) * 0.5f), COL_TEXT_DIM, buf);
        }
    }
}

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

    // Connectors
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

    // Segments
    for (int i = 0; i < (int)body.size(); i++) {
        auto& s = body[i];
        ImVec2 p0(gx + s.x * cs + pad, gy + s.y * cs + pad);
        ImVec2 p1(gx + (s.x + 1) * cs - pad, gy + (s.y + 1) * cs - pad);
        ImU32 col = (i == 0) ? ((flash > 0) ? COL_SNAKE_FLASH : COL_SNAKE_HEAD) : COL_SNAKE;
        dl->AddRectFilled(p0, p1, col, cornerR);
    }

    // Body dots
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

void App::drawCenteredText(ImFont* font, const char* text, float cx, float cy, unsigned col) {
    if (!font) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 sz = font->CalcTextSizeA(font->FontSize, 10000, 0, text);
    dl->AddText(font, font->FontSize, ImVec2(cx - sz.x * 0.5f + 2, cy - sz.y * 0.5f + 2), IM_COL32(0, 0, 0, 120), text);
    dl->AddText(font, font->FontSize, ImVec2(cx - sz.x * 0.5f, cy - sz.y * 0.5f), col, text);
}

void App::drawOverlay(float gx, float gy, float gw, float gh) {
    float cx = gx + gw * 0.5f, cy = gy + gh * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (game_.state() == GameState::READY) {
        dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + gw, gy + gh), COL_OVERLAY, 4);
        drawCenteredText(fontTitle, "NEURAL SNAKE", cx, cy - 40, COL_ACCENT);
        drawCenteredText(fontMed, "Press SPACE to play", cx, cy + 25, COL_TEXT);
        drawCenteredText(fontSmall, "Arrow keys or WASD to move", cx, cy + 55, COL_TEXT_DIM);
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

    if (game_.state() == GameState::PAUSED) {
        dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + gw, gy + gh), IM_COL32(0, 0, 0, 100), 4);
        drawCenteredText(fontLarge, "PAUSED", cx, cy - 10, COL_TEXT);
        drawCenteredText(fontSmall, "Press SPACE to resume", cx, cy + 30, COL_TEXT_DIM);
    }
}

// ========== Right Panel ==========

void App::drawPanel(float px, float py, float pw, float ph) {
    ImGui::SetNextWindowPos(ImVec2(px, py));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.047f, 0.078f, 0.118f, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::Begin("##panel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // Mode buttons
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6);
    float btnW = (pw - 54) * 0.5f;

    bool playActive = (mode_ == AppMode::PLAY);
    if (playActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.27f, 0.45f, 0.91f, 1));
    if (ImGui::Button("PLAY", ImVec2(btnW, 36))) {
        if (mode_ == AppMode::AI_TRAIN) { stopTraining(); mode_ = AppMode::PLAY; game_.reset(); }
    }
    if (playActive) ImGui::PopStyleColor();

    ImGui::SameLine();

    bool aiActive = (mode_ == AppMode::AI_TRAIN);
    if (aiActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.27f, 0.45f, 0.91f, 1));
    if (ImGui::Button("AI TRAIN", ImVec2(btnW, 36))) {
        if (mode_ != AppMode::AI_TRAIN) { mode_ = AppMode::AI_TRAIN; startTraining(); }
    }
    if (aiActive) ImGui::PopStyleColor();

    ImGui::PopStyleVar();
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    if (mode_ == AppMode::AI_TRAIN) {
        // Stats
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.44f, 0.83f, 0.93f, 1));
        ImGui::Text("TRAINING STATS");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::Text("Generation:   %d", dGen_);
        ImGui::Text("Best Fitness: %.0f", dBestFit_);
        ImGui::Text("Best Score:   %d", dBestScore_);
        ImGui::Text("Species:      %d", dSpecies_);

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // Speed
        ImGui::Text("Replay Speed");
        ImGui::SliderFloat("##speed", &replaySpeed_, 1.0f, 20.0f, "%.1fx");
        ImGui::Checkbox("Turbo (skip replay)", &turbo_);

        ImGui::Spacing();

        // Pause / Stop buttons
        if (replayPaused_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.1f, 1));
            if (ImGui::Button("RESUME (Space)", ImVec2(pw - 40, 30))) replayPaused_ = false;
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("PAUSE (Space)", ImVec2(pw - 40, 30))) replayPaused_ = true;
        }

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.2f, 0.2f, 1));
        if (ImGui::Button("STOP TRAINING", ImVec2(pw - 40, 30))) {
            stopTraining();
            mode_ = AppMode::PLAY;
            game_.reset();
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // Fitness graph
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.44f, 0.83f, 0.93f, 1));
        ImGui::Text("FITNESS");
        ImGui::PopStyleColor();

        {
            std::lock_guard<std::mutex> lock(trainMutex_);
            if (!dFitHist_.empty()) {
                float maxVal = *std::max_element(dFitHist_.begin(), dFitHist_.end());
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.27f, 0.45f, 0.91f, 1));
                ImGui::PlotLines("##best", dFitHist_.data(), (int)dFitHist_.size(), 0,
                    "Best", 0, maxVal * 1.1f, ImVec2(pw - 40, 80));
                ImGui::PopStyleColor();
            }
            if (!dAvgHist_.empty()) {
                float maxVal = *std::max_element(dAvgHist_.begin(), dAvgHist_.end());
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.5f, 0.8f, 0.3f, 1));
                ImGui::PlotLines("##avg", dAvgHist_.data(), (int)dAvgHist_.size(), 0,
                    "Average", 0, maxVal * 1.1f, ImVec2(pw - 40, 80));
                ImGui::PopStyleColor();
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // Network visualization
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.44f, 0.83f, 0.93f, 1));
        ImGui::Text("NEURAL NETWORK");
        ImGui::PopStyleColor();

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float vizH = 220;
        drawNetworkViz(cursor.x, cursor.y, pw - 40, vizH);
        ImGui::Dummy(ImVec2(pw - 40, vizH));
    } else {
        // Play mode panel
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.44f, 0.83f, 0.93f, 1));
        ImGui::Text("CONTROLS");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextWrapped("Arrow keys / WASD - Move");
        ImGui::TextWrapped("SPACE - Start / Pause");
        ImGui::TextWrapped("ESC - Quit");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("Switch to AI TRAIN to watch a neural network learn to play Snake using NEAT evolution.");
    }

    ImGui::End();
}

// ========== Network Visualization ==========

void App::drawNetworkViz(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(8, 14, 22, 200), 6);

    auto& nodes = vizNet_.nodes();
    auto& conns = vizNet_.connections();
    auto& acts  = vizNet_.activations();
    if (nodes.empty()) return;

    // Compute positions: inputs left, outputs right, hidden middle
    float padX = 40, padY = 10;
    float innerW = w - 2 * padX, innerH = h - 2 * padY;

    // Collect by type
    std::vector<int> inputIdx, hiddenIdx, outputIdx;
    for (int i = 0; i < (int)nodes.size(); i++) {
        switch (nodes[i].type) {
            case NodeGene::INPUT:  inputIdx.push_back(i); break;
            case NodeGene::OUTPUT: outputIdx.push_back(i); break;
            case NodeGene::HIDDEN: hiddenIdx.push_back(i); break;
        }
    }

    // Node positions
    std::unordered_map<int, ImVec2> pos; // node id -> screen pos

    auto layoutColumn = [&](const std::vector<int>& indices, float xFrac) {
        int n = (int)indices.size();
        for (int i = 0; i < n; i++) {
            float yy = (n == 1) ? 0.5f : (float)i / (n - 1);
            pos[nodes[indices[i]].id] = ImVec2(x + padX + xFrac * innerW, y + padY + yy * innerH);
        }
    };

    layoutColumn(inputIdx, 0.0f);
    layoutColumn(outputIdx, 1.0f);
    if (!hiddenIdx.empty()) layoutColumn(hiddenIdx, 0.5f);

    // Draw connections
    for (auto& c : conns) {
        if (!c.enabled) continue;
        auto itA = pos.find(c.inNode), itB = pos.find(c.outNode);
        if (itA == pos.end() || itB == pos.end()) continue;

        float absW = std::min(std::abs(c.weight), 2.0f);
        float thick = 1.0f + absW * 1.5f;
        ImU32 col = (c.weight > 0)
            ? IM_COL32(80, (int)(160 + absW * 40), 80, 140)
            : IM_COL32((int)(160 + absW * 40), 60, 60, 140);
        dl->AddLine(itA->second, itB->second, col, thick);
    }

    // Draw nodes
    for (int i = 0; i < (int)nodes.size(); i++) {
        auto it = pos.find(nodes[i].id);
        if (it == pos.end()) continue;

        float act = (i < (int)acts.size()) ? acts[i] : 0;
        int brightness = 40 + (int)(act * 200);
        ImU32 col;
        switch (nodes[i].type) {
            case NodeGene::INPUT:  col = IM_COL32(brightness, brightness, (int)(brightness * 1.3f), 255); break;
            case NodeGene::OUTPUT: col = IM_COL32((int)(brightness * 1.2f), brightness, brightness / 2, 255); break;
            default:               col = IM_COL32(brightness, brightness, brightness, 255); break;
        }

        float r = (nodes[i].type == NodeGene::HIDDEN) ? 4.0f : 5.5f;
        dl->AddCircleFilled(it->second, r, col, 12);
        dl->AddCircle(it->second, r, IM_COL32(100, 140, 180, 100), 12, 1.0f);
    }

    // Labels for outputs
    static const char* outLabels[] = {"UP", "RT", "DN", "LT"};
    for (int i = 0; i < (int)outputIdx.size() && i < 4; i++) {
        auto it = pos.find(nodes[outputIdx[i]].id);
        if (it != pos.end() && fontSmall)
            dl->AddText(fontSmall, 11, ImVec2(it->second.x + 8, it->second.y - 6), COL_TEXT_DIM, outLabels[i]);
    }
}

// ========== Input ==========

void App::keyCallback(int key, int action) {
    if (action != GLFW_PRESS) return;

    if (mode_ == AppMode::PLAY) {
        if (key == GLFW_KEY_UP    || key == GLFW_KEY_W) game_.setDirection(Direction::UP);
        if (key == GLFW_KEY_DOWN  || key == GLFW_KEY_S) game_.setDirection(Direction::DOWN);
        if (key == GLFW_KEY_LEFT  || key == GLFW_KEY_A) game_.setDirection(Direction::LEFT);
        if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_D) game_.setDirection(Direction::RIGHT);

        if (key == GLFW_KEY_SPACE) {
            if (game_.state() == GameState::READY || game_.state() == GameState::GAME_OVER)
                game_.start();
            else
                game_.togglePause();
        }
        if (key == GLFW_KEY_ENTER && game_.state() == GameState::GAME_OVER)
            game_.start();
    }

    if (mode_ == AppMode::AI_TRAIN) {
        if (key == GLFW_KEY_SPACE) replayPaused_ = !replayPaused_;
    }

    if (key == GLFW_KEY_ESCAPE) shouldClose_ = true;
}
