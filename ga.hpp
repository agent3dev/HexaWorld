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

    HareGenome() = default;
    HareGenome(float thresh) : reproduction_threshold(thresh) {}

    HareGenome mutate(std::mt19937& gen) const {
        std::normal_distribution<float> dist(0.0f, 0.1f); // Small mutations
        HareGenome child = *this;
        child.reproduction_threshold += dist(gen);
        child.reproduction_threshold = std::clamp(child.reproduction_threshold, 1.0f, 2.0f); // Clamp to reasonable range
        return child;
    }

    bool operator<(const HareGenome& other) const {
        return reproduction_threshold < other.reproduction_threshold;
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
        for (auto& genome : population) {
            std::uniform_real_distribution<float> dist(1.0f, 2.0f);
            genome.reproduction_threshold = dist(gen);
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