#pragma once

struct NeatParams {
    int populationSize      = 500;

    // Mutation rates
    float weightMutateRate     = 0.80f;
    float weightPerturbRate    = 0.90f;
    float weightPerturbStr     = 0.5f;
    float addConnectionRate    = 0.05f;
    float addNodeRate          = 0.03f;
    float toggleRate           = 0.01f;

    // Crossover
    float crossoverRate        = 0.75f;

    // Speciation
    float compatC1             = 1.0f;   // excess
    float compatC2             = 1.0f;   // disjoint
    float compatC3             = 0.4f;   // weight diff
    float compatThreshold      = 3.0f;

    // Species management
    int   stagnationLimit      = 15;
    float survivalFraction     = 0.20f;
    int   elitesPerSpecies     = 1;

    // Evaluation
    int   maxStepsPerFood      = 100;
    int   gamesPerGenome       = 1;

    // Network topology
    int   numInputs            = 28;
    int   numOutputs           = 4;
};
