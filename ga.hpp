#pragma once

#include <vector>
#include <random>
#include <algorithm>
#include <functional>

namespace hexaworld {

// Simple Genetic Algorithm for hare behaviors
// For now, evolving reproduction threshold

struct HareGenome {
    float reproduction_threshold = 1.5f;
    float movement_aggression = 0.5f; // 0 = random movement, 1 = always seek plants
    float weight = 1.0f; // 0.5 = light/fast, 1.5 = heavy/slow
    float fear = 0.5f; // 0 = fearless (takes risks), 1 = fearful (avoids danger)
    float movement_efficiency = 1.0f; // 0.5 = inefficient/high cost, 1.5 = efficient/low cost
    bool can_burrow = false; // Can burrow to hide

    HareGenome() = default;
    HareGenome(float thresh, float aggression, float w, float f) : reproduction_threshold(thresh), movement_aggression(aggression), weight(w), fear(f) {}

    HareGenome mutate(std::mt19937& gen) const {
        std::normal_distribution<float> dist(0.0f, 0.1f); // Small mutations
        HareGenome child = *this;
        child.reproduction_threshold += dist(gen);
        child.reproduction_threshold = std::clamp(child.reproduction_threshold, 1.0f, 2.0f);
        child.movement_aggression += dist(gen);
        child.movement_aggression = std::clamp(child.movement_aggression, 0.0f, 1.0f);
        child.weight += dist(gen);
        child.weight = std::clamp(child.weight, 0.5f, 1.5f);
        child.fear += dist(gen);
        child.fear = std::clamp(child.fear, 0.0f, 1.0f);
        child.movement_efficiency += dist(gen);
        child.movement_efficiency = std::clamp(child.movement_efficiency, 0.5f, 1.5f);
        // Burrow trait: rare mutation
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(gen) < 0.01f) {
            child.can_burrow = !child.can_burrow;
        }
        return child;
    }

    bool operator<(const HareGenome& other) const {
        if (reproduction_threshold != other.reproduction_threshold) {
            return reproduction_threshold < other.reproduction_threshold;
        }
        if (movement_aggression != other.movement_aggression) {
            return movement_aggression < other.movement_aggression;
        }
        if (weight != other.weight) {
            return weight < other.weight;
        }
        if (fear != other.fear) {
            return fear < other.fear;
        }
        if (movement_efficiency != other.movement_efficiency) {
            return movement_efficiency < other.movement_efficiency;
        }
        return can_burrow < other.can_burrow;
    }
};

struct FoxGenome {
    float reproduction_threshold = 4.0f;
    float hunting_aggression = 0.5f; // 0 = passive, 1 = aggressive
    float weight = 1.0f; // 0.5 = light/fast, 1.5 = heavy/slow
    float movement_efficiency = 1.0f; // 0.5 = inefficient/high cost, 1.5 = efficient/low cost

    FoxGenome() = default;
    FoxGenome(float thresh, float aggression, float w) : reproduction_threshold(thresh), hunting_aggression(aggression), weight(w) {}

    FoxGenome mutate(std::mt19937& gen) const {
        std::normal_distribution<float> dist(0.0f, 0.1f); // Small mutations
        FoxGenome child = *this;
        child.reproduction_threshold += dist(gen);
        child.reproduction_threshold = std::clamp(child.reproduction_threshold, 2.0f, 4.0f);
        child.hunting_aggression += dist(gen);
        child.hunting_aggression = std::clamp(child.hunting_aggression, 0.0f, 1.0f);
        child.weight += dist(gen);
        child.weight = std::clamp(child.weight, 0.5f, 1.5f);
        child.movement_efficiency += dist(gen);
        child.movement_efficiency = std::clamp(child.movement_efficiency, 0.5f, 1.5f);
        return child;
    }

    bool operator<(const FoxGenome& other) const {
        if (reproduction_threshold != other.reproduction_threshold) {
            return reproduction_threshold < other.reproduction_threshold;
        }
        if (hunting_aggression != other.hunting_aggression) {
            return hunting_aggression < other.hunting_aggression;
        }
        if (weight != other.weight) {
            return weight < other.weight;
        }
        return movement_efficiency < other.movement_efficiency;
    }
};

class GeneticAlgorithm {
public:
    std::vector<HareGenome> population;
    std::mt19937& gen;
    std::function<float(const HareGenome&)> fitness_func;

    GeneticAlgorithm(std::mt19937& rng, std::function<float(const HareGenome&)> fit_func)
        : gen(rng), fitness_func(fit_func) {}

    void initialize(int pop_size) {
        population.resize(pop_size);
        std::uniform_real_distribution<float> thresh_dist(1.0f, 2.0f);
        std::uniform_real_distribution<float> aggression_dist(0.0f, 1.0f);
        std::uniform_real_distribution<float> weight_dist(0.5f, 1.5f);
        std::uniform_real_distribution<float> fear_dist(0.0f, 1.0f);
        for (auto& genome : population) {
            genome.reproduction_threshold = thresh_dist(gen);
            genome.movement_aggression = aggression_dist(gen);
            genome.weight = weight_dist(gen);
            genome.fear = fear_dist(gen);
        }
    }

    void evolve(int generations) {
        for (int gen = 0; gen < generations; ++gen) {
            // Evaluate fitness
            std::vector<std::pair<float, HareGenome>> evaluated;
            for (const auto& genome : population) {
                evaluated.emplace_back(fitness_func(genome), genome);
            }
            // Sort by fitness descending
            std::sort(evaluated.begin(), evaluated.end(), std::greater<>());
            // Select top half
            population.clear();
            for (size_t i = 0; i < evaluated.size() / 2; ++i) {
                population.push_back(evaluated[i].second);
            }
            // Crossover and mutate to fill population
            std::uniform_int_distribution<size_t> dist(0, population.size() - 1);
            while (population.size() < evaluated.size()) {
                // Simple crossover: average
                size_t p1 = dist(this->gen);
                size_t p2 = dist(this->gen);
                HareGenome child;
                child.reproduction_threshold = (population[p1].reproduction_threshold + population[p2].reproduction_threshold) / 2.0f;
                child.movement_aggression = (population[p1].movement_aggression + population[p2].movement_aggression) / 2.0f;
                child.weight = (population[p1].weight + population[p2].weight) / 2.0f;
                child.fear = (population[p1].fear + population[p2].fear) / 2.0f;
                child = child.mutate(this->gen);
                population.push_back(child);
            }
        }
    }

    HareGenome get_best() const {
        HareGenome best;
        float best_fit = -1;
        for (const auto& genome : population) {
            float fit = fitness_func(genome);
            if (fit > best_fit) {
                best_fit = fit;
                best = genome;
            }
        }
        return best;
    }
};

} // namespace hexaworld