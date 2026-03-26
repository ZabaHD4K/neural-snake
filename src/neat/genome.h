#pragma once
#include <vector>
#include <unordered_map>
#include <cstdint>

struct NodeGene {
    int id;
    enum Type { INPUT, OUTPUT, HIDDEN } type;
    float bias = 0.0f;
};

struct ConnectionGene {
    int   inNode;
    int   outNode;
    float weight;
    bool  enabled    = true;
    int   innovation = 0;
};

class InnovationCounter {
public:
    int get(int inNode, int outNode);
    int current() const { return counter_; }
private:
    int counter_ = 0;
    std::unordered_map<uint64_t, int> history_;
    static uint64_t key(int a, int b) { return ((uint64_t)(uint32_t)a << 32) | (uint32_t)b; }
};

class Genome {
public:
    std::vector<NodeGene>       nodes;
    std::vector<ConnectionGene> connections;
    float fitness = 0.0f;

    static Genome createInitial(int numInputs, int numOutputs, InnovationCounter& innov);

    void mutateWeights(float perturbRate, float perturbStr);
    void mutateAddConnection(InnovationCounter& innov);
    void mutateAddNode(InnovationCounter& innov);
    void mutateToggle();

    static Genome crossover(const Genome& better, const Genome& worse);
    static float  compatibility(const Genome& a, const Genome& b, float c1, float c2, float c3);

    int  nextNodeId() const;
    bool hasConnection(int in, int out) const;
    bool wouldCreateCycle(int from, int to) const;
};
