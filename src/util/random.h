#pragma once
#include <random>
#include <algorithm>

inline std::mt19937& rng() {
    static thread_local std::mt19937 gen{std::random_device{}()};
    return gen;
}

inline float randFloat()    { return std::uniform_real_distribution<float>(-1.0f, 1.0f)(rng()); }
inline float randFloat01()  { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng()); }
inline int   randInt(int n) { return n <= 0 ? 0 : std::uniform_int_distribution<int>(0, n - 1)(rng()); }
inline float randGauss(float s) { return std::normal_distribution<float>(0, s)(rng()); }
