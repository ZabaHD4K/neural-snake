#pragma once
#include "genome.h"
#include "species.h"
#include "neat_params.h"
#include <vector>

class Population {
public:
    void init(const NeatParams& p);
    void epoch();   // evaluate + speciate + reproduce

    const Genome& bestGenome() const       { return genomes_[bestIdx_]; }
    int           generation() const       { return generation_; }
    int           numSpecies() const       { return (int)species_.size(); }
    float         bestFitness() const      { return bestFit_; }
    int           bestScore() const        { return bestScore_; }
    const std::vector<float>& fitHistory() const  { return fitHist_; }
    const std::vector<float>& avgHistory() const  { return avgHist_; }

    NeatParams params;

private:
    void evaluate();
    void speciate();
    void reproduce();

    std::vector<Genome>  genomes_;
    std::vector<Species> species_;
    InnovationCounter    innov_;
    int   generation_ = 0;
    float bestFit_    = 0;
    int   bestScore_  = 0;
    int   bestIdx_    = 0;
    std::vector<float> fitHist_;
    std::vector<float> avgHist_;
};
