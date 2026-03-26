#pragma once
#include "genome.h"
#include <vector>

struct Species {
    Genome representative;
    std::vector<int> members;     // indices into population
    float bestFitness     = 0;
    float totalAdjFitness = 0;
    int   stagnation      = 0;
    int   offspring        = 0;

    void reset() { members.clear(); totalAdjFitness = 0; }
};
