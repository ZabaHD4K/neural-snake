#pragma once

struct NeatParams {
    int populationSize      = 2000;  // more diversity, more CPU (was 1000)

    // Mutation rates
    float weightMutateRate     = 0.80f;
    float weightPerturbRate    = 0.90f;
    float weightPerturbStr     = 0.5f;
    float addConnectionRate    = 0.08f;   // more topology exploration (was 0.05)
    float addNodeRate          = 0.05f;   // more hidden nodes (was 0.03)
    float toggleRate           = 0.02f;   // more enable/disable (was 0.01)

    // Crossover
    float crossoverRate        = 0.75f;

    // Speciation
    float compatC1             = 1.0f;   // excess
    float compatC2             = 1.0f;   // disjoint
    float compatC3             = 1.5f;   // weight diff — balanced: 0.4 gave 1 species, 3.0 gave 1000 micro-species
    float compatThreshold      = 2.0f;   // allows natural grouping without fragmenting

    // Species management
    int   stagnationLimit      = 30;     // more time for complex strategies (was 20)
    float survivalFraction     = 0.25f;  // keep more parents (was 0.20)
    int   elitesPerSpecies     = 2;      // protect top 2 per species (was 1)

    // Evaluation
    int   maxStepsPerFood      = 200;  // base value for dynamic: 200 + score*2
    int   gamesPerGenome       = 4;    // reverted from 6, more gens/min matters more

    // Network topology
    int   numInputs            = 64;
    int   numOutputs           = 4;
};
