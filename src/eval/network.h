#pragma once
#include "neat/genome.h"
#include <vector>
#include <unordered_map>

class Network {
public:
    void build(const Genome& genome, int numInputs, int numOutputs);
    void forward(const float* inputs, float* outputs);

    const std::vector<NodeGene>&       nodes()       const { return nodes_; }
    const std::vector<ConnectionGene>& connections() const { return conns_; }
    const std::vector<float>&          activations() const { return act_; }
    int numInputs()  const { return nIn_; }
    int numOutputs() const { return nOut_; }

private:
    std::vector<NodeGene>       nodes_;
    std::vector<ConnectionGene> conns_;
    std::vector<int>            evalOrder_;
    std::vector<float>          act_;
    std::unordered_map<int,int> idx_;   // node id -> index
    int nIn_ = 0, nOut_ = 0;
};
