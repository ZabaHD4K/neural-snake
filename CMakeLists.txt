cmake_minimum_required(VERSION 3.24)
project(NeuralSnake LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

# ---- Dependencies via FetchContent ----
include(FetchContent)

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW ON)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.8 GIT_SHALLOW ON)
FetchContent_MakeAvailable(imgui)

# ---- GLAD 2 (OpenGL 4.6 core loader) ----
add_library(glad STATIC extern/glad/src/gl.c)
target_include_directories(glad PUBLIC extern/glad/include)

# ---- ImGui library ----
add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC glfw glad)

# ---- Main executable ----
add_executable(neural_snake
  src/main.cpp
  src/app.cpp
  src/game/snake_game.cpp
  src/neat/genome.cpp
  src/neat/population.cpp
  src/eval/network.cpp
  src/eval/evaluator.cpp
)

target_include_directories(neural_snake PRIVATE src)
target_compile_options(neural_snake PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>
)
target_link_libraries(neural_snake PRIVATE glfw glad imgui_lib)
