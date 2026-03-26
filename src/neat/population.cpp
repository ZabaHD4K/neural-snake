#include "population.h"
#include "eval/network.h"
#include "eval/evaluator.h"
#include "util/random.h"
#include <algorithm>
#include <numeric>

void Population::init(const NeatParams& p) {
    params = p;
    genomes_.clear();
    species_.clear();
    fitHist_.clear();
    avgHist_.clear();
    generation_ = 0;
    totalGames_ = 0;
    bestFit_ = 0;
    bestScore_ = 0;
    bestIdx_ = 0;

    genomes_.reserve(params.populationSize);
    for (int i = 0; i < params.populationSize; i++)
        genomes_.push_back(Genome::createInitial(params.numInputs, params.numOutputs, innov_));
}

// ---- Evaluate all genomes ----

void Population::evaluate() {
    bestFit_ = 0;
    bestScore_ = 0;
    bestIdx_ = 0;
    float totalFit = 0;

    for (int i = 0; i < (int)genomes_.size(); i++) {
        Network net;
        net.build(genomes_[i], params.numInputs, params.numOutputs);

        float fit = 0;
        int maxScore = 0;
        for (int g = 0; g < params.gamesPerGenome; g++) {
            auto res = Evaluator::run(net, params);
            fit += res.fitness;
            maxScore = std::max(maxScore, res.score);
        }
        genomes_[i].fitness = fit / params.gamesPerGenome;
        totalFit += genomes_[i].fitness;

        if (genomes_[i].fitness > bestFit_) {
            bestFit_ = genomes_[i].fitness;
            bestScore_ = maxScore;
            bestIdx_ = i;
        }
    }

    totalGames_ += (int)genomes_.size() * params.gamesPerGenome;
    fitHist_.push_back(bestFit_);
    avgHist_.push_back(totalFit / genomes_.size());
}

// ---- Speciate ----

void Population::speciate() {
    for (auto& s : species_) s.reset();

    for (int i = 0; i < (int)genomes_.size(); i++) {
        bool placed = false;
        for (auto& s : species_) {
            float d = Genome::compatibility(genomes_[i], s.representative,
                          params.compatC1, params.compatC2, params.compatC3);
            if (d < params.compatThreshold) {
                s.members.push_back(i);
                placed = true;
                break;
            }
        }
        if (!placed) {
            Species ns;
            ns.representative = genomes_[i];
            ns.members.push_back(i);
            species_.push_back(ns);
        }
    }

    // Remove empty species
    species_.erase(std::remove_if(species_.begin(), species_.end(),
        [](const Species& s) { return s.members.empty(); }), species_.end());

    // Update stagnation + adjusted fitness
    for (auto& s : species_) {
        float best = 0;
        int bestIdx = s.members[0];
        for (int idx : s.members) {
            if (genomes_[idx].fitness > best) {
                best = genomes_[idx].fitness;
                bestIdx = idx;
            }
        }
        if (best > s.bestFitness) { s.bestFitness = best; s.stagnation = 0; }
        else s.stagnation++;

        s.representative = genomes_[bestIdx];
        for (int idx : s.members)
            s.totalAdjFitness += genomes_[idx].fitness / (float)s.members.size();
    }

    // Remove stagnant (keep at least 2)
    if ((int)species_.size() > 2) {
        species_.erase(std::remove_if(species_.begin(), species_.end(),
            [&](const Species& s) { return s.stagnation > params.stagnationLimit; }),
            species_.end());
        if (species_.empty()) {
            // Fallback: re-speciate with a fresh species for the best genome
            Species ns;
            ns.representative = genomes_[bestIdx_];
            ns.members.push_back(bestIdx_);
            ns.bestFitness = bestFit_;
            species_.push_back(ns);
        }
    }
}

// ---- Reproduce ----

void Population::reproduce() {
    float totalAdj = 0;
    for (auto& s : species_) totalAdj += s.totalAdjFitness;
    if (totalAdj <= 0) totalAdj = 1;

    // Allocate offspring
    int totalOff = 0;
    for (auto& s : species_) {
        s.offspring = std::max(1, (int)(s.totalAdjFitness / totalAdj * params.populationSize));
        totalOff += s.offspring;
    }
    // Adjust to match target
    while (totalOff < params.populationSize) { species_[0].offspring++; totalOff++; }
    while (totalOff > params.populationSize) {
        for (auto& s : species_) {
            if (s.offspring > 1 && totalOff > params.populationSize) { s.offspring--; totalOff--; }
        }
    }

    std::vector<Genome> newPop;
    newPop.reserve(params.populationSize);

    for (auto& s : species_) {
        std::sort(s.members.begin(), s.members.end(),
            [&](int a, int b) { return genomes_[a].fitness > genomes_[b].fitness; });

        // Elites
        int elites = std::min(params.elitesPerSpecies, (int)s.members.size());
        for (int i = 0; i < elites && (int)newPop.size() < params.populationSize; i++)
            newPop.push_back(genomes_[s.members[i]]);

        int pool = std::max(1, (int)(s.members.size() * params.survivalFraction));

        // Fill with crossover + mutation
        for (int i = elites; i < s.offspring && (int)newPop.size() < params.populationSize; i++) {
            Genome child;
            if (pool >= 2 && randFloat01() < params.crossoverRate) {
                int p1 = s.members[randInt(pool)];
                int p2 = s.members[randInt(pool)];
                if (genomes_[p1].fitness >= genomes_[p2].fitness)
                    child = Genome::crossover(genomes_[p1], genomes_[p2]);
                else
                    child = Genome::crossover(genomes_[p2], genomes_[p1]);
            } else {
                child = genomes_[s.members[randInt(pool)]];
            }

            if (randFloat01() < params.weightMutateRate)
                child.mutateWeights(params.weightPerturbRate, params.weightPerturbStr);
            if (randFloat01() < params.addConnectionRate)
                child.mutateAddConnection(innov_);
            if (randFloat01() < params.addNodeRate)
                child.mutateAddNode(innov_);
            if (randFloat01() < params.toggleRate)
                child.mutateToggle();

            child.fitness = 0;
            newPop.push_back(child);
        }
    }

    genomes_ = std::move(newPop);
}

// ---- Epoch ----

void Population::epoch() {
    evaluate();
    speciate();
    reproduce();
    generation_++;
}
