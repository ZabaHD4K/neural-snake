#include "genome.h"
#include "util/random.h"
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <cstdio>

// ---- InnovationCounter ----

int InnovationCounter::get(int inNode, int outNode) {
    auto k = key(inNode, outNode);
    auto it = history_.find(k);
    if (it != history_.end()) return it->second;
    int id = counter_++;
    history_[k] = id;
    return id;
}

// ---- Genome ----

Genome Genome::createInitial(int numInputs, int numOutputs, InnovationCounter& innov) {
    Genome g;
    for (int i = 0; i < numInputs; i++)
        g.nodes.push_back({i, NodeGene::INPUT, 0.0f});
    for (int i = 0; i < numOutputs; i++)
        g.nodes.push_back({numInputs + i, NodeGene::OUTPUT, randFloat() * 0.5f});

    // Fully connected inputs -> outputs
    for (int i = 0; i < numInputs; i++)
        for (int o = 0; o < numOutputs; o++) {
            int outId = numInputs + o;
            g.connections.push_back({i, outId, randFloat(), true, innov.get(i, outId)});
        }
    return g;
}

int Genome::nextNodeId() const {
    int mx = 0;
    for (auto& n : nodes) mx = std::max(mx, n.id);
    return mx + 1;
}

bool Genome::hasConnection(int in, int out) const {
    for (auto& c : connections)
        if (c.inNode == in && c.outNode == out) return true;
    return false;
}

bool Genome::wouldCreateCycle(int from, int to) const {
    // Adding from->to creates a cycle if there's a path from to->...->from
    int maxId = nextNodeId();
    std::vector<bool> visited(maxId, false);
    std::vector<int> stack = {to};

    while (!stack.empty()) {
        int cur = stack.back(); stack.pop_back();
        if (cur == from) return true;
        if (cur < 0 || cur >= maxId || visited[cur]) continue;
        visited[cur] = true;
        for (auto& c : connections)
            if (c.enabled && c.inNode == cur)
                stack.push_back(c.outNode);
    }
    return false;
}

// ---- Mutations ----

void Genome::mutateWeights(float perturbRate, float perturbStr) {
    for (auto& c : connections) {
        if (randFloat01() < perturbRate)
            c.weight = std::clamp(c.weight + randGauss(perturbStr), -4.0f, 4.0f);
        else
            c.weight = randFloat();
    }
    for (auto& n : nodes) {
        if (n.type == NodeGene::INPUT) continue;
        if (randFloat01() < perturbRate)
            n.bias = std::clamp(n.bias + randGauss(perturbStr * 0.5f), -4.0f, 4.0f);
    }
}

void Genome::mutateAddConnection(InnovationCounter& innov) {
    for (int attempt = 0; attempt < 30; attempt++) {
        auto& src = nodes[randInt((int)nodes.size())];
        auto& dst = nodes[randInt((int)nodes.size())];
        if (src.id == dst.id) continue;
        if (dst.type == NodeGene::INPUT) continue;
        if (hasConnection(src.id, dst.id)) continue;
        if (wouldCreateCycle(src.id, dst.id)) continue;
        connections.push_back({src.id, dst.id, randFloat(), true, innov.get(src.id, dst.id)});
        return;
    }
}

void Genome::mutateAddNode(InnovationCounter& innov) {
    std::vector<int> enabled;
    for (int i = 0; i < (int)connections.size(); i++)
        if (connections[i].enabled) enabled.push_back(i);
    if (enabled.empty()) return;

    int idx = enabled[randInt((int)enabled.size())];
    auto old = connections[idx]; // copy before modifying
    connections[idx].enabled = false;

    int newId = nextNodeId();
    nodes.push_back({newId, NodeGene::HIDDEN, 0.0f});
    connections.push_back({old.inNode, newId,      1.0f,       true, innov.get(old.inNode, newId)});
    connections.push_back({newId,      old.outNode, old.weight, true, innov.get(newId, old.outNode)});
}

void Genome::mutateToggle() {
    if (connections.empty()) return;
    auto& c = connections[randInt((int)connections.size())];
    c.enabled = !c.enabled;
}

// ---- Crossover ----

Genome Genome::crossover(const Genome& better, const Genome& worse) {
    Genome child;
    child.nodes = better.nodes;

    std::unordered_map<int, int> worseMap;
    for (int i = 0; i < (int)worse.connections.size(); i++)
        worseMap[worse.connections[i].innovation] = i;

    for (auto& bc : better.connections) {
        auto it = worseMap.find(bc.innovation);
        if (it != worseMap.end() && randFloat01() < 0.5f)
            child.connections.push_back(worse.connections[it->second]);
        else
            child.connections.push_back(bc);
    }

    // Ensure child has all referenced nodes
    std::unordered_set<int> ids;
    for (auto& n : child.nodes) ids.insert(n.id);
    for (auto& c : child.connections) {
        for (int nid : {c.inNode, c.outNode}) {
            if (ids.count(nid)) continue;
            for (auto& n : worse.nodes) {
                if (n.id == nid) { child.nodes.push_back(n); ids.insert(nid); break; }
            }
        }
    }
    return child;
}

// ---- Save / Load ----

bool Genome::saveToFile(const std::string& path) const {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;

    fprintf(f, "NEAT_GENOME v1\n");
    fprintf(f, "fitness %.6f\n", fitness);
    fprintf(f, "nodes %d\n", (int)nodes.size());
    for (auto& n : nodes)
        fprintf(f, "N %d %d %.6f\n", n.id, (int)n.type, n.bias);
    fprintf(f, "connections %d\n", (int)connections.size());
    for (auto& c : connections)
        fprintf(f, "C %d %d %.6f %d %d\n", c.inNode, c.outNode, c.weight, c.enabled ? 1 : 0, c.innovation);

    fclose(f);
    return true;
}

Genome Genome::loadFromFile(const std::string& path) {
    Genome g;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return g;

    char header[64];
    if (fscanf(f, "%63s %*s", header) < 1 || std::string(header) != "NEAT_GENOME") { fclose(f); return g; }

    fscanf(f, " fitness %f", &g.fitness);

    int numNodes = 0;
    fscanf(f, " nodes %d", &numNodes);
    for (int i = 0; i < numNodes; i++) {
        NodeGene n;
        int type;
        fscanf(f, " N %d %d %f", &n.id, &type, &n.bias);
        n.type = (NodeGene::Type)type;
        g.nodes.push_back(n);
    }

    int numConns = 0;
    fscanf(f, " connections %d", &numConns);
    for (int i = 0; i < numConns; i++) {
        ConnectionGene c;
        int en;
        fscanf(f, " C %d %d %f %d %d", &c.inNode, &c.outNode, &c.weight, &en, &c.innovation);
        c.enabled = (en != 0);
        g.connections.push_back(c);
    }

    fclose(f);
    return g;
}

// ---- Compatibility ----

float Genome::compatibility(const Genome& a, const Genome& b, float c1, float c2, float c3) {
    if (a.connections.empty() && b.connections.empty()) return 0;

    std::unordered_map<int, int> mapA, mapB;
    int maxA = 0, maxB = 0;
    for (int i = 0; i < (int)a.connections.size(); i++) {
        int inn = a.connections[i].innovation;
        mapA[inn] = i;
        maxA = std::max(maxA, inn);
    }
    for (int i = 0; i < (int)b.connections.size(); i++) {
        int inn = b.connections[i].innovation;
        mapB[inn] = i;
        maxB = std::max(maxB, inn);
    }

    int minMax = std::min(maxA, maxB);
    int excess = 0, disjoint = 0, matching = 0;
    float wDiff = 0;

    std::unordered_set<int> all;
    for (auto& [k, v] : mapA) all.insert(k);
    for (auto& [k, v] : mapB) all.insert(k);

    for (int inn : all) {
        bool inA = mapA.count(inn), inB = mapB.count(inn);
        if (inA && inB) {
            matching++;
            wDiff += std::abs(a.connections[mapA[inn]].weight - b.connections[mapB[inn]].weight);
        } else if (inn > minMax)
            excess++;
        else
            disjoint++;
    }

    // Cap N to prevent speciation collapse with many inputs.
    // With 89 inputs (356+ connections), N=356 makes excess/disjoint negligible.
    // Capping at 100 keeps structural differences meaningful.
    float N = (float)std::max(a.connections.size(), b.connections.size());
    if (N < 20) N = 1;
    else if (N > 100) N = 100;
    float avgW = matching > 0 ? wDiff / matching : 0;
    return c1 * excess / N + c2 * disjoint / N + c3 * avgW;
}
