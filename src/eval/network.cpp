#include "network.h"
#include <unordered_set>
#include <cmath>
#include <algorithm>

void Network::build(const Genome& genome, int numInputs, int numOutputs) {
    nIn_  = numInputs;
    nOut_ = numOutputs;
    nodes_ = genome.nodes;
    conns_ = genome.connections;
    act_.assign(nodes_.size(), 0);

    idx_.clear();
    for (int i = 0; i < (int)nodes_.size(); i++)
        idx_[nodes_[i].id] = i;

    // Topological sort of non-input nodes (Kahn's algorithm)
    std::unordered_set<int> inputIds;
    std::unordered_map<int, int> inDeg;
    std::unordered_map<int, std::vector<int>> outEdges;

    for (auto& n : nodes_) {
        if (n.type == NodeGene::INPUT)
            inputIds.insert(n.id);
        else
            inDeg[n.id] = 0;
    }

    for (auto& c : conns_) {
        if (!c.enabled) continue;
        if (inDeg.count(c.outNode)) {
            inDeg[c.outNode]++;
            outEdges[c.inNode].push_back(c.outNode);
        }
    }

    evalOrder_.clear();
    std::vector<int> queue;
    for (auto& [id, deg] : inDeg)
        if (deg == 0) queue.push_back(id);

    while (!queue.empty()) {
        int cur = queue.back(); queue.pop_back();
        evalOrder_.push_back(cur);
        for (int tgt : outEdges[cur])
            if (--inDeg[tgt] == 0) queue.push_back(tgt);
    }

    // Catch any nodes missed (shouldn't happen in acyclic graph)
    for (auto& [id, deg] : inDeg) {
        bool found = false;
        for (int eo : evalOrder_) if (eo == id) { found = true; break; }
        if (!found) evalOrder_.push_back(id);
    }
}

void Network::forward(const float* inputs, float* outputs) {
    for (auto& a : act_) a = 0;

    // Set inputs
    for (int i = 0; i < nIn_; i++)
        if (idx_.count(i)) act_[idx_[i]] = inputs[i];

    // Evaluate in topological order
    for (int nid : evalOrder_) {
        auto it = idx_.find(nid);
        if (it == idx_.end()) continue;
        int i = it->second;

        float sum = nodes_[i].bias;
        for (auto& c : conns_) {
            if (!c.enabled || c.outNode != nid) continue;
            auto si = idx_.find(c.inNode);
            if (si != idx_.end())
                sum += c.weight * act_[si->second];
        }
        act_[i] = 1.0f / (1.0f + expf(-sum)); // sigmoid
    }

    // Read outputs
    for (int i = 0; i < nOut_; i++) {
        auto it = idx_.find(nIn_ + i);
        outputs[i] = (it != idx_.end()) ? act_[it->second] : 0.5f;
    }
}
