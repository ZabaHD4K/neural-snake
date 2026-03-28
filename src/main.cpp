#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include "app.h"

static App g_app;

static void keyCallback(GLFWwindow* /*win*/, int key, int /*scancode*/, int action, int /*mods*/) {
    g_app.keyCallback(key, action);
}

int main() {
    if (!glfwInit()) { fprintf(stderr, "GLFW init failed\n"); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Neural Snake", nullptr, nullptr);
    if (!window) { fprintf(stderr, "Window creation failed\n"); glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync
    glfwSetKeyCallback(window, keyCallback);

    if (!gladLoadGL(glfwGetProcAddress)) {
        fprintf(stderr, "GLAD init failed\n"); return 1;
    }

    glEnable(GL_MULTISAMPLE);

    // ---- ImGui init ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // no imgui.ini

    // Load fonts at multiple sizes (Segoe UI from Windows)
    const char* fontPath   = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* fontPathB  = "C:\\Windows\\Fonts\\segoeuib.ttf";

    g_app.fontSmall = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f);
    if (!g_app.fontSmall) g_app.fontSmall = io.Fonts->AddFontDefault();

    g_app.fontMed = io.Fonts->AddFontFromFileTTF(fontPath, 22.0f);
    if (!g_app.fontMed) g_app.fontMed = g_app.fontSmall;

    g_app.fontLarge = io.Fonts->AddFontFromFileTTF(fontPathB, 34.0f);
    if (!g_app.fontLarge) g_app.fontLarge = g_app.fontMed;

    g_app.fontTitle = io.Fonts->AddFontFromFileTTF(fontPathB, 52.0f);
    if (!g_app.fontTitle) g_app.fontTitle = g_app.fontLarge;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // Dark style (won't be visible but just in case)
    ImGui::StyleColorsDark();

    g_app.init(window);

    // ---- Main loop ----
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window) && !g_app.shouldClose()) {
        glfwPollEvents();

        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        if (dt > 0.1f) dt = 0.1f; // clamp large dt

        g_app.update(dt);

        // Render
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.34f, 0.54f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        g_app.render();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    g_app.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
